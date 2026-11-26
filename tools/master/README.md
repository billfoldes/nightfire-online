# NightFire's server list

The server behind F1 > Server Browser's internet games. Anyone can run one. The Windows exes know the project's own
lists and relays, so a player sets nothing; **Server Browser's game list** (`net_master` in settings.toml, under
Servers) adds others,
or `none` for this network's games alone. Hosts whose **Who can join** lists their game
(`net_who` Anyone, or Only people I tell) announce their game to it every minute. Each player's game fetches the list and then asks every host
on it directly how it is doing and how far away it is. The list holds no game data, only who is hosting where.

A host's listed address is the address its announcement came from, so nobody can put someone else's PC on the list.
Listing does show the host's IP address to everyone who looks, and the host's game port (UDP 26565 unless
`net_port` says otherwise) must be forwarded on its router. Off by default. The relay below avoids both.

## The relay: join codes, no IP shown, no port forwarding

`master.py --relay 26581` also runs a relay on UDP 26581. The game knows the project's relays; **Relay server**
(`net_relay`) puts `<your server>` (or `<your server>:<port>` for another port) before them, or `none` for no relay. A host whose
**How players reach me** is **A join code through a relay** (`net_host_by` 1; the old `net_host_relay` reads as it) gets a join code such as `NF-K7Q3MZ` on its connection screen and in its log. Joiners type the
code as the host's address (`join_ip`). Each machine then talks only to the relay, which passes each packet on, so:

- no player sees another player's IP address, only the relay's (unless both connect directly, below),
- the host forwards no port (its own traffic to the relay opens the way back through its router),
- a game on the server list shows its code and relay, never its address. Pressing it in F1 > Server Browser joins with
  the host's relay (`join_ip` becomes `NF-K7Q3MZ@<relay>`), whatever the player's own Relay server says.

A host keeps its code while it plays (across the switch to Driving.xbe and F1's Restart online play). It gets a new
one the next time the game is started. The relay forgets a game a minute after its host stops. Players on the
host's own network can still join it by its address.

Each packet goes to the relay and back out, so the relay's distance adds to the game's delay: pick a server near the
players. So a host and a joiner that both have **Connect directly when possible** on (`net_direct`, the default) are
told each other's address by the relay and punch through their routers to each other; once that works both ways their
packets go straight and the relay only hears from them every 5 s, until the direct path stops. Most home routers let
it through; a pair it does not work for stays on the relay. Either one turning it off keeps their addresses from each
other, as before. Each player's input is at most about 16 KB/s, and the host sends each joiner everyone else's, so a game of 4
passes up to about 200 KB/s each way through the relay. A rejoin's 27 MB state passes through it in a few seconds. A
Cloudflare Worker cannot relay UDP, so run the relay on a VPS. The list can still be on Cloudflare: it shows relayed
games by their code. One address can host 8 relayed games and a game takes 16 joiners. Packets beyond 5000 a second from
one player are dropped.

Open UDP 26581 in the firewall as well as TCP 26580.

## Several lists and relays, so none is the only one

The game asks every list it knows, at once: a host announces its game to each, and a browser shows every list's
games (a game on two lists once). A list that does not answer is said once in the host's log and skipped. A host
through a relay takes the first relay that gives it a code (one that refuses, or says nothing for 3 s, gives way to
the next), and a joiner typing a bare `NF-` code asks each relay in turn until one has the game. A code from the
browser or the host's Copy carries its relay (`NF-K7Q3MZ@<relay>`) and asks that one alone.

The lists it knows are, in order: the settings' own, the exe's built-in ones (`BuiltInServers` in
`src/common/net.cpp`: the Cloudflare list alone, so the release carries no IP address), then those the lists named
the last time it asked. A first start knows no relay until a list names one: a host through a relay (or a joiner by
a bare code) waits for the lists' answer, 8 s at most (`scripts/net-first-start.sh`). Once a start, it asks every list
`GET /servers` and keeps the answers, plus every list that answered, in `servers.txt` beside the exe. So when a
new list or relay comes up, the running lists name it and every copy already out there learns of it; when one goes
away, the copies still have the others. Each list says what it names from its own file:

```
python3 tools/master/master.py --port 26580 --relay 26581 --servers /opt/nightfire/servers.txt
```

with lines like

```
list https://nightfire-master.example.workers.dev
list http://203.0.113.7:26580
relay 203.0.113.7:26581
```

(`#` starts a comment; the file is read again at every request, so an edit needs no restart). On Cloudflare the same
lines go in the `SERVERS` variable (`[vars]` in wrangler.toml, or the dashboard's Variables). Each list should name
every other one and every relay, itself too if it likes. A domain name instead of an IP address lets a server move
without anyone noticing.

## On a VPS (or any always-on PC)

```
python3 tools/master/master.py --port 26580
```

Python 3.8 or newer, nothing to install. Open TCP 26580 in the firewall; to reach players, add
`list http://<your server>:26580` to the other lists' `--servers` files (or to `BuiltInServers` for the next release),
or players set `net_master = "http://<your server>:26580"`. Behind nginx, Caddy or Cloudflare's proxy (for https), add
`--trust-proxy` so the player's own address is read from the proxy's header.

To keep it running on a Linux server with systemd, copy `master.py` to `/opt/nightfire/` and
`nightfire-master.service` (list on TCP 26580, relay on UDP 26581: edit its `ExecStart` line for other ports or no
relay) to `/etc/systemd/system/`, then:

```
sudo systemctl daemon-reload
sudo systemctl enable --now nightfire-master
journalctl -u nightfire-master -f     # its log
```

## On Cloudflare (free plan)

```
cd tools/master
npx wrangler login
npx wrangler deploy
```

That deploys `worker.js` with the Durable Object in `wrangler.toml` that holds the list, at
`https://nightfire-master.<your account>.workers.dev`: name it in the other lists' `--servers` files and
`BuiltInServers`, as above. `node tools/master/test-worker.mjs` checks the
script without an account.

## The protocol

```
POST /announce  form: port, version, flags, in, wanted, id, name, note[, code, relay]  ->  "ok\t<address seen>\n"
POST /remove    form: port                                                           ->  "ok\n"
GET  /list      ->  "NFM1\n", then a line per game: address, port, version, flags, in, wanted, id, name, note,
                    code, relay
GET  /servers   ->  "NFS1\n", then "list <url>" and "relay <host:port>" lines (16 at most): the other servers
```

An announcement of a game by its address (no code) gets a third field in its answer, `reachable` or `unreachable`:
master.py sends the game the browser's own query (UDP, to the address the announcement came from and its port) and
waits a moment for its answer, so the host learns whether its port is forwarded (asked again after 5 minutes, or 20 s
if it did not answer). worker.js cannot send UDP and leaves the field out.

The fields are separated by tabs. `flags` is the game's own byte, passed through: 1 under way, 2 a seat free,
4 in Driving.xbe, 8 locked, 16 locked by its join code. A relay's game whose host gives its code to its players alone
announces `code=-` with its relay, and the list shows `-` as its code. The relay stops answering an address that asks
for 20 codes of no game in a minute. A game with a code shows `-` and `0` as its address and port. The relay's UDP
packets are described at the top of `master.py`. A game drops off 150 s after its last announcement. One address may list 8 games,
and the list holds 1000 at most.
