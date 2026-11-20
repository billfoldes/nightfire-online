#!/usr/bin/env python3
"""NightFire's internet game list: the master server, for a VPS or any PC with an open port.

Hosts whose net_who lists their game announce it here over HTTP every minute; the game's F1 Server Browser fetches the
list and asks each host itself (UDP, its game port) how it is doing and how far away it is. A host's address is the
one its announcement came from: no host can list another machine. Python 3.8+, the standard library alone.

    python3 master.py [--port 26580] [--bind 0.0.0.0] [--trust-proxy] [--relay 26581] [--servers servers.txt]

Players set net_master to http://<this machine>:26580 (or the https address of a proxy in front of it). The same
protocol is served by worker.js on Cloudflare; README.md has both.

    POST /announce  form: port, version, flags, in, wanted, id, name, note[, code, relay]  ->  "ok\t<address seen>\n"
                    or "ok\t<address seen>\treachable|unreachable\n": a game by its address (no code) is asked from
                    here, as a browser asks it (UDP, its port), and the host told whether the internet reaches it
    POST /remove    form: port                                                           ->  "ok\n"
    GET  /list      ->  "NFM1\n" then a line a game: address, port, version, flags, in, wanted, id, name, note,
                        code, relay (tabs). A game with a code (hosted through a relay) shows "-" as its address;
                        code "-": a relay's game whose host gives its code to its players alone
    GET  /servers   ->  "NFS1\n" then the --servers file's lines, "list <url>" or "relay <host:port>": the other lists
                        and relays there are, which the game keeps for its next start (read again at each request)

--relay also runs the relay (UDP, players' net_relay = <this machine>:26581): a host registers and gets a join code,
joiners ask for the code's game, and the relay passes each packet of theirs on, so no player sees another's address
and nobody forwards a port. Its packets: "NFR1", a type, then
    1 register  host -> relay: u32 token, the 6-letter code it had (zeros: none)
    2 code      relay -> host: the code
    3 join      joiner -> relay: the code, the u16 connection it had (0: none)
    4 joined    relay -> joiner: the u16 connection, the code
    5 refused   relay -> either: 1 no game has the code, 2 that game is full, 3 the relay is full; the code
    6 data      either way: the u16 connection (the joiner's), the game's packet; passed on as it came
Integers little endian. Hosts register every 5 s and joiners ask every 5 s; what is not heard of for a minute goes.
"""
import argparse
import secrets
import socket
import struct
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs

EXPIRE_S = 150      # a host announces every 60 s
PER_ADDRESS = 8     # games one address may list
TOTAL = 1000
NAME_LEN, NOTE_LEN = 15, 40
CODE_CHARS = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"
CODE_LEN, RELAY_LEN = 6, 64

games = {}  # (address, port) -> dict
lock = threading.Lock()
checked = {}  # (address, port) -> (when, reachable): asked again after REACHED_S, or UNREACHED_S if it did not answer
REACHED_S, UNREACHED_S = 300, 20
FIND_MAGIC, FIND_QUERY, FIND_INFO = 0x4E464631, 1, 2  # the game's LAN browser query (net.cpp's kFindQuery)


def reachable(addr, port):
    """The game at addr:port answers its browser query from here: the internet reaches it (its port is forwarded)."""
    token = secrets.randbits(32)
    query = struct.pack("<IBI", FIND_MAGIC, FIND_QUERY, token)
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        s.settimeout(0.5)
        for _ in range(3):
            try:
                s.sendto(query, (addr, port))
                end = time.monotonic() + 0.5
                while time.monotonic() < end:
                    data, src = s.recvfrom(512)
                    if src[0] == addr and len(data) >= 9 and data[:4] == query[:4] and data[4] == FIND_INFO and \
                            data[5:9] == query[5:9]:
                        return True
            except socket.timeout:
                continue
            except OSError:
                return False
    return False


def check(addr, port):
    now = time.time()
    with lock:
        for key in [k for k, (t, ok) in checked.items() if now - t > REACHED_S]:
            del checked[key]
        was = checked.get((addr, port))
        if was and now - was[0] < (REACHED_S if was[1] else UNREACHED_S):
            return was[1]
    ok = reachable(addr, port)
    with lock:
        checked[(addr, port)] = (now, ok)
    return ok


def code_ok(code):
    return len(code) == CODE_LEN and all(c in CODE_CHARS for c in code)


class Relay:
    MAGIC = b"NFR1"
    REGISTER, CODE, JOIN, JOINED, REFUSED, DATA = range(1, 7)
    NO_GAME, FULL, BUSY = 1, 2, 3
    EXPIRE_S = 60
    PER_ADDRESS = 8     # hosts one address may register
    HOSTS = 500
    PER_GAME = 16       # joiners a game may have
    LARGEST = 1500      # a game's packet
    RATE = 5000         # packets a second from one address and port (a rejoin's state comes in bursts)
    MISSES = 20         # codes of no game one address may ask for a minute (a hidden code is not guessed)

    def __init__(self, sock):
        self.sock = sock
        self.hosts = {}    # code -> {"ep", "token", "seen", "conns"}
        self.host_at = {}  # (address, port) -> code
        self.conns = {}    # connection -> {"ep", "code", "seen"}
        self.conn_at = {}  # (address, port) -> connection
        self.next_conn = 0
        self.counted, self.second = {}, 0
        self.missed, self.minute = {}, 0  # address -> codes of no game asked for this minute

    def send(self, ep, kind, body):
        self.sock.sendto(self.MAGIC + bytes([kind]) + body, ep)

    def drop_conn(self, conn):
        c = self.conns.pop(conn, None)
        if c:
            self.conn_at.pop(c["ep"], None)
            if h := self.hosts.get(c["code"]):
                h["conns"].discard(conn)

    def drop_host(self, code):
        h = self.hosts.pop(code)
        self.host_at.pop(h["ep"], None)
        for conn in list(h["conns"]):
            self.drop_conn(conn)

    def expire(self, now):
        for code in [c for c, h in self.hosts.items() if now - h["seen"] > self.EXPIRE_S]:
            self.drop_host(code)
        for conn in [c for c, j in self.conns.items() if now - j["seen"] > self.EXPIRE_S]:
            self.drop_conn(conn)

    def register(self, ep, data, now):
        if len(data) < 9 + CODE_LEN:
            return
        token, had = data[5:9], data[9:9 + CODE_LEN].decode("ascii", "replace")
        code = self.host_at.get(ep)
        if code is None:
            h = self.hosts.get(had)
            if h and h["token"] == token:  # the same host from another port (a relaunched exe)
                self.host_at.pop(h["ep"], None)
                h["ep"] = ep
            elif len(self.hosts) >= self.HOSTS or sum(a == ep[0] for a, _ in self.host_at) >= self.PER_ADDRESS:
                return self.send(ep, self.REFUSED, bytes([self.BUSY]) + bytes(CODE_LEN))
            else:  # the code it had if free (this relay restarted), else a new one
                code = had if code_ok(had) and had not in self.hosts else None
                while code is None or code in self.hosts:
                    code = "".join(secrets.choice(CODE_CHARS) for _ in range(CODE_LEN))
                self.hosts[code] = {"ep": ep, "token": token, "seen": now, "conns": set()}
                had = code
            code = had
            self.host_at[ep] = code
        self.hosts[code]["seen"] = now
        self.send(ep, self.CODE, code.encode())

    def join(self, ep, data, now):
        if len(data) < 5 + CODE_LEN + 2:
            return
        raw = data[5:5 + CODE_LEN]
        code, (had,) = raw.decode("ascii", "replace"), struct.unpack_from("<H", data, 5 + CODE_LEN)
        if int(now // 60) != self.minute:
            self.minute, self.missed = int(now // 60), {}
        if self.missed.get(ep[0], 0) >= self.MISSES:
            return
        h = self.hosts.get(code)
        if not h:
            self.missed[ep[0]] = self.missed.get(ep[0], 0) + 1
            return self.send(ep, self.REFUSED, bytes([self.NO_GAME]) + raw)
        conn = self.conn_at.get(ep)
        if conn is not None and self.conns[conn]["code"] != code:
            self.drop_conn(conn)
            conn = None
        if conn is None:
            if len(h["conns"]) >= self.PER_GAME or len(self.conns) >= self.HOSTS * self.PER_GAME:
                return self.send(ep, self.REFUSED, bytes([self.FULL]) + raw)
            if had and had not in self.conns:
                conn = had
            else:
                while conn is None or conn in self.conns:
                    self.next_conn = self.next_conn % 65535 + 1
                    conn = self.next_conn
            self.conns[conn] = {"ep": ep, "code": code}
            self.conn_at[ep] = conn
            h["conns"].add(conn)
        self.conns[conn]["seen"] = now
        self.send(ep, self.JOINED, struct.pack("<H", conn) + raw)

    def data(self, ep, data, now):
        if len(data) < 7 or len(data) > 7 + self.LARGEST:
            return
        (conn,) = struct.unpack_from("<H", data, 5)
        j = self.conns.get(conn)
        if not j:
            return
        if (code := self.host_at.get(ep)) is not None:
            if j["code"] == code:
                self.hosts[code]["seen"] = now
                self.sock.sendto(data, j["ep"])
        elif j["ep"] == ep and (h := self.hosts.get(j["code"])):
            j["seen"] = now
            self.sock.sendto(data, h["ep"])

    def run(self):
        last = 0
        while True:
            try:
                data, ep = self.sock.recvfrom(2048)
            except socket.timeout:
                data, ep = b"", None
            except OSError:  # Windows: an ICMP port unreachable from an earlier send
                continue
            now = time.monotonic()
            if now - last >= 5:
                last = now
                self.expire(now)
            if len(data) < 5 or data[:4] != self.MAGIC:
                continue
            if int(now) != self.second:
                self.second, self.counted = int(now), {}
            self.counted[ep] = self.counted.get(ep, 0) + 1
            if self.counted[ep] > self.RATE:
                continue
            handler = {self.REGISTER: self.register, self.JOIN: self.join, self.DATA: self.data}.get(data[4])
            if handler:
                try:
                    handler(ep, data, now)
                except OSError:
                    pass


def clean(text, length):
    return "".join(c for c in text if c.isprintable() and c != "\t")[:length]


def number(form, key, low, high):
    try:
        value = int(form.get(key, ["0"])[0])
    except ValueError:
        return None
    return value if low <= value <= high else None


def expire(now):
    for key in [k for k, g in games.items() if now - g["seen"] > EXPIRE_S]:
        del games[key]


def servers(path):
    """The --servers file's "list <url>" and "relay <host:port>" lines (# starts a comment)."""
    out = []
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                words = line.split("#")[0].split()
                if len(words) == 2 and words[0] in ("list", "relay") and len(words[1]) <= 100:
                    out.append(" ".join(words))
    except OSError:
        pass
    return out[:16]


class Handler(BaseHTTPRequestHandler):
    server_version = "NightFireMaster/1"
    trust_proxy = False
    servers_file = None

    def address(self):
        if self.trust_proxy:
            for header in ("CF-Connecting-IP", "X-Real-IP", "X-Forwarded-For"):
                if value := self.headers.get(header):
                    return value.split(",")[0].strip()
        return self.client_address[0]

    def reply(self, code, text):
        body = text.encode()
        self.send_response(code)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def form(self):
        length = min(int(self.headers.get("Content-Length") or 0), 4096)
        return parse_qs(self.rfile.read(length).decode("utf-8", "replace"))

    def do_GET(self):
        if self.path.split("?")[0] == "/servers":
            return self.reply(200, "\n".join(["NFS1", *(servers(self.servers_file) if self.servers_file else [])]) + "\n")
        if self.path.split("?")[0] != "/list":
            return self.reply(404, "not here: GET /list\n")
        lines = ["NFM1"]
        with lock:
            expire(time.time())
            for (addr, port), g in games.items():
                shown = ("-", 0) if g["code"] else (addr, port)
                lines.append("\t".join(str(v) for v in (*shown, g["version"], g["flags"], g["in"], g["wanted"],
                                                         g["id"], g["name"], g["note"], g["code"], g["relay"])))
        self.reply(200, "\n".join(lines) + "\n")

    def do_POST(self):
        path, form, addr = self.path.split("?")[0], self.form(), self.address()
        port = number(form, "port", 1, 65535)
        if port is None:
            return self.reply(400, "port\n")
        if path == "/remove":
            with lock:
                games.pop((addr, port), None)
            return self.reply(200, "ok\n")
        if path != "/announce":
            return self.reply(404, "not here\n")
        fields = {k: number(form, k, 0, hi) for k, hi in
                  (("version", 65535), ("flags", 255), ("in", 255), ("wanted", 255), ("id", 0xFFFFFFFF))}
        if None in fields.values():
            return self.reply(400, "fields\n")
        code, relay = form.get("code", [""])[0], clean(form.get("relay", [""])[0], RELAY_LEN)
        if code and not ((code == "-" or (code.startswith("NF-") and code_ok(code[3:]))) and relay):
            return self.reply(400, "code\n")
        now = time.time()
        with lock:
            expire(now)
            key = (addr, port)
            if key not in games and (len(games) >= TOTAL or sum(a == addr for a, _ in games) >= PER_ADDRESS):
                return self.reply(429, "the list is full\n")
            games[key] = dict(fields, seen=now, name=clean(form.get("name", [""])[0], NAME_LEN),
                              note=clean(form.get("note", [""])[0], NOTE_LEN), code=code, relay=relay if code else "")
        if code or ":" in addr:  # through a relay, or an IPv6 address the game's IPv4 socket cannot be asked at
            return self.reply(200, f"ok\t{addr}\n")
        self.reply(200, f"ok\t{addr}\t{'reachable' if check(addr, port) else 'unreachable'}\n")

    def log_message(self, fmt, *args):
        pass


def main():
    p = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    p.add_argument("--port", type=int, default=26580)
    p.add_argument("--bind", default="0.0.0.0")
    p.add_argument("--trust-proxy", action="store_true",
                   help="behind nginx, Caddy or Cloudflare: take the player's address from the proxy's header")
    p.add_argument("--relay", type=int, default=0, metavar="PORT",
                   help="also run the relay on this UDP port (26581 is the game's default)")
    p.add_argument("--servers", metavar="FILE",
                   help='the other lists and relays to tell the game of: lines "list <url>" and "relay <host:port>"')
    a = p.parse_args()
    Handler.trust_proxy = a.trust_proxy
    Handler.servers_file = a.servers
    server = ThreadingHTTPServer((a.bind, a.port), Handler)
    print(f"NightFire game list on {a.bind}:{a.port}", flush=True)
    if a.relay:
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 << 20)
        sock.bind((a.bind, a.relay))
        sock.settimeout(1)
        threading.Thread(target=Relay(sock).run, daemon=True).start()
        print(f"NightFire relay on UDP {a.bind}:{a.relay}", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
