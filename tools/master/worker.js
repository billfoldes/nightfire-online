// NightFire's internet game list on Cloudflare: master.py's protocol (see there), the list held in memory by one
// Durable Object (a Worker alone has no memory shared between requests). Hosts announce every minute, so a list lost
// when the object is evicted is whole again a minute on. README.md: deploying it with wrangler.

// A Worker cannot relay UDP: a game hosted through master.py's relay is listed here by its code and relay all the same.
// GET /servers answers the SERVERS variable (wrangler.toml's [vars]): lines "list <url>" and "relay <host:port>", the
// other lists and relays there are, which the game keeps for its next start.
const EXPIRE_MS = 150 * 1000, PER_ADDRESS = 8, TOTAL = 1000, NAME_LEN = 15, NOTE_LEN = 40, RELAY_LEN = 64;

function text(status, body) {
  return new Response(body, {status, headers: {"Content-Type": "text/plain; charset=utf-8", "Cache-Control": "no-store"}});
}

function clean(s, length) {
  return [...(s || "")].filter(c => c >= " " && c !== "\x7f").join("").slice(0, length);
}

function number(form, key, low, high) {
  const s = form.get(key) ?? "0";
  if (!/^\d+$/.test(s)) return null;
  const v = Number(s);
  return v >= low && v <= high ? v : null;
}

export class Lobby {
  constructor(state, env) {
    this.games = new Map();  // "address port" -> game
  }

  expire(now) {
    for (const [key, g] of this.games) if (now - g.seen > EXPIRE_MS) this.games.delete(key);
  }

  async fetch(request) {
    const url = new URL(request.url), now = Date.now();
    const addr = request.headers.get("CF-Connecting-IP") || "0.0.0.0";
    this.expire(now);
    if (request.method === "GET" && url.pathname === "/list") {
      const lines = ["NFM1"];
      for (const g of this.games.values())
        lines.push([...(g.code ? ["-", 0] : [g.addr, g.port]), g.version, g.flags, g.in, g.wanted, g.id, g.name, g.note,
                    g.code, g.relay].join("\t"));
      return text(200, lines.join("\n") + "\n");
    }
    if (request.method !== "POST") return text(404, "not here: GET /list\n");
    const form = new URLSearchParams((await request.text()).slice(0, 4096));
    const port = number(form, "port", 1, 65535);
    if (port === null) return text(400, "port\n");
    const key = addr + " " + port;
    if (url.pathname === "/remove") {
      this.games.delete(key);
      return text(200, "ok\n");
    }
    if (url.pathname !== "/announce") return text(404, "not here\n");
    const g = {addr, port, seen: now};
    for (const [k, hi] of [["version", 65535], ["flags", 255], ["in", 255], ["wanted", 255], ["id", 0xFFFFFFFF]])
      if ((g[k] = number(form, k, 0, hi)) === null) return text(400, "fields\n");
    if (!this.games.has(key)) {
      let mine = 0;
      for (const o of this.games.values()) mine += o.addr === addr;
      if (this.games.size >= TOTAL || mine >= PER_ADDRESS) return text(429, "the list is full\n");
    }
    g.name = clean(form.get("name"), NAME_LEN);
    g.note = clean(form.get("note"), NOTE_LEN);
    g.code = form.get("code") || "";
    g.relay = g.code ? clean(form.get("relay"), RELAY_LEN) : "";
    // code "-": a relay's game whose host gives its code to its players alone
    if (g.code && !(/^(-|NF-[ABCDEFGHJKLMNPQRSTUVWXYZ23456789]{6})$/.test(g.code) && g.relay)) return text(400, "code\n");
    this.games.set(key, g);
    return text(200, "ok\t" + addr + "\n");
  }
}

function servers(text) {
  return (text || "").split("\n").map(l => l.split("#")[0].trim().split(/\s+/))
    .filter(w => w.length === 2 && (w[0] === "list" || w[0] === "relay") && w[1].length <= 100)
    .slice(0, 16).map(w => w.join(" "));
}

export default {
  async fetch(request, env) {
    if (request.method === "GET" && new URL(request.url).pathname === "/servers")
      return text(200, ["NFS1", ...servers(env.SERVERS)].join("\n") + "\n");
    return env.LOBBY.get(env.LOBBY.idFromName("lobby")).fetch(request);
  },
};
