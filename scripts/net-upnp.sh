#!/usr/bin/env bash
# UPnP's test (net_upnp; src/common/upnp.cpp): a fake router on this PC (its description, its WANIPConnection control,
# which takes permanent leases alone as some routers do, and an SSDP answer), and a host by its IP address that must
# have it forward its UDP port: told the description's address (NF_NET_UPNP_AT), refused a lease of an hour, asked
# again for a permanent one, and the request naming the port, UDP and this PC. SEARCH=1 finds it by the SSDP search
# instead, multicast on this PC's real network: there the fake answers every device's search, and the game asks
# every router that answers it, the real one too. Only on a network of your own.
#   [SEARCH=1] scripts/net-upnp.sh [seconds, default 20]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-20}
[ -d generated/game ] || { echo "net-upnp: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-UPNP FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/upnp-run
rm -rf "$RUN"
mkdir -p "$RUN/1"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
ROUTER_PORT=${ROUTER_PORT:-26594}
GAME_PORT=26587
SEARCH=${SEARCH:-0}
python3 - "$ROUTER_PORT" "$RUN/asked.txt" "$SEARCH" > "$RUN/router.out" 2>&1 <<'EOF' &
import socket, struct, sys, threading
from http.server import BaseHTTPRequestHandler, HTTPServer
port, asked, search = int(sys.argv[1]), sys.argv[2], sys.argv[3] == "1"
DESC = """<?xml version="1.0"?><root xmlns="urn:schemas-upnp-org:device-1-0"><device>
<deviceType>urn:schemas-upnp-org:device:InternetGatewayDevice:1</deviceType><deviceList><device>
<deviceType>urn:schemas-upnp-org:device:WANDevice:1</deviceType><serviceList><service>
<serviceType>urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1</serviceType><controlURL>/wrong</controlURL>
</service></serviceList><deviceList><device><serviceList><service>
<serviceType>urn:schemas-upnp-org:service:WANIPConnection:1</serviceType>
<controlURL>/ctl/IPConn</controlURL></service></serviceList></device></deviceList></device></deviceList>
</device></root>"""
FAULT = """<?xml version="1.0"?><s:Envelope xmlns:s="http://schemas.xmlsoap.org/soap/envelope/"><s:Body><s:Fault>
<faultcode>s:Client</faultcode><faultstring>UPnPError</faultstring><detail><UPnPError
xmlns="urn:schemas-upnp-org:control-1-0"><errorCode>725</errorCode><errorDescription>OnlyPermanentLeasesSupported
</errorDescription></UPnPError></detail></s:Fault></s:Body></s:Envelope>"""
class H(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def answer(self, code, body):
        b = body.encode()
        self.send_response(code)
        self.send_header("Content-Type", "text/xml")
        self.send_header("Transfer-Encoding", "chunked")
        self.end_headers()
        for i in range(0, len(b), 100):
            part = b[i:i + 100]
            self.wfile.write(b"%x\r\n" % len(part) + part + b"\r\n")
        self.wfile.write(b"0\r\n\r\n")
    def do_GET(self):
        self.answer(200, DESC) if self.path == "/desc.xml" else self.answer(404, "")
    def do_POST(self):
        body = self.rfile.read(int(self.headers.get("Content-Length", 0))).decode()
        with open(asked, "a") as f:
            f.write(self.headers.get("SOAPAction", "") + " " + body + "\n")
        if self.path != "/ctl/IPConn":
            return self.answer(404, "")
        if "<NewLeaseDuration>0<" not in body:
            return self.answer(500, FAULT)
        self.answer(200, "<?xml version=\"1.0\"?><s:Envelope><s:Body><u:AddPortMappingResponse/></s:Body></s:Envelope>")
    def log_message(self, *a):
        pass
def ssdp():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("", 1900))
    s.setsockopt(socket.IPPROTO_IP, socket.IP_ADD_MEMBERSHIP, struct.pack("4s4s", socket.inet_aton("239.255.255.250"),
                                                                          socket.inet_aton("0.0.0.0")))
    while True:
        data, ep = s.recvfrom(2048)
        if data.startswith(b"M-SEARCH"):
            print("searched by", ep, flush=True)
            s.sendto(b"HTTP/1.1 200 OK\r\nCACHE-CONTROL: max-age=120\r\nST: upnp:rootdevice\r\nUSN: uuid:fake\r\n"
                     b"Location: http://127.0.0.1:%d/desc.xml\r\n\r\n" % port, ep)
if search:
    threading.Thread(target=ssdp, daemon=True).start()
HTTPServer(("127.0.0.1", port), H).serve_forever()
EOF
sleep 0.5
at=""
[ "$SEARCH" = 1 ] || at="http://127.0.0.1:$ROUTER_PORT/desc.xml"
(
  cd "$RUN/1"
  export INSTANCE=1 SCRIPT="$PAD" NF_SKIP_INTRO=1 NF_NET_PORT=$GAME_PORT NF_NET_NAME=Machine1 NF_NET_MASTER=none \
    NF_NET_FULLSCREEN=0 NF_NET=host NF_NET_WAIT=$SECS NF_NET_PLAYERS=2 NF_NET_UPNP_AT="$at"
  exec "$EXE" "$GAME"
) > "$RUN/1/run.out" 2>&1 &
sleep "$SECS"
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
grep -h 'UPnP\|router' "$RUN"/1/logs/*.log | sed 's/^.*\] //'
cat "$RUN/router.out"
fail=""
grep -q "the router forwards UDP $GAME_PORT to this PC now" "$RUN"/1/logs/*.log || fail="the router was not asked, or refused"
[ "$(grep -c "^\"urn:.*#AddPortMapping\"" "$RUN/asked.txt" 2>/dev/null)" = 2 ] || fail="${fail:-not asked twice, for an hour then for good}"
grep -q "<NewExternalPort>$GAME_PORT</NewExternalPort><NewProtocol>UDP</NewProtocol><NewInternalPort>$GAME_PORT</NewInternalPort><NewInternalClient>127.0.0.1</NewInternalClient><NewEnabled>1</NewEnabled><NewPortMappingDescription>NightFire Online</NewPortMappingDescription><NewLeaseDuration>0<" "$RUN/asked.txt" 2>/dev/null ||
  fail="${fail:-the request did not name the port, UDP and this PC}"
grep -q '/wrong' "$RUN/asked.txt" 2>/dev/null && fail="${fail:-it asked the wrong service}"
how="named by NF_NET_UPNP_AT"
[ "$SEARCH" = 1 ] && how="found by SSDP"
if [ -z "$fail" ]; then
  echo "NET-UPNP OK: the router, $how, forwards UDP $GAME_PORT: a permanent lease after it refused an hour's"
  exit 0
fi
echo "NET-UPNP FAIL: $fail (the run: $RUN)"
exit 1
