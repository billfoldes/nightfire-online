# Online multiplayer

The console game has no networking of any kind: its multiplayer is split-screen, one machine simulating every
player from the pads plugged into it. This feature is those pads over the Internet, so the console's own
multiplayer — its modes, maps, bots, menus — plays as it did, with the mouse and keyboard of a PC shooter.

## How it works

**Lockstep, not state sync.** Every machine runs a complete copy of the match. Each tick (a port-0 pad poll on the
game's thread — the game's own call pattern, identical on every machine) each machine sends one small packet: what
its player did (the final pad the runtime made of their devices, the mouse's movement in counts, their scope-zoom
wheel notches, whether they play the keyboard). Every machine applies every player's input at the same tick, so
the copies stay identical and no game state is ever sent. A tick's packet carries that tick and the 7 before it
(30 bytes each, 253 a packet); 60 ticks a second, four players, about 60 KB/s a machine.

**Boot together.** Lockstep needs a state every machine shares, and the only free one is power-on. A session is set
up before boot (GameInfo::init): the host waits for joiners (`net_host`, UDP `net_port`), each joiner is given a
controller (`net_join`, the host's address in the settings' `join_ip`), and when the host starts (its `net_wait`
over, or NF_NET_PLAYERS reached) every machine boots at once and the first ticks align. No one can join a match
already going; a new player means a new session and a restart. State transfer and catch-up fast-forward are the
known ways around that, both for later.

**Sessions are controllers.** The host is controller 1; a joiner plays the controller it was given. Each machine's
own devices play *its* player only — its local pipeline (the mouse in the menus, the direct aim, the styles) runs
live for its player, and what that pipeline produces is what travels. Everything the game reads for a player
arrives the same on every machine:

- the pads, per port, held `net_input_delay` ticks (1..12, the host's setting) so a tick can absorb the round
  trip — this is the input lag every classic lockstep game had;
- the mouse's movement (src/game/mouse.cpp's turn and pitch writes, per player, including the missile/turret
  right stick and the wheel's scope zoom, which write game state outside the pad);
- the keyboard flag per player (the controls style and the auto-aim follow it: a keyboard player reads style 0 and
  no auto-aim, wherever their avatar is);
- the rumble is remapped so a joiner's own events play on their own device.

**Topology.** Joiners send to the host, and the host relays each joiner's packets to the others: one address to
configure per joiner, and no peer needs to reach another. A lost packet's ticks come in the next ones; a tick
still missing is asked for again (kResend: a joiner asks the host, the host the joiner it is from) and the game
waits for it, as no machine may play input the others did not (logged; NF_NET_DROP=percent drops that share of a
machine's packets to test it: 10% costs the match test 0.4% of its frames, 25% 5%, its memory as without); a
silent machine freezes the session while it logs every 2 s — v0's answer to a crash or an alt-tab gone wrong.

**Finding a game.** F1 > Server Browser (opening on its Internet tab) lists the games heard on the LAN (a broadcast
query), on the internet lists (the Windows exes' built-in ones, those `net_master` adds, and those the lists named,
kept in `servers.txt`; `tools/master/README.md`, and `scripts/net-lists.sh` tests it past a dead list and relay) and at the addresses saved in Favorites and History, which are asked directly. Each shows its
players, ping, whether it is locked, whether it is on-foot or driving, and how it was heard; Connect fills in
`join_ip` and restarts, and the box beside the tabs connects to an address or join code typed or pasted. The Map
column is empty until hosts send one. NF_NET_FIND=1 logs what the finder hears and NF_NET_FIND_ASK=a,b adds
addresses to ask. A joiner's connection screen (Windows) has a box too: the host's address or code, or the password
or code its host asked for; Enter saves it and starts again with it, and the wait counts from the last key typed.
Streamer mode (`net_default_names`, NF_NET_DEFAULT_NAMES; GameInfo::streaming): typed names drawn as Player N, the
browser's games as Game and a number of their own (saved servers as Saved server N), and no IP address in F1 or on
the connection screen (boxes as dots, a locked game's code hidden on its host's screen); `STREAM=1
scripts/net-browse.sh` tests the browser's names.
On either side's connection screen F1 ends the wait (the game boots alone, `net_join` stays on) and opens F1's
Online page as the game starts, for the Server Browser or another setting and then Restart online play.
`net_join` goes off by itself once its session is over (the machine left, or the host is gone) or none came of it,
so the next start plays alone; a join the harness asks for (NF_NET) leaves the settings alone.

**The settings' layout** (F1 > Online): Hosting (Host online play, How players reach me, Players, Who can join, the
password), Joining (Join online play, the host's address or join code, empty searching this network, and the
password), then Restart online play, Servers (the Server Browser's game list and the relay, which hosts and joiners
both use) and Advanced. **How players reach me** (`net_host_by`, NF_NET_HOST_RELAY=1; the old `net_host_relay`
reads as its join code): by the IP address, or by a join code through a relay; the Relay server row is greyed while
hosting by the address.

**Who can join** (`net_who`, NF_NET_WHO; the old `net_public` reads as Anyone), under Hosting. Not listed: on no
server list (`net_master`, "Server Browser's game list"), open to anyone with the address or code. Anyone: on the
server list too, by the address its announcement came from (or its join code through the relay); the choice says so
when the game list is none. A listed host waits for its players with no time limit (net_wait 0 does that for any wait).
master.py asks a game listed by its address from outside (the browser's query) and answers its announcement
reachable or unreachable: the host logs it, and its connection screen says players anywhere can join, or that its
UDP port needs forwarding (or the relay). Only people I tell: listed locked (find
flag 8). Through the relay the key is the join code: the list gets the code `-` and the relay alone (flag 16 too),
a joiner through the relay proved it there, and one by the LAN address gives it as its password. Otherwise the key is
`net_password` (NF_NET_PASSWORD). A hello without the key is refused (kRefused, seq 1 for a password, 2 for a code);
the joiner's key is `join_password` (NF_NET_JOIN_PASSWORD), which the browser and the connection screen ask for.
The key travels in plain text: it keeps strangers out, it is not security; the relay stops answering an address
that asks for 20 codes of no game in a minute, so a withheld code is not guessed. The host's connection screen copies
`NF-code@relay` to the clipboard (C, or its button). `scripts/net-password.sh` tests a password; `scripts/net-invite.sh`
a withheld code: listed without it, heard locked, a wrong code refused, the code through the relay and by address.

## What is where

| Piece | Where |
| --- | --- |
| Session, transport, tick barrier, capture and apply | `src/common/net.cpp` |
| The settings (`net_host`, `net_host_by`, `net_join`, `net_port`, `net_wait`, `net_input_delay`, ...) and the pad/rumble wiring | `src/common/game_info.cpp` |
| Per-player aim, zoom, missile stick, auto-aim | `src/game/mouse.cpp` |
| Per-player controls styles | `src/game/controls.cpp` |
| The game-thread poll marker (input::GameThreadPoll) | `sdk/runtime/core/input.*`, `sdk/runtime/xdk/xapi.cpp` |

With no session every one of these takes exactly the path it did before: `net::Active()` gates them all.

## Before trusting it online: determinism

Lockstep is only as good as the game's own copy-to-copy determinism. `scripts/net-probe.sh` runs the same
headless match (mp_bots.txt, paused at the same point) twice on one machine and compares the whole of memory.
**Result (2026-11-08): the games were identical** — 6 bytes of 128 MB differed, all in the kernel variables'
first megabyte: KeTickCount one tick apart and the kernel's wall-clock/uptime caches, host clocks ~2 ms apart
because the two dumps landed ~2 ms apart (`scripts/net-diff.py` names that region and does not fail the
comparison on it). The game's own state, everywhere above that megabyte, matched byte for byte — including the
evidence that the game never reads those clocks into its state, or it would have diverged too.

The probe compares the same machine; a cross-machine pair (different CPU, different build) is the stronger
test once this one passes. The residual risk the probe cannot see: if the game steps its simulation by
real-measured time (a frame's dt) rather than by tick, two machines paced differently would still diverge —
the session test (`scripts/net-loopback.sh`) is where that shows, and the input delay setting is what would
hide it if it does.

## The known limits of this first version

- **The audio ring (found; the mixer stays wall-clocked).** The first session run compared identical games
  except one contiguous 606 KB region - a streaming audio ring whose phase differed because the harness's
  mixer pulls every 10 ms by the host clock; two machines drift a few samples a minute that way. Making the
  pulls follow the game's frames (XBE_AUDIO_TICKS) proved that drift's shape but deadlocks the front end: it
  blocks writing its music stream while nothing consumes, consumption waits for frames, and frames wait on the
  blocked render - a three-way gridlock the wall-clocked device can never form, which is why the harness paces
  it that way. The tests therefore run the wall-clocked mixer and net-diff reads one contiguous run as the ring
  (a verdict of its own, distinct from a desync); the game demonstrably never reads the ring back - two machines
  stayed identical for 88 s with their rings apart. What a real online session wants is the Windows XAudio2
  path consuming at a rate both machines agree on, and what that wants is an audio clock derived from shared
  state - an open design question, noted here.
- **The drift float.** The probe's own comparisons of the title screen found one ambient float (0x37DC1E0 in
  default.xbe's memory, a camera-like direction component) that diverges between two runs of the same input -
  same place every time, different value every time, everything else in 128 MB byte-identical. One
  wall-clock-stepped decorative value; a session that sits in the title with it either matches or not by
  rounding luck, and the menus the match itself runs in have shown no such drift. The desync checksum (below)
  will watch it live; if it ever turns out to feed the game's choices, the fix is to find its writer in Ghidra
  and pin it to the tick.
- **Relaunch port handover (done).** `GameInfo::relaunching` (`net::Close`) closes the UDP socket in the old
  process before `kernel::Relaunch` starts the next executable, and `SO_REUSEADDR` is set on bind, so the
  relaunched process binds `net_port` at once while the old window waits out the handover.
- **Window responsiveness & focus (done).** `net::Init` runs in `GameInfo::window` (after `--prepare-code` and
  once the window is up), pumps Win32 messages during the pre-boot wait with live status in the window title bar,
  lets the host press Enter / Space / Start to begin early once at least one joiner is in, forces
  `pause_unfocused` off during an active session, and sends an idle pad from unfocused windows so alt-tabbing
  (or testing two windows on one PC with `--instance 2`) never feeds background inputs into the unfocused player.
- **A session that plays the campaign's driving levels desyncs at mounted guns.** src/driving/mouse.cpp has not
  had the per-player transport that src/game/mouse.cpp has; multiplayer (on foot) is unaffected.

- **Full-screen view per player (`net_fullscreen`, on by default).** In an online session, every machine gives all
  four player cameras (`0..3` at `0x1F661C`) the full `640x480` screen rectangle (`0x25B80`, `0x25190`) and the
  1-player HUD layout (`0xB1AB0`), keeping guest memory identical across all machines, while `0xDD950` and `0xD9410`
  suppress host GPU draws (`d3d::SuppressDraws`) for remote players' cameras and `0xE7130` suppresses the split-screen
  divider bars. Each PC renders only its own player's view and HUD at full screen (16:9 / ultrawide).
- **Per-player scope zoom (`src/game/mouse.cpp`).** `ViewZoom(int player)` reads each player's own camera pointer
  at `0x1F661C + 4 * player`, so joiners' scope wheel-zoom and zoomed-in aim slowdown follow their own weapon zoom.
- **No desync detector.** A checksum of known game state, exchanged every few seconds, would tell a session it
  has diverged (and pause it) instead of playing a subtly different match on each machine. The probe's memory
  comparison is the offline form of it.
- **The player-index assumption.** The actor byte at +0x8DE naming a controller port (0..3) is taken from the
  player rows (0x158 each from 0x1FE6D0) and 0xB9950's use of it; if a bot's index reads into 0..3 the per-player
  hooks would aim it. The multiplayer menus and a bot match under the probe confirmed it holds in a 2P + bot match.

## Building & playing on Windows / Proton from Linux

`scripts/build-win.sh` cross-compiles `Nightfire.exe` and `NightfireDriving.exe` on Linux using `clang++`,
`lld-link`, and `xwin` (fetching the MSVC CRT, Windows SDK, and `SDL3` once into `.local/win-sdk`):

```bash
scripts/build-win.sh /path/to/your/NightFire/play
```

That installs the new `Nightfire.exe`, `NightfireDriving.exe`, and `SDL3.dll` into your Proton game folder
(backing up the originals to `*.orig`) and marks your existing `Nightfire.dll` and `NightfireDriving.dll`
current so they load immediately without rebuilding.

## The plan from here

1. The determinism probe passes headless (same machine, twice): `scripts/net-probe.sh`. **Done (2026-11-08).**
2. Two harness machines in one session over loopback, **into a multiplayer bot match** — `scripts/net-loopback.sh`:
   **Done (2026-11-08): 15,780 frames, 5,218,164 draws, identical on both machines.** The test plays split-screen:
   with net_fullscreen each machine draws its own player's view, so its frame buffers and push buffers differ by
   design (2026-11-18: those alone, the game's memory the same).
3. Windows / Proton in-game play: `scripts/build-win.sh`, host and join over loopback (`--instance 2`), LAN, and
   the Internet; `net_input_delay` tuned per connection.
4. Then, in whatever order the play demands: full-screen per-player viewport (so each machine renders its own
   player's view full-screen instead of split-screen), the per-player camera (joiners' zoom), the desync
   checksum, and upstream.
