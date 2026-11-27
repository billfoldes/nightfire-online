# 007: NightFire recomp - progress log

Newest first. Each entry: date, what changed, what was learned, what's next.

## Status
- Released: v1.0.2 (public, MIT); v1.0.3 withdrawn; v1.1.0 drafted (prebuilt package), to be published by Harry.
- Milestone: 3 - plays on Windows. Harry (2026-10-28, D3D12, RX 7900 XT): the Paris driving mission to its end and
  back, then 8 minutes of default.xbe's on-foot play with his saves: no traps, 60 frames a second (a slow frame only
  when a shader compiles). Fixed on the way: black screen (gamma), mission-end crash (Driving.xbe's Reset), menu
  panels (SetPalette), the scope's colours (vertex stream read-back), a D3DX trap (recompiler)
- XDK: 4831 named in the SDK (branch nightfire): default.xbe 137 of 252 functions, Driving.xbe 115 (6 by hand);
  layout proven from NightFire's code
- Push buffers: none
- SDK: the fork's main at 97b1654 (2026-11-04: F1 overhaul, SMAA, prebuilt launcher; 37fa3dc D3D12 stage memo by contents; c4000f8 the launcher on drives without owners; 66a7066 the launcher takes every xbe; 0b5ff49 launcher updates; ed5e147 edge bars, unsuspended relaunch; 8acd824 vertex buffer locks wait for the renderer; ed1796c replacements for linear textures; 2026-10-28 ca1d0f9 point sprites) (2026-10-28: nightfire fast-forwarded in after EoN, PGR2 and PGR1 smoke and
  PGR1's frame check); the launcher can build this game now
- PC features (pc-features): skip intro, mouse look and menu mouse, key layouts, PC Settings in the main menu, sharp
  2D, ultrawide and field of view, the on-foot crosshair's colour and size (F1, 2026-11-19); on Windows (2026-10-29)
  PC Settings, mounted guns, achievements and the icon keypads and ultrawide work
- Smoke: scripts/smoke.sh, title -> START -> Driving.xbe's Paris mission, about 4 minutes
- Online multiplayer (net): **the session machinery works, proven (2026-11-08)** - two harness machines meet
  over loopback, boot together and play 88 s in lockstep with identical frames and draws, their whole memories
  identical but the host's clocks. Determinism gate passed before it (the probe: same input twice, one ambient
  float at 0x37DC1E0 the only game-side wobble, named in docs/online.md). Remaining to a real online match: the
  relaunch chain (the title relaunches into Driving.xbe, the front end on the multiplayer's path, the session
  re-meeting at each boot), the multiplayer menus and a bot match, then Windows (XAudio2 tick-pacing, LAN)
- Online matches (2026-11-17): the players who are in plus the bots, up to 10 in all (net_bots: the bot menu's or
  0..8; the bots past the game's six in the slots below 4); the host sets every bot's character or each one's
  (or Random), shown in and edited from the game's bot menu, and Bots 7 and 8's fields; the match's Start page
  lists all the bots net_bots chose (2026-11-25); unlock_multiplayer opens
  every character, map and scenario (the host's)
- Campaign co-op (net, src/game/view.cpp, src/game/characters.cpp): up to 4 players through the campaign on the
  mission's own models plays again (2026-11-10 retest on both PCs); a dead player respawns on a teammate's own spot,
  and a character change waits for the next level (2026-11-21). The chosen multiplayer characters now load with
  the level from a match's archive, their textures' aliases resolved, and the players wear them with Bond's
  hardpoints as in a match: proven headless on two machines (Drake and Rook, Bond Tux and Kiko in the second level),
  single player unchanged. Every co-op player moves with a match's third-person animations by weapon (they held
  one pose). Each player reads their own controller whoever pressed START, and shows their own stance icon. Co-op
  leaves the two machines' memories apart in the same places with or without characters (open). Light glows
  no longer show through other players' walls (the game decided them from player 1's view for every camera; the
  earlier MSAA workaround is gone). Co-op players step sideways as in a match; a first-person model no longer leaves
  its attachments floating in the world after a weapon change. A machine that quits no longer freezes the others; in co-op its player drops out of the level;
  The pause screen in a session is its player's alone (every page of it), the game going on for the others; its
  QUIT's YES takes that player to the game's menu offline (the host's takes everyone to the menus, still in the
  session, with F1's Everyone quits with the host, on by default, 2026-11-19); RESTART is the host's alone, and
  F1's Restart the session for everyone (the host's) relaunches every machine into the session again. Paused players stay in the
  world (enemies shoot them; a death waits for the unpause), pauses open in the same frame and no pause holds another.
  A downed co-op player respawns armed beside a teammate 3 s on, the sound going on; only the last one down fails the
  mission.
  A match's clock counts the session's ticks: every machine ends it on the same tick (2026-11-11, John: no issues).
  Co-op shows each player's typed name (net_name, else Player N) over their head; matches use typed names too, and
  net_default_names draws Player N instead on one PC (2026-11-12). Co-op name tags are where the game puts a
  match's again. A match's Health Handicap is every player's: host and guest on their join boxes, and anyone
  (players 5 to 10 too) with net_handicap (2026-11-12). F1's name and handicap change in a session (no restart);
  the settings that do need one say so (2026-11-12). A player who left can take their seat back in a running
  default.xbe session: the host's snapshot goes to their new boot over the session (about a second's hold on
  loopback, 4 s with a tenth of the packets lost), proven headless on two and three machines (2026-11-12; John: 2
  players on two PCs). Co-op with 3 or 4 players no longer crashes as player 3 spawns (the sprite pool, 2026-11-12).
  Driving co-op (branch driving-coop, in progress): player 2 aims and fires player 1's mounted gun; Driving.xbe's
  clock counts frames in a session (2026-11-13); any player's START pauses, the level starts on every machine
  together; a player who left gets their seat back in a driving mission, from default.xbe too (headless). Doubled
  sound fixed (John). Frames are paced to the game's timer; player 2 can fire before player 1 has; a player's QUIT
  on the pause takes them alone out. unlock_missions (F1 or settings.toml) opens every mission, the host's setting
  for the whole session. A mounted gun's mouse aim is the same on every machine (Alpine Escape's turret desync); a
  check every 60 ticks logs "net: the games differ" if the machines drift. A player who goes on after player 1 leaves
  takes the car, the pause and the gun (2026-11-13; John: works). W and S move the menus on the keyboard (2026-11-14).
  Player 1 locked out of the gun and the pause fixed: the game played the port default.xbe's launch data named, player
  2's; seat 1's input goes to that port now (2026-11-14; John: W and S work, no crash, no drift). In the car
  (Enemies Vanquished) the other players fire the car's guns with A, no reticle there (a mounted gun's alone); the
  others' turret shots at player 1's own rate. A session's player plays on any of their machine's controllers (a pad
  SDL lists twice locked player 1 out on foot); a trigger or stick that rests part way reads from its rest
  (2026-11-14). The host alone skips cutscenes; the title menu's Up moves one row; the driving missions' ammo is
  one pool, what the game gives times the players (2026-11-14). A rejoin no longer crashes the rejoining machine
  (the snapshot's load waits for the title's threads to be in the kernel and holds them there, 2026-11-14). A
  driving mission's opening movie skips on the driver's A, B or START (Enter, the left button) on every machine
  (2026-11-14). The machines no longer drift apart in a driving mission (the sound library's frees, by the frames
  now) nor after a rejoin (2026-11-14, headless), nor after a mission's Replay (its load's sound services,
  2026-11-16, headless; the frame clock kept on through the loads, 2026-11-18, John to retest). In the co-op campaign every enemy comes with one more per extra
  player and their health is 1 + 0.5 per extra player of the game's, each the host's F1 switch (2026-11-14; John: the
  extra guards spawn). Every machine of a session plays from the host's saves (a joiner's own made the
  Exchange's on-foot desync; 2026-11-14, headless). F1's Restart online play goes to the connection screen, the
  driving missions tell the player who takes the car, the unlock switch says greyed missions play (2026-11-15).
  Server browser (2026-11-15): F1's Online page lists the games on this network (LAN broadcast, headless test
  scripts/net-browse.sh) and an internet list's (net_master, a server anyone runs: tools/master's master.py or
  Cloudflare worker.js; a host lists itself with net_public; scripts/net-internet.sh), and a relay with join codes
  (master.py --relay; net_relay, net_host_relay: no IP shown, no port forwarded; scripts/net-relay.sh, RELAY=1
  scripts/net-rejoin.sh). On foot the sounds end by the session's ticks (the game's voice frees: John's Linux and
  Windows co-op apart in 07000006, 2026-11-15; John: no desync or sound issue in co-op since), and the sin, atan2, pow and the rest are musl's on every OS.
  NightfireDriving.exe no longer aborts as it starts on Windows (a thread_local built on Windows' early threads,
  John's dumps; 2026-11-15; John: no crash). The relay plays across two networks (John, 2026-11-15). A full level
  heap refuses its allocations instead of crashing (Phoenix Fire's helicopter in co-op); a guided rocket's sight is
  its player's alone; 2-player co-op has the 1024-sprite pool (2026-11-15). The guided rocket's sight is off the
  other screens, and every player's watch gadgets show the watch (2026-11-15, headless); the door hint shows once a
  level, both kinds of door (2026-11-16; John: the sight fixed, the watch shows; each player's watch hands their own (2026-11-16, headless)
  F1's Server Browser page (tabs, sortable table, filters, favourites, history) and game passwords (2026-11-16,
  headless); Who can join (a relay game's withheld code as its key), a Connect box, the connection screen's address
  box, Join off after its session (2026-11-16, headless where it can be). No setup to see internet games: the Windows
  exes know the project's lists and relays, ask all of them, and keep what the lists name in servers.txt, so one
  going away leaves the others; the browser opens on Internet (2026-11-17, headless past a dead list and relay).
  A new install starts in Fullscreen, which holds the cursor on the game's screen (2026-11-17, Windows build).
  F1 > Online is laid out by role: Hosting (with How players reach me: the IP address or a relay's join code),
  Joining, Servers (the Server Browser's game list, the relay) and Advanced (2026-11-18, Windows build).
  Driving co-op: another player's car machine gun and turret sound; a guided torpedo is steered by whoever fired
  it (2026-11-19; John: all of them work). Every co-op player floats in Equinox's space part (2026-11-19,
  headless).
  Next: John

## Disc
- PAL, title 0x45410026 (EA), certificate region 0x4, version 2. Ghidra: `~/ghidra-projects/nightfire` (game.exe,
  driving.exe)
- default.xbe SHA-1 796222e048be3595c323b4c6d9795107132dc4ca, built 2002-10-20. Libraries XAPILIB, D3D8, D3DX8,
  XGRAPHC, DSOUND, XBOXKRNL, LIBCMT, XMV, all 1.0.4831. Sections .text 0x11000-0x10046C, D3D, D3DX, XGRPH, DSOUND, XMV,
  XPP 0x157200-0x15D150, .rdata, .data, DOLBY
- Driving.xbe SHA-1 e0c41209d8979fcd03ac3d656be78759006023cb, built 2002-10-18. Libraries XAPILIB, D3DX8, D3D8,
  XGRAPHC, XBOXKRNL, LIBCPMT, LIBCMT, DSOUND, all 1.0.4831 (no XMV)
- Data: `eurocom/filesys.d00`-`d10` and `eurocom/25_fps` (on foot), `driving/*.viv`, `.mus`, `.spe` (driving)

## Log

### 2026-11-26 - John's co-op list (v2.0.8)
- **Extra co-op enemies no longer stand in walls or seal a hall** (src/game/enemies.cpp): a copy stood 0.8 to a fixed
  side of its original, whatever was there. Now each copy tries eight places around the original and takes one whose
  way is clear (0x2C5B0, the sight test 0xB54D0 uses for combatants, mask 0x623) a little past it, with a floor
  under it, preferring the least open ones (toward walls and corners, out of walkways); no place, no copy. The
  placed characters' turn is (0, heading in radians, 0). The Exchange in co-op: 12 copies placed, both machines
  identical; the probes tell walls apart (30 m probes mostly blocked, some sides refused).
- **The sniper scope's blurred surround is the game's own**: the Paris van tutorial zoomed in (LT, Driving.xbe
  vblank ~1600 on) rendered by the CPU reference renderer (REF=40) shows the same soft, dimmed view outside the
  scope circle and a sharp one inside, so the Windows renderer draws what the Xbox does. Not changed.
- **The other players' reticles on the mounted guns** (net_others_reticles, F1 "You (this PC)", off by default):
  every machine already steps every player's aim (src/driving/coop.cpp g_aim), so nf::Reticles now hands the
  interface each shown player's, this machine's own as before and the others' (not player 1's, the game's crosshair)
  smaller and fainter in their colour.
- **Host co-op settings travel like the bot settings** (net.cpp, kVersion 39): kHostSettings values after the 24 bot
  settings in the host bundle's rotation (host_bot_at, 26 of its 32), and in kStart (kStartHost), in the snapshot.
- **The mission fails when** (net_coop_fail): the last player down (as before), player 1 down, or anyone down; a
  death that fails it is not respawned and its fail screen request ([0x1DF19A]) stands. Tested with NF_COOP_KILL
  in The Exchange, each mode, both machines: player 2 respawned / player 1's death failed it / player 2's failed it.
- **Pickups** (net_coop_pickups): Shared (as before) or Separate: a one-off weapon, ammo or armor pickup with no
  mission flag stays until every player still playing has taken it once (0xA7710's gone mark undone, the takers out
  of 0x1F6654 for its update 0xA84D0); still drawn for those who took it. NF_PICKUP_TEST: player 1 took an ammo
  and an armor pickup and they stayed, player 2 then took them and they went, the same on both machines.
- **Master volume** (F1 General, SDK Settings::volume, squared as a level; GameInfo::default_volume, NightFire's 50)
  and a **stick dead zone** (F1 Controls, SDK Settings::stick_deadzone, round, on top of the game's own).
- **The launcher remembers the game across installs**: SetGame also writes %LOCALAPPDATA%\NightFireOnline\
  launcher.json; a launcher with no choice of its own takes that one, else an install's beside its folder, and links
  it in its first job. launcher_cli saved: found from the user file, then from a sibling install alone.
- **Unlock every reward** (unlock_rewards, the host's as the session starts, kStart flag 4): 0xB1740, which decodes
  the medals' rewards from the codename's 64 bits at 0x25D6E8 (the bits the cheat codes PASSPORT, Q LAB ... set),
  reads all 64 set; the save keeps what was earned. The hook fires as the codename loads.
- **Each player's own view on a mounted gun** (net_own_view, F1 "You (this PC)", on by default; NF_OWN_VIEW): the
  view's camera rows (0x96EB0, +0x10 right, +0x20 up, +0x30 forward) are turned to this machine's player's aim for the
  draw and put back after it, so the game's state is untouched; the reticles are placed across the turned view
  (player 1's too). The Paris van's scope, player 2 aiming right: their own view looks along their aim, off it
  shows player 1's; both machines identical either way.
- **F1's number popup stays open on − and +** (SDK overlay.cpp): it closes on Enter alone.
- **Nightshift's guards get their extra copies** (src/game/enemies.cpp): its 20 placed characters are turned into 6
  spawners' records at the level's start (0x30670, deleted with 0x2D080) and come back from the spawners (0x323E0)
  as the script's flags (0x1DF138 + 131..136) wake them, on team 3, not 1; the copies took team 1 alone. 0x364F0
  (who a drone fights) puts teams 2 and 3 on one side and 1 on the other; Enemy() now takes 1 and 3 (2 is one
  character in Paris, left alone). With the flags poked: each spawned guard one more beside it, x1.5 health, the
  same on both machines.

### 2026-11-25 - Through the relay, straight when it can; UPnP for hosts by IP address (v2.0.7)
- **Direct connections through the relay** (net_direct, on by default, every player's own): the relay gives a host
  and a joiner that both allow it each other's address and a secret (master.py's peer packet), they punch through
  their routers, and once each is told its punches are heard the connection's packets go straight. A punch saying
  "I do not hear you" or 2 s of silence puts them back on the relay. Old relays and old games ignore the flag (relay
  alone). Each PC learns the other's address, never shown; John chose on by default, used only when both have it on.
- **UPnP** (net_upnp, on by default, src/common/upnp.cpp): a host by its IP address asks the router for its UDP port
  (SSDP, the description's WANIPConnection, AddPortMapping for an hour or for good), again every half hour.
- The main README points self-hosters at tools/master/README.md. kVersion 37.
- Evidence: scripts/net-relay.sh OK three ways (both direct: both connect straight; DIRECT=0: no address given out;
  CUT=40: the joiner's door shut, both back on the relay within half a second of each other, the game frozen about
  2 s, then on to the end). RELAY=1 scripts/net-rejoin.sh OK (the rejoined machine straight again, memories
  identical), net-invite OK. scripts/net-upnp.sh OK against a fake router (chunked replies, the wrong service
  skipped, the hour refused and a permanent lease taken; SSDP found it once, on this LAN, before the search was made
  opt-in). build-win OK, smoke OK.
- Next: the relay change goes on the Vultr relay (and the Oracle one); a real test across two homes, where the
  routers decide.

### 2026-11-25 - The match's Start page lists the bots F1 chose (v2.0.6)
- John: the bots picked in F1 (net_bots) joined the match but the Start page's AI Bots row didn't show them. That
  row is the bot menu's Playing entries: the page (0x891E0, its 0x4C) runs 0x75DD0 first, which moves the Playing
  ones (+0x0E) to the front and makes their count 0x245281, then draws a picture each (0x728F0 column 0x24, the
  character's 0x17CB98 record) in the team's list (0x10000233 / 0x10000234, ten rows). Now with net_bots set every
  machine makes the first of the session's bots (at most six) the menu's Playing ones each frame (bots.cpp,
  PlayingFromSession), and the page's hook adds Bots 7 and 8's pictures after them with the team and character
  their spawn gives them. kVersion 35.
- Headless, two machines from a fresh boot through Multiplayer (net_bots 8): the Start page shows eight bot
  pictures, the match has 2 players and 8 bots, the memories identical (with REF on the host only its frame
  buffers differ); net_bots 3 lists three. SMOKE OK.
- The smoke test's FAIL ("the next xbe did not get the launch data") was a stray .local/harness/NightfireDriving
  (from scripts/net-bootstrap.sh): the first run relaunched into it and used the launch data up. Removed.

### 2026-11-21 - Co-op: any character, mid-level (loaded when it is picked)
- John: changing mid-level worked (v2.0.4), but only to Bond or a character the level loaded with. Now a character
  the level lacks is loaded when it is picked (characters.cpp's LoadCoopCharacter): its pack and model, and the
  multiplayer hands and skins if the level has none, read from 07000023.bin and handed to the loader's dispatcher
  (0xBE7B0) with archive mode (0x2ADF70) on for it, so 0xDCA30 returns the entry; their textures go after the
  level's (the table's 2048 slots checked first), and the new models get Bond's hardpoints as 0x11200 gives them.
  Every machine loads it at the same tick (the choice travels with the session's input).
- Headless (07000005, the joiner watching): the host Bond to Oddjob (7 entries, 308 KB, 8 textures, 33 ms) and Ninja
  to Drake (2 entries, 142 KB, 50 ms): both machines swapped, in step to the run's end, the new model textured and
  holding the P99. kVersion 33. Not tried: a level near the heap's end (a load with no room logs it and waits).

### 2026-11-21 - Characters change mid-level again, the game's way; every character holds a gun in co-op
- John: v2.0.3's wait for the next level took away changing character during a level. The change (sub_000AD520)
  now rebuilds the model as the weapon change rebuilds a view model: the weapon kept (KeepWeapon), 0x17A60 frees
  the instance and its attachments, 0x11440 builds the new one (its skeleton data sized for it), the weapon drawn
  again. The spawn (0xACCFC) builds a player's model through 0x11440 too, which is why level start never crashed.
  A choice the level did not load waits for the next one. Headless (07000005): Ninja to Bond and Bond to Oddjob at
  tick 2400, both machines played 78 s on in step (the bare record swap crashed both 13 s after). kVersion 31.
- Oddjob with no weapon: not reproduced. All 28 multiplayer characters (choices 2-29), the host's as the joiner
  sees them at 07000005's start, hold the P99; Oddjob too after a respawn. 0x11200 gives them all Bond's 60
  hardpoints (kind +0x2C 0 or 1), every level load ([0x1D6A65] cleared by 0x14B80). The character table's diag
  lines now give each loaded record's kind and hardpoints. Waiting on John: which weapon, level and moment.

### 2026-11-21 - Co-op: a multiplayer character no longer moves a level's textures
- John (v2.0.3, playing Oddjob): The Exchange's first door's window drew black shapes. The texture table's next slot
  is [0x19480C]; 0xDDF90 copies an alias ("TD") from the table's slot (low + 1), numbered as a whole load of the
  archive would. The chosen characters' packs go in just before the level's main pack, so that pack's aliases into
  its own earlier textures named a slot as many too early as the characters brought. Now (characters.cpp's
  ShiftAliases) those aliases move past them. Headless, 07000005 with the host Oddjob: "8 textures in from slot 1D9;
  1 of the level pack's aliases moved past them". To see in play: the window at that door.
- Next: changing character mid-level the game's way (0x17A60 frees the instance, 0x11440 builds the new model);
  Oddjob holding no weapon in co-op.

### 2026-11-21 - Co-op: no crash after a character change, no respawn under the level (v2.0.3)
- John's co-op session (logs 006/271): player 2 picked another character mid-level and both machines crashed 8 s
  later on the same guest thread (a read of 0000000B, the stack through 0x159D0's skeleton code). The swap
  (sub_000AD520) put the new model's record on an actor whose skeleton data was built for the old one. Headless
  (.local/coop-level.sh 07000005, the joiner Ninja then Bond at tick 2400 through NF_NET_CHANGE's new character
  field): the old code crashed both machines 13 s after the change; now the choice waits for the next level (co-op)
  or match, logged "is theirs from the next level", and both played on 78 s. Matches no longer swap mid-match
  either (the same skeleton; net_character's help always said the next match or level).
- His second session (007/272): a respawn left a player under the level. A co-op respawn was put 1.2 beside the
  teammate, which can be past a ledge or a wall; now it is on the teammate's own spot, the respawned player passed
  through (+0xCC 0x20, a dead player's) until the two are 1.0 apart (g_overlapping, in the snapshot). The respawn
  line logs the position. Headless (health poked to 0): player 2 respawned on player 1 at y 25.0 on both machines,
  stepped away and was "clear of the others again", the machines in step. kVersion 29.
- "T2" as everyone's name: no default of ours; it was typed once into a settings.toml on the shared folder the
  joiner plays from. An empty name already shows Player N.
- NF_LEVEL works in a session again (v2.0.1's always-skipped tutorial took the request it replaces).
  scripts/build-win.sh pins the Windows SDK to 10.0.22621 (xwin 0.6.5 finds no headers in 10.0.26100's).
  build-win, SMOKE OK.

### 2026-11-20 - Bots fill the places of players who did not come (v2.0.2)
- John: a host who started early with fewer players than it asked for got only the bots it had set. net_bots has a
  new choice, Fill to the Players count (10, kBotsFill): net::Bots() gives max(players, g_target) - players, the
  host's net_players as every machine has it (kStart's wanted count, in the snapshot), and the match start hook
  (src/game/view.cpp) spawns that many as for a set count. kVersion 27.
- Headless (scripts/net-loopback.sh, HOST_PLAYERS=4, HOST_WAIT=20, NF_NET_BOTS=10): the host's wait ended with two
  machines, both logged "a match of 2 players and 2 bots", NET-LOOPBACK OK with the game's memory identical.
  build-win, SMOKE OK.

### 2026-11-19 - README: setting up and online play only
- John: the GitHub page described Harry's port (its controls, graphics settings and features); this project is
  about online play. The README now has Harry's credit, what NightFire Online adds, setting up with the launcher,
  a Playing Online section (hosting, joining and the player's own rows, by F1's own names), building from source,
  troubleshooting, the licence and credits; it points to Harry's for the port's controls and graphics. The
  screenshots only the old sections used (docs/img) are gone.

### 2026-11-19 - Intro movies off by default, and always off in a session (v2.0.1)
- John (v2.0.0 on Windows: the launcher, the build and an online game all worked): a new PC joined with the logo
  movies still playing while the host was already at the title. skip_intro is on by default now, and HideFile hides
  the EA and MGM logos in any session whatever the machine's setting (a settings.toml from before keeps its own
  value). The first-boot Paris tutorial is never forced in a session either (src/game/hooks.cpp).
- Headless, two machines, the joiner with NF_SKIP_INTRO=0: both logged the logos hidden and only the language
  select's background opened. scripts/smoke.sh pins NF_SKIP_INTRO=0 (its pad script is timed for the logos).
  build-win, SMOKE OK.
- Release v2.0.1 (the latest; v2.0.0 stays a pre-release).

### 2026-11-19 - The release package is made on Linux; v2.0.0, the first NightFire Online release
- scripts/release-package.sh: what release-package.bat makes on Windows, from build-win.sh's output: the exes,
  SDL3.dll, d3dcompiler_47.dll and the compiler folder (xbe_recomp.exe cross-compiled here; clang.exe and
  lld-link.exe from LLVM 19.1.7's Windows release, toolchain.json's, sha256 checked; all three on the static C
  runtime, so no runtime DLLs; the configs, XDK layouts, headers and the exes' import libraries). 65 MB.
- build-win.sh's DLL link wrote its own import library over the exe's Nightfire.lib / NightfireDriving.lib (lld-link's
  default /implib), so the .lib it copied into an install's compiler folder was the DLL's: a rebuild there could not
  link. The DLL's goes to obj/ now.
- Proven under Wine with the package's own tools, as a player's first start runs them: xbe_recomp.exe's output for
  both xbes is byte-identical to the Linux tool's (38 files each), clang.exe compiles every file with game_code.cpp's
  flags, and lld-link.exe links Nightfire.dll and NightfireDriving.dll against the package's .libs, importing from
  their exes alone. Not yet run through the launcher on Windows.
- Removed the README's banner (docs/img/nightfire-pc-port.webp). Release notes: docs/releases/v2.0.0.md.
- Next: John: the v2.0.0 pre-release through NightFireOnlineLauncher.exe on Windows, then it is made the latest.

### 2026-11-19 - The project's own name and GitHub home: NightFire Online
- John: a GitHub repo of his own, nightfire-online (github.com/billfoldes/nightfire-online), crediting Harry's
  nightfire-recomp, which this started from (an import of his source on 2026-11-08, not a git fork). The launcher
  now gets its releases and source there (launcher/game.json) and is NightFireOnlineLauncher.exe, working in a
  NightFireOnline folder (no clash with Harry's NightFire); the workflow that builds it on GitHub is renamed to
  match. README: the new name, Harry credited at the top and in Credits, what this adds, issues here. LICENSE
  keeps Harry's notice with this project's beside it. No code changed (SMOKE OK from the last entry stands).
- Published (later the same day): github.com/billfoldes/nightfire-online, public, starting from one commit of this
  tree (bd2144d) made under John's GitHub noreply address, so the earlier commits' emails stay private; the
  history before it is kept in the private Cursor repo (nightfire-redux, remote `cursor`, branch `cursor-main`).
- Next: a first release (the launcher from the workflow on a tag, plus NightFire-package.zip from
  release-package.bat).

### 2026-11-19 - F1: Restart the session for everyone (host only)
- John: a host's way out of a stuck session. F1's Hosting section has a Restart button, greyed unless this
  machine hosts a session that has booted. It sends kRestart (20, outside the lockstep, five times) and
  relaunches; a joiner that hears it from its host restarts online play as F1's own Restart does (Join stays on)
  and meets the host again. The new boot waits for every player who was in (NF_NET_RESTART_PLAYERS, read once
  and cleared, so a later restart waits for net_players again). A relay host keeps its join code (KeepRelayCode).
  kVersion 25.
- SDK: Option.button, a settings row drawn as a button that runs changed (no value, not in the settings file).
- Headless, two machines, level 07000015, a temporary trigger 30 s in: both relaunched, met again (the host's
  saves went over), booted the level together and played 88 s more with no stall. The harness relaunches by
  name: a `Nightfire` link to `game` in .local/harness. build-win, SMOKE OK.
- Next: John

### 2026-11-19 - The host's QUIT can take everyone to the menus (F1: Everyone quits with the host)
- John: when the host wanted to switch to something else, every player had to quit and rejoin. New host setting
  net_quit_together (F1's Hosting section, "Everyone quits with the host", on by default, applies at once): the
  host's QUIT YES on the pause screen is the game's own on every machine, so the level or mission ends for all and
  the session goes on in the menus. Off: the host leaves alone as before. A joiner's QUIT is still theirs alone.
- The setting is the host's by the tick (bit 0x80 of the bundle's mouse/host_bot byte, applied from player 0's
  bundle as friendly fire, in the snapshot; NF_NET_QUIT_TOGETHER overrides). On foot: src/game/pc_settings_item.cpp's
  QUIT YES hook skips LeaveToMenu and the others' forced NO when the quitter is player 0. Driving: the same in
  src/driving/hooks.cpp's 0xDCC10 (no LeaveToMenu, no CONTINUE substitution).
- Headless, two machines: The Exchange, host's QUIT at 165 s: both logged it, both reached the main menu in the
  same frame (the joiner's last frame the mission select), no drop and no desync. With NF_NET_QUIT_TOGETHER=0 the
  host left alone and the joiner played on (as before). Alpine Escape: the host's QUIT at tick 1952 on both
  machines, both relaunched into default.xbe. build-win, SMOKE OK.
- Next: "Restart session for everyone" (host only), for a stuck session.

### 2026-11-19 - Co-op: every player floats in Equinox's space part
- John (772d73e): every driving sound now plays for player 2 (Enemies Vanquished, Deep Descent, Island
  Infiltration, Alpine Escape). Next: in the final mission's space part Bond floats and player 2 does not.
- Found (two-machine co-op dumps of Equinox, 0700001B, against 07000001): a player's movement state is
  [outer+0xD2] (0xA9720); player 1 is in 8, floating, player 2 in 0, on foot, and falls (-3 to -15.5 with no
  input). The spawn 0xACAB0 takes it from its start point's +0x38 (2: 0xAC7E0 with state 8) and every spawn sets
  the level's gravity 0x1F6648 to (0, -9.8, 0), a floating one zeroing it: co-op spawned players 2..4 from no start
  (state 0) after player 1, gravity back on. src/game/view.cpp: they spawn from a copy of player 1's start when it
  floats them (other levels' spawns as before), a respawn or a dropped-out player's carry takes the teammate's
  state, and player 1 going from on foot to floating or back takes the others (0xA9720's hook).
- Evidence (headless, two machines): Equinox before, gravity -9.8, player 2 state 0, falling; now gravity 0,
  both state 8, player 2 still with no input and flying with the stick (forward 23.9 to 5.0, sideways 2.5 to 19.9)
  in state 8; no state differences. 07000001 spawns as before (the same actor, no float). Smoke OK, build-win OK.
- Next: John's retest of Equinox in co-op (the space part, and on foot again inside if the level switches).

### 2026-11-19 - Driving co-op: another player's turret shots sound (Alpine Escape)
- John (f399a17): the guided torpedo follows its player and feels fine. Alpine Escape's turret sounds for player 2
  while the shutter is down (the car's path, the entry below), then silent once it is up (the mounted gun's).
- Found (one-shot sounds logged while player 1 fires, then their owner in a memory dump): the turret's shots sound
  from the player's vehicle ([[0x234E40]], vtable 0x18F580): its update 0x6A840 sets its sound object's (+0x50)
  firing byte +0xC1, the gun sound component 0x129250 playing a shot while it is set. On a gun of type 0x18, 0x1B
  or 0x1D it is the weapons manager's +0x27, which the fire request 0xBB2B0 sets as the gun's animation starts and
  the release event 0x20 clears; nothing else reads it. The copied shot (0x4AC40) plays none for type 0x18.
  src/driving/coop.cpp holds it while another player's trigger fires the gun with rounds left, player 1's own (+0x25)
  on release; in the rejoin snapshot.
- Evidence (headless WAV, two machines, .local/alp, RT held from frame 4000 for 1200): RMS by 4 s over the burst,
  player 1 firing 0.164 0.146 0.153 0.156 0.170; player 2 before 0.132 0.106 0.113 0.116 0.127 (as no gun), now
  0.163 0.148 0.154 0.158 0.165 on both machines, the same as player 1's after the release; no state differences.
  Smoke OK, build-win OK.
- Next: John's retest; then the final mission's space part (player 2 doesn't float).

### 2026-11-19 - Driving co-op: a guided torpedo follows the player who fired it
- John: in Deep Descent player 1 steers the guided torpedo player 2 fires.
- Found: the guided missile is [0x239220]+0x47C, set by 0xB80B0 (thiscall, the missile; 0 when it is gone). While it
  is set the game's steering events (0x2C..0x31 from seat 1's stick, 0xB7B09) turn it (0x5F600 yaw, 0x5F640 pitch,
  0x60110 the flight) and fire (0x1F) sets it off (+0x709). src/driving/coop.cpp's 0xB80B0 hook names the player
  whose trigger fired the car's weapon that tick (player 1's own when the manager's held flags are set), and
  DrivingSeat (src/common/game_info.cpp) gives seat 1 that player's controller while it flies, START and BACK still
  player 1's. In the rejoin snapshot.
- Evidence (headless, two machines, .local/dd with entry 22 selected at tick 300; the same presses on either pad:
  A at frame 2500, left 300 frames, up 300): player 2's torpedo logs "steered by player 2" and its turn targets
  follow player 2's stick (yaw -0.004 while left is held, pitch -0.004 while up is); player 1 firing, the same values
  at the same ticks, steered by player 1. Smoke OK, build-win OK.
- Next: John's retest; then the final mission's space part (player 2 doesn't float).

### 2026-11-19 - Driving co-op: another player's car machine gun sounds
- John (833f8cb on Windows): player 2 fires the car's machine gun now, but silently on both PCs (player 1's shots
  and everything else sound). His other reports, one at a time next: Deep Descent's guided torpedo steered by
  player 1 whoever fires it; player 2 not floating in the final mission's space part; Island Infiltration's machine
  gun wrong for player 2; Enemies Vanquished's turret silent for player 2.
- Found: the player's own car plays no gun sound from the shot (0x4AC40's sound switch: weapon 0 from a car sounds
  only for a car not [0x234E40]'s). The fire event 0x1F reaches the car's input too (0x66640: +0x25B set, 0x20
  clears it, while [[0x23F44C]+0x2B0] is 0), and the car's sound plays the gun while it is set. src/driving/coop.cpp
  holds it while another player's trigger fires the car and gives it back to player 1's (the manager's +0x25, set
  by the same events) on release; in the rejoin snapshot.
- Ruled out on the way: the car's timed channels (0x959B0 via [[0x234E40]+0x4C]+0x30, the model's render switch
  effects, 0x958A0's "Render_SwitchEffects"), [0x1DDA2D] (the turret's pop-up animation), the manager's +0x25.
- Evidence (the harness's WAV, Enemies Vanquished from .local/lvl6, the machine gun selected at tick 300, 20-frame
  presses 40 apart): player 1 firing 0.08 RMS with a 1.5 Hz pulse (the presses) of 3.37; player 2 before 0.06 and
  0.52; player 2 now 0.08 and 3.35 on both PCs, the sound ending with the presses; no state differences. Smoke OK,
  build-win OK.
- Next: John's retest; then the torpedo's steering.

### 2026-11-19 - The on-foot crosshair's colour and size are settings (F1's Game page)
- John: let the player change the reticle's colour and size, single player and multiplayer (the on-foot one; a list
  of colours). `crosshair_color` (the game's own red, white, green, cyan, yellow, magenta, orange, blue, pink,
  black) and `crosshair_size` (25 to 400 %), applied at once in every mission, co-op and match.
- The crosshair is a sprite of the view's HUD list (default.xbe 0xD9410's): red 0xFF0000FF, 11 by 11, flags 0x800
  (centred on x and y, 0xD9630); the only centred picture through two levels' fights, but a match's radar blips are
  centred too (4 by 4, flags 0x900) and come first in some views: they are left out by flag 0x100 and size (6..32).
  src/game/hud.cpp sets its colour (the game's alpha kept) and size for the draw only and puts them back, so the
  game's memory is every machine's whatever each player picks.
- SDK: F1's Game page draws a value option with choices as a list (only the Online page did), and calls an option's
  changed callback there too (no non-net option had one).
- Evidence: level 07000002 headless with green at 250 %: the crosshair green and larger where it was, the HUD else
  unchanged. Net-loopback with the same setting on both machines: OK, memories identical; with it on the host alone
  only pixels differ (as net_fullscreen's frames do). Two later runs with the same setting failed on pixels alone:
  the pause menu's cursor and the hand at its edge (both machines in lockstep, frames and draws identical), not the
  crosshair; that menu's wall-clock animation is a flake of this check, open. Smoke OK; build-win OK.
- Next: John's look on Windows; the car machine gun on the current build (833f8cb).

### 2026-11-19 - Driving co-op: another player's car trigger fires the car's own weapon

- John (the build before): player 2's car gun drew a tracer at first, then only a sound, not the gun's, at each press.
  Those were copies of player 1's last car shot task, its objects player 1's of that moment.
- src/driving/coop.cpp: the copies are gone (ShootCar, the car's branch of 0x4AC40, 0x8ECA0's turned matrix, the
  first-shot trigger sharing). Player 2's trigger makes the request player 1's held trigger makes (FireCar: 0xBB2B0 on
  the weapons' manager for its current weapon, the machine gun's after 0xB72F0's flag 0x16), unless player 1 holds
  theirs. The next weapons' update fires it: the game's shot, tracer, sound, interval and rounds.
- Evidence: Enemies Vanquished headless (lvl6), player 1 firing its missiles out and stopping, player 2 holding A:
  the gun fires on at its interval, the shot count (+0x50) rising, tracers on both machines' frames alike, no
  differ line. net-driving-gun OK, net-driving-rejoin OK, SMOKE OK, Windows build.
- Next: John retests the car gun from player 2's PC.

### 2026-11-18 - Bot 8's F1 rows answer for themselves; the Debriefing lists the bots below slot 4

- John: with 8 bots the Debriefing showed Player 1 first, a "Bond" of zeros and Player 1 again (all Rook, and all
  different alike); and Bot 8's rows in F1 picked Bot 7's.
- F1 (SDK, sdk/runtime/app/overlay.cpp): every settings row is under its key's ImGui ID. Bot 7's and Bot 8's rows
  share their labels, so they were one widget.
- Debriefing (src/game/view.cpp): 0x8B890 lists the players as slots 0..[0x260044] and the bots as 4..4+[0x260048],
  sorts [0x26002C] places by score and shows the first four. The two bots in slots 2 and 3 (net_bots past the game's
  six) were in no list, so two places were stale stack. For that page and 0x80590 (the same lists), the bots below 4
  are counted as players (g_low_bots, the ones SpawnLowBots made, in the snapshot): named by their slot and their
  character's picture (0x25FE38). A first try counted them as the combatants less the players and [0x260048], which
  is not the menu's bots in a match of the menu's (net-loopback's: the machines drew the Debriefing apart).
- Driving co-op (src/driving/coop.cpp): John's player 2 got a few rounds of Enemies Vanquished's car gun, then none.
  Headless (lvl6, player 1 firing its missiles out onto the gun, player 2 holding A from tick 1700) player 2 fired
  400 copies to the end; the weapon table's +0x50 counts each weapon's shots and the manager's +0x14 is the current
  weapon's object, the copied shot's. Not reproduced: the log says now why player 2's car trigger fires nothing (no
  shot to copy, its weapon changed, or no rounds) and player 1's car weapon as it changes.
- Evidence: an 8-bot match cut to 40 s (.local/mp-results.sh, the limit at 0x26292C poked): before, Player 1 first
  with 0 points and a Bond of zeros; after, ranked by score with tied places. net-loopback OK, net-rejoin OK,
  net-driving-gun OK, net-driving-rejoin OK, SMOKE OK, Windows build.
- Next: John retests Bot 8's rows, an 8-bot Debriefing, and the car gun with the new log lines.

### 2026-11-18 - Driving.xbe: a failed mission's Replay no longer parts the cars

- John (his PC hosting, a laptop on WiFi, through the VPS relay): Alpine Escape failed, Replay, both back at tick
  911, the checks equal to tick 1920, then the shown camera's x 0.09 apart (nightfire_305.log, nightfire_157.log).
- Cause: Driving.xbe put the sounds' frame clock off at every stretch outside frames (the Replay's load) and on again
  at the next frame, and dsound::SetFrameClock(true) takes each playing buffer's place from the mixer, each machine's
  own. The sounds still playing through the load ended (and their voices were freed) at different ticks, the next
  sound took another emitter: the 2026-11-14 drift again.
- Fix (src/driving/clock.cpp): in a session the clock stays on from its first frame to its end, and the sound thread
  does only refills there through the loads too (no frees by wall clock). Single player is unchanged. Tried first:
  SetFrameClock(true) without the mixer's places (SDK). That broke default.xbe, whose clock goes on once with sounds
  the mixer has already ended (net-loopback desynced), so it was reverted; the SDK is unchanged.
- Evidence: the harness doesn't reproduce John's drift: three 720 s Alpine Replay runs on the old build matched, as
  two runs on the new one did. net-driving-gun OK, net-driving-rejoin OK, net-loopback OK, SMOKE OK. net-coop's
  memory compare differs as before (the open co-op difference).
- Next: John retests Alpine Escape's fail and Replay across two networks.

### 2026-11-18 - The public relay: a Vultr VPS (66.42.113.222), named by the Cloudflare list

- John set up a Vultr VPS (Ubuntu 24.04, master.py under nightfire-master.service: list on TCP 26580, relay on UDP
  26581, ufw open). From here: its /list NFM1, /servers names the worker; a host and a joiner on this PC through its
  relay (NF_NET_RELAY=66.42.113.222, unlisted): the host got NF-SBK8WG, the joiner found it on the relay, both booted
  with 2 connected. wrangler.toml's SERVERS names the VPS's list and relay instead of the test server's (deployed;
  the worker's /servers answers them), so every copy learns them at its next start and drops the old address.
- The VPS's /opt/nightfire/servers.txt should name its relay too (`relay 66.42.113.222:26581`): a start whose only
  answering list is the VPS's keeps what that list names, and without the line it would know no relay.
- Evidence: the session above, SMOKE OK.
- Next: John adds that line; a Windows test across two networks through the relay.

### 2026-11-18 - F1's page no longer jumps to the top while the mouse scrolls it

- John: scrolling the Online page fast, the page sometimes jumped back up. The taller footer (earlier today) scrolled
  the selected row into view whenever it grew; with the mouse the footer's help is the hovered row's, and the
  selected row could be at the top. SDK: only while the keyboard or a controller drives the selection
  (NavDisableMouseHover set, the highlight shown).
- John's play folder's settings.toml still has the test net_master and net_relay he typed (the IP he saw in the game
  list row): the built-in list is the worker's, which its servers.txt shows. Cleared in F1, both are the game's own.
- Evidence: build-win clean, SMOKE OK; the scrolling is John's to confirm.

### 2026-11-18 - The release carries no IP address: the Cloudflare list alone is built in

- John: the exes people download must not hold his VPS's address. BuiltInServers is the worker's list alone; its
  /servers (wrangler.toml's SERVERS) names the VPS's list and relay, which a first start learns and servers.txt keeps.
  So that a first start can host through the relay: every start asks the lists' /servers (it was asked only with a
  session or the browser open), and a host through a relay, or a joiner by a bare code, knowing no relay waits for
  the answer (AwaitRelays, 8 s at most). scripts/net-first-start.sh (new): an exe knowing list A alone, whose
  /servers names relay B, hosts through B and is joined there by the bare code.
- Evidence: net-first-start.sh, net-lists.sh, net-relay.sh OK; build-win clean, and `strings` finds the address in
  no exe or DLL (the worker's in both exes); SMOKE OK.
- Next: John's VPS servers file names the worker; the Windows test across two networks.

### 2026-11-18 - The Cloudflare list is built in: https://nightfire-master.nfsb.workers.dev

- John deployed tools/master/worker.js (wrangler deploy; its login needed --callback-host=127.0.0.1 or [::1] in the
  browser, Node listening on IPv6 loopback alone). From here: /list NFM1, /servers names the VPS's list and relay,
  and an announce (curl, port 26999) was listed and removed. BuiltInServers: the worker's list, then the VPS's list
  and relay (the Windows client's WinHTTP takes https; the Linux harness's http.cpp asks http:// alone, so the
  worker is not in a headless test).
- Evidence: build-win clean, net-lists.sh OK, SMOKE OK.
- Next: John adds `list https://nightfire-master.nfsb.workers.dev` to the VPS's servers file (copies out there learn
  of it); a host on Windows with Who can join Anyone shows in another PC's Internet tab.

### 2026-11-18 - Streamer mode: no typed names or IP addresses in the Server Browser and the connection screen

- John: Show Player 1, Player 2... hid typed names in the game alone; a streamer's Server Browser showed hosts'
  names (and History kept them) and the tooltips their IP addresses. net_default_names is now "Streamer mode: hide
  typed names and IP addresses" (the key kept): the browser lists a game as Game and a number of its own (its host
  id's, the same each time it is heard), unheard saved servers as Saved server N; SDK GameInfo::streaming leaves the
  address out of the tooltips and draws the Connect box, Add a server's box and join_ip's row as dots. The connection
  screen shows no address (the host's own, the host joined) and the address box is dots; a locked (Only people I
  tell) game's code is "(hidden)" on its host's screen, C copying it still. NF_NET_DEFAULT_NAMES is read before the
  no-session return (a browser alone). The joiner's empty address row says "Search this network (LAN)".
- Evidence: STREAM=1 scripts/net-browse.sh OK (Game 5053 waiting, then playing, Machine1 nowhere), net-browse.sh
  OK, build-win clean, SMOKE OK. The F1 page and the connection screen are not drawn headless: John's eye.
- Next: John checks it on Windows; then the servers.

### 2026-11-18 - F1's footer grows to hold a long help above the button prompts

- John: a long help (Relay server's) ran over the footer's "[A] Select  [LB/RB] Section  [B] Close" line; the
  footer was 3.4 lines tall whatever its text. SDK: it is as tall as the selected row's help, wrapped, needs (the
  last frame's, so no measuring before the page draws), half the window at most, the help clipped above the prompts
  past that; when it grows the selected row is scrolled back into view above it.
- Evidence: build-win clean, SMOKE OK; the window is not drawn headless (no virtual display here), so John's eye.
- Next: John looks at F1 > Online; then the servers.

### 2026-11-18 - The online settings by role: Hosting, Joining, Servers; How players reach me

- John found the Online page unclear before the servers go in: the Server list read as only where a hosted game is
  listed, not what every player's Server Browser reads; the joiner's empty address box ("Auto (LAN broadcast)")
  read as a host's way to list its IP; and Host through the relay, three sections from Host, looked like a hosting
  of its own, with Connection's rows not its.
- The page now: Hosting (Host online play; How players reach me, `net_host_by`: My IP address (direct) or A join code
  through a relay, which replaces the `net_host_relay` flag, the old key reading as the join code; Players; Who can
  join; the password), Joining (Join online play, then the SDK's address row, now "Host's address or join code",
  empty "Search this network", and Restart online play under it), Servers (hosting and joining both use them:
  "Server Browser's game list", `net_master`, whose help starts with what it is; "Relay server (hides IP
  addresses)", greyed while hosting by the IP address, saying it lists no games) and Advanced. The help of each says
  the other's job: the IP address's costs (shown when listed, the port forwarded) are How players reach me's, Who can
  join's choices say "in every Server Browser", and How players reach me's says Players, Who can join and the
  password work the same with both. A relay choice with Relay server none says so, as Who can join's do.
- SDK 44ee0bc: the joiner's address row's label, help and placeholder (RowText's default placeholder is empty now).
- Evidence: build-win clean; net-relay.sh OK (NF_NET_HOST_RELAY sets net_host_by), net-invite.sh OK (the withheld
  code and the password rule, AsksPassword on net_host_by); SMOKE OK. The page itself is not drawn headless.
- Next: John looks at F1 > Online on Windows; then his Cloudflare worker and second server go into BuiltInServers.

### 2026-11-18 - The match "desync" was the full-screen view: the game's memory is the same on both machines

- net-loopback.sh's comparison had failed since net_fullscreen became the default (2026-11-17). Its 4 differing
  runs are all drawing memory, at the top of the physical memory where the renderer's buffers are: two of exactly
  300 4K blocks (640 x 480 x 4 bytes: frame buffers), one of 113 blocks with 1129 bytes apart, and the render records
  at 0x3BFB000 (2026-11-11). With net_fullscreen each machine draws its own player's view (2,857,357 draws on one,
  2,675,218 on the other), so those differ by design. The older 0x1C4C068 (the matrices' unread fourth floats) is
  gone.
- Evidence: the same match test with NF_NET_FULLSCREEN=0 (both split-screen): "the game's memory is identical",
  NET-LOOPBACK OK, the kernel's clocks apart only.
- scripts/net-loopback.sh plays split-screen unless NF_NET_FULLSCREEN=1 is given; docs/online.md says why.
- Tests: smoke OK.
- Next: John's next list.

### 2026-11-17 - Unlock all of multiplayer (unlock_multiplayer), the missions left as they are

- John: an unlock-all-multiplayer option.
- The game's every-mission flag 0x25D79E is read at 11 places: beside each list's lock byte (+0x10 of a 0x18-byte
  entry) in the characters' list (0x7F3D0, 0x17C8E0's: a locked one is left out of the list), the bots' character
  (0x8AB70, 0x245348's), the scenario (0x87460, 0x17C7A8's), the map (0x87790, 0x17C6A0's), 0x86410's list
  (0x17C760's, three entries), the missions (0x75950 from 0x86E80 and 0x870F0, 0x86690's choice), the save
  (0x19A10, 0x19DB0) and its own toggle (0x82080). unlock_missions sets it each frame, so it unlocks multiplayer too.
- unlock_multiplayer (new, src/game/hooks.cpp): the flag set for the five multiplayer calls alone and cleared after,
  so the mission list and the save never see it; online the host's (kStart's flags byte, 2; kVersion 23).
- Evidence (two machines, NF_UNLOCK_MULTIPLAYER on the host): the joiner logged the host's setting; the bot menu's
  Bond, "This character is locked", took the A and opened his fields, and his entry read as MI6's afterwards. The
  lists draw a locked entry greyed as before (the help says so).
- Tests: build-win, smoke OK.
- Next: the match desync (2026-11-18: none; the full-screen view's drawing).

### 2026-11-17 - The host sets the bots: each one's character (or Random), Bots 7 and 8's fields, in the game's menu too

- John: the Player 1..10 character overrides become bot settings: Every bot's character kept, each bot its own too,
  a Random, the two greyed against each other, and the game's bot menu showing what F1 set (and its edits coming
  back); Bots 7 and 8, past the menu, folded sections with its fields; every bot's skill tied to Bot 1.
- The bot menu (Multiplayer Bot Setup, page 0x4000002C: 0x8ADE0, its changes 0x8AB70) read from its code and the
  disc's strings: the entry's fields are Accuracy Rating +0x00 (Poor 8, Average 5, Good 3, Very Good 1), Aggression
  +0x02 (word, 2..4), Health +0x04 (word, 50..300), Move Speed +0x06 (0..2), Reaction Time +0x07 and Recovery Rate
  +0x08 (50..200), the side +0x09, Personality +0x0B (None, Collector 1, Guardian 2, Team Player 3, Judge 4 on one
  side; Berserker 5, Greedy 6, Vengeful 7, Assassin 8 on the other), Playing +0x0E, team +0x0F, character +0x10,
  edited +0x11. A character's own statistics are 0x163628 + c * 14 (0x1A250); picking it sets the team to its side
  and the bot's name to the character's; from 15 on they are fixed. The menu's team is the side: there is no team
  row.
- Settings (game_info.cpp): Bots in a match, Every bot's character (Default, the 29, Random), Every bot's skill (Each
  bot's own, Bot 1's), Bot 1..6 character, folded Bot 7 and Bot 8 sections (character and the seven fields). The
  Player N overrides are gone (net_char_1..10, the session's host_slot_char). SDK: Option::enabled greys a row
  (every bot's against each bot's, the skill against Bots 7 and 8's fields, the fields of a fixed character).
- The session (net.cpp, kVersion 21): kStart carries the 24 settings and a seed for Random; the host's bundles carry
  one a tick (the slot that was the overrides', 5 bits of index now), so a change reaches every machine at the same
  tick. src/game/bots.cpp writes the characters into the menu's entries as its character list would (team, name,
  statistics unless edited) at each match's start (a new round of Random picks) and at the frame once a change has
  arrived whole (a round of 24 ticks after the last) or the menu got its defaults (0x88EA0's 0x51); the host's
  edits there come back to its settings each frame (one that makes the bots differ turns Every bot's off and gives
  each its own). Bots 7 and 8's entries are built at the spawn. An entry the menu never set up (no health: the
  bots past its Playing ones) plays its character's statistics, not zeros.
- Evidence (two machines, scripts as .local/pad-bots2.txt: the bot page's character list, one DOWN, the match):
  F1's Bot 1 shown in the menu (Bond); the DOWN to Drake came back as the host's Bot 1 character and both spawned
  it. NF_NET_BOT_CHANGE (new) at tick 3000 to every bot 13 and Bot 1's skill: the menu showed Bond Tux on both
  before the page; the DOWN (NF_UNLOCK_MISSIONS: a locked character cannot be picked, and none came back) turned
  Every bot's off, each bot its own, one write to the menu on each machine, and both spawned Bot 1 as 13, the
  other seven as 12 with Bot 1's accuracy. Random: the same pick on both. Bot 7's Fast and Very Good reached its
  entry. The comparison: the older desync.
- A restart in place (0xA2990) respawns the bots it has: Random picks again at the next match, not at a restart.
- Tests: build-win, smoke OK.
- Next: the unlock-all-multiplayer option; the match desync.

### 2026-11-17 - Bots below slot 4: 2 players and 8 bots, up to 10 in all with any number of players

- John: can the 6-bot limit be got around, so 2 players can have 8 bots?
- The engine's bots (0x1B170) live in slots 4..9, their state 0x768 bytes at 0x1D98E0 + (slot - 4) * 0x768. Only
  0x1B191 computes that address (every other use goes through the actor's +0x974), so a bot in slot 2 or 3 gets a
  block of net::BotBlocks() instead (the hook BotStateBlock): 4 blocks in the title's memory (AllocContiguous,
  in the rejoin snapshot), their address in the session's state. The state's +0x752 (slot - 4) indexes six-entry
  tables (each state's +0x3C seen times, 0x2600B4's 64 rows of 22, 0x37C10's), so such a bot has its slot's index
  (BotStateIndex): it shares those notes with the bot 4 slots up, no memory past the tables.
- The code that names bots by the six's table (0x262814): pickups (0xA84D0, the players at 0x1F6654 then the six
  bots) offer them to the bots below 4 too (PickupBot, PickupBots at 0xA87CD / 0xA8850); the restart in place
  (0xA2990) respawns them (its wrapper); 0x1A660, 0x1DA80, 0x37C10 (the bots' choice of target: who else hunts it)
  and 0x4E630 (team messages) leave them out, a weighting only. 0x26002C (the players and bots, 0x9CC50's
  last-one-standing count) counts them.
- net_bots (new, the host's): "The bot menu's" (the game's menu, six at most, as before) or 0..8, sent in kStart
  (kVersion 19); a match has as many as fit in 10. The bots past the game's six spawn after its own (SpawnLowBots:
  0x9D3D0's spawn point, 0x1B170 with the bot menu's first entries again), named on from the game's ("Bot 7").
- Evidence: net-loopback.sh HOST_ENV=NF_NET_BOTS=9 (2 machines): "a match of 2 players and 8 bots", bots in slots 2
  and 3 fighting, dying and respawning as the six do, slot 2's bot picking up 8 items and scoring a kill, the same
  on both machines; RESTART (the host's, YES) set the match up again with both; both machines to the end with no
  trap. The bot menu's 6 (HOST_PLAYERS=4, poked): 2 players and 6 bots, as before. The comparison: the older desync.
- Tests: build-win, smoke OK.
- Next: the unlock-all-multiplayer option; the match desync.

### 2026-11-17 - A match has the players who are in, the bots fill to 10, and every bot can be one character

- John: with net_players at 10 and two people in, setting bots crashed; he wants the live players plus the bots the
  host picks, up to 10, and the choice of one character for every bot. His log 134: 10 player slots, 2 connected.
- Cause: net::Players() was the larger of the players connected and net_players since 6a73ffa, so every seat no
  one took was an idle human, spawned into the bot slots 4..9 (sub_1B170 makes those players); the bot menu's bots
  had no slot left. Now a session counts only the players who are in (Players, and the host's Serve).
- The engine's limit (0x1B170, 0x1B4F0): bots live in slots 4..9 only, their state at 0x1D98E0 + (slot - 4) * 0x768,
  six at most; a bot below slot 4 is not possible. So 10 in all needs at least 4 people; with 2 it is 2 + 6.
  sub_A25F0's hook gives the players past 4 the first bot slots and the bot menu's bots the rest, as many as fit
  (LOGR "a match of N players and M bots"), and the menu's count back after the spawn (the match reads it as on/off).
- net_bot_character (new, the host's, 0 the menu's): sent in the session's start (kStartBots, kVersion 17) and the
  snapshot; sub_1B170's hook hands each bot a copy of its menu entry (or the game's default, 0x163636's) at 0x1DC550
  with the character set, the respawn the same. NF_NET_BOT_CHARACTER sets it for a test.
- The match test's walk had gone stale: a fresh profile now plays the title sequence (40 to 78 s by the wall clock)
  before the main menu, so the walk's Down missed it and the machines went into the co-op campaign. A second START
  (1300) skips it (mp-match, mp-match-stay, mp-quit). net-loopback.sh: HOST_PLAYERS, HOST_WAIT, HOST_ENV.
- Evidence: net-loopback.sh HOST_PLAYERS=4 HOST_WAIT=3 HOST_ENV=NF_NET_BOT_CHARACTER=12, the bot menu poked to 6:
  both machines "a match of 2 players and 6 bots" (before: 4 players), every bot and every respawn character 12, both
  to the end of the run with no trap. The comparison still differs: the older match desync (2026-11-10 entry).
  Not run: 5 or more machines (the past-4 path is the one 6a73ffa proved, now with fewer bots).
- Tests: build-win, smoke OK.
- Next: the unlock-all-multiplayer option; the match desync.

### 2026-11-17 - A new install starts in Fullscreen, and Fullscreen holds the cursor on the game's screen
- John: in Borderless the mouse goes off the game's window (onto another monitor), and wanted exclusive fullscreen
  as a new install's default. The runtime's Fullscreen is the same popup over the monitor (plus a mode change when a
  resolution is set; no DXGI exclusive mode, which a flip-model swap chain over the whole monitor does not need for
  its speed). What it lacked was the cursor: held in the window only while mouse look plays (input::MouseLooking).
- SDK: in Fullscreen the cursor is clipped to the window while it is in front and shown (HoldCursor, now asked on
  every WM_SETCURSOR, the 500 ms cursor timer and WM_ACTIVATE, so alt-tab lets it go), F1's settings window open or
  not. GameInfo::display_mode is a new install's mode (no settings file yet; Borderless unless a game says), so the
  other ports start as before.
- NightFire's GameInfo says Fullscreen. Existing installs keep theirs (settings.toml always holds display_mode).
- Evidence: Windows build OK; SMOKE OK. Not tried on a PC yet: John, with settings.toml moved aside for a first start.
- Next: John on Windows and under Proton (Wine clips the cursor by grabbing the pointer); then the bots.

### 2026-11-17 - Several server lists and relays, built in, and the lists name the others
- A new player saw no internet games until they typed the Server list's address (net_master empty meant this network
  alone), and every copy hung on John's one VPS. Now (src/common/net.cpp, `BuiltInServers`, `ServersOf`):
  - the Windows exes know the project's list (http://64.131.13.51:26580) and relay (64.131.13.51:26581); net_master
    and net_relay add theirs before them (commas between several), and `none` turns a kind off. The harness (Linux)
    has none built in (NF_NET_BUILTIN stands in), so the tests never reach a real server;
  - a host announces to every list, each on its own thread (a dead one logged once), and is off all of them at once
    on leaving; a browser fetches every list at the same time and shows a game on two lists once; the error row only
    when none answered;
  - a host through a relay takes the first that gives it a code (refused, or silent 3 s: the next); its kept code
    remembers its relay across relaunches. A joiner by a bare NF- code asks each relay in turn till one has the game
    (a round of refusals waits 5 s, under the relay's 20 misses a minute); NF-code@relay still asks that one alone;
  - once a start, every list is asked GET /servers (new in master.py, `--servers FILE`, and worker.js, the SERVERS
    variable): "list <url>" and "relay <host:port>" lines. The answers, and every list that answered, go to
    servers.txt beside the exe and are asked the next start. So a list or relay added later reaches copies already
    out there, and one that goes away leaves them the others.
- SDK: F1's Server Browser opens on its Internet tab.
- Evidence: scripts/net-lists.sh (new): a dead list and a dead relay first; the host went on to relay B for its code
  and was on lists A and B (the dead list said once), a browser found it on B, a joiner by the bare code was refused
  by relay A and joined through B, the session booted with 2 and stayed together; servers.txt kept list B and relay B
  as list A named them; a browser whose only list was the dead one found the game through the list its servers.txt
  kept. NET-RELAY, NET-INVITE, NET-INTERNET, NET-BROWSE, NET-PASSWORD OK; node test-worker.mjs OK; Windows build OK.
- Next: John adds the Cloudflare worker and his other server to `BuiltInServers` (and to his master.py's
  --servers file and the worker's SERVERS) once they are up; then the bots' count and characters.

### 2026-11-17 - A Replay's load holds the clock; the car's copied shots pay from the gun they copy
- John (internet, both PCs on the Replay fix): Enemies Vanquished failed, replayed, apart at once again (tick 3960,
  the game's time 0x2D against 0x2E); and once the missiles were out player 2's A fired nothing.
- Headless Enemies Vanquished (.local/lvl6) with the Replay: apart at tick 9300 with the sound services held. The
  log of the load's scheduler runs: only the read wait (0x10C92F), the services held, and still one count on one
  machine, two on the other: what the load stamps with the clock is a count apart. The hold was half of it.
- clock.cpp: from a session's disc wait to the frames (or another wait) the timer's callback counts nothing (the
  first wait switches under the frames' lock, no count between); a movie playing (its own thread, a mission's opening
  beside the load) counts as before. The sound services' hold is gone (no count, none due).
- The car: player 2's copies are player 1's machine-gun shot (a missile leaves no muzzle, never copied), but each
  took a round of player 1's current weapon: with the missiles chosen it emptied them, then fired nothing.
  coop.cpp: the copy pays from the weapon player 1 fired it with (g_car_weapon, in the rejoin snapshot) at its rate.
  Headless: player 1 fires the gun, then chooses missiles; player 2 holds A: the gun's rounds 993 -> 783, the
  missiles 60 throughout, the machines the same.
- net.cpp kVersion 15 (Driving.xbe 16): a build without these is "another build of the game" and cannot join.
- Evidence: Enemies Vanquished Replay (machine 2's disc at 2 MB/s) together 2186 ticks on; Alpine Escape's together;
  NET-DRIVING-GUN OK x5 (one earlier run crashed in the missing NTSC snow_intro.mad's stream, 0x14C800 -> 0x14B520,
  with the clock held under the movie: now it counts there; not seen again), NET-DRIVING-REJOIN OK, NET-RELAY OK,
  SMOKE OK; Windows build.
- The car-gun branch's unfinished diagnostics (another session, kept in .local/car-gun-session.diff) are replaced:
  its aimed car copies are not taken (John asked for no reticle in the car, the missiles home).
- Next: John: both PCs on this build (the version gate tells), a driving mission's Replay and the gun after missiles.

### 2026-11-16 - Wine and Proton draw this repo's builds (they were black)
- John: the launcher's build plays in Steam/Proton, but this repo's (scripts/build-win.sh's) under Wine or Proton
  runs and sounds with a black window; copied to Windows PCs it draws.
- His Wine log (.local/win): one "d3d12: vs_5_0 compile failed: <anonymous>:50:51: E5000: syntax error", then no
  shader compiled. Line 50 is the vertex prelude's ilu_rsq: `1.#INF`, an infinity Microsoft's compiler reads and
  Wine's d3dcompiler_47 (vkd3d-shader) does not. Windows has Microsoft's in system32; release-package.bat copies the
  SDK's redistributable beside the exe (the launcher's play folder has it); build-win.sh did not.
- sdk/runtime/gpu/shaders.cpp: infinity is asfloat(0x7F800000u) (ilu_rsq, ilu_log). build-win.sh copies
  d3dcompiler_47.dll (D3DCOMPILER_DLL, or .local/win-sdk/d3dcompiler_47.dll) beside the exes and into an install
  without one; plain Wine loads it only with WINEDLLOVERRIDES=d3dcompiler_47=n.
- Evidence: a test exe compiling the harness's dumped HLSL (AUDIT) under Wine 11.21's builtin compiler: the old
  prelude fails at 50:51 as John's log, the new compiles (4 of 4); DXC (shadercheck) 4 of 4; Windows build; SMOKE OK.
- Not covered: the post passes (SMAA, FXAA, scaling) are not in the dump. Next: John, the build under Wine/Proton.

### 2026-11-16 - A driving mission's Replay keeps the machines together; F1 on the connection screen
- John (an internet session through his own relay, 2 missions then Alpine Escape): apart at tick 3000, the first
  check after the mission failed and was replayed (his log: "every player at tick 2986", then the game's time 0x23
  against 0x24 and the camera a frame on).
- Headless: Alpine Escape on two machines left alone fails, player 1's A takes Replay; apart at tick 6120 in 2 runs
  of 3 the same way (0x28 against 0x27). The Replay's load is 20 ms in the disc's waits (0x10C860 a read, 0x10BEA0
  all of them), whose scheduler runs follow the timer: one machine counted 1 there, the other 2, and the only slot due
  was the sound library's services (0x13D3E0 -> 0x13F980, the streams' 0x13BF10), one run more on one machine. The
  game's time starts from the streams, a frame earlier there. (First tried: the frame's last step left a count
  behind in clock.cpp's Set; it was not, both stamped right.)
- src/driving/clock.cpp: in a session the services wait through the disc's waits (their due 0x10000 counts on, no
  timer count reaches it) and are due at the frames' first count on every machine; a movie's waits run them again.
- Evidence: with machine 2's disc at 2 MB/s, apart at tick 6120 with the hold off, together 2785 ticks past the
  Replay with it; NET-DRIVING-GUN OK, NET-DRIVING-REJOIN OK, SMOKE OK.
- F1 on the connection screen (John: it did nothing, the window's own F1 drew nothing before boot): the wait ends
  without a session (host or joiner), Join stays on for Restart online play, and the settings window opens on Online
  at the game's first frame (app::OpenOverlayAt); the screen's hints say "F1 settings". The meet's pump takes F1's
  key itself, so the window's toggle can't close it again. Windows build; not testable headless.
- John's nightfire_283.log is another start (his player's ESC on the join screen, then single player), not the
  session's joiner.
- Next: John: the same Replay over the internet.

### 2026-11-16 - The server list tells a host whether the internet reaches it
- John: add the port check.
- tools/master/master.py: an announcement of a game by its address is answered "reachable" or "unreachable" after
  the list sends the game the browser's query (UDP, the address the announcement came from, its port; 3 tries of
  0.5 s; kept 5 minutes when it answered, 20 s when not; IPv6 and relay games are not asked). worker.js cannot send
  UDP and answers as before.
- net.cpp: the host reads it (Lister::reach), logs it once per change (report level when reached, an error with what
  to do when not), and its connection screen's hint says "players anywhere can join" or "the internet cannot reach
  UDP port N here: forward it on your router (or host through the relay)".
- Evidence: master.py's probe False on a closed port, True on one answering as the game does; net-internet (now
  checks "the server list reached this game") OK, net-relay, net-invite OK; SMOKE OK.
- Next: John; the list server he runs needs a restart for it (and for the withheld codes).

### 2026-11-16 - Online page: a listed host waits with no limit; Who can join under Connection; Server list
- John: a listed host's wait should not run out; "Internet game list" does not say it is where games are listed;
  how does an IP host get on the list.
- net.cpp: a host on the server list waits with no time limit (its players come from the list while it waits; the
  players asked for, Enter / START or ESC end it), and net_wait 0 means no limit for any host or joiner; the
  connection screen drops its seconds left then.
- game_info.cpp: Who can join and the password moved into Connection (after Players; Restart online play after
  them): an IP host lists its game there with Anyone, no relay needed. net_master is "Server list (address)", and
  Who can join's listed choices say "but no Server list is set below" while it is empty.
- Evidence: net-internet (now also checks the no-limit wait), net-invite, net-password, net-browse OK; SMOKE OK;
  built for Windows, net.cpp and game_info.cpp clean with -Wall.
- Next: John looks at the page; later, the list server could check a host's port from outside and say if it is
  reachable.

### 2026-11-16 - Joining made easier: Who can join, a Connect box, the connection screen's box
- John: drop the Online page's games section; let a joiner type the address on the connection screen; through the
  relay, the code is the key (no password); a password for IP hosting; Join off after the session.
- Who can join (net_who, replaces net_public; an old net_public = true reads as Anyone): Not listed, Anyone (on the
  internet list), Only people I tell (listed locked). Through the relay the key is the join code: the list gets
  code "-" and the relay (tools/master: master.py and worker.js take it; find flag 16), a joiner through the relay
  passes, one by the LAN address gives the code as its password (kRefusedCode). Otherwise net_password, its row
  shown only then (SDK: Option::shown). The relay stops answering an address after 20 codes of no game a minute.
- SDK (overlay.cpp, app.h): the Online page's Games on the network gone (GameInfo::online_games_after with it);
  the Server Browser's Connect box (an address or code typed or pasted joins at once); its prompt asks for the join
  code (unmasked; with the relay the list names, joined as NF-code@relay) or the password; FoundGame::relay and
  asks_code.
- Connection screen (Windows): a joiner's box with join_ip, Enter saves it (first in the history) and starts again;
  refused, it asks for the password or code instead, the wait from the last key typed. A host's "Copy join code"
  button (or C) copies NF-code@relay.
- net_join goes off by itself once the session is over (this machine left, or the host is gone) or none came of it
  (net::join_setting; join is the session's role from Init). Harness joins (NF_NET) leave the settings alone.
- Evidence: scripts/net-invite.sh (new) NET-INVITE OK (the list shows "-", the browser hears it locked by its code,
  a wrong code refused, the code joins through the relay and by address as a password, 3 boot together);
  net-password, net-relay, net-browse, net-internet OK (its ports moved off 26581, the relay's default, which a relay
  on this PC holds); worker.js test OK; SMOKE OK; built for Windows, net.cpp and overlay.cpp clean with -Wall.
- Next: John: the connection screen's box and Copy button, the browser's Connect box and code prompt, Join off after
  a session (none of them headless). A relay already running needs a restart to list withheld codes.

### 2026-11-16 - F1's Server Browser page; game passwords
- John: a real server browser (Steam's Game Servers window), its own F1 page below Online, with passwords; Map and
  Mode may stay empty for now.
- SDK (overlay.cpp, app.h, settings.cpp): the Server Browser page: Internet, LAN, Favorites and History tabs; a
  sortable table (lock, Server, Status, Mode, Map, Players, Ping, Via, or Joined on History); filters (hide full,
  other builds, not answering, locked; a name search); Refresh, Add server..., Favorite, Forget, Connect (or A /
  a double click on a row); a password prompt before a locked game. Favorites and history (30) are saved in the
  settings, with join_password. The Online page's games section is one row that opens the page. Option::secret
  (a password row shows stars); GameInfo::ask_games and refresh_games.
- net.cpp: net_password ("Password for my game", NF_NET_PASSWORD): the game is listed locked (find flag 8, passed
  through by tools/master), the hello carries the joiner's join_password (NF_NET_JOIN_PASSWORD), the host refuses a
  wrong or missing one (kRefused seq 1, logged); the joiner's connection screen says so and it boots alone after
  5 s. The finder asks the favourites' and history's addresses directly (NF_NET_FIND_ASK too), refreshes on
  demand, and tells each game's mode (on foot or driving) and how it was heard (LAN, internet, relay, direct).
- Evidence: scripts/net-password.sh (new) NET-PASSWORD OK (heard locked, the wrong password refused, the right one
  plays); net-browse, net-internet, net-relay OK; SMOKE OK; built for Windows. heap.cpp's unsequenced ++ fixed.
- Next: John tries the page and a locked game; a password through the relay is untested; the Map column waits for
  hosts to send their level.

### 2026-11-16 - F1's Online page: Advanced folds under Connection; the relay rows reordered
- John: Advanced right under Connection, folded (its header with an arrow to open it); Internet and relay as Host
  through the relay, List my game on the internet, Relay server, Internet game list. The page: Connection, Restart
  online play, Advanced (closed at first), Games on the network, Internet and relay, Co-op, You, the host's
  character overrides.
- SDK: Option::folded (a section header that opens and closes its rows: a triangle, right when closed and down
  when open, by mouse, keys or pad; closed at first) on the Online and Game pages; GameInfo::online_actions_after
  split in online_restart_after and online_games_after (null: the end, as before).
- Next: John looks at the page. SMOKE OK; built for Windows.

### 2026-11-16 - F1's Online page in sections
- John's layout, with headers: Connection (Host, Join, the host's address or join code, Players), then Restart
  online play and Games on the network under it; Internet and relay; Co-op (the host's settings are everyone's:
  friendly fire, more and tougher enemies); You (this PC: name, character, handicap, Player N names, full-screen
  view); the host's character overrides; Advanced at the bottom (UDP port, wait, input delay). The settings' keys
  are the same, so settings.toml files carry over. Host through the relay is greyed while Relay server is empty.
- SDK: GameInfo::online_actions_after (the option Restart online play and the games found follow; null: the end,
  as before) and Option::needs (greyed while a text option is empty). Other games' pages are unchanged.
- Next: John looks at the page. SMOKE OK; built for Windows.

### 2026-11-16 - Co-op: each player's watch hands on their own dial
- John: in co-op, player 1's watch hands (the laser) came off the dial when he moved (the Exchange, down the stairs
  after the safe); not in single player. 0xB8CB0 (from the player's update 0xAE1C0) makes the hands as three world
  objects (0x2000876-78) once, at 0x279150/4C/48, and each frame puts them on the watch model's dial, or deletes
  them when that player's model isn't the watch. One set for every player: player 2's update moved player 1's hands
  or deleted them. Its hook gives each player their own set (player 1's in the game's globals, cleared with them by
  0xB6C20; kept in the rejoin snapshot). The game makes them in every camera (0x1F; the view model is in none, its
  own pass): in full-screen view they are in every camera on their player's machine, in none on the others.
- Headless (07000005, characters 0 and 12, player 1 takes the watch out and walks, player 2 walks without it): both
  machines make player 1's hands at the same addresses, and they sit on the watch model's world position (0.75 units
  off its origin) after the walk. New memory differences: the hands' masks (6 bytes), the frame's sorted draw list
  (0x3766840) and its count (0x3BE6FCC), which hold the hands on player 1's machine alone: draw-only. The reference
  renderer draws no hands, in single player too, so they are John's to see.
- Next: John retests the watch and the door hint. SMOKE OK.

### 2026-11-16 - The door hint once a level, both kinds of door; the watch's hands not seen headless
- John: the sight is fixed; the door hint showed twice in a level; the watch shows, but its hands come off it when
  he moves on stairs. The second door hint: 0xB9580's type 0x1C door shows 0x200000B (from 0x2F5B0), also "Press
  Action to open door" (read headless), the first 3 times a boot ([0x279154]). Both texts now count to the one per
  level, and after it 0xB9580's hook holds [0x279154] at 3 through the call and puts the game's own back.
- The watch: two machines (07000005, characters 0 and 12), each taking the watch out, then walking and looking up and
  down, snapped through it: on both the watch stays on the wrist, and no hands are drawn on its dial at all (John's
  screenshot has a white arrow and a line). Asked John which gadget and whether single player does it too.
- Next: John retests the door hint; the watch's hands from his answer. SMOKE OK.

### 2026-11-15 - The door hint once a level
- John: "Press Action to open door" should show at the first door, not every one. 0xB9580 (every 6th frame, the
  player's object) shows it when the wait at 0x279158 is 0 and sets it to [0x17C0F4]*20, counting it down first; the
  text is 0x6D460(0x2000009) returning to 0xB9874. 0x6D460's hook counts that fetch, and after the first in a level
  (id [0x1F658C]) 0xB9580's hook holds the wait at 2 or more, so the test for 0 never passes again. Kept in the
  rejoin snapshot (SaveViewState). Single player too; the 0x1C hint keeps its own cap of 3.
- The castle co-op run's memory difference at 0x1F75000-0x1FA6000 (about 90 KB in one level-data block, tag 0x1204,
  same block layout on both machines, no "games differ") is the same on the build before the sight and watch change
  (833568e): not theirs; the only new runs are the sight's mask bytes.
- Next: John retests the sight, the watch and the door hint. SMOKE OK.

### 2026-11-15 - Co-op: the guided rocket's sight off the other screens; the watch on every wrist
- John's screenshots: player 1's guiding sight (brackets and arc) on player 2's screen, and the watch gadgets drawn
  as the dial's marks on a bare wrist. The sight is 0xB66C0's three 3D objects in camera 5 ([obj+0xD4] 0x20), the
  2D pass every machine draws: 0xB4760's hook now leaves another machine's player's in no camera (and puts a
  machine's own back to 0x20, as a rejoin's snapshot brings the host's 0). Headless the run's only new memory
  difference is those 6 mask bytes (draw-only; the 0x3BF9000 runs are there without the change too).
- The watch: Bond had the Ninja's skin 4. 0x11440's hook applied the co-op hands inside the spawn (0xACAB0), before
  the spawn's end read the mission's skin from the player, so the mission skin recorded was the first multiplayer
  character's. The hands now wait for the spawn's end. And the watch gadgets' model (0500004B) has its watch body
  in the mission skins' textures alone ([b8+0x88], 0x1634C0's entry by skin): a multiplayer character's skin left
  the dial's marks. 0x118F0's hook gives that model the mission's skin, so a Ninja's watch is Bond's gloved wrist.
  Headless (07000005, characters 0 and 12, Black twice to the Laser): both wrists show the whole watch.
- The "Press Action to open door" hint (0x2000009) is the game's own: 0xB9580 shows it near a closed door every
  20 s with no count (the other hint, 0x1C, has a cap of 3 at 0x279154); asked John whether to cap it.
- Next: John retests the sight and the watch. SMOKE OK.

### 2026-11-15 - The level heap in every campaign level, 1 to 4 players; a full heap is fatal: what fills it, logged
- scripts/heap-levels.sh <players> [levels]: each level started headless (offline, or a co-op session of 2 to 4
  machines), heap.cpp's "starts with" line. The campaign's on-foot levels are 07000001-0D and 07000011-16 (19; the
  others that load, 1B and 23-29, are the multiplayer maps). Free heap at the start, MB, 1 / 2 / 3 / 4 players:
  the tightest 07000007 13.8 / 12.5 / 12.1 / 11.6, 0700000C 13.2 / 12.1 / 11.9 / 11.7, 07000014 14.0 / 13.0 / 12.9
  / 12.7; Phoenix Fire 07000008 17.0 / 15.9 / 15.7 / 15.5; the roomiest 07000004 25.7 / 24.7 / 24.6 / 24.6. The
  second player costs 1 to 1.3 MB (their characters' 879 KB), each more 0.2 to 0.5 MB (07000002's 63 more enemies
  with 4). Every level starts with 11.6 MB or more: what filled 07000008's 16 MB came at the helicopter, not from
  co-op's start, and a few MB more of heap per player would not have held it. Not scaled.
- NF_HEAP_FILL=<session tick> (headless): the heap filled at the first next fit from that tick. At the level's start
  the game's own 90 KB setup then fails and crashes; mid-fight (tick 3767) the refusals go on 0.17 s, then 0x18BB0
  (a player's update, 0xAD210) writes through a refused 180 bytes (guest 0x7C): the game does not check every
  allocation, so a full heap is fatal whatever the allocator does. The first refusal now logs the heap's blocks by
  tag (0x70AE0's second argument), before anything can crash: "1204 33214 KB in 227, FF04 15850 KB ..." (0xFF04 the
  test's own).
- Next: John: the helicopter in co-op again; the "full heap's blocks by tag" line names what filled it. SMOKE OK.
- John (097 Linux, 296 Windows): through Phoenix Fire's helicopter, no crash and no refusal (07000008 started with
  16.4 MB free; about 4 minutes in the level against the crash's 7.5). The guided rocket's red reticle still shows
  on player 2's screen while player 1 steers: not the sight's 3D objects (the "type 01 drawn in cameras 01" line at
  each level's start is them, player 1's camera alone) nor 0xB4760's sprites (0xB6190's hook hides a remote
  player's after it). Asked John for screenshots of both screens.

### 2026-11-15 - Co-op: a full level heap no longer crashes; the guided missile's sight is its player's alone
- John and a player on another network, through the relay (join code; no desync): both machines stopped together at
  Phoenix Fire's helicopter (07000008) as the last rocket hit, C0000005 reading guest 0x0000000B. The registers
  (ebx 0x120, ebp 3, edi 0x13E) and the guest stack (0x2DC29, 0xCD198, 0x6C254, 0x2DD84) place it in 0x70AE0, the
  level heap's allocator, at 0x70BC9: its next fit had found no block, stored 0 as the next place ([0x2237D8]) and
  returned 0, and the next allocation read through the 0. The heap is 0x3100000 bytes of contiguous memory (0xDC870,
  0x70A40), so it was full: an effect's object (0xCD140, 0x2DC10) at the helicopter's fall.
- src/game/heap.cpp: the next fit starts at the heap's start when that pointer is 0, so a full heap refuses the
  allocation (0, which the callers take) and plays on. A refusal is reported (the 1st, 2nd, 4th... with the free
  space and the largest block), and each level's start reports the heap and the console's memory free.
  Headless co-op 07000008 (Ninja and Ninja): 16 MB of the heap free at the start, 15.7 MB still after 4 minutes of
  fighting (dumps at ticks 3000 to 12000, .local/heapwalk.py): no steady leak, so the fill came with the boss
  itself, which the harness does not reach; 7.3 MB of the console's memory is free, the same in every level (room to
  grow the heap if John's next log shows it full again).
- The guided rocket's red sight showed on the other players' screens: 0xB4760 updates a player's missile view, and
  beside its sprites (hidden for the others with the HUD) its group has three 3D objects ([group+0x04]) shown while
  the player steers ([obj+0xD2] 10). World objects are drawn in the cameras of their [+0xD4] bits, all of them here;
  in a session they are now the player's own camera's alone (view.cpp). Not seen headless (no guided rocket in the
  pad scripts). LogLeftObjects no longer reports them, nor the missiles (type 05: not in the steering player's
  camera), the "co-op object ... type 05" lines of John's log.
- Found on the way, headless: level-fight.txt in 07000008 opens both players' pauses (the level is up before its
  START) and its presses go into PC Settings: both machines crashed at frame 5520 writing through a null text sprite
  (0x90FC0, the menu's line) with the 2D sprite pool full (512: two HUDs, two pauses, a PC Settings page). A 2-player
  co-op level now gets 1024 as 3 and 4 do (35 KB of the heap): the same run plays to its end.
- Tests: the co-op runs above (each machine's frames and the free heap differ as before, the session's check never
  says the games differ), SMOKE OK, build-win OK.
- Next: John: the helicopter again (the log's "heap" lines), and a guided rocket in co-op.

### 2026-11-15 - No thread_local with a constructor: Windows' early threads aborted the exe (John's driving relaunch)
- John's three WER dumps (LocalDumps, full memory), read with python's minidump against each build's .map:
  - 31064, the relaunch's C0000409: a loader pool thread in abort, from __CxxFrameHandler3 under __dyn_tls_init,
    __tls_init and the constructor of fibers.cpp's thread_local std::unordered_map (t_fibers); the main thread was
    still in the CRT's own start (__acrt_initializers, setmbcp), before .CRT$XCB, so no stop note.
  - 28152 and 27088 (27088 his direct start at 14:42, log 277 ending at 0.77 s): an access violation in ntdll on a
    pool thread under __dyn_tls_init, the main thread in wWinMain's dispatch::Load (LoadLibrary of the game DLL).
- The cause: MSVC builds a thread_local with a dynamic constructor (or a destructor to register) in a TLS callback on
  every thread the process starts, Windows' loader and thread pool workers too, at any point of the start. The
  unordered_map's allocation there ran against a heap the CRT had not set up; how early the pool thread came is
  chance, so it struck some starts and never under Wine.
- SDK: fibers.cpp's map, threads.cpp's current thread (t_current) and dsound.cpp's flushed list are pointers to
  objects made on first use; ExitThread frees the current thread's. Neither exe's .map has a __tls_init now.
  Tested: SMOKE OK; NightfireDriving.exe under Wine boots and draws (291 of 299 frames in 10 s).
- Next: John: the driving relaunch on the Windows joiner again.
- John (15:49): no crash; his WER dumps stay on, and other people test on their own PCs.

### 2026-11-15 - NightfireDriving.exe's stop on John's Windows joiner: abort before its log (found: the next entry)
- John's 276 (Windows joiner, default.xbe) logged the new line: "relaunch: NightfireDriving.exe ended before its first
  frame, exit code C0000409", 1.8 s after the relaunch. Windows' Application log: the fault in NightfireDriving.exe at
  +0x29B900, the static CRT's abort (raise(SIGABRT), then int 29h code 7, FAST_FAIL_FATAL_APP_EXIT). No log of its own
  and no crash dump: the SIGABRT handler (set first in wWinMain, it writes CrashDumps and flushes the log) had not
  run, so the stop came before wWinMain, as the exe's static objects were built. Started by hand (277) the same exe
  boots and asks the host; under Wine it starts as a relaunched joiner too, and a Wine joiner of a headless host
  never reached a mission (the title's START opens the menus in a session). Not found by reading: the statics that
  read the environment or command line, the named objects (all made in wWinMain).
- SDK: app/main.cpp installs from .CRT$XCB (before the exe's own statics) handlers for abort, std::terminate (the
  exception's type and what()), the CRT's invalid parameter and pure virtual calls: logs\startup-crash.txt with the
  reason, the command line and the stack as module+offset, and the log's error line once it is open. Tested with a
  static whose constructor throws (Wine): "terminate: class std::runtime_error: static init threw" and its stack.
  scripts/build-win.sh writes each exe's .map (the 14:07 build relinked with one: .text identical).
- Next: John: the relaunch again; logs\startup-crash.txt (and the .map files beside the exes, if he keeps them).
- John again (278, 15:18, the build with the notes): the same C0000409 1.5 s after the relaunch and no
  startup-crash.txt: the abort comes before .CRT$XCB too. The two exes' early slots (.CRT$XI*, XCA/XCAA, the TLS
  callbacks XLC/XLD) are the same CRT pieces; the namespace thread_locals (built by the TLS callback) are trivial.
  Three ways of reading it found nothing: stopped guessing. A full WER dump (LocalDumps, DumpType 2, John's
  nightfire-dumps.reg) of the next stop, read here with python's minidump against the build's .map.

### 2026-11-15 - A relaunch that never booted no longer leaves its mission to the next start
- John (Linux host 070-073, Windows joiner 272-275; Windows' clock 4 min 49 s behind): player 1 picked a driving
  mission, both relaunched into Driving.xbe (071, 274). Windows' NightfireDriving.exe wrote no log at all, so it
  ended before wWinMain's first line (CreateProcess worked: its error is flushed and 274 has none; both exes import
  the same DLLs, and the same build ran Driving.xbe under Proton). Linux's (072) sat meeting, waiting for player 2.
  Both restarted default.xbe by hand, and each booted on "launch data from the previous run": the page default.xbe
  had written for Driving.xbe, played as a return from the mission (its results screen).
- SDK: kernel/boot.cpp reads the image a launch page names (+8) and drops a page for another image ("launch data for
  ... left from a relaunch that never booted"). On Windows a relaunched process that ends before its first frame
  is logged by the old one with its exit code (the loader's or an antivirus's stop leaves no log of its own).
- Headless: default.xbe with .local/menu-launch.bin (default.xbe's page for Driving.xbe) as its launch.bin boots to
  the title afresh, the file gone. Smoke OK (Driving.xbe takes its page), build-win OK.
- Next: John: if Driving.xbe stops again, default.xbe's log has its exit code; Windows Security's Protection
  history for NightfireDriving.exe.

### 2026-11-15 - On foot the sounds end by the session's ticks (John's co-op desync in 07000006)
- John's logs (Linux host, Windows joiner, before the relay): the two logs line for line the same until the host's
  "net: the games differ from player 2's machine at tick 8760" about 37 s into 07000006: the level and every
  player's place the same, the random numbers (0x18CDF8, 0x18CDFC) apart and the joiner 20 objects ahead
  (0x1DF828). From there each machine respawned a different player ("co-op player 2 respawned" on the host, "1" on
  the joiner): player 2's PC had player 1 dead (no model; the name tag and the sounds go on with the entity).
- The cause: the game's sound frame (0xE19C0, from 0xDD8E0 at the end of each drawn frame) asks each of its 64
  voices' buffers (0x2AE8C0, 0x40 bytes each) GetStatus and frees the voices done, and the next sound takes the
  first free one. GetStatus was the host mixer's: two PCs' sound devices end the same sound a frame apart, the
  shape Alpine Escape's driving desync had (2026-11-14, the sound library's frees). Now src/game/hooks.cpp turns
  dsound's frame clock on at a session's first frame and steps it by the session's ticks in the game's frame
  (0xDD1D0): a buffer still plays by the ticks, the same on every machine (the mixer keeps its own pace for what
  is heard). Its tick goes in the rejoin snapshot; net kVersion 13 (14 Driving.xbe). The log says "the sounds end
  by the session's ticks". The other position read (0xC9030 -> 0xE0DE0) is the music stream's refill.
- Also tested, not the cause, kept: the x87's fsin, fpatan, f2xm1, fyl2x and the rest and the CRT's sin, pow,
  atan2 and the rest called the host's libm, which differs between glibc and Windows by an ulp or two (sin 3%,
  exp2 38%, log10 14% of inputs). SDK: cpu/xmath.cpp (musl 1.2.5, third_party/musl_math) on every OS, the game
  DLLs importing x86_sin and the rest from the exe; Linux and Windows (Wine) give the same bits for all 12. A
  split-screen 07000005 for 300 s with one machine on glibc's (XBE_HOST_MATH=1) stayed identical, as two on musl.
- Headless (one PC's two mixers too alike to split): 07000006 with the host fighting (scripts/pad/level-fight.txt)
  for 240 s, without and with the change: no "games differ", the same respawns, the same draws to the end. A
  split-screen 07000006 draws a little apart on the two machines from frame 2040 the same way in every run, with
  the change or without (something each machine draws its own way, not yet named; the checks stay equal). Mouse zoom: its log line is the local
  player's (it said every frame with two players). Smoke OK, build-win OK.
- Next: John, two PCs: co-op through 07000005-06 shooting and scoping; any "games differ" line with its tick.

### 2026-11-15 - The server browser, step 3: the relay (join codes, no IP shown, no port forwarding)
- tools/master/master.py --relay <udp port> (26581 the game's default): a host registers and gets a join code
  (NF- and 6 letters), a joiner asks for the code's game and gets a connection number, and the relay passes each
  wrapped packet on unchanged (packet types at the top of master.py). Only the host's endpoint and the joiner's own
  connection pass packets; a host's random token lets the exe it relaunches into (Driving.xbe, F1's restart) take
  its code back from another port, and after a relay restart a host and joiner ask for the code and connection they
  had. 8 hosts an address, 16 joiners a game, 1500-byte packets, 5000 packets/s an endpoint, forgotten after 60 s.
- src/common/net.cpp: settings net_relay (the relay's host[:port]) and net_host_relay ("Host through the relay").
  join_ip "NF-XXXXXX" (or "NF-XXXXXX@relay:port", what the list's press writes) joins by code. A relayed machine has
  a stand-in address in 240.0.0.0/8 (the host 240.0.0.0:1 on a joiner, connection c 240.0.c:c on the host):
  Send wraps for the relay, Receive unwraps, and the session's code (FromSession, rejoins, relaying) is unchanged.
  The receive thread registers / asks every 500 ms till answered, then every 5 s. The host's connection screen and
  log show the code, never its address; a joiner's say "through the relay". A relayed host announces its code and
  relay to the list; master.py and worker.js show such a game's address as "-" (older builds skip it), and the
  browser lists it "- through a relay", joinable without a UDP query.
- SDK (overlay.cpp): join_ip's row is "Host IP address or join code".
- Tests: scripts/net-relay.sh (a list and relay on this PC; a relayed, listed host; a browser; a joiner by the
  code from the host's log): the list shows "-" and the code alone, the browser found "Machine1 at
  NF-ZDVRVY@127.0.0.1:26593, waiting for players, 1 of 2 - through a relay", the session booted and both machines
  drew the same (66828 draws at frame 3540). RELAY=1 scripts/net-rejoin.sh: the new joiner by code got the 27 MB
  state through the relay in 4.4 s and the games are the same 40 s on. A protocol check by hand (fake host and
  joiners): a spoofed connection's packets dropped, a relaunched host keeps its code with its token and not
  without. net-internet OK, test-worker OK, smoke OK, build-win OK. On Windows: John (a VPS relay).
- Next: John on two PCs through a deployed relay.

### 2026-11-15 - The server browser, step 2: the internet game list
- tools/master: the list server anyone runs. master.py (Python's standard library, a VPS; --trust-proxy behind a
  proxy) and worker.js + wrangler.toml (Cloudflare's free plan, the list in one Durable Object), one protocol: POST
  /announce and /remove (forms), GET /list (tab-separated lines). A game's address is the announcement's source,
  so nobody lists another's PC; 8 games an address, 1000 in all, gone 150 s after its last announcement. README.md.
- src/common/http.cpp: a blocking HTTP client, WinHTTP on Windows (http and https; winhttp linked in CMake and
  build-win.sh), a socket on Linux (http, for the harness).
- src/common/net.cpp: settings net_master (the list's address, empty: LAN alone) and net_public (off; its help says
  the list shows the host's IP and the port must be forwarded). A listed host announces at Init, every 60 s and
  within 5 s of its players changing, and leaves the list as it leaves its session (or boots without one). The
  browser fetches the list every 15 s while F1 shows it and asks each listed game as a LAN one (UDP query, ping);
  one that does not answer shows "not answering (its port may not be forwarded)", greyed. The query's answer
  carries a random host id, so a host heard on the LAN and on the list shows once (the LAN's). join_ip takes
  ip:port; a joiner of a public address no longer broadcasts its hellos to the LAN, so a LAN host cannot take it.
  SDK (overlay.cpp): a row without an address (the list's error) shows its status as help.
- Tests: scripts/net-internet.sh (master.py on this PC, a listed host, a browser on another port that can only know
  it from the list, a joiner by 127.0.0.1:port): listed at once, "waiting for players, 1 of 2" 2 ms, then
  "playing, 2 players, full". node tools/master/test-worker.mjs: worker.js OK. net-browse OK, net-rejoin OK,
  smoke OK, build-win OK. On Windows: John (a deployed list; https through WinHTTP).
- Next: the relay (join codes, no IP shown, no port forwarding).

### 2026-11-15 - The server browser, step 1: F1 Online's games on this network
- John: a server browser as old PC shooters had, LAN first, then an internet list anyone can run (a Cloudflare
  Worker or a VPS), and a relay so a host needn't give out their IP.
- src/common/net.cpp: a host answers a 9-byte query (magic "NFF1", never versioned, so another build still hears and
  is shown as such) on its session port with its name, wire version, players in and wanted, and flags (under way, a
  seat free, driving). A finder thread with its own socket broadcasts every second (255.255.255.255, the subnet's,
  loopback) while F1 shows the list, forgets a game after 3.5 s; the ping is the query's round trip.
- SDK (app.h, overlay.cpp): GameInfo::find_games and a "Games on the network" section on F1's Online page, one
  button a game (greyed if full, under way without a free seat, or another build); pressing one sets Join to its
  address and restarts online play.
- Test: scripts/net-browse.sh (a host waiting for 2, a browser with NF_NET_FIND=1 logging what it hears, a joiner at
  20 s): the browser saw "waiting for players, 1 of 2", then 2 of 2, then "playing, 2 players, full", 2 ms. net-rejoin
  OK, smoke OK, build-win OK. The list itself (ImGui) needs Windows: John.
- Next: the master server (tools/master: a Cloudflare Worker and a Python script), then the relay.

### 2026-11-15 - F1's Restart online play meets again; the driving takeover notice; the unlock switch's note
- John: F1's Online Restart went back to the title without the connection screen, on any PC, in a game or not;
  a notice when the next player takes the car; the unlock switch should say greyed missions play.
- The restart's new process inherited NF_NET_OFFLINE, which QUIT on the pause screen sets (LeaveToMenu, so the
  exes it relaunches into stay out of the session): net::Init then booted without meeting. SDK (overlay.cpp,
  app.h): the button is "Restart online play", greyed while Host and Join are both off, and calls the new
  GameInfo::restarting before the relaunch; net::Restarting leaves the session first (kLeave, as closing the
  window: the host offers the seat at once, the rejoin path) and clears NF_NET_OFFLINE, so the new boot meets as a
  first boot does. A restarted joiner binds a new port, so the host never takes it for the old one.
- src/common/game_info.cpp: when player 1 is gone in a driving mission (net::Driver), the machine whose player takes
  the car shows "Player 1 has left / You are driving now" for 6 s (the Ui hook's foreground list, beside the
  reticle; drawn by the PC, not the game). unlock_missions: "Unlock every mission (greyed ones can be picked)",
  the help says the list keeps them greyed but they play.
- Tests: net-rejoin OK, smoke OK, build-win OK. Neither the button nor the notice runs headless (no overlay, no
  interface): John on two PCs.
- Next: John.

### 2026-11-14 - A session plays from the host's saves (the Exchange's on-foot desync)
- John (logs 063/280, 064/281, the Exchange 07000005): player 2 seen walking in a small part of the map while
  further on their own screen, the players then lost from each other's view; in one session the joiner restarted
  07000006 while the host went on to 07000007.
- Cause: each machine booted from its own disk's saves. The joiner's had a second profile (0055004E0FC5, opened in
  the menus beside the host's 004F004E0F7A) and its own copy of the BOND profile's save.dat, which the level then
  read: the session's machines no longer started from one state. Headless the two disks were always fresh. Proof:
  net-coop.sh with John's BOND save on the joiner's disk alone (JOIN_UDATA, new; HOST_UDATA too): the joiner stayed
  in the menus while the host played the level.
- Ruled out first: the time stamp counter's readers (0xE8F20: 0xDFFB0 the game clock, pinned to the ticks already;
  0xE41E0 a frame's time at 0x2FF448, written alone; 0xC4F90 / 0xC8D00 / 0xCA050 the music's fades; the rest in
  D3D and DSOUND); the enemy copies (12 and 23 on both machines in John's logs).
- src/common/net.cpp (wire version 11 / 12: the builds before it never meet these): the host packs its title's save
  folder (hdd/E/UDATA/45410026, each file's name, size, write time and bytes) before boot and at a rejoin's offer;
  a joiner's hello says which saves it has whole, and the host sends them (kSaves, 1 KB pieces) to one that lacks
  them. The host starts once every joiner has them (at most 15 s past its start, then an error line); kStart names
  them. The joiner writes them to hdd/E/NFSESSION/45410026 and links \Device\Harddisk0\Partition1\UDATA\45410026
  there (kernel::CreateSymbolicLink): its game reads and writes the host's copy, its own saves stay as they were.
  "net: this machine plays from the host's saves" in a joiner's log, or an error that they did not come.
- src/game/view.cpp: the co-op campaign's check every 60 ticks (net::CheckState: the level, the random numbers
  0x18CDF8 / 0x18CDFC, the objects made 0x1DF828, the characters set up 0x1E5860, the players' places) logs "net:
  the games differ" on both machines' logs the first tick they do.
- Headless: the joiner with the BOND save, the host none: both in the level, the joiner on the host's (empty)
  saves, its own untouched; split screens (NF_NET_FULLSCREEN=0) give the same draw count on both (42102326). The
  host with the BOND save, the joiner none: the joiner reads the host's profile, both machines alike (570868 draws
  each). The memory compare differs as it does without the change (the open co-op memory difference). net-rejoin
  OK (the saves sent again at the offer, 3 files), net-driving-rejoin OK, smoke OK, build-win OK.
- Next: John, both PCs on this build (an older joiner cannot meet a host on it).

### 2026-11-14 - Co-op campaign: F1 switches for the extra enemies and their health
- John: the extra guards spawn; an F1 option to turn the scaling off, the number or the health.
- net_coop_more_enemies and net_coop_tougher_enemies (F1, settings.toml; on by default; NF_NET_COOP_MORE_ENEMIES /
  NF_NET_COOP_TOUGHER_ENEMIES for tests): the host's travel in its input each tick (the bundle's spare bits, byte 0
  0x80 and byte 1 0x80, as net_coop_friendly_fire's), so every machine has the host's at the same tick; in the rejoin
  snapshot. The copies are decided at a level's start (and a spawner's or the script's spawn), the health at each
  enemy's setup.
- Headless co-op: host more on / tougher off, joiner both off: 21 copies on both, health 12 on both; host more off /
  tougher on, joiner the other way: no copies, health 18 on both. The test's memory difference as before. Smoke OK;
  build-win OK.
- Next: John.

### 2026-11-14 - Co-op campaign: more enemies from every spawn, their health with the players
- John: each spawn point in every level spawns more enemies, and their health scales with the players.
- src/game/enemies.cpp: a level's characters come three ways, each through 0x2FC50: placed by the level's data as it
  loads (0x2FCF0 from 0xA4BC0), spawned by its script (0x2FD80) and by its spawners (0x323E0, a spawn point's
  record). The team ([state+0x44], 1 the enemies) is set by the setup 0x42A90, which the placed ones get together
  at the level's start (0x31FD0's walk of the list at 0x1E5634) and the others as they spawn. In a co-op session
  each enemy gets one more per extra player (up to 3), 0.8 beside it, set up as a spawner's are; the placed
  ones' copies are made after 0x31FD0, from their place, turn and level data kept at the load (the loader's buffer
  is gone by then: copies made from it were refused). Every enemy's health (+0x90, its most +0x94) is the game's
  times 1 + 0.5 per extra player, after the setup sets it from zero (no compounding on a restart's setup).
- Headless co-op (two machines, level 07000002): 21 more enemies beside the level's 21 placed ones (its 4 allies and civilians
  left alone), the same on both machines; guards' health 12 to 18. A copy's object and state match its original's
  but for pointers, its place and what follows from it, and the setup's serial +0xEC (unbounded, a spawner's too).
  The test's memory difference is the one it has without this change (1.87 MB in the same runs). The spawner and
  script paths did not run in the test (the players stand still).
- Tests: smoke OK; build-win OK (a forward declaration of an X86_FUNC cannot add dllexport: the hook comes first).
- Next: John, on a level with spawners and scripted waves too.

### 2026-11-14 - Driving co-op: the two drifts (the sound library's frees, a rejoin's first frame)
- Tick 1680, before any rejoin: reproduced at will with machine 2's disc at 2 MB/s (net-driving-gun's DISC_RATE_2=2;
  memory dumps by RAM copies, coop.cpp's NF_DUMP_RAM=1, as file writes changed the timing and hid it). EA's sound
  library thread 0x13D980 serves its voices every 10 ms of the wall clock; its scan 0x13E530 asks each buffer's
  GetStatus and frees the voices done (0x13DE50, the game's sound handles 0x234038 with them). The two machines
  freed one a tick apart (1733 of 1817 frees from the thread), the game's next sound took another emitter (the pool
  at 0x81C25xxx, the serial 0x1C8FBC) and the car and camera went apart by ulps from tick 1622.
- clock.cpp: in a session's frames those frees are the game thread's, 100 passes to 60 frames under the library's
  lock (0x13E7B0/0x13E7C0); the thread keeps the rest of its pass. SDK (runtime/xdk/dsound.cpp, new dsound.h):
  dsound::FrameClock plays the buffers by the title's frames for GetStatus (each voice's own position, the mixer
  unchanged), in the snapshot. A first try moved the whole service and the events (0x13EFD0) to the game thread: the
  rejoin's host then crashed, its game thread held for the transfer: the streams' refills (0x142150) stopped and
  0x13EE00 overran the events' queue (it adds without a bound; the callbacks 0x244FB8 are null).
- Tick 3600, after a rejoin (5 of 11 runs, then every run): the joiner's ticks a frame after the host's, the game
  itself the same. Between the load and net::Active (the rejoin's last messages) the game's own timer callback
  counted twice on the loaded clock (0x242428/0x24242C), the scheduler's dues moved and the pad task (0x516D0,
  every second frame) read a frame early. A load now counts frames and the timer's hook leaves the clock alone.
- Tests: net-driving-rejoin 3 of 3 OK (the game the same at tick 4457); the slow disc 5 of 5 without a drift (2 of 2
  apart before); net-driving-gun OK; smoke OK; build-win OK. The SDK change waits on the other games' smoke tests.
- Next: John; then more enemies from every spawn point.

### 2026-11-14 - Driving co-op: the host skips a mission's opening movie
- John (hosting, Enemies Vanquished and others): Enter and the left button, which skip it alone, skipped nothing.
  The opening cutscene is a movie, Driving.xbe's player 0x1308A0, before the main loop's frames: each picture it
  polls the pads (0x108490, returning to 0x130A89) and ends on A, B or START held on its port (+0x1C, +0x1D, +0x1A
  bit 0x10 of the input object) by setting the movie's +0x10. In a session the clock counts time there and the poll
  is not run (clock.cpp: no machine's press alone skips), so nobody could skip.
- clock.cpp: at that poll in a session this machine's controller 1 is read (input::Poll; game_info.cpp's NetPad
  notes A, B or START before the session's pad, nf::SkipHeld). The driver's press (net::Driver: the host, or the
  player who took the car) names the movie (its number since boot, the same on every machine, in the rejoin
  snapshot) in a new message, kSkip, sent for a second and relayed by the host; each machine sets the movie's +0x10
  when it plays the movie named, the others' presses do nothing.
- Headless (two machines, Enemies Vanquished): the host's A skipped the movie on both at 5.2 s (32 s unskipped), both
  at tick 0 together and no drift for a minute; the joiner's A alone skipped nothing.
- Tests: smoke OK; build-win OK; net-driving-rejoin logged the open tick-1680 drift (before its rejoin, no presses).
- Next: John; the drift.

### 2026-11-14 - A snapshot's load no longer races the title's other threads (the rejoin crash)
- The crash: net-driving-rejoin's rejoining machine segfaulted as the host's state loaded (3 of 6 runs before this
  change). The thread was 0x28, the library's service thread 0x13D980 (`while ([0x244C3D]) { services; sleep }`),
  started by the CRT's 0x10EBD2: woken from its sleep while memory was cleared for the load, it read the flag as 0,
  left its loop and returned through a stack of zeros (EBP 0, the write to EBP - 0x28). snapshot::Load held only
  GuestLock, which DPCs, D3D and DirectSound take, not the title's threads.
- SDK (runtime/core/snapshot.cpp, kernel): each title thread is marked parked while it is in the kernel (a wait,
  KeDelayExecutionThread, NtYieldExecution, before its start); it clears the mark before it goes back to guest code
  and, if a save or load is under way, marks it again and waits for it to end (the mark and the busy flag set before
  the other is read, so one side always sees the other). Save and Load wait up to a second for every thread but
  their own to be parked (LOGE naming one that is not). A thread running APCs is in guest code meanwhile.
- Evidence: 11 runs since, no crash. Threads 0x28 and 0x18 (the guest's timer-queue thread, 0x10ECE5) are the ones
  a load holds, on both machines; at the start of every save and load every thread was already in the kernel.
- The drift is still open and timing-sensitive: 6 of 11 runs logged "the games differ", 5 at tick 3600 (just after
  the rejoin; the game's time 0x234E34 a frame apart, the host's ahead) and 1 at tick 1680 (before the rejoin, where
  this change does nothing). The unchanged code's runs were 1 OK and 1 crash. The game's time is written through
  the state block 0x234E24's pointer; its writers and what reaches it off the game thread are the next lead.
- Tests: smoke, net-rejoin (default.xbe) OK; build-win OK. The other games share this runtime and are not on this
  machine: their smoke tests come before the SDK's main takes it.
- Next: the drift, then more enemies from every spawn point.

### 2026-11-14 - Driving co-op: one ammo pool for every player, what the game gives times the players
- John: the missile car's ammo already looks shared; the same for the other driving missions, more by the players.
- Each weapon's rounds are the weapon table's +0x18 (the table at [[0x23923C]+0x10], 0x54 an entry; 0xBBDC0 fires
  while +0x18 - +0x1C is above 0, the HUD shows +0x18). Found headless by memory dumps around Deep Descent's three
  torpedoes (entry 21: 30, 29, 28, 27 at 0x825FAB7C); Enemies Vanquished's missiles are entry 2 (30), its gun entry 0
  (500); Alpine Escape's turret entry 24 (about 3000), a second gun entry 25 (10). The current weapon is the
  manager's +4.
- coop.cpp, after each weapon update in a session: rounds the game gives (the mission's start, a pickup; a mission
  begun again counts from none) come once more for each other player, counts of 1000 or more (the turret's) as they
  are; each copied shot of another player's takes a round of player 1's current weapon and none fires at 0. The car's
  copies wait at least the weapon's own interval. The rounds as last seen go in the rejoin snapshot.
- Headless, two machines: Deep Descent 60 torpedoes (and 30 of entry 22), player 1's shot and player 2's two from the
  one count (60, 59, 58, 57 on the HUD); Enemies Vanquished 60 missiles, both players' from it; Alpine Escape's turret
  unscaled. No drift.
- net-driving-rejoin failed 2 of 4 runs today (once before this change): the rejoining machine segfaults as the
  host's state arrives, in a guest thread's start (0x10EBD2, the CRT's thread entry) whose stack pointer is 0 (a
  write to base - 0x28): a thread started across the snapshot's load (open). The tick-1680 drift did not show today.
- Tests: net-driving-gun (Alpine), net-rejoin, smoke OK; build-win OK.
- Next: more enemies from every spawn point, their health with the players; the rejoin thread race.

### 2026-11-14 - The main menu's Up moves one row again
- John: on the title's main menu Up selects the item two above (keyboard; Down fine). Headless the same on the
  controller: PC Settings -> Multiplayer, Multiplayer -> Quit Game, Codenames -> NightFire.
- 0x943FA (a page's input) moves the focus with 0x734D0 (page, 1 up, 2 down, 4 left, 8 right): for Up a widget
  counts only when its height (+0x76) is less than its distance above (the current y +0x72 minus its y); Down has
  no such test. pc_settings_item.cpp's five rows are 26 apart (the game's three were 32), each 28 high (Codenames'
  record), so the row above never counted. The five rows are 25 high now (logged with the main menu line).
- Headless: Down x3, Up, Up, Down, the stick up (W), the controller's Down and Up each move one row.
- Tests: smoke OK; build-win OK.
- Next: the driving missions' shared ammo pool, scaled by the players.

### 2026-11-14 - Co-op: the host skips the cutscenes
- John's next list, in this order: the host skips cutscenes, the title menu's Up (keyboard), a shared driving
  ammo pool scaled by players, more enemies from every spawn point (with health).
- A cutscene object's update 0xC4730, while it plays, asks 0x6C9A0(-1, 0x19, 1) (any controller's press of the
  skip, the controls' table +0x104 + 0x19) and skips with 0xC2F60(its scene, 1); the game's state 6 in 0x6ACA0
  ends on the same query. In a session pause.cpp's 0x6C9A0 hook asks those two (returns 0xC498D, 0x6AE5D) of
  player 1's row alone, or of the first player still in when the host has left; "player N skips the cutscene"
  logged.
- Headless (net-coop.sh, level 7000002, whose intro plays frames 600-1800): the joiner's A at 700, 900 and 1100
  skipped nothing, the intro played out; the host's A at 700 skipped it on both machines (play by frame 750).
  Driving.xbe's cutscenes get seat 1's input alone already (the others' START only with a menu up).
- Tests: net-rejoin, smoke OK; build-win OK. net-coop.sh's memory compare differs as before (the open co-op
  memory difference, Status).
- Next: the title menu's Up on the keyboard.

### 2026-11-14 - Deep Descent on the keyboard: W climbs, S dives, Left Shift the gas, Left Ctrl the brake
- John plays it on the keyboard with WASD: W and S were the gas and brake, the pitch the arrows and Shift/Ctrl alone.
  His choice: W climbs and S dives, Left Shift the gas, Left Ctrl the brake; the arrows still dive and climb.
- Driving.xbe copies the launch data's data to 0x243B90 (headless dump: its first 0x150 bytes there); the level id
  is at +0x14C, 0x243CDC (nf::Level; Deep Descent 0x09000002). menu_mouse.cpp remaps in that mission's play on the
  keyboard (the stick-up and stick-down keys' second bindings are the gas and brake), menus and pads unchanged.
- Not testable headless (no keyboard there); build-win and smoke OK.
- Next: John on Deep Descent.

### 2026-11-14 - A session's player on any of the machine's controllers; the turret's rate; no reticle in the car
- John (main 0cbd409): on foot player 1 is locked out now; in the driving missions the down key (S) is held all the
  time; player 2's turret shots in Alpine Escape follow the reticle but come slower than the turret's own; the cars'
  missiles (Enemies Vanquished, Island Infiltration's first part) home and the others fire them: no reticle wanted
  there, only on a turret; Deep Descent's craft does not go up or down.
- His Linux PC's logs (play/logs 001-018, all with a new Flydigi Vader 5 Pro, USB 37D7:2401): SDL lists the pad
  twice, as controllers 1 and 2, and "co-op player 1 read controller 2", Driving's "the game plays controller port
  2": START came from controller 2. A session sent this machine's controller 1 (and the keyboard) alone as its
  player, so with the live half of the pad on controller 2, player 1 played nothing. Nothing on foot changed in the
  code since his last good on-foot run.
- SDK (core/input.cpp): input::MergeControllers - every controller plays as controller 1 too, each control the most
  any gives; src/common/game_info.cpp turns it on in a session (and while a left seat is kept). Each pad's triggers
  and stick directions now read from the least each has given since it was plugged in (after its first half second):
  an axis mapped wrong rests part way and reads as released from there; "input: controller N's X rests at P%" logs
  it (a candidate for the held S, from either half of the pad).
- Alpine Escape: player 1's turret fires every 4 game-time counts, the weapon table's +0x3C says 5: the others'
  copies now come at player 1's measured gap (headless: "4 apart, the weapon table's 5"), the weapon table's
  until one is measured.
- In the car (control map mode 0) the others' A still fires the car's shots, at player 1's rate, but along the
  view's centre and with no reticle; the reticle is a mounted gun's alone (Island Infiltration's turret has one).
  Headless Enemies Vanquished: player 2 fires the car's guns (5 apart), no reticle, no drift.
- Deep Descent headless: the gas with the left stick up pitches the craft nose down into the sea floor (up dives,
  as a plane's), so the stick pitches it; the keyboard's pitch is the arrows and Left Shift / Left Ctrl, not W/S.
  John's report may be his keyboard's W/S or the doubled pad: asked.
- Tests: net-driving-gun (Alpine), Enemies Vanquished two machines, net-rejoin, smoke OK; build-win OK.
  net-driving-rejoin now fails 3 of 3: the host and joiner differ at tick 1680, before the rejoin, by one bit of
  the camera's height (414853B1 against B0), with no input. 0cbd409 passes; with this commit's coop.cpp taken out
  it still fails, so the cause is the one NetPad line, a no-op in the harness: the machines' timing shifted onto a
  drift that follows the host's pace (open, like John's tick-60 one in Deep Descent). net-driving-gun's two idle
  machines on the same launch data did not drift (dumps at 1500-1680 kept for the diff).
- Next: John on both PCs: on foot with the Flydigi, the held S (a "rests at" line in the log), Deep Descent's pitch
  with the stick and the arrows.

### 2026-11-14 - Driving co-op: the other players fire the car's guns (Enemies Vanquished's reticle)
- John (main ac2e232 not yet tested): W and S work, no new crash, no drift; asked for player 2's reticle in Enemies
  Vanquished.
- The car's shots are the mounted gun's task (vtable 0x18C520) with no gun (+0x14 0), two a shot from the car's two
  muzzles (+4), 5 game-time counts apart while A is held; the shooter is the weapon's +0x28, player 1's car
  [[0x234E40]] (the enemy cars' shots are the same task, their own shooter). 0x4AC40 takes the muzzle's world matrix
  from 0x8ECA0 (cdecl: the node, the matrix, 1; returning to 0x4ACC9) and fires along its row 1, unchanged for the
  player (the enemies' rows get their aim error). The drawn camera is the view's [[[0x1EC488]]+4] (0x96EB0 reads its
  +0xB4): +0x10 right, +0x20 up, +0x30 forward, +0x40 its place (headless: 9 units behind the car, 8 degrees down);
  its +0x180 aim, the gun camera's, is unset in the car.
- coop.cpp: in the car (the control map's mode 0, in play) each other player has a reticle (mouse or sticks as on the
  gun) and A fires: player 1's last car shot is copied from each muzzle, row 1 turned toward the point 100 units
  along the camera's view through the reticle, at player 1's own rate. Till player 1 has fired, the others' A is
  player 1's (FirstShot). The drift check also compares the drawn camera's place.
- Headless (Enemies Vanquished, two machines): player 2's shots from tick 2293 at player 1's 5 counts, the reticle at
  its top right turned the car's forward right and up; both machines the same (no drift line).
- Tests: net-driving-gun (Alpine, busy mouse), net-driving-rejoin, net-rejoin, smoke OK; build-win OK.
- Next: John on two PCs (the reticle where the shots land, the rate, Deep Descent's craft).

### 2026-11-14 - Driving co-op: seat 1's input on the port the game plays (player 1 locked out)
- John (main 9029207): Alpine Escape, player 2 firing first still locks player 1 out; W and S work; no crash, no
  drift. His logs (both PCs the same): player 1's START at ticks 845 and 858 given to the game in play opened no pause,
  player 2's at 914 did; the gun's own shots at 449, 541 and 903 came from player 2's trigger alone.
- Driving.xbe plays one controller: [0x244520], from the launch data default.xbe wrote (0x50EE0's pad-removed check,
  the input objects' +0x18 into the pad table 0x241C50 + port * 0x90, filled by the poll 0x108490). In John's
  session it named port 2, so the game read player 2's controller alone (net::Pad gave port 1 player 2's pad), and
  player 1's input reached port 1 where nothing read it. Firing first was not the cause.
- In a session the composed seat-1 pad (player 1's or the driver's, the others' START, menus and first shot) goes to
  the game's port and every other port is idle (game_info.cpp DrivingSeat, nf::PlayerPort); rumble follows it.
  Headless with the port forced to 2: before, player 1's shots and START did nothing (as John's); after, player 1
  fires and pauses on both machines; port 1 unchanged.
- Tests: net-driving-gun (Alpine, busy mouse), net-driving-rejoin, net-rejoin, smoke OK; build-win OK.
- Next: Enemies Vanquished's reticle for player 2.

### 2026-11-14 - Driving co-op from John's fourth test: W and S in the menus, the clock's switch, logs for the lockout
- John (main 8c32b31, his USA disc: the same two xbes): Alpine Escape and Deep Descent: player 2 firing first still
  locks player 1 out of firing and the pause, player 1 first is fine and synced; Enemies Vanquished and Deep Descent's
  pause: W and S do not move the choices; player 1 leaving and player 2 taking over works; a crash on Windows. His
  Linux PC's logs (play/logs 199-215): one drift, Deep Descent at tick 60 (the game time 0x80 against 0x7F, the
  camera a frame apart); no other mission's check differed. The Windows logs sent are default.xbe's: the crash is in
  the Windows Driving.xbe, whose log is asked for.
- Menus: Driving.xbe's move on the stick and the d-pad; the driving keys have W and S as the gas and brake. On the
  keyboard with a menu up those press up and down (menu_mouse.cpp). Headless: the pause's choice 0 -> 1 -> 2 -> 0 on
  S, 2 -> 1 -> 0 on W.
- The clock: the frame timer's callback counting time and the game thread's switch to frames now share a lock, so a
  callback that read "time" cannot add a count after the frames began (one machine one step ahead). Not reproduced
  (Deep Descent headless, both discs fast, either at 3 MB/s, the callback slowed): a candidate for the tick-60 drift.
  The drift check runs every tick for the first 60, so a drift at the start shows its tick.
- The lockout: not reproduced headless with both on the keyboard and mouse (player 2 clicking first, held or not:
  player 1 fired and paused). Logged now for John's next run: each START the game is given (the tick, whose, in play
  or a menu up) and the gun's own shots after a pause in them (the tick, whose triggers were held).
- Tests: net-driving-gun (Alpine, busy mouse), net-driving-rejoin, net-rejoin, smoke OK; build-win OK.
- Next: John's logs from both PCs for the lockout and the Windows crash; then Enemies Vanquished's reticle.

### 2026-11-13 - Driving co-op from John's third test: the turret's mouse, the host's unlocks, a drift check, the car taken over
- John (main 0fc7760): Alpine Escape's turret moved on player 1's screen alone, and player 1 could not fire or pause
  after player 2 fired first; a mission unlocked only on the host could not be joined; Enemies Vanquished had no
  reticle for player 2 and drifted; player 1 loads faster; Paris works.
- The turret: src/driving/mouse.cpp turned the gun with this machine's mouse. It now reads session player 1's
  counts at the tick (net::MouseFor/KeyboardFor), used once a tick. Proven: a host moving the mouse split the two
  games at tick 840 before, identical after. Player 2's stuck fire and pause did not reproduce headless after it
  (the desync is the likely cause).
- A drift check (net::CheckState, wire message kCheck): every 60 ticks each machine sends the game time, the camera's
  place and aim; a machine whose values differ logs once "net: the games differ from player N's machine at tick T".
  scripts/net-driving-gun.sh and net-driving-rejoin.sh fail on it (their old globals check compared physical memory,
  not Driving.xbe's data).
- unlock_missions: the host's value goes in the start packet and is every machine's (net::UnlocksMissions); proven
  with only the host's option on, both machines relaunched into the mission.
- Enemies Vanquished has no mounted gun: no reticle for player 2 is the game's (a role for them is a design question).
  Two machines with pads, field of view and a slow disc stay the same over 99 checks.
- Player 1 loads faster: the intro movie and the load run per machine; the first frame waits for every player
  (machine 1 held 3.9 s for a slow disc). Nothing can be drawn while it waits (the game presents no frame).
- Taking over (net::Driver): once the host leaves or goes silent, a joiner plays on alone, and in Driving.xbe its own
  player now plays seat 1 (the car, the pause, the gun, rumble) and is not an extra gunner. Proven: the host left at
  tick 2100 of Enemies Vanquished, the joiner steered the car (it stayed where it was with the joiner idle).
- Tests: net-driving-gun (Alpine, busy mouse), net-driving-rejoin, net-rejoin, smoke OK; build-win OK.
- Next: John on two PCs.

### 2026-11-13 - Driving co-op from John's second test: paced frames, player 2's first shot, one player's QUIT, every mission

- John (main ee4e0ef, two PCs): the doubled sound gone; the shared pause's QUIT quit both games; player 1 still
  started a little ahead; player 2 could not fire until the first car blew up and a cutscene played, and the games
  were further apart after it; asked to unlock every mission to test the driving ones.
- Pacing (src/driving/clock.cpp Pace): frames that present nothing (the cutscenes, the mission's first frames) left
  the main loop unpaced, up to 1099 frames in 10 s through Paris's first cutscene, so each PC ran those at its own
  speed. A main-loop frame now waits for the game's frame timer (625 counts in 10 s), a 3-count lead at most, and no
  longer than 50 ms. Headless: at most 622 frames in 10 s, the globals identical.
- Player 2's first shot (src/driving/coop.cpp, game_info.cpp FirstShot): the others' shots copy player 1's last one,
  so before player 1 had fired, player 2 had nothing to fire. While the gun has no copied shot, any player's RT is
  player 1's too, so the gun's own first shot comes from whoever pulls first. The copied shot is no longer dropped
  when the camera changes (a cutscene between shots), only when its gun goes (the gun's class and player 1's flag).
  Headless with player 1 idle: player 2 fires at tick 1217, the globals identical.
- QUIT (src/driving/hooks.cpp, 0xDCC10 the HUD's menu events): A on the pause's QUIT "are you sure" YES (the MISSION
  tab +0x470 0, QUIT +0x474 2, YES +0x484 1) is the players' whose A is down at that tick: their machine leaves the
  session (net::LeaveToMenu, as default.xbe's QUIT) and quits to default.xbe alone; the others see CONTINUE, their
  pause closes and the mission goes on. Headless: player 2's QUIT at tick 1533, machine 2 relaunched into
  default.xbe offline, machine 1 played on. The mission-failed menu's QUIT (+0x470 5) is still everyone's.
- SDK: the guest's clocks (interrupt time, the performance counter, KeTickCount) continue from a loaded snapshot's
  machine (kernel::GuestNanos, the snapshot's "clock" section, version 2). A rejoined machine had the host's timer
  deadlines on its own younger clock: the frame timer waited 143 s (it showed with the paced frames). Headless:
  net-driving-rejoin OK (the globals identical 900 ticks after), net-rejoin OK.
- unlock_missions (src/common/game_info.cpp, F1 "Unlock every mission" or settings.toml `unlock_missions = true`):
  sets the game's own every-mission flag (0x25D79E, read by the mission list 0x75994 and choice 0x8672B); the save
  keeps only the per-mission bytes, so nothing is written to it. Headless: a locked driving mission on a fresh
  profile starts with it on, not with it off.
- Smoke OK, net-driving-gun OK (its presses moved after the paced cutscene), Windows builds. Other games' smoke
  tests for the SDK change: not run here. Next: John on two PCs.

### 2026-11-13 - Driving co-op: a player who left takes their seat back (rejoin), the doubled sound looked into

- Rejoin in Driving.xbe. The snapshot point is the main loop's call of the scheduler (0x5AE98), before the frame's
  timer calls (clock.cpp): the sound thread they wake (0x4FE10) ran its frame a frame earlier. What it holds in
  Paris, frames 1200 to 15600 alike (NF_INVENTORY=N, now in Driving.xbe too): 7 handles (misc.viv, mis01.mus,
  mis01en.spe, 2 events, 2 threads), 4 guest threads (14 the game's; 18 from 0x10ECE5, the frame timer; 20 from
  0x10A730, the sound thread; 28 from 0x13D980, the library's services), 2 armed timers. The snapshot carries the
  session's frame count and the co-op gun's state (every player's aim, player 1's last shot and its barrels).
  Test (net-driving-gun.sh, new HOST_ENV/JOIN_ENV): the host saved at frame 1500 and the joiner loaded it in place;
  through player 2's shots after it, the game's globals matched the host's exactly, the rest apart only in the
  heap's order (20.7 KB, 20.8 KB with no snapshot).
- A machine starting again boots default.xbe while the host plays Driving.xbe, and the two exes' packets differ in
  version (each ignored the other's), which is why John's restarted player 2 never got back in. Now a Driving.xbe
  host answers a default.xbe machine's hello while a seat is free with the launch data it booted with (net.cpp's
  kLaunch, 4 KB in 4 pieces; Init reads the host's launch.bin before the kernel's boot deletes it, as the title frees
  the page). That machine writes it as its launch.bin and relaunches into Driving.xbe, which asks again and
  rejoins as default.xbe's machines do.
- Test, scripts/net-driving-rejoin.sh (Paris; FROM_DEFAULT=1 the new machine starts as default.xbe with no launch
  data): player 2 left at tick 2700, the new machine got the launch data, relaunched, took controller 2 again at
  tick 4352 (the session held 959 ms, 15 MB); 900 ticks on the game's globals identical and 15 bytes of all
  memory apart. Also left at 200 s (tick 6001): back at 7632, 14 bytes apart. net-rejoin.sh (default.xbe) OK.
- net-driving-gun.sh now decides by the game's globals (identical), as net-diff.py counted the known heap order
  as a desync.
- The doubled sound: not reproduced headless. Paris in a session and alone, recorded (WAV=) together: the same
  sound second by second (level and timing); the session's clock switched to frames once and stayed there. In a
  session each main loop pass is one step and wakes the sound thread once, so a main loop faster than the timer
  would play everything faster; headless both machines run 600 frames in 10 s against the timer's 625 (it runs
  at 62.5 a second, so a session plays about 4% slower than alone). Driving.xbe's log now says so every 10 s in
  a session ("driving: N frames in 10.0 s, the timer M") for John's PC.
- Smoke OK, Windows builds. Next: John on two PCs (close player 2's game in a driving mission, start it again);
  the log line from the PC whose sound doubles, and whether Paris alone sounds doubled on it.

### 2026-11-13 - Driving co-op from John's test: every player's pause, the level starts together

- John on two PCs (Paris): shooting and the enemies fine; player 2 had no pause menu; player 1 was in the level
  first; the audio sounded doubled on loading in; closing player 2's game and starting it again did not reconnect.
- The pause: Driving.xbe opens its pause on START (event 0x47, 0xE02B0) from player 1's controller alone, and one
  game runs for everyone. In a session any player's START is player 1's now, and while a menu is up every player's
  d-pad, Back, A, B and left stick play it (game_info.cpp's SharedMenus, from the session's pads at the tick).
  Headless: player 2's START opened the pause on both machines in the same frame, their second one closed it.
- The start: net::Gather (net.cpp): the next tick waits until every player has sent it (while they are heard).
  clock.cpp gathers when the frames start counting, so the first machine in keeps its loading screen until the
  last one is there. Headless with machine 2's disc at 3 MB/s: machine 1 waited 3.9 s, both began tick 0 within
  7 ms. Measured before the gather with the same slow disc: the two machines' audio was already within about 70 ms
  (the first machine's game, music included, waits at its first poll), so the doubling John heard is not that
  offset by itself; asked whether it was the two PCs' speakers together.
- Rejoin: Driving.xbe has no snapshot point yet (net::EnableRejoin is default.xbe's alone): not started.
- Tests: net-driving-gun.sh (shots on both machines at the same tick; memory apart only in the known drawing order),
  smoke OK, Windows builds. The script takes DISC_RATE_2 (machine 2's disc in MB/s).


### 2026-11-13 - Driving.xbe in a session: what still differs after the clock fix (investigation)

- Method: a test-only probe (not committed) recorded every change to Driving.xbe's globals (0x1B3000-0x24C000)
  each tick on both machines; offline the two were rebuilt tick by tick and compared, counting only differences
  that last 30 ticks (short ones were a critical section at 0x1D2F48 caught held by another thread).
- Without shots, to tick 3200: the only lasting differences are drawing scratch: a vertex array (0x202CB0, filled
  at 0xA9FF5-0xAA58D, handed to a draw at 0xAA9C8) and vertex colours (0x208EA0) holding the same entries in another
  order, heap pointers (0x23441C), and a jittering value at 0x2246F0 (no code names it). The 0x203230 and 0x235120
  differences seen before did not recur. With player 2 shooting, to tick 1650: one particle-like table
  (0x216D30-0x216FE8, points spread a metre around a hit) reordered at tick 1401, different values from 1402,
  and two heap pointers. Nothing else in the globals: the game's state, the clocks and the mission's own globals
  matched throughout. The session's clock never left counting frames during play.
- The common cause: heap addresses differ between the machines from the loading screens on (the loading and
  streaming threads allocate at moments of their own), and the game keeps objects in tables keyed by address
  (comparing the two heaps' object graphs from the globals finds buckets filled on one machine and empty on the
  other). Whatever the game walks in address order comes out in another order: draw lists, the particles a full
  pool recycles, perhaps more. Whether any gameplay (enemies, hits) follows it is not settled: the dumps hold no
  thread stacks (0xD0000000, where [0x1EC488]'s cameras live) and the heap compare is drowned by the bucket order.
- Next: decide whether the visible game needs it (John on two PCs), or make the game thread's allocations
  deterministic (its own arena, apart from the other threads').

### 2026-11-13 - Driving.xbe in a session: its clock counts frames (it ran on the wall clock)

- Found while testing the driving co-op gun (branch driving-coop, src/driving/coop.cpp: player 2's reticle and
  shots from player 1's mounted gun): two machines at the same session tick had run different numbers of frames,
  with no shot fired too. Each tick ran 2 frames, sometimes 3, the 3s at different ticks on each machine.
- Why: Driving.xbe's frame timer (0x10AE50, a periodic host timer) counts the game's clock [0x242428] (0x10A6C0, 40
  callers) and the scheduler's [0x24242C], and its call 0x5B9F0 counts the frames due [0x1E5204]: the frame 0x5BA80
  runs one step for each since its last. The scheduler 0x10AC40, called before each frame of the main loop (0x5AE98)
  and in the movies' and loading screens' waits, runs the pad poll 0x516D0 every 2 counts. So frames and polls (the
  session's ticks) both followed each machine's own clock, and the polls in the intro movie and loading screens set
  the tick the mission began at (793 on one machine, 794 on the other).
- Fix (src/driving/clock.cpp, sessions only): each main-loop frame is one count of the timer, run on the game thread,
  and the counts are set to the session's frame count (the scheduler's dues and the frame's last step move with
  them). Outside the main loop the timer counts on time (holding the game's clock there stalled the intro movie) and
  the pads are not read, so no tick passes and no machine's press alone skips a movie.
- Evidence (scripts/net-driving-gun.sh, Alpine Escape, 2 machines): every tick runs exactly 2 frames on both, the
  mission starts at tick 0 on both, game time and the clocks equal at every frame. Memory still differs: from tick
  30 a steady region (0x168C000-0x171C000, 140-170 blocks) and the audio library's globals (0x244xxx), wall-clock
  log text, profiler tables, heap pointers in different orders; and then, at a different tick each run (once by
  1300, once not by 1300, with or without player 2's shots), game data apart (0x203230, 0x235120). Open: likely
  the streaming threads' loads landing at different frames, or audio; not the clock.
- scripts/net-loopback.sh (Driving.xbe's menus in a session, then a match): the menus work with the pads read in the
  main loop only. Its match comparison fails with and without this change (the same 600 blocks, 0x3D85000-0x3FDE000):
  not from it, open.
- net-driving-gun.sh: DUMP_TICKS=a,b,.. dumps both machines at those ticks too; JOIN_PAD='\n' a baseline (no shots).
- Next: what sets the game data apart (dump between the last equal tick and the first apart, find the writer).

### 2026-11-12 - Co-op with 3 or 4 players: the 2D sprite pool ran out (crash as player 3 spawned)

- John saw it on two PCs too. The crash (0xAE870 spawning player 3 -> 0xACAB0 -> 0xB6AA0 building their HUD ->
  0xB5C30) wrote through a null sprite: 0xCEDF0 answers a full pool (0x29AA98, 512 sprites of 0x44 bytes; free count
  at +8, in use at +0x14) with none, and 0xB5C30 adds its offset to [sprite + 0x24] unchecked. Each player's HUD,
  with the radar group the name tags use, takes about 170 sprites: level 7000002 peaks at 536 with 3 players, 706
  with 4, 769 with 4 and every pause up (measured with a 2048 pool).
- Fix (view.cpp's 0xCEB30): a 3 or 4 player co-op level makes the pool 1024 (35 KB more of the level heap; 5+ player
  matches keep 2048). A player's log says once when the pool is nearly full (fewer than 32 free).
- Tests: 4 machines with every pause up played the level to frame 11700, no warning; scripts/net-rejoin.sh THIRD=1
  passes: the third machine learns player 2's return (kBack) and plays on with the host. Its memory differs from the
  host's only in co-op's known place (18 blocks, the same bytes two machines differ in without a rejoin; the
  rejoined machine took the host's copy), which the script now names instead of failing on. Smoke OK; Windows builds.
- Next: John on PCs with 3 and 4 players.

### 2026-11-12 - Rejoining: a player who left takes their seat back in the running session (headless, 2 machines)

- How it goes (src/common/net.cpp, version 9): a machine started as a joiner while the session plays asks as any
  joiner does; the host offers it a seat whose player left (the one with its net_name first). It boots alone, and
  at its frame 600 holds its game at the snapshot point (0xE8EE0's call of 0xDD1D0) and asks for the state. The
  host holds its own game at the same point, takes the snapshot, compresses it (LZ4, 54 MB to 27 MB) and sends it
  in 1200-byte chunks, acknowledged with bitmaps, at a rate that grows while the acknowledgements keep up. The
  machine checks it can load it (snapshot::Check: the same handles and threads), loads it and says so; the host's
  other joiners are told the tick the seat plays again from (kBack, acknowledged) before the host's game goes on.
  That tick is the next one the host captures: no machine has played it, so every one sees the seat gone before it
  and back from it (back_at; a player can leave again after). The net section carries the session's ticks, inputs
  and players but not the machine's own seat, socket or addresses; a game section carries the hooks' state that
  steers the game (view, pause, controls, mouse, characters, the clock base). A co-op player who comes back loses
  the "dead" collision flag CarryGone set (the cameras give them their body back each frame anyway).
- Test, scripts/net-rejoin.sh (split screen on both, so whole memories compare): the joiner leaves at 140 s in the
  level, a new process takes player 2 again. The session held 1.2 s; 2400 ticks later the host's and the new
  machine's game memory are identical (the kernel's clock bytes aside). LEAVE_HOW=kill NF_NET_DROP=10 (the joiner
  killed, a tenth of the packets lost): held 3.9 s, the same game but the streaming audio ring (the known class).
  Two fixes on the way: the sender ran past the acknowledgements' window under loss and throttled itself (41 s),
  and a rejoining machine that played on while the chunks came reached the title's attract movie, a handle the
  host has not, and could not load.
- Found, not this change: three players in co-op crash every machine headless as player 3 spawns (level 7000002,
  0xAE870 -> 0xACAB0 -> 0xB6AA0 -> 0xB5C30 reading guest 0x24; the previous commit's build too, split or full
  screen). So THIRD=1 (a third machine learning of the return through kBack) is untested.
- Windows: scripts/build-win.sh builds both exes. Driving.xbe has no snapshot point yet: no seat is offered there.
- Next: John on two PCs (quit with QUIT or close the window, start the game again with the same net_join); the
  three-player co-op crash; then a smaller transfer (pages the rejoining machine already has).

### 2026-11-12 - Rejoining: a snapshot of a running co-op level loads on another machine (step 1 of the snapshot way)

- John chose the snapshot. The SDK has one now (sdk/runtime/core/snapshot.h): the title thread's registers, guest
  memory (the title's 64 MB of physical memory, every committed page outside it but zero pages, the allocators'
  records), the kernel's handles (checked to be the same set; their signals and the files' positions carried),
  threads (checked), the objects by pointer (their host state read again from the loaded headers), Direct3D's host
  state (gpu::State, the shaders, program memory, the frame buffers), DirectSound's voices and pending callbacks,
  then sections a title registers. The host's part of physical memory (the renderer's and the hooks' own buffers)
  stays each machine's; Direct3D's pointers into it are the loading machine's own. The vertical blank, DPC and
  DirectSound threads hold snapshot::GuestLock around their guest calls.
- What the snapshot point holds (NF_INVENTORY=frames, src/game/hooks.cpp, both machines, frames 1800 to 9000 alike):
  12 handles, all files (filesys.d00 to d10 and z:\cached_filesys.dat), 6 objects by pointer, 1 timer, 1 guest
  thread (the game's, at 0xE8EE0's loop): the kernel's part is made at boot and never changes.
- Test (net-coop.sh with HOST_ENV="NF_SNAPSHOT_SAVE=7500,file" JOIN_ENV="NF_SNAPSHOT_LOAD=7500,file"): the host
  saved tick 7500 in the level (54 MB, 31 ms), the joiner loaded it at its tick 7500 (12 ms) and both played on in
  lockstep to frame 10320. Their memories at frame 10230 differ only in blocks that also differ between two machines
  that never took one (the same 175 s run without: 819 blocks; with: 803, none new). The 16 gone are the co-op's
  open difference at 0x287C000-0x2892000: the joiner took the host's copy and nothing set them apart again.
- A fresh process loads it too: a solo harness boot (NF_SKIP_INTRO=1, no session) loaded the host's tick 7500
  snapshot at its own frame 600 (NF_SNAPSHOT_LOAD=f600,file) and played the co-op level from there, player 1's view,
  the game asking for controller 2 (no session, so no player 2): a boot's handles and threads by frame 600 are the
  level's.
- Next: the transfer over the session, the seat given back, the hooks' own state.

### 2026-11-12 - Rejoining: how fast a replay of the session could run (measured)

- John: measure the replay way first (the rejoining machine plays the session's input from tick 0, fast).
- A probe timed the game thread's CPU over each call of 0xDD1D0 (CLOCK_THREAD_CPUTIME_ID; waits take none), in
  net-coop.sh's co-op level with player 1 firing, the harness's null renderer, a Ryzen 9 7900X: 1.1 ms a frame in
  the menus, 1.3 to 1.5 ms in the level, 2.1 ms in the last 600 frames; single frames of 70 to 130 ms at loads and
  a respawn, 555 ms at boot. A frame is 16.7 ms: the game's own work runs about 10 times as fast as it plays.
  Drawing is not in it (the null renderer; a replaying machine would draw nothing).
- FPS=0 alone (scripts/harness.sh, the level pad) is no test of it: the vertical blanks then count frames, the pad's
  presses (in vertical blanks) all came in the first second, and the game sat on a screen of 1 or 2 draws for
  1.47 million frames. A replay's input comes by ticks, not vertical blanks; whether the game runs unpaced through
  its boot, menus and loads alike is not known yet (a movie or a load may follow the clock).
- At 10 times, with the others playing on meanwhile, a machine gains 9 ticks a tick: about 7 seconds of catching
  up a minute played, 3.5 minutes after 30, 7 after an hour; slower on a slower PC and in a 10-player match.
- Next: John decides.

### 2026-11-12 - Rejoining a running session: the snapshot point exists (investigation)

- John: a player who quit should be able to rejoin a running session, with the game's state sent to their machine
  (a death voice note first: every player's death is sound 0x89, the same in a match as in co-op; John: what the
  game uses is fine, so it stays).
- Guest calls are native calls, so a snapshot can only be taken where the host's call stack is the same on every
  machine. The game's thread (thread 14, from 0xEB238, the CRT's start) runs 0xE8E90: its setup, then
  0xE8EE0 call 0xDD1D0; jmp 0xE8EE0. 0xDD1D0 sets the frame's pace (0x6B040) and jumps to the frame (0x6ACA0, a
  switch on the game's mode; 0x6AA90 the menus). A probe: 8400 calls of 0xDD1D0 for 8394 ticks through the
  menus, a co-op level's load and play, esp D005FF44 at every one, none longer than 500 ms but the first (boot).
  Between two calls of 0xDD1D0 is the snapshot point: two game frames deep, the same in every mode.
- The game has one thread of its own (thread 10 returns at boot). The runtime calls game code from its vertical
  blank thread (d3d8.cpp VerticalBlank), its DPC thread (objects.cpp RunDpcs) and DirectSound's completions
  (dsound.cpp Run): short calls, which a snapshot holds off with their locks.
- What a snapshot must carry beyond the game's frame: guest memory (user space from 0x10000, the 64 MB at
  0x80000000, the system pool at 0xB0000000, the stacks at 0xD0000000; about 22 MB compressed in a match) and the
  allocators' records of it (memory.cpp: the contiguous, system and stack heaps, the user space's regions); the
  kernel's objects (objects.cpp: handles, guest dispatcher objects by address, names, timers, the DPC queue),
  threads (threads.cpp) and files (files.cpp: open files and positions); D3D (d3d8.cpp: gpu::State, the vertex
  and pixel shaders by address, the frame buffers) and the renderer's caches, rebuilt from memory (texture.cpp);
  DirectSound's voices (dsound.cpp g_voices); and this repo's hooks' globals (about 70, most in src/game/view.cpp,
  mouse.cpp, pause.cpp and src/common/net.cpp), less each machine's own view (the full-screen view's rectangles,
  its HUD, its menus' mouse).
- Next: John decides (snapshot, or a slower replay of the session's input); then a save and load of one machine in
  the harness, its memory compared with a machine that played on.

### 2026-11-12 - Co-op friendly fire: a host option

- John: a friendly fire option for co-op. The game has it on: a probe (player 2 held 2 units in front of player 1,
  who fired the pistol) took player 2 from 100 to 75.5 on both machines.
- How a hit on a player comes: 0xADD00 (outer, inner, message) handles the player's messages, every frame, in the
  menus too. A hit's attacker is at [message+0x4C]. 0xADD66 picks the handler by the attacker's type (+0xDB): 0x05,
  0x27, 0x2C, 0x2D; any other is ignored (0xAE16B: 0x15D2E8's float back). A player's bullet comes as type 0x05,
  whose +0xBC's +0x24 is the shooter's actor (type 0x03); its damage goes through 0xADF87's call to 0xAC0A0 (3.5 a
  pistol shot).
- net_coop_friendly_fire (F1 Online, "Co-op friendly fire", on; NF_NET_COOP_FRIENDLY_FIRE): the host's travels in
  bit 6 of the first byte of its bundles (wire version 7), so every machine has it at the same tick and the host
  can change it in a session. Off: in co-op 0xADD00 skips another player's hit (no damage, no flinch), answering as
  an ignored one does. Matches keep their own friendly fire option.
- Evidence: the probe with it off: player 2 at 100 throughout, on both machines; on: 100 to 75.5. The memories
  differ in co-op's usual 18 runs either way. Untested: grenades and rockets (attacker types 0x27, 0x2C, 0x2D, whose
  owner is not looked at: a teammate's explosion may still hurt). Windows build.
- Next: the co-op death voice.

### 2026-11-12 - The online port's default is 26565

- John: the default port should be 26565. src/common/net.cpp: net_port 26565. The settings file saves every
  option, so a file saved before holds 26555 whether the player chose it or not: LoadKeys turns a 26555 into
  26565 once, and SaveKeys writes net_port_26565 = true so a 26555 chosen later stays. The headless sessions
  (net-loopback.sh, net-coop.sh) move to 26575, off the game's port. A host forwards UDP 26565 now.
- Evidence: Windows build (both exes); the migration is Windows' alone (the harness has no settings file).
- Next: friendly fire in co-op; the co-op death voice.

### 2026-11-12 - F1's name and handicap apply in a session; "needs a restart" on the settings that need it

- John: a player changing an F1 option mid-session had to restart for it to apply.
- Before: net_name and net_handicap travelled in kHello and kStart alone, so they were fixed at boot.
  net_character already travels in every tick's bundle.
- src/common/net.cpp (wire version 5; an older build ignores it): each tick's bundle carries the player's handicap
  and one letter of their name (the tick's count modulo 16). A player's handicap changes at the tick it is
  played; their name at the end of each round of 16 ticks, on every machine at the same tick, never half
  changed. The settings window changes net_name on its own thread; NameChanged (sdk: Option::changed, called
  after the window changes an option) copies it, under the session's lock, into what the game thread sends.
  The handicap is applied as the next match loads, the name at once (the tags write it each frame).
  NF_NET_CHANGE=tick,name,handicap (net-loopback.sh JOIN_CHANGE=) changes both mid-session for a test.
- F1: "(needs a restart)" on skip intro, host, join, host IP, players, port, wait and input delay, and on
  the SDK's aspect ratio, low latency, renderer and language. The help texts of character, handicap and
  full-screen view say when they apply. The character's says it is for matches and campaign co-op.
- Evidence: net-loopback.sh HOST_NAME=Johnny JOIN_CHANGE=8000,Harry,8, the joiner picking +25 on its box. Both
  machines spawned player 2 as Harry at 200 (net_handicap +100 over the box's +25) on ticks 9283 and 9284. Only
  the known differences (render records, audio rings, the ADPCM stream). SMOKE OK, Windows build.
- Next: John on two PCs (both on this build).

### 2026-11-12 - Match options: health handicap for every player; co-op name tags back to the game's

- John: the game's own match tags are mid-body near a player and shift up and grow farther away (for small
  CRTs), so co-op goes back to how it was: the 0xDB230 over-the-head change is gone. And: the options a player
  picks in a match, such as more or less health, for host and guest (matches only, not co-op).
- The match's options are the host's: map, scenario, weapon set (Normal, Pistols, Automatic, Sniping,
  Explosives 1 and 2, MI6 Operative, Phoenix Weapons, State of the Art, Cloak and Dagger, Random), points,
  minutes, lives, duration, friendly fire, spawning, vehicles, the On/Off options and the bots. They travel
  as the host's pad presses, so every machine has them. Each player's own are on their join box (0x88720):
  team (team modes), character, then Health Handicap (-75 to +100, 0x161870..0x161890), stored at the
  combatant's +0x2C (0x88884; a saved profile sets it too, 0x7F025). The spawn starts a match's combatant at
  100 plus it (0xACD7E, any slot).
- Online: each machine's player has their own join box, so host and guest pick their handicap there. A probe of
  the box steps ([0x245698 + 4 * player]) had both at step 4 (the handicap) from tick 6901; the joiner's DOWN
  and RIGHT there gave player 2 +25 on both machines. Players 5 to 10 have no join box: 100, always.
- net_handicap (F1 Online, "My health handicap"; NF_NET_HANDICAP=0..8): Join screen's, or -75..+100 for this
  player. It travels in kHello after the name and in kStart after the names (an older build's shorter packets
  read as none). ApplyCharacterSelections writes it to the combatant's +0x2C as a match loads; not in co-op.
- Evidence: net-loopback.sh with HOST_HANDICAP=8 (HOST_HANDICAP, JOIN_HANDICAP: new) and the joiner picking +25
  on its box. Both machines spawned player 1 at 200 and player 2 at 125 on ticks 9283 and 9284. Only the known
  differences (render records, audio rings, a stereo ADPCM stream). Untested: players 5 to 10 (the loopback has
  2 machines). SMOKE OK, Windows build.
- Next: John on two PCs.

### 2026-11-12 - Name tags: over the head in co-op, typed names in matches, a default-names option

- John: co-op names showed in the middle of the body (in a match they are above the model); typed names should
  apply in matches; and an option to see Player 1, Player 2 instead of typed names.
- Placement: 0xB54D0 projects a combatant's +0x24 (0xDB230), the middle of the body in both modes. The eye is
  0.70 higher standing, in a match dump and in co-op. Placement is the same code in both modes; in a match the
  dots used for names were radar blips first, which may be why they show higher (not looked at). In co-op the
  point projected is now the player's eye (their camera's +0xB8, lower when crouched) plus 0.45, through 0xDB230
  from 0xB5906 with +0x28 changed for the call alone.
- Matches: the session's players' names (0x25FE38) are their net_name when they typed one. Written by
  InitDefaultCombatant, as a match loads (ApplyCharacterSelections) and each frame of the name tags; without one,
  the game's own name stays.
- net_default_names (F1 Online, "Show Player 1, Player 2... names"; NF_NET_DEFAULT_NAMES): this PC draws the
  typed names as Player N. A name tag is a text sprite pointing at the name ([sprite+0x18]); 0x6A340 (from
  0xD950D: the string at [esp+0x18], its length at [esp+0x28]) draws it with Player N there, then the name and
  both lengths are put back. The text is queued and drawn later (0x29E810), so a second, suppressed draw of the
  real name to restore the text's state overwrote it: this PC's queued glyphs and text state stay its own.
- Evidence: net-coop.sh HOST_NAME=Johnny JOIN_DEFAULT_NAMES=1. The joiner's snapshot shows "Player 1" over the
  crouched host's head; both machines hold Johnny and Player 2; the usual co-op differences (1.87 MB). A bot match
  with HOST_NAME=Johnny JOIN_NAME=Harry (net-loopback.sh: HOST_NAME, JOIN_NAME): both machines' names are
  Johnny, Harry, then the game's; only the known differences (render records, audio rings). SMOKE OK, Windows build.
- Next: John on two PCs; match options.

### 2026-11-12 - Co-op: each player's name over their head

- John: show the players' names in co-op. Each player types a name; with none it is Player 1, Player 2 and so on.
- How a match does it: name tags are the radar group's dots. 0xB6AA0 builds a player's HUD from 22 groups: a
  match's list is 0x1812F0, a level's 0x180EE0. The radar group is entry 14 (0x1806D8; 0xB69E0 makes it, 0xB54D0
  updates it). For each other combatant who is on screen and in sight (0x2C5B0), 0xB54D0 draws their name
  (0x25FE38, 0x30 apart) over their head, with [0x26005C] set (it already is in a campaign). A level's list has
  no entry 14.
- src/game/view.cpp: in co-op 0xB6AA0 builds the radar group as well: the level's entry 14 is set for the call
  and put back. Its radar finds no targets (0x9DA80 from 0xB5561) and its ring is hidden, so its dots are the
  names alone. Each player's name is their net_name, else Player N.
- Names: net_name (F1 Online: "My name", or NF_NET_NAME) goes in the joiner's kHello. The host's kStart carries
  everyone's (16 bytes each, printable ASCII, 15 characters at most). Every machine writes the same names, so the
  memories stay alike. An older build's shorter packets read as no name. sdk/runtime/app: an Option can be a text
  setting (text, text_empty), saved and drawn as a text row.
- Evidence: net-coop.sh with HOST_NAME=Johnny (scripts/net-coop.sh: HOST_NAME and JOIN_NAME). Both machines
  hold Johnny and Player 2, and the joiner's snapshot shows "Johnny" over the host's character. The usual co-op
  memory differences only; no traps. SMOKE OK, Windows build (both exes).
- Next: John on two PCs; match options.

### 2026-11-11 - Online: a match ends on the same tick on every machine

- John (2026-11-10): player 2 won a 10-kill match. Player 2's machine went to the results, and player 1's played on.
- Cause: the match's clock. 0xDFFB0 (hundredths of a second since 0xDFFA0's base) reads the time stamp counter
  (0xE8F20 rdtsc), which is each machine's own wall clock. The end check (0x9CC50) adds its steps to the match's
  time ([0x262928]) and compares it with the time limit ([0x26292C]), so each machine reached the end on its own
  tick. Its other callers: the match's state machine (0xA2B80), the frame's stamp ([0x1F65CC]) and the pause's
  sound (0xB0570). The match test never saw it: net-diff.py compares the physical dumps, and the xbe's globals
  are in the virtual one. Its match clock read 129.514 s on machine 1 and 129.554 s on machine 2.
- src/game/hooks.cpp: in a session 0xDFFB0 counts the ticks played since 0xDFFA0, a 60th of a second each (the
  frame's step [0x17C104] is that); src/common/net.cpp: net::Tick(). Offline, the clock is unchanged.
- The other match differences are not the game's state. 0x1C4C068 and 0x37D8000 are both players' outer+0x7C,
  and outer+0x8C is off by one. Those are the pad words of the player matrix at +0x70: the matrix code
  (0xD6710, 0xD6D20) never reads a row's fourth float. 0x3BFC000's 200 bytes are one byte of each render
  record: each machine draws its own view.
- Evidence: the match test with no START (mp-match-stay.txt on both machines) and a probe that set the limit to
  60 s at 50 s in. Both machines left play on tick 12884 (match state 2, 3, 4 on the same ticks), clock 60.000134
  on each. The physical memories were the same but the audio ring. The xbe's globals had 69 differing runs and now
  have 4: the unused rdtsc base, a system time, a counter at 0x299474 and a byte at 0x1110D8 (not looked at).
  SMOKE OK, Windows build (both exes).
- John's Nov 10 test also ran while the headless tests used the game's port (fixed: the earlier entry "A session
  hears its own machines alone").
- Next: John retests a match to its end on two PCs; net-diff.py could compare the xbe's globals too; names in
  co-op; match options.

### 2026-11-11 - Online: a lost packet no longer parts the machines

- Before: each bundle carried one tick and none was sent again. A machine that saw a later tick arrive first
  played the missing one as the tick before, while its sender played it as sent: a lost packet parted them.
- src/common/net.cpp (wire version 3; an older build ignores it):
  - a bundle carries its tick and the 7 before it (253 bytes);
  - every machine keeps 600 ticks of every player's input;
  - a tick still missing is asked for again every 50 ms (kResend). A joiner asks the host; the host answers
    from its own copy or asks the joiner it is from, and relays the answer as any bundle;
  - the game waits for the tick: no input is ever guessed.
  - NF_NET_DROP=percent drops that share of a machine's own packets, to test it.
- Evidence: the match test (net-loopback.sh, a bot match) without loss and with 10% and 25% of both
  machines' packets dropped (20 re-requests logged, the cap, each). Its memory differs in the same places as
  without loss (0x1C4C068, the ambient float, the audio ring). At 25% two more runs differ: Xbox ADPCM blocks,
  the music streamed on the harness's wall clock. Frames reached: 15840, 15780 (10%), 15000 (25%). Co-op at
  10%: both players' positions and health identical on both machines. SMOKE OK, Windows build (both exes).
- Next: John on two PCs (both on this build); the match desync (0x1C4C068); names in co-op; match options.

### 2026-11-11 - A player killed under their own pause no longer vanishes (fixed)

- John retested: the body of a player who died under their own pause still vanished. The earlier entry below is
  the cause: a dying player's update is skipped under the pause, and their model leaves the level's rooms.
- A different way: hold the death itself, not the dying player. The death is decided each frame inside the
  player's own update (0xAE52D calls 0xAC390(outer, 3), its result unused). Under the player's own pause, when
  their health reaches 0, the 0xAC390 hook in src/game/view.cpp keeps them alive on 0.01 health. Their update
  goes on with idle controls (pause.cpp) and their model stays in the rooms. When the pause closes, health goes
  to 0 and that frame's 0xAC390 kills them as before.
- Evidence: the earlier repro (scripts/pad/level-pause-long.txt, the joiner .local/look-away.txt), player 1's
  health set to 0 2500 frames into their pauses. Held from 166 s; machine 2's pictures show player 1 at 10350
  and 10500 (lost there before), the death at 10650 after the pause closed, the respawn at 10800. SMOKE OK,
  Windows build.
- Next: John on two PCs; the lost-tick case; the match desync (0x1C4C068).

### 2026-11-11 - Co-op: the sound no longer dies with a player

- John: when player 2 died the sound went silent until the level was restarted (new with the co-op respawn).
  Out of a match a death (0xAC390) sets the music's death cue, 0xCBCE0(6, 1) at [0x29A198]. The music player
  (0xCA805) then fades out (0xC4F90 counts [0x182A9C] from 10000 to 0). The boot's fade (0xC5030(1) from
  0x7CE05) left [0x182AA8] set, so the fade takes every sound with it. Only a level's start (0xC9CB8) brings it
  back, which single player needs (the level ends there).
- src/game/view.cpp: the 0xAC390 hook clears the cue for a co-op player with a teammate alive. The last player
  down still gets it, then the mission failed screen.
- Evidence: headless, player 2 killed at 141 s, both machines recorded (WAV=). Before the fix every voice fell
  to 0 within 2 s and stayed there (RMS 0 from 150 s to the end). After it the sound matches a run with no
  death, second by second (both have a quiet stretch at 155-170 s), but for the death's own sound. SMOKE OK,
  Windows build.
- The vanishing dead model (the entry below it): John doesn't see it now. Nothing aimed at it changed, so it
  stays open until a run shows it again.
- Next: John on two PCs; the lost-tick case; the match desync (0x1C4C068).

### 2026-11-11 - Co-op: a teammate's CONTINUE no longer ends the level; a downed player respawns armed

- John: player 1 paused and died; player 2 paused; player 1 unpaused and their death played; the game froze
  when player 2 unpaused. Out of a match the death (0xAC690) ends at its animation's end or at button 0x19 on
  any controller, and then asks for the mission failed screen (byte 0x1DF19A, opened from 0x9C2BC). Player 2's
  CONTINUE was that button, so the game went to mission failed mid-session.
- The co-op respawn had never run: it waited for state 3 at [outer+0xD0], which players never reach (a death
  sits at 2). src/game/view.cpp's AD520 hook now counts 180 frames for a co-op player at state 2 or 3 while a
  teammate is alive (not while their own pause is open), and clears 0x1DF19A after their update, so only the
  last player down fails the mission.
- The respawn (9F0B0 hook) now puts the player 1.2 units off the teammate (at 0,0 player 1 was stuck in them).
  It brings back the HUD parts shown while alive ([inner+0x770], 22 of 0x20 bytes, +0x24; a death hides all
  but 12, the red view). It also draws the weapon held before: [outer+0xB8]+0x52 is held, +0x53 next and +0x54
  last, 0x47 none. The death holsters the weapon (0xB9B90), and the camera (0xA8A60) hides a holstering
  player's gun. The game's own loadout (0x9D5B0) ends the same way.
- Evidence, headless on two machines: player 1 killed under their own pause, player 2 pausing and pressing
  CONTINUE. Player 1's death plays when they unpause, with no mission failed screen. 3 s later they respawn
  beside player 2 with HUD, pistol and ammo, and walk on. Both players killed: "Mission Failed" with REPLAY and
  QUIT, and REPLAY restarts the level. SMOKE OK, Windows build.
- Next: John on two PCs; the lost-tick case; the match desync (0x1C4C068).

### 2026-11-11 - A session hears its own machines alone (John's desync was the headless tests)

- John's match (2026-11-11 18:27, host under Wine on this PC, joiner on Windows) fell apart: at 46 s the host gave
  player 2 character 4 (Kiko), at 118 s a pause for player 2, neither on player 2's machine. Both were the headless
  co-op test running on this PC at that time (its joiner is character 4, its pad pressed START at frame 7000): both
  bound UDP 26555 (SO_REUSEADDR) and the host took any sender's input packets as its players'.
- src/common/net.cpp: once booted, bundles, heartbeats and leaves count only from the session's machines (a joiner
  its host, the host each joiner at the address it joined from); others are dropped with a log line. The test
  scripts (net-coop.sh, net-loopback.sh) use UDP 26565 (NET_PORT=).
- Evidence: co-op and match runs on the new port meet and play in lockstep, none of their packets dropped; memory
  differs as before (co-op's known places; the match's 0x1C4C068). SMOKE OK, Windows build.
- Seen on the way (open): a bundle that is late or lost is played as the tick before on the other machines but as
  sent on its own, which would part them; each packet carries one tick and none is sent again.
- Next: John on two PCs; the lost-tick case; then the match desync (0x1C4C068).

### 2026-11-11 - A player killed under their own pause vanishes from the others' views (3 attempts; fixed above)

- John (two PCs): player 2 paused and was killed; when player 1 looked away and back, player 2's model was gone
  until player 2 closed the pause, then the death played. Kept in view, the model stayed. A level change with a
  player paused is fine (they come in unpaused).
- Cause so far: the fatal hit sets the dying state at once (0xAC390, state 2 at 0xAC52A), and a paused player who is
  not alive has their update (0xAD520) skipped, so the death waits; something in that update keeps the model in
  the level's rooms. Memory at frames 9630 (dead, in view) and 10380 (dead, looked away and back): the model object
  ([outer+0xB8]) lost its room list (+0x134 count 4 -> 0, +0x138..+0x158 pointers zeroed), inner+0x81C zeroed.
- Reproduced headless: net-coop.sh, HOST_SCRIPT=scripts/pad/level-pause-long.txt, the joiner turning away at 9600
  and back at 10200 (RSR/RSL 60), a test switch zeroing player 1's health and setting state 2 500 frames into the
  level's pause; machine 2's pictures lose player 1 at 10350 and 10500, the death showing at 10650.
- Tried, each with the frozen player's update still skipped, none kept the model: 1) the pose update (0x18690) each
  frame; 2) the views mask (0xDB3A0 -> 0xDB320, +0xD4: it was already 0x1E, every view but their own); 3) the
  update's last step, +0xD6 |= 0x20 (cleared again by the next dump, the rooms still empty). Code back as before.
- Next: find what fills +0x134 (the writers of the room list), or run the dying player's whole update with its
  death held some other way (a zero time step is unsafe: 0xAEC36 and 0xAEC86 divide by 0x17C0FC).

### 2026-11-11 - Pause in a session: the fire picture, menus held by player 1's, same-frame pauses, enemies

- John's report: coloured blobs on the right of every player's pause screen; after player 1 paused, a later
  player's pause took no input until player 1 closed theirs; enemies coming for two paused players turned back.
- The blobs: the character picture's fire (0xE0020) runs only while the world is paused, which in a session it no
  longer is; src/game/pause.cpp runs it after the menus while a pause of one's own is up.
- Menus held: the menu loops (0x94370, 0x94720) skip a player's menu while the previous player's is up (the chain
  reads at 0x94412 and 0x9473E); two [[hook]]s (OwnMenuNotHeld, config/nightfire.toml) say "not held" when that
  previous menu is a pause of its own player.
- Two players pressing START in one frame: building the first one's pause page (0x93960) clears every player's
  controls, so the second's START was lost; the other players' control blocks are kept across the build.
- Enemies: they never read the pause flags; they turned back because a paused player's update (0xAD520) was skipped.
  A paused, living player now updates with idle controls (nf::PausedUpdate, src/game/view.cpp), so enemies see and
  shoot them; once their health is gone the update waits for them to close the pause, and the death plays then.
- Pauses of one's own survive the game's own whole-game pauses (cutscenes): 0x6AF70's hook sets them again.
- Evidence: co-op, the fire picture as offline; the joiner closes their pause while the host's stays up; both
  pressing START in frame 9000: both "opened their own pause" in that frame. Health zeroed under the host's pause
  (a test switch, removed): the view behind the menu unchanged, the death's blood wipe right after CONTINUE. Match
  with a bot, both paused 95 s: in lockstep. Memories differ only in the known co-op places. SMOKE OK, Windows build.
- Next: John on two PCs; then the match desync (0x1C4C068), names in co-op, match options.

### 2026-11-11 - RESTART on the pause screen is the host's alone in a session

- John's choice: RESTART restarts the level for everyone, so in a session only the host's does; a guest's press on
  it does nothing (src/game/pc_settings_item.cpp, the pause choices' A with RESTART, 1, chosen by a player who is
  not 0).
- Evidence: co-op, the joiner pauses, presses A on RESTART, then CONTINUE: "player 2 chose RESTART on the pause
  screen: the host's alone" on both machines, no restart, the pause closes on CONTINUE and both play on in lockstep
  (the memories differing as co-op runs do, plus the pause's picture data and two streamed-audio buffers).
- Next: John on two PCs; then the match desync (0x1C4C068), names in co-op, match options.

### 2026-11-11 - The pause screen is one player's; its QUIT takes them to the menu

- John: QUIT should take the player to the game's menu, not close it; when player 1 paused and quit, player 2 stayed
  on the pause screen; the pause should not be shared and keep all its items.
- The stuck pause: a joiner hearing that the host leaves (kLeave) took every later tick of the host's as idle at
  once, so the host's own QUIT press (below the tick it left at) never reached it. Now the host's ticks play to the
  one it left at, and then the others count as gone (src/common/net.cpp).
- The engine already keeps a pause flag for each player (0x262738, 0x30 apart) beside the global one (0x1F65D0):
  0x6AF70(on) sets all and pauses the sound (0xB0570); 0x6AFB0(player) stops that player's update (0xAD520),
  0x6AFB0(-1) the world's (0x6AA90). In a session START's 0x6AF70(1) (from 0xAD58C) sets the pauser's flag alone and
  closing it (0x942B0, from 0x94317) clears it: the world, the sound and the others play on, the pauser stands
  still, and their menu is the game's with every page: Mission, Objectives, Controls, Score, CONTINUE, RESTART,
  QUIT, PC SETTINGS (src/game/pause.cpp). START is not seen by 0xAD520 while it is up, nor in the frame it closed it.
- Drawn on the pauser's machine alone: every menu's sprites are camera 7's 2D list (0x72550), items of the sprite
  pool 0x29AA98 (0x6E6F0 / 0x6E740). Those taken while a player's pause is built (0x93960 from 0xAD57E) or their
  menu updated (0x95A80) are tagged with that player outside guest memory, and 0xD9410 draws the list without
  another player's pause (its array compacted for the call and put back). MenuUp is a menu every player has or the
  local player's own (MenuUpFor in the mouse's per-player writes, which every machine makes alike).
- QUIT asks "QUIT: ARE YOU SURE?" (the same list widget, index 0xD; YES 0). YES in a session: the presser's machine
  leaves (net::LeaveToMenu: net::Leave, the session off, NF_NET_OFFLINE=1 for the exes it relaunches into) and the
  game's own QUIT runs; until the menus are back its controller keeps its seat and the session's other ports read
  connected and idle (otherwise "reconnect the controller to port 2" stops it). Every other machine takes the press
  as NO and closes that player's pause at the next menu update.
- Evidence (headless, two machines): co-op joiner pauses (scripts/pad/level-pause.txt): menu on the joiner's
  screen, the host's shows the level and HUD, both in lockstep to frame 12930; the memories differ as co-op runs do
  plus 18 KB of picture data (the pause's character picture, drawn on the pauser's alone). Joiner quits
  (level-quit.txt, now QUIT then YES): the joiner is on the Single Player mission list, the host plays on, player 2
  dropped. Host quits (net-coop.sh HOST_SCRIPT=...): the host on the mission list, the joiner plays on. A match
  (net-loopback.sh JOIN_PAD=scripts/pad/mp-quit.txt, now with YES): the joiner at the Debriefing, the host in the
  match. SMOKE OK; scripts/build-win.sh builds.
- Next: John on two PCs; then the match desync (0x1C4C068), names in co-op, match options.

### 2026-11-10 - QUIT on the pause screen: that player alone leaves (co-op and matches)

- John: QUIT should let one player leave a running game, not end it for everyone. A press on the pause screen is the
  same input on every machine at the same tick, so each decides alike: the dispatcher 0x8E320 gets the player who
  pressed (its first argument); with QUIT chosen (2) in a session, the presser's machine closes (app::Quit, whose
  WM_CLOSE leaves the session, net::Leave), and every other machine handles the press as CONTINUE: the list widget's
  handler (type 6, [widget + 0x7B], 0x91B10 from 0x72630) answers the choice message 0x34 with 0 while it runs.
- Evidence: co-op (scripts/pad/level-quit.txt as the joiner's): the joiner's pause screen shows on both, QUIT closes
  the joiner, the host's screen returns to the level and player 2 drops out (tick 9370). A match
  (scripts/net-loopback.sh with HOST_PAD=scripts/pad/mp-match-stay.txt JOIN_PAD=scripts/pad/mp-quit.txt): the host
  plays on (8:42 on the match clock at the end), player 2 idle in the match.
- Next: the match desync (0x1C4C068), then names in co-op, match options, the pause for one player.

### 2026-11-10 - Co-op: a player who leaves drops out of the level

- John: a co-op player who quits should drop out, not stand in the level. Since the leaving work their controller
  idles from the tick they left; now (net::PlayerLeft, that tick reached, the same on every machine) the player is
  CoopGone (view.cpp): drawn in no camera (0xDB320 with no cameras after the update and the camera update, body and
  first-person model), +0xCC 0x20 as a dead player has it (nothing stands on or bumps into them), no hit hurts them
  (health 1e9 while 0xADD00 / 0xAE1C0 run: 0xADD00 answers on the x87 stack, so it runs), no enemy picks them
  (0x37C10), no respawn lands beside them (0x9F0B0, so the last player down still fails the mission), their split
  screen view goes dark, and each frame they are put where the first player still playing is (0xAC7E0): the scripts
  that follow player 1 (triggers, the teleports after a cutscene) keep working when the host is the one who left.
- Evidence: two machines, the host leaving at tick 7799 while the joiner strafes (LEAVER=1, JOINER_STARTS=1,
  level-strafe.txt): the joiner's path the same as with no one leaving (y 20.473 at the end; before 0x20 he stood
  on the carried host, y 21.823), the host's body gone from the joiner's frames, which match otherwise.
- Open: whether every mission trigger fires for a carried player 1 is not proven headless (none is on this pad's
  path); a player who leaves while down is respawned beside the others first.
- Next: Quit on the pause screen for one player alone.

### 2026-11-10 - Online: a machine that quits no longer freezes the others

- John: when player 1 quits, the other players' games freeze. Each tick every machine waits for every player's
  input (net::Pad) and a machine that closed sent none again: the others waited forever (a log line after 30 s).
- Now (src/common/net.cpp): closing the window (Quit Game, the close button, Alt+F4: GameInfo::message's WM_CLOSE)
  sends kLeave with the first tick it will not send, five times; the host relays it, and every machine plays that
  player's controller idle from that tick, the same on all of them. The network thread sends a heartbeat (kAlive)
  every 500 ms whatever the game is doing, so a machine loading or compiling shaders is never taken for gone; a
  joiner silent 6 s (crashed, cable out) is given up by the host at the tick it waits on (no machine has that
  input: it all comes through the host) and the host tells the others; a joiner whose host goes silent, or leaves,
  plays on with the other players idle (the relay is gone).
- Evidence: scripts/net-coop.sh with LEAVER=1|2 and LEAVE_HOW=leave|kill (NF_NET_LEAVE=seconds, the harness's
  window close): all four cases play on to the end (the joiner leaving at tick 7736, both machines naming the same
  tick; a kill costs the 6 s wait). No leave in a normal match. scripts/net-loopback.sh fails at 0x1C4C068 with or
  without this change: the match desync is older (John's results screen on one machine only), next on the list.
- Next: the leaving player's character drops out of a co-op level; Quit on the pause screen leaves for that
  player alone.

### 2026-11-10 - The MGM lion's roar: plays headless, not reproduced

- John: with the intro movies on, the MGM logo (07380048.xmv) showed the lion roaring with no sound, on Windows and
  on Linux (Proton); the EA logo and every other movie had sound. Last heard before skip_intro (2026-10-28).
- Headless boots (NF_SKIP_INTRO=0, WAV=boot.wav) record both roars where ffmpeg's decode of the file has them (2.8 s
  and 6.8 s into the movie), stereo as in the file (L/R correlation 0.99 both): with an empty disk, with John's
  disk (his play/hdd), at FPS=15 and 5 (the movie and its sound slow down together, nothing dropped), and from his
  own ISO. The track is the EA logo's format (Xbox ADPCM, 2 ch, 44.1 kHz); both come out 4.4 times under the file
  (stream volume 0.925 x the 2D voice's 600 mB headroom x mix bin headroom 1 = 0.23, XBE_DSOUND_LOG=2). The MGM track
  is mastered 8 dB under the EA one and opens on 2.5 s of silence, so the roar is quiet, but there.
- Next: John boots the current build with Skip Intro Movies off and listens; if still silent, his log with the
  dsound report (XBE_DSOUND_LOG=2) shows the stream's gain on Windows.

### 2026-11-10 - Light glows through player 2's walls: decided from player 1's view

- John: the castle's window glows (07000005) show through walls only in co-op, on player 2's PC, whatever the MSAA;
  the same PC alone never shows them. So not the W-buffer: 8486da0's workaround (XBE_WBUFFER=0 with MSAA, read at
  startup only, so a menu toggle never changed it) is removed, and its entry below was wrong about the cause.
- Two updates decide once a frame whether a glow is drawn over the walls: a lamp's (0x6DCB0: glow [[lamp+0xBC]+0x24])
  and a glow's own (0xA3EB0). Each casts a line (0x2C5B0) from camera 0 (+0xB8) to the glow (+0x24) through player
  1's room (or camera 0's, +0x04); clear, the glow's +0xCC 0x2000 puts it in every camera's mode 2 list (0xDABB0),
  drawn in 0xDAC20's last pass over a cleared depth buffer. Player 1 outside seeing the castle put its glows over
  player 2's corridor walls.
- Fix (view.cpp): the two updates note their glows; after 0xDABB0 builds another player's lists, that camera's
  mode 2 list keeps only the glows its own player's line reaches. A glow player 1 can't see but player 2 can is
  still drawn with the world (depth tested), as for player 1.
- Evidence: two machines, player 2 strafing into the corridor (JOIN_SCRIPT=scripts/pad/level-strafe.txt): camera
  1's mode 2 list 15 entries, 9 glows blocked from player 2 removed (kept 6), frames 10800-13800 without the glows
  as in single player; outside, all kept. A first attempt (masking the glow to the cameras that list the lamp)
  missed 0xA3EB0's glows, which are most of the castle's.
- Next: John checks the glows in co-op on Windows.

### 2026-11-10 - Player 2's floating pistol: a first-person model's attachments left behind

- John: an extra pistol hanging in the air in player 2's view, after picking up a weapon, not every time; player 1
  doesn't see it. His screenshot: the pistol at hand height in the snow a few metres ahead.
- The world's objects (list from [0x1DF34C], next +0x14) are drawn in the cameras whose bits +0xD4 holds (0xDA490
  against the pass's [0x29E80C]); each frame the camera update 0xA8A60 gives a body every camera but its player's
  (0xDB3A0) and the first-person model (inner + 0x778) its player's alone (0xDB380), passed on to the model's
  attachments ([b8 + 0xA0], as many as its record's +0x29). A weapon change (0xB7DF0) frees the model with 0x17A60,
  which deletes as many attachments as the record now in [b8 + 0x74] counts, and builds the new one with 0x11440.
- ApplyPlayerHands (view.cpp) wrote the multiplayer hands' record into [b8 + 0x74] whenever weapon 1 was out and the
  record was another: also partway through a switch, with the weapon already 1 and the model still the pistol's. The
  next weapon change then freed by the hands' count and left the pistol's attachments in the world, frozen, drawn in
  player 2's camera alone. Evidence (temporary count of one-camera objects every 10 s, castle level 07000005,
  player 2 pressing B four times): player 2 went from 7 such objects to 13, then 19, the older sets never moving
  again; player 1 stayed at 7.
- Fix: the swap only replaces the other hands (the spawn's, built outside HandsScope), and as 0xB7DF0 does: 0x17A60,
  0x11440 with the hands, 0xDB3D0, then the skin. The game's own switch to weapon 1 runs inside the player update
  (0xAD520 -> 0xBD740 -> 0xBCAA0), under HandsScope, so it already loads the right hands. Same run: player 2 at 7,
  1 with the hands out, 7 again, no swaps. Smoke OK, the Windows build builds.
- `LogLeftObjects` (report level): an object drawn in some cameras only that is no player's body, first-person model
  or their attachments is logged once ("... is no player's"), up to 20 a level.
- Next: John, player 2 picking up weapons in co-op; then the MSAA + W-buffer bleed's real cause.

### 2026-11-10 - Jumps play no body animation, in a match either

- John: side steps work (eca57fb); jumps show no animation on the other player's body.
- The jump is 0xA9990 (from 0xAD210, walking): Y (button 7) on the ground sets [inner+0x8DC] 1, an upward speed
  (0xD7430) and sound 0xA3; landing sets it back to 0 with the same sound. Only physics and the view (0xAB180,
  0xAB5D0, 0xAE1C0, 0xAB280, 0xABBD0, 0xAC7E0) read +0x8DC, and no record, set or layer play follows it. Every body
  play goes through 0x17220 (record), 0x17B90 (set) or 0x15CA0 (0x18BB0's side steps, 0xAF5A0's, 0x33F20's).
- Evidence: a temporary trace of 0x17220 and 0x17B90 on the players' actors in a 2-player bot match
  (net-loopback with Y pressed six times in the match) logged the spawn record 060001D4, set 0017E71C and the walk
  set 0017E764, nothing else. A body stays in its walk pose through a jump in a match as well; nothing to change.
- Next: John checks whether the other player's body rises in a jump at all (a position, not an animation,
  problem if not), then player 2's floating pistol.

### 2026-11-10 - Co-op players step sideways as in a match

- Why strafing did not animate: a movement layer ([inner+0x820], made by 0x17B90 from a set) is played each frame by
  0x18BB0 (actor, layer, speed, top speed, ...; from 0xAD210 walking, 0xAD340, 0xAA6F0 in state 9): the walk record
  by the speed's share of the top speed, then, only with [0x260018] (0x18F4C), the side step: the angle of the two
  speeds picks direction 0, 1 or 2 (+0x2F), and the layer's list (+0x18, record IDs) after its count (+0x31) gives the
  record, list[count + 1] or list[count + 3], played into +0x14 (0x15CA0).
- `config/nightfire.toml` [[hook]] after 0x18F4C calls `CoopStrafe` (`src/game/characters.cpp`): in a session, for
  the four players' outer actors (0x1F6654), when both side records are registered, eax 1 as in a match. Only that
  branch changes (0x18BB0 reaches 258 functions, some reading 0x260018, so the flag itself is not set).
- Evidence: `JOINER_STARTS=1 JOIN_SCRIPT=scripts/pad/level-strafe.txt scripts/net-coop.sh` (new script: the joiner
  starts, then holds the stick right 160-185 s): both machines log the side steps (sets 060009E4/E5 and
  060009D7/D8); at frames 10860-11040 player 2's layer has a side layer (+0x14) and direction 2, gone at 11220 after
  the stick is let go, player 1's none, the same on both machines. Memory differs in the known co-op places. The
  Windows build exports the hook and its DLL calls it. SMOKE OK.
- Fire and melee: every fire, melee, reload and idle record default.xbe plays (0xBCAA0, 0xBB300 and their chunks)
  goes on the first person model ([inner+0x778]); 0x260018 there picks the model's match offsets (0xBD5E2), and
  0x160A0's scales a height. No body animation for them gated to a match was found: next, John compares a real
  split screen match.

### 2026-11-10 - Lights through walls with MSAA: W-buffering off while MSAA is on (workaround)

- Player 2's PC (MSAA 8x, render scale 100%, smooth alpha edges off, edge smoothing off, Direct3D 12, 16:9; log
  nightfire_114: twins of 83D86000 and 83EB3000 1920x1080 and depth 83C59000): `XBE_WBUFFER=0` (a bat file) takes
  the glows behind the walls away. So D3D12's W-buffered depth (the pixel shader writes depth from w) goes wrong
  with MSAA twins; read and not found: the twins' clears (both cleared), their copy and resolve shaders (sample 0),
  the pipeline (only the sample count changes), alpha to coverage (every draw blended).
- John chose a workaround for now: `src/common/game_info.cpp` (Init, both exes) sets XBE_WBUFFER=0 when the
  settings start with MSAA on and no XBE_WBUFFER is given, logged at report level. MSAA switched on in the F1
  menu later keeps W-buffering until a restart (the renderer reads the switch once).
- Evidence: the Windows build; SMOKE OK (headless has no MSAA: unchanged). The root cause in the SDK's D3D12 MSAA
  path is open (a run under Wine and vkd3d here, draw by draw, is the way to it).

### 2026-11-10 - Lights through walls: correct headless, so a Windows renderer difference (open)

- John (both PCs, ab518de): each player's controller and stance icon confirmed. Player 2's floating pistol is not
  there every time. His screenshot of the castle (07000005, The Exchange's first level): warm yellow glows drawn over
  a wall standing between the camera and the castle's lit windows.
- The glows: 4-vertex fans with a 128x128 texture (t0 832FEB80 and 83314480, 82A7BC00), additive (blend 302 1),
  alpha tested, W-buffered (`zen 2`), depth tested LESSEQUAL with depth writes off, drawn by a vertex program
  (B00012E1, as the walls). No draw filter, no visibility tests (no NV2A report or pixel count methods in default.xbe),
  no reads of the frame or its depth before them: one target all frame.
- Headless (`NF_LEVEL=7000005`, REF=1500, REF_DRAWLOG=1): the reference renderer hides the glows behind the wall
  (frame 4500, crouched behind the first wall: glows behind it `colour 0 zfail N`). It and D3D12 compute W-buffered
  depth the same way (a program's w, offsets in its units), so the difference is in something D3D12 does that
  headless does not (resolution scale, MSAA, aspect). The 4096-point snow (prim 1) is a different draw.
- John: his PC (antialiasing off, 16:9) shows no glows through walls; player 2's, antialiasing on, does. So the
  D3D12 MSAA path. Not alpha to coverage (every draw in the frame is blended, 302 303 or 302 1); one target all
  frame, so no settle and refresh of the depth twin between the walls and the glows. Wine, vkd3d and lavapipe are
  here, but no virtual display or pad script for the Windows exe.
- Next: on player 2's PC, the setting (FXAA or MSAA 2x/4x/8x), the same wall with `XBE_WBUFFER=0`, and an F3 capture
  (`XBE_CAPTURE_DIR`) with MSAA on.

### 2026-11-10 - Co-op: each player reads their own controller and shows their own stance icon

- John (both PCs, 203caee): forward, back, crouch and two-handed holds animate; strafing, melee, maybe firing and
  weapon swaps do not; player 2 sees an extra pistol floating after picking up a weapon (player 1 does not); a
  player crouching shows the crouch HUD icon's fade on every screen; when player 2 starts the campaign, player 1
  moves with player 2; lights (the second level's castle) glow through walls.
- Player 1 moving with player 2: each player's controls row (0x1FE6D0 + 0x158 * i) reads the controller at +0x156.
  The campaign sets row 0's to the controller that pressed START (0x7CD50, 0x86690, 0x19DB0); co-op copies row 0 into
  the others' with their own port (InitDefaultCombatant), so with the joiner's START rows 0 and 1 both read
  controller 2. `src/game/view.cpp` (sub_0006CF50): in co-op each tick row i reads controller i (rumble, 0x6CC00 and
  0x6CC40, follows the row's port too), logged once at report level.
- The stance icon: the full HUD's update (0xB2B50, args actor, group, outer actor) shows 0x3000173 while the outer
  actor's +0xD2 is 4 (crouched) and sets a one-second fade at 0x2790AC; while it runs a player not crouched shows
  0x3000174 fading. The icon picked by [actor+0x8EF] (0x3000064-6A) fades the same way on 0x2790B0. One timer each
  for every player: `sub_000B2B50` in co-op swaps in the player's own two (PlayerSlotState::icon_fade, reset with
  the level's cameras). Other shared HUD bytes 0x2790A0-DC (0xB3AE0, 0xB3250, 0xB54D0, ...) are not changed.
- Evidence: `JOINER_STARTS=1 scripts/net-coop.sh` (new: the joiner's controller takes the menus, START, and one A
  in the level, `scripts/pad/level-crouch.txt`): both machines log `co-op player 1 read controller 2; now controller
  1`; at the snapshot player 2's outer +0xD2 is 4 and player 1's 0 on both; player 2's screen has the crouch icon,
  player 1's none. Memory differs in the known co-op places (open). SMOKE OK.
- Not yet: strafing, melee and firing on the body (0x18BB0 picks a set's record by speed; the swap's event 0x9E430
  (2, 0x44) is sent only with [0x26001C]); the floating pistol; the lights (no visibility tests or depth reads in
  default.xbe: next, how the glows are drawn and a reference frame of them).

### 2026-11-10 - Co-op players move with a match's animations (they held one pose)

- John (both PCs, 70db3cd): the chosen characters load and wear their models and hands, but moving, jumping or
  crouching they stay in one gun-holding pose.
- Why: the four stance functions (0xA91E0, 0xA9350: a movement set of 9 records; 0xA94C0, 0xA9590: one record)
  pick by the weapon's stance ([0x18D024 + 0x10C * weapon], 1-6) only in a match (0x260018); otherwise every
  weapon gets the sets 0x17E68C / 0x17E6B0, whose 9 records are all 06000A00 / 060009FF, a held pose, since a
  campaign never shows its player's body. The match's records (0x060001C8-F4, 0x060009AA-0x06000A1A) and their
  animations are in every match's archive and no campaign level's. A record (type 5) is 0x20 bytes, its
  animation's ID low half at +0xE; 0x110C0 only checks it, so it finds its animation by ID as it plays.
- `src/game/characters.cpp`: on every co-op level (characters chosen or not) the stances' 75 records and their 75
  animations (528 KB) come from 07000023.bin with the characters, everything in the archive's order (a match's);
  and the four stance functions are overridden: in a session out of a match they pick by the weapon's stance as a
  match does, when the record they pick is registered, else the game's own code runs. 0x260018 is never set (the
  2026-11-10 campaign break).
- Evidence, `scripts/net-coop.sh` (characters 13 and 4, the second level): on both machines `159 of 159 entries
  (1051 KB, 9 texture aliases, 0 left blank; 75 stance records, 75 animations) ... in 138 ms`; both players play
  the match's sets (17E764, the weapon's stance 1, and 17E788) and records (060009DA, 060009DD), the game handing
  back an animation each time, the same lines on both machines. The memories at frame 12930 differ as a co-op run
  without characters does (16 runs, 1.85 MB: two players' actors, the heap's top and above it). SMOKE OK.
- `scripts/net-coop.sh` compares the same frame: a dump lands on the next 30th frame after its trigger, so a
  machine half a second behind dumped a frame earlier (12540 against 12570); now six dumps each and the newest
  frame both have.
- Next: John, both PCs: the other player walking, running, strafing, crouching and jumping with each weapon, in a
  chosen character and on the mission's model. Then what the co-op memory difference is.

### 2026-11-10 - The chosen characters load with the level (headless: Drake and Rook in the second level)

- `src/game/characters.cpp` (new): in a session of two or more, as a campaign level's archive loads, the chosen
  multiplayer characters' models (type 3) and texture packs (type 2), the multiplayer hands 050000B0 and their four
  skin packs are read from 07000023.bin (the first multiplayer map's archive; a match's archive holds all 29) with
  the game's own packed file system and EDL unpacker, and handed one at a time to the loader's own entry dispatcher
  (0xBE7B0) just before the level's main pack (type 1, the archive's last entry), whose 0x11200 then gives them
  Bond's hardpoints as a match's does. Their data is in the level heap (0x70AE0) and goes with the level. No session,
  or every player on the mission's model: the level loads as it always has.
- An archive stores a texture once: a later copy is an alias ("TD", the slot - 1) naming the slot the first one
  takes when the whole archive loads, and 14 of the 29 characters' packs alias another pack's textures. The first
  try, packs as stored, crashed in 0xDDF90 seven seconds in (an alias copying the frames of a slot that in a
  campaign level is the level's own). Each pack is rebuilt with its aliases replaced by the textures they name, the
  slots numbered as 0xDDF90 numbers a whole load (from 1, every pack of types 0, 1, 2 and 11 in directory order); an
  alias not resolving to a texture of its own size and format would be drawn with the default texture, and logged.
  Offline, on the whole archive: all 330 aliases resolve so.
- `src/game/view.cpp`, the last entry's step 4: `EnsureCharacterModelLoaded` (the reader that never read), the
  per-frame model poll and the borrowed-hardpoint fallback are gone. `ResolveCoopModel` takes a choice the level
  registered, else the mission's model; a choice changed during a level waits for the next one (a report line). The
  need flags (0x1637CC) are no longer set: nothing reads them (0x1A1E0 sets them for a match, whose archive has all
  29 anyway).
- Headless tools: `NF_LEVEL=<hex>` (src/game/hooks.cpp) starts that level at the first boot's START instead of Paris;
  `NF_NET_CHARACTER=0..29` (src/common/net.cpp) is net_character (0 the menu's).
- Steps 1-3 came out otherwise than planned: the loader and the file system are read from the code (FINDINGS.md,
  "Levels and characters"); no room was needed (two characters with the hands take 485 KB of the level heap: 15.85 MB
  free in the second level without them, 15.37 MB with); the need flags are dead, so the level gets the characters
  instead.
- Evidence, harness, the second level (07000002) from a fresh disk:
  - Single player: draws identical to HEAD's (with NF_LEVEL alone) at all 198 logged frames, to frame 11880; no co-op
    lines.
  - Two machines, host choice 2 (Drake), joiner 3 (Rook), 300 s: on both `co-op characters: 9 of 9 entries (485 KB, 5
    texture aliases, 0 left blank) from 07000023.bin in 95 ms, hands 050000B0 registered`, `co-op player 1 model set
    to 050000A5 (choice 2)`, `player 2 ... 0500008E (choice 3)`, the multiplayer hands with skins 4 and 5. Dumped at
    frame 17340: 050000A5 and 0500008E at the same addresses on both machines with Bond's hardpoints (+0x1C his
    record, +0x2B 60, +0x2A 0), as a match's dump has them; both machines to frame 17820 together. Their memories
    differ where a co-op run without characters (same build) differs too: 5 bytes in player 2's actor, a few hundred
    near the heap's top, each machine's own views above the heap.
  - Snaps: the multiplayer hands in first person on both machines; the other player was not in view.
- Found on the way: a joiner on the same PC (`NF_NET=join,127.0.0.1`) is registered twice, its hello arriving from
  127.0.0.1 and by LAN broadcast from the PC's address; the host then waits for a third player's tick 0 for ever.
  The runs here joined by the LAN address.
- The first-person hands: a player wearing a multiplayer character has a match's hands (050000B0, mode 2, the
  character's skin), everyone else the mission's; the global [0x18D188] is that player's while their update and
  camera run (view.cpp's HandsScope). A player with no choice no longer gets a multiplayer skin (`slot % 6`) whose
  textures the level lacks.
- `scripts/net-coop.sh [secs] [host's character] [joiner's]` (LEVEL=<hex>): the two-machine campaign test.
  Characters 13 (Bond Tux) and 4 (Kiko), the second level, 220 s: on both machines 9 of 9 entries (521 KB, 9
  aliases, 0 blank) in 106 ms, both players in their models with the multiplayer hands, skins 4 and 6.
- Its memory comparison fails, and not because of the characters: the same run with no characters (0 0, same
  build) differs in the same 18 runs of blocks (1,851,605 bytes against 1,848,713), the heap's top
  (0x83BFA000-0x83C04000) and above it, two floats in 0x8354C000/0x83557000. So co-op itself leaves the machines'
  memories apart there, and the script says FAIL until that is explained.
- `scripts/smoke.sh` (NF_SKIP_TUTORIAL=0: the route is the first boot's): SMOKE OK.
- FINDINGS.md: "Levels and characters".
- Next:
  1. John, both PCs: the second level in co-op with two chosen characters: the other player's model, its textures
     (no grey default ones), its animations and the weapon in its hands, and each player's first-person hands.
  2. What the co-op memory difference above is (both with and without characters): two floats and the heap's top.
  3. The third-person weapon stances for multiplayer characters in the campaign, from what John sees.
  4. Headless, the characters whose packs alias the most (choices 19, 20, 21) and four players.

### 2026-11-10 - The retest: the campaign plays again; the co-op character reader has never read its archive

- John (both PCs, main after 2c22598; the later commits are text), the second campaign level online, Linux host and
  Windows joiner: the level loads and draws on both machines (camera 0 `counts 23/5/6`, camera 1 `88/43/16` at
  2 s), player 2 is a real actor (`co-op campaign spawned player 2 (actor 82123970)`), and every co-op line is the
  same on the two machines. Player 1 chose 5 (Alura), player 2 chose 12 (Ninja): both `could not be loaded; keeping
  the mission's default model 05000007`, so both played as Bond.
- The reader reads nothing. `diag 07000023: magic 98989898 nchunks 2560137368`: the 8-byte header was read straight
  into the 64 KB buffer from the game's heap (`hdr_va`, below 64 MB, so no bounce) and came back as it was, the 0x98
  fill (not the runtime's; the game's allocator, presumably). 104/106's `magic 00000000` was the same thing in
  zeroed host memory, so reads above 64 MB being refused (960cd1b's reading) was not the cause, and in no log yet has
  the reader read the archive. `07000023.bin` is a file inside `filesys.d00`-`d10` (the kernel opens only those,
  once at boot, and `z:\cached_filesys.dat`): `sub_000E3100`/`31A0`/`31F0` are the game's own packed file system,
  and their arguments were never read from its code (the read may be queued for the streaming system, or take its
  arguments in another order).
- In the level all 29 table entries are `need 0 loaded 00000000` and the multiplayer hands 050000B0 are not loaded:
  the campaign archive holds none of them.
- Next, in this order, on John's Linux PC (the game files are there and are never uploaded; scripts/ghidra.sh and
  the harness need them):
  1. Ghidra: how a match loads its characters (where the need flags at 0x1637CC are read, which archive and heap
     they go to, where skeletons, animations and the hands are set up), the campaign level heap's size and where
     it comes from, and `sub_000E3100`/`31A0`/`31F0`'s real arguments.
  2. Room, in a session only: a few MB more for the level heap, or a heap of the characters' own, from the memory
     past 64 MB (game code already reads there: the PC Settings strings). Single-player keeps the stock layout.
  3. In a session, the chosen characters' need flags set before the level loads (`ApplyCharacterSelections`
     without its `MatchLoading` gate), so the game loads them as for a match; players 2-4 spawned with their models.
  4. `EnsureCharacterModelLoaded`, the per-frame model poll and the borrowed-animation fallback removed; a choice
     holds from the level's load (an F1 change takes effect at the next load).
  5. A campaign version of scripts/net-loopback.sh: two machines into the second level with chosen characters,
     checking the table's loaded column and the frames; then smoke; then John's PCs.
  The third-person weapon stances (the removed hooks' aim) the same way: the stance read found in Ghidra and
  changed for co-op players only.

### 2026-11-10 - The campaign broke again in single-player too: the multiplayer flag set by the stance hooks

- John (both PCs, 1fbf22d), the second campaign level: in plain single-player as well as online co-op, a black
  world under a working HUD, audio from another part of the game, full armor at the start, the player already
  crouching, the weapon name and ammo drawn over a second set. The two machines' logs (Linux host, Windows joiner)
  have identical game-side lines (addresses, models, camera values), so lockstep held: the fault is deterministic.
  Camera 0's two 512-entry draw lists stay empty at 2 s and at 20 s (`counts 0/0/7`) with its fade off: the world
  draws nothing, rather than a fade staying on.
- Single-player: four hooks, `sub_000A91E0`, `sub_000A9350`, `sub_000A94C0` and `sub_000A9590` (the "third-person
  weapon stances"), set `[0x260018]` (is-multiplayer) around every call with no session check, so those functions
  took their multiplayer path in single-player. Added in a0224bc, removed by bc1107e ("Fix single-player and co-op
  campaign level loading"), put back word for word by 1fbf22d, where they were its only change that runs without a
  session (its DLL patcher change redirected 0 calls on both machines). Removed. Don't set `[0x260018]` around
  campaign player code: the stance fix wants the stance read itself (the hooks' note: singleplayer hardcodes
  0x3E7 / 3 instead of `[weapon * 0x10C + 0x18D024]`) found in Ghidra and changed there.
- Without a session the game now has the same hooks as at bc1107e: `sub_00011440`'s +0x2A/+0x2B model bump
  (0c3fead, for live character switching) runs only in a session. Still different without one: the menu's one-time
  character name lookup (`sub_0006CF50` -> `sub_0007EB90`) and the DLL patcher at start.
- Co-op: the spawn returned 0. `view.cpp`'s `CallGuest` put back every register but eax, and `sub_000ACAB0`'s hook
  calls `ApplyPlayerHandSkin` after the original in any session, so the spawn returned the last call's eax: `co-op
  campaign spawned player 2 (actor 00000000)` on both machines, and `sub_000AE870`'s own spawn of player 1 got the
  same. `CallGuest` restores eax now, as `kernel::CallGuest` does; `sub_000CEB30` sets its eax itself.
- Co-op characters, from the same logs: every `07000023.bin` read into the host-memory scratch (0x8456E000, above
  the title's 64 MB) came back zero (`diag 07000023: magic 00000000 nchunks 0`, four times at spawn), so nothing
  loaded, yet each try opened and read the file and switched the heap (`sub_000706C0`) and `sub_000DCA60` during
  the spawn. All 29 multiplayer models `need 0 loaded 00000000`; players 1 and 2 got mission models 05000030 and
  0500000D. 960cd1b (after these logs) reads through a 64 KB guest-RAM bounce buffer and keeps the mission's model
  when a choice can't be loaded: untested. `0x274CA4`, zeroed by the loader and never put back, is now restored
  with the loader globals around it.
- The character loader has had at least six approaches since bc1107e: streaming on demand (ff67ead), the 0x7000500
  queue (4bb4ae6), each character's own archive (f545db2), extraction from `07000023.bin` into host memory
  (24649bd), into guest RAM (b79e540), host memory again (1fbf22d), a guest bounce buffer (960cd1b). When a model
  did load it had no animations and the first-person hands stayed Bond's; the animation fallback copies Bond's
  +0x1C/+0x2B onto another skeleton and reruns `sub_00011200` after clearing `0x1D6A65`, both unverified.
  By the three-attempt rule this wants a different approach, not another patch: load the chosen characters as a
  multiplayer match does, when the level loads (the need flags at 0x1637CC, which `ApplyCharacterSelections` sets
  in `sub_0001A1E0`). That needs room in the level heap (bc1107e: the 2048-sprite pool alone crowded out a campaign
  level's scene and sound) and the level loader read in Ghidra.
- Not run here: this checkout has no game files or generated code; `view.cpp` syntax-checked with g++ and a stub
  `image_symbols.h`. So it went to `main` without scripts/smoke.sh: John gets changes only by pulling `main`, and his
  runs are its test. Next: build on Linux and copy `Nightfire.exe` with the `Nightfire.dll` from the same build to
  the Windows PC, as John does. The exe no longer exports the four stance hooks, so a `Nightfire.dll` linked before
  this fails to load ("The game's code could not be loaded"); scripts/build-win.sh relinks it on every run. Then 1)
  single-player on the second level, 2) co-op with everyone on Default, 3) co-op with chosen characters; in the co-op
  logs `co-op campaign spawned player 2 (actor ...)` should be nonzero.

### 2026-11-08 - Full-screen per-player online view, per-player scope zoom, and relaunch packet versioning

- Two-PC online play (Linux host + Windows 11 joiner over LAN) confirmed working in-game with both players joining
  and controlling their characters.
- Full-screen view per player (`net_fullscreen`, on by default, toggleable in F1 -> Online):
  - `sub_00025B80` and `sub_00025190` (`src/game/view.cpp`) give all four player cameras (`0..3` at `0x1F661C`)
    the full `640x480` screen rectangle and 1-player full-screen aspect (`16:9` or `4:3`) and pixel scales (`+0xEC`,
    `+0xF0`), and `sub_000B1AB0` temporarily sets the player count `[0x260044]` to `1` during HUD layout so every
    player's HUD widgets are positioned at full-screen single-player coordinates. Because every machine sets the
    same full-screen camera state in guest memory for all four cameras, guest memory remains bit-for-bit identical
    across the session.
  - `sub_000DD950` (`src/game/view.cpp`) and `sub_000D9410` (`src/game/hud.cpp`) suppress host GPU draws
    (`d3d::SuppressDraws` in `sdk/runtime/xdk/d3d8.cpp`) while a remote player's camera (`player != net::LocalPlayer()`)
    is being drawn, and `sub_000E7130` suppresses the split-screen divider bars. `D3DDevice_SetViewport` and
    `D3DDevice_Swap` reset draw suppression so shared menus and pause screens after the camera passes always draw.
- Per-player scope zoom (`src/game/mouse.cpp`): `ViewZoom(int player)` now reads `R32(0x1F661C + 4 * player)`, so
  every player's mouse wheel scope zoom and zoomed-in aim slowdown track their own camera.
- Relaunch packet versioning (`src/common/net.cpp`): `WireVersion()` includes `kDriving` so a dying `Driving.xbe`
  and a newly launched `default.xbe` ignore each other's packets during relaunch transitions; joiner sockets bind
  `INADDR_ANY:0` before starting `Receive()` to avoid `WSAEINVAL` (10022) log noise.

### 2026-11-08 - Windows / Proton in-game play ready, and Linux-to-Windows cross-build (scripts/build-win.sh)

- With `scripts/net-loopback.sh` passing end-to-end into the multiplayer match, the Windows / Proton path is
  wired up and verified by cross-compiling and linking both `Nightfire.exe` and `NightfireDriving.exe` on Linux:
  - `GameInfo::relaunching` (`net::Close`) closes the UDP socket in the old process before `kernel::Relaunch`
    spawns the next executable on Windows (and `SO_REUSEADDR` is set on bind), so the new process can bind UDP
    `net_port` immediately while the old window waits out the handover.
  - `net::Init` moved from `GameInfo::init` to `GameInfo::window`: `--prepare-code` never blocks on a session,
    the game window is visible during `Meet()`, Win32 messages are pumped with live connection status in the
    window title bar, and the host can press Enter / Space / Start to begin early once at least one joiner is in.
  - `net_players` option added (2..4, default 2) so a 2-player session starts in milliseconds as soon as the
    joiner connects instead of waiting out the full `net_wait` timer; duplicate `kHello` packets from the same
    endpoint are deduplicated, and a missed `kStart` packet is re-sent if a registered joiner asks again.
  - Testing two windows on one PC: launching a second copy with `--instance 2` when `net_host = true` is set
    automatically joins `127.0.0.1` using `hdd2/`; `pause_unfocused` is forced off during an active session, and
    unfocused windows send an idle pad so background controller inputs never move the unfocused player.
  - `scripts/build-win.sh`: cross-compiles and links `Nightfire.exe` and `NightfireDriving.exe` on Linux using
    `clang++`, `lld-link`, `xwin` (MSVC CRT + Windows SDK), and `SDL3`, and installs them into an existing
    Proton/Windows game folder while touching the existing `Nightfire.dll` and `NightfireDriving.dll` so they
    load immediately without rebuilding.

### 2026-11-08 - THE MATCH RAN IN LOCKSTEP: two machines into a bot match over the net, draw for draw

- With the main-menu launch page made by the player's own hands (scripts/grab-menu-launch.sh watching their
  install while they play to the menu - thirty seconds of gameplay replacing every scripted attempt at the
  game's scripted content), net-loopback's match test ran whole: two machines meeting at the boot, straight to
  the main menu, mp_bots' walk through Multiplayer (both players joining through the wire), into the Skyrail
  bot match. They left the menus at the same tick 0.002 s apart, and ended frame 15720 with 5,173,164 draws -
  identical on both machines, 5.2 million draws in lockstep.
- The comparison: 7 bytes of 134 million - three small scattered floats, the probe's ambient drift class (one
  16 KB from its original find), wall-clock-stepped smoothed directions too small to change a single draw.
  net-diff gives that class its own verdict now, and the test accepts it only when the machines' frame and draw
  lines match - the lockstep evidence, not a tolerance.
- Where this leaves the feature: the console's split-screen has been played by two machines over a network,
  byte-verified. Remaining: Windows (the real thing: XAudio2's pacing, the relaunch's port handover), the drift
  writers pinned to the tick in Ghidra, the lobby UI, the desync checksum, and upstream.

### 2026-11-08 - The chain runs: relaunch, re-meet, live transport - and the audio mixer's gridlock

- With the packer fixed (mouse_y was written into the bundle but never counted: 38 bytes on the wire against a
  42-byte format, every bundle parsed and rejected - the trace caught it in one look), the loopback chain ran for
  the first time: the A press landed at 5.3 s, both machines relaunched into Driving.xbe within 2 ms of each
  other at 6.7 s, re-met at their own boot (the second "booting with 2 players" at 0.28 s), and the transport ran
  live - 42-byte bundles both directions at matched sequence numbers. The meet, the relaunch, the re-meet, the
  pad transport: all proven working.
- The front end then ground from 60 fps to 4, stopped presenting at ~24 s, and never dumped - a three-way
  gridlock of my tick-paced mixer (XBE_AUDIO_TICKS): the front end blocks writing its music stream while nothing
  consumes, consumption waits for presented frames, frames wait on the blocked render. The wall-clocked device
  can never form that loop, which is why the harness paces it so. The net tests are back on the wall-clocked
  mixer, and net-diff now reads one contiguous difference as the audio ring - its own verdict, distinct from a
  desync (the games stay identical with the rings apart; proven over 88 s).
- Next: the loopback chain again on the wall-clocked mixer - the chain verdict this time; then the multiplayer
  menus and a bot match.

### 2026-11-08 - The relaunch chain test: the harness execs the other exe, as Windows does

- HalReturnToFirmware writes the launch data page before Relaunch, and the harness's Relaunch on Linux is an
  execl into the exe GameInfo::images names, looked for beside the current one: building the driving harness as
  .local/harness/NightfireDriving makes the whole chain happen in-process - the title's A relaunches, the new exe
  boots Driving.xbe from the launch data in the run folder (GAME_DIR and the pad script's env carried by the
  exec), and the relaunched machines meet again at their own boot. scripts/net-loopback.sh does that now: both
  harnesses built, the driving code generated if missing, a fresh first boot, one A (scripts/pad/chain.txt) that
  relaunches the title and lands wherever the front end has got to, the comparison at the end.
- POSIX sockets survive an exec, so the session socket would still be holding UDP net_port when the relaunched
  process binds it: close-on-exec on the socket now (the harness's chain is clean; Windows's handover is a
  different path, noted in docs/online.md's limits).

### 2026-11-08 - NET-LOOPBACK OK: the session machinery works end to end

- With the audio pulled by the game's frames, the loopback session compared clean: 4 kernel clock bytes and
  nothing else in 128 MB - two machines that met over UDP (a joiner given controller 2 at 3 ms), booted together
  at 25 ms, and played 88 s in lockstep (frames 5220/5280, draws 71868/72048, matching throughout) ended the same
  game. The meet, the handshake, the boot together, the tick barrier, the pad transport: proven.
- Next: the relaunch chain - the title's START relaunches into Driving.xbe (the front end on the multiplayer's
  path), so the test chains the two harness exes as scripts/smoke.sh does, the session re-meeting at each boot,
  both machines' front ends writing the same launch data and resuming the same game on the other side. Then the
  multiplayer menus (mp_bots.txt's flow once launch data reaches them), a bot match with both players, and then
  Windows.

### 2026-11-08 - The session forms and holds: 88 s in lockstep, identical frames and draws; the audio ring named

- net-loopback's second run (Artix): the meet took 3 ms (a joiner given controller 2 at 0.003 s), both machines
  booted together at 0.025 s, and ran 88 s - frame counts and draw counts identical between the machines the whole
  way (71868 and 72048 draws at frames 5220/5280, matching). The session machinery works: the handshake, the boot
  together, the tick barrier, the pads.
- The memory comparison found one contiguous difference: 365,540 bytes in 151 blocks, 0x251D000-0x25B4000, one
  ~606 KB region (three seconds of 48 kHz stereo - an audio ring) - everything else in 128 MB identical. The
  harness's mixer pulls 480 samples every 10 ms by the host clock; two machines' pulls drift a few samples a
  minute, so the ring's phase differs while the game itself, whose simulation never read it back, stayed
  identical. XBE_AUDIO_TICKS=1 in null_audio.cpp: pulls by the frames the game presents (kRate/60 a frame, the
  game's own clock, holding when the game holds); both net tests set it. The Windows XAudio2 path wants the same
  treatment for the real feature (docs/online.md's limits).
- Next: net-loopback again for the clean verdict; then the relaunch chain (the title relaunches into Driving.xbe,
  the front end on the multiplayer's path: scripts/smoke.sh's two-exe chaining, the session re-meeting at each
  boot), then the multiplayer menus and a bot match.

### 2026-11-08 - The session's first runs: the meet never happened, and the flow runs through Driving.xbe

- net-loopback's first run (Artix, headless): both machines played the title identically, frame for frame - and
  died at 6.6 s when the title's START (the pad script's A: the title takes it too) relaunched into Driving.xbe,
  which the harness could not run. No net: line in any log: the session had never formed, because Init returned
  at its !host && !join guard before reading NF_NET - the harness drives the settings by env, no settings file
  exists headless, and the guard came first. The env now comes first.
- The relaunch is the discovery, not the accident: the path from the title to the multiplayer menus runs through
  Driving.xbe (the front end), so the net code's "default.xbe only" refusal was wrong. Sessions now form in both
  exes: a relaunch ends one session, the launch data it leaves is written identically by both machines (they
  played it in lockstep), and the next exe's machines meet again at their own boot. The harness chains the exes
  as scripts/smoke.sh does - a later net-loopback phase will press through the relaunch and prove the re-meet.
- The probe re-run: the drift float is at the same 0x37DC1E0 with a different value every time (0.21/0.20 one
  run, 0.41/0.24 the next) - one ambient variable, named in docs/online.md's limits now. Everything else in
  128 MB matched again.
- net-loopback.sh reworked for the clean first proof: both machines idle (scripts/pad/idle.txt) at the language
  select - a standing screen with no attract movie, so the drift float cannot pollute the verdict - in one
  session for 90 s, then the probe's comparison. loop_host/loop_join.txt removed; the phased presses come with
  the relaunch chain.
- Next: net-loopback with the meet fixed; then the relaunch chain (driving harness from the launch data,
  re-meet), then the multiplayer menus and a bot match.

### 2026-11-08 - The determinism gate passes; the session test written

- net-probe on a second machine (Artix, headless): the same pad script twice, whole memories compared at a pause
  - 6 bytes of 128 MB differ, all in the kernel variables' first megabyte (memory.h: kKernelDataBase): KeTickCount
    one tick apart (81 vs 82), and at +0x510 the kernel's time caches - a 64-bit FILETIME at
    0x01dd6f8f688cdc38, ~2 ms between the dumps (the comparison is touched at one instant, each machine dumps at
    its next frame), an ms-since-boot that matches (one host), and 0x2710, KeTimeIncrement's 1 ms. The game's own
    state, everywhere above that megabyte, is byte-identical - so the game never reads those clocks into its state
    either, or it would have diverged with them.
- scripts/net-diff.py is the tests' verdict now: it names the kernel's clock megabyte and does not fail on it,
  and prints each differing byte with context (cmp prints its offsets in octal on some systems and decimal on
  others - Artix's had 9s in them).
- scripts/net-loopback.sh: two harness machines on one host meeting as the net code makes them (NF_NET=host /
  NF_NET=join,127.0.0.1, NF_NET_PLAYERS=2, input delay 2), booting together and playing the menus with their own
  pads (scripts/pad/loop_host.txt, loop_join.txt), the games compared at the end. The first test of the session
  machinery itself: the meet, the boot together, the pads over the wire, the tick barrier.
- Learned getting here: cmp's -l offsets are octal on one system and decimal on another; a script's own paths
  must be resolved before a cd; the harness's build.sh excludes the other exe's sources by what is in generated/
  (its -x does it whatever is); the harness's input stand-in (pad_script.cpp) is why input::GameThreadPoll had
  to be inline in the header; Artix's glibc keeps ftruncate out of sys/mman.h's reach.
- Next: net-loopback on the machine with the game; then a two-player pad script into a real match with bots; the
  residual risk it will answer is whether the game steps its simulation by measured time rather than by tick
  (two machines paced differently would diverge even with ticks aligned).

### 2026-11-08 - Online multiplayer, first version: lockstep of the pads and the mouse

- The consoles' game has no networking (no XNet, no system link), so online play is the split-screen over the wire:
  every machine runs its own complete copy of the match and each tick sends only what its player did - the final
  pad the runtime made of their devices, the mouse's movement as counts, their scope-zoom wheel notches, whether
  they play the keyboard - and applies every player's input at the same tick, so the copies stay identical with no
  game state sent. A tick is a port-0 poll on the game's thread (the game's own call pattern, the same on every
  machine), input held net_input_delay ticks (1..12, the host's) to absorb the round trip. Sessions meet before
  boot: the host (net_host, UDP net_port) gives each joiner (net_join, the settings' join_ip) a controller, and
  everyone boots from power-on together, the one state they share; no one joins a match already going. Joiners send
  to the host, which relays to the other joiners (src/common/net.cpp; the design and the limits: docs/online.md).
- The mouse writes game state outside the pad (the direct aim's turn and pitch, the missile/turret stick, the
  wheel's scope zoom), so those travel too and every machine makes the same writes: 0xA9CF0's hook applies each
  player's movement to their own actor (its +0x8DE index), 0xB9950 turns each keyboard player's auto-aim off,
  0xDE720 holds each keyboard player's controls at style 0 wherever their avatar is, and nf::MenuMouse keeps
  running live for each machine's own player, its pad effects travelling inside the pad. The rumble a joiner causes
  plays on their own device. With no session every path is exactly the one it was: net::Active() gates them all.
- SDK: input::GameThreadPoll (set around XInputGetState and XInputGetCapabilities) tells GameInfo::pad which poll
  is the game's own thread, so a session's ticks advance on the game's polls alone and the device poller reads
  standing pads (src/common/game_info.cpp's NetPad).
- Evidence: net.cpp, game_info.cpp, menu_mouse.cpp, controls.cpp, mouse.cpp, input.cpp and xapi.cpp syntax-checked
  with the harness's own flags (mingw headers unavailable here; a stub image_symbols.h, no game on this machine, so
  no build and no headless run yet). Two real bugs the check caught on the way: a declaration in an if's second
  operand, and net:: resolving only inside namespace nf.
- Next: scripts/net-probe.sh - two headless machines, the same mp_bots pad script, paused at the same point, their
  whole memories compared - is the gate: the same input twice must be the same game twice, and anything that
  differs is a desync an online session would hit. Then two harness instances in one session over loopback
  (NF_NET=host / NF_NET=join,127.0.0.1, NF_NET_WAIT short, a pad script into the multiplayer lobby for two
  players), then Windows over a LAN and the Internet, tuning net_input_delay.

### 2026-11-04 - v1.1.0 drafted; v1.0.3 withdrawn

- Harry's test of the package: a launcher with NightFire-package.zip beside it, the USA image, over an install the
  v1.0.2 launcher had built from source: unpack 1.5 s, the game's code 19.5 s (66 files, 16 at a time), 21 s in all;
  the game ran from play (USA image accepted as the same code, into Driving.xbe and its mounted gun).
- Tag v1.1.0 on 52739bb: Actions run 41850674533 made the draft with the launcher. Harry removed v1.0.3's release, so
  v1.1.0's notes take its changes (driving at 60 on PAL, the settings window and options); the draft's text was set
  at the tag, so Harry pastes docs/releases/v1.1.0.md into it, attaches the package and publishes.

### 2026-11-04 - The prebuilt package (for v1.1.0)

- Harry: NightFire as PGR2 (1.6.0) and Agent Under Fire (1.0.0) ship: a package (runtime, exes, compiler, no game
  code), the player's PC compiling each program's code into a DLL from their disc. As AUF's opt-in (b6443c6,
  d317dcd, 290e7ef): XBE_GAME_DLL on Windows (CMakeLists.txt), [xbe] library Nightfire.dll / NightfireDriving.dll
  in nightfire.toml and driving.toml (the first start finds each exe's config by its xbe's name, case aside), one
  "package" in game.json (NightFire-package.zip: the USA and PAL discs run the same code, as PGR2's one package),
  release-package.bat (sdk/scripts/release-package.bat for Nightfire and NightfireDriving), the release workflow
  makes a draft. AUF's DLL-mode fix (a function declared X86_EXTERN_FUNC and overridden in one file) has no case here;
  every __imp__ our code calls is in the generated imports.def or exports.def.
- Linux: both recompile (exports.def, imports.def, hook_exports.h now written), smoke OK (driving 468 draws a frame).
  The DLL build is Windows-only: Harry to build, make the package and test it from an empty folder before the tag.
- README: the launcher flow (Get the game ready; old installs offered A faster way to play), the release's files, the
  package for maintainers. docs/releases/v1.1.0.md.

### 2026-11-04 - Release v1.0.3

- Harry: driving at 60 works on Windows; ship it with SDK 97b1654. docs/releases/v1.0.3.md; tag v1.0.3.
- Movies stay at their own rate (frame blending and interpolation declined); a movie scale option (4:3, fill, zoom)
  estimated at about a day, left for now.

### 2026-11-04 - Driving at 60 on the PAL disc

- Harry: driving 50 FPS (on foot 60), the same at render scale 100% and 200% and in v1.0.2, so not the GPU nor the new
  SDK. XBE_PERF: 50.0 fps flat, the renderer 6 ms a frame and idle the rest, the title thread 5 ms recording and
  waiting (one read-back a frame). Headless and unpaced Driving.xbe made exactly 50 frames a second: its own limit.
- Driving.xbe's rate 0x117DA0 is the refresh of the video standard [0x243540] from the table 0x1A204C (60, 60, 50 PAL-I,
  60); the frame timer takes it (0x10AE50 from 0x59920) and the steps are 1 / it (0x95380, 0xB49F0, 0x5CEB0). The USA
  disc runs the same code at 60. src/driving/refresh.cpp: for the call alone a PAL standard reads as NTSC-M, so 60;
  the movies and files still PAL. Headless unpaced: 62.5 a second (16 ms steps), so 60 paced. The mission clock fell
  31 s over 1800 frames in 31.6 s of real time: real time, not faster. Harry to check on Windows.
- 813f7ba went to main on a failed smoke (my push did not wait for the result): the driving window of 150 s ended
  inside paris2.mad, the movie after the tutorial, which now comes later in frames (the USA disc failed the same way).
  A 260 s run plays on into the chase and Mission Failed. SMOKE_NEXT_SECS 240: SMOKE OK on the PAL files (433 draws a
  frame) and the USA image (438).

### 2026-11-04 - SDK at the fork's main 97b1654

- The SDK session's request (Harry wants every game on it): a fast-forward of 55 commits from 37fa3dc, the 4831
  signatures untouched: the F1 window overhaul, SMAA, sharp scaling, alpha edges with MSAA, Enhanced world textures
  (off; F1 warns of hitches), low latency, the prebuilt-package launcher (XBE_GAME_DLL opt-in, unused here).
  recompile_Nightfire and recompile_NightfireDriving OK; rescans reproduce game.toml and driving_game.toml; smoke OK
  (driving 208 draws a frame); wincheck of src/ OK. Not yet played on Windows.

### 2026-11-01 - Release v1.0.2 (main ed66329, sdk 37fa3dc)

- Tag v1.0.2: Actions run 41246356420, NightFireLauncher.exe 2,338,816 bytes (sha256 cd85e43a...). Harry's call to ship; his
  graphics shots show MSAA, FXAA and sharpening running, but prompts, driving menu panels and snow on this SDK were
  not reported. Watch for wrong textures (D3D12 stage memo).
- TGP482's own NightFire port (V1, 2026-10-31) compared: ahead on uncapped frame rate, HDR, ambient occlusion, SMAA,
  texture replacement, gyro, 5.1; no licence, so ideas only. Our order if pursued: AO and SMAA in the SDK, small
  items, high frame rate last.

### 2026-10-31 - README: graphics settings compared, ultrawide, the menu, Discord

- Harry's 3440x1440 shots (Alpine level) on one spot: the view drifted between them (up to 73 px) and snow and guards
  moved, so each panel's crop is lined up to the default shot by a measured shift (squared differences around the
  crop); nothing else is altered. docs/img: wipe, edges, detail (the castle's windows), sharpness, menu (Harry had the ultrawide shot under Controllers removed).
  MSAA cleans edges, FXAA softens, render scale smoothest, sharpening 100% gritty on stone: confirmed by these shots,
  which also show MSAA, FXAA and sharpening running on Windows with NightFire.
- The wipe is animated as PGR2's: 800x520, 58 frames (50 ms, 600 ms at each end), a 1200x780 crop on the castle.
- README Graphics settings and Community and Support (Discord); v1.0.2 notes: the font, graphics settings.

### 2026-10-31 - SDK at the fork's main 37fa3dc, for v1.0.2

- Brings D3D12's texture stage memo keyed by the header's contents and repeat draws skipping lookups (PGR2's session:
  not exercised headless, not yet run with NightFire), and the launcher window's "about 3 GB". Smoke OK, wincheck OK.
- Next: Harry's Windows build before tagging v1.0.2: prompts keyboard/controller on foot and driving, fonts, driving
  menu panels (one texture, several palettes), snow, sharpening and MSAA/FXAA in F1.

### 2026-10-31 - README: about 3 GB free, plus the game

- Harry's decision, through PGR2's session: "About 3 GB free, plus room for your copy of the game" in both places,
  replacing this README's 15 GB figures (18bd768). When the 7.3 GB copy happens stays on the launcher's line. The
  launcher window says the same on fork main (reaches NightFire with its SDK and a new launcher).

### 2026-10-31 - README: install sizes

- Harry's request to every game (PGR2's fff8d0f). NightFire: disc image 7.3 GB (7,825,162,240 bytes, both discs), copied
  only when it cannot be hard-linked (core/builder.cpp), a folder linked; tools about 1.83 GB and source about 0.33 GB
  are PGR2's measurement of the same toolchain, not NightFire's; 1.4 GB download from a NightFire player's log. So
  about 10 GB or about 2.5 GB installed: estimates, not measured on a NightFire install. The 15 GB first build kept
  (build output on Windows not measured). The source build's "about 15 GB" and Remove the build files (not in this
  README) unchanged.

### 2026-10-31 - MinGit's busybox edition (launcher/toolchain.json)

- From PGR2's session: a PGR2 player's builds stopped at git submodule: MinGit runs it as a script under MSYS2's sh,
  whose forks died on their PC (0xC0000142), CMake's FetchContent too; the busybox edition fixed it there. NightFire
  takes the same entry (size and SHA-256 checked against the download; launcher_cli tool git installs it, cmd/git.exe
  there). Launchers read toolchain.json from main at each build, so no release; each reinstalls Git once. Not run on
  Windows here.

### 2026-10-31 - A player's build stopped on an exFAT-like drive (sdk c4000f8)

- A player's launcher.log (GitHub, v1.0.1, USA disc, E: drive): git "detected dubious ownership" on the launcher's
  own clone, each Build the same. Reproduced with GIT_TEST_ASSUME_DIFFERENT_OWNER=1 in launcher_cli.
- SDK c4000f8: BuildEnv sets safe.directory=* through GIT_CONFIG_COUNT/KEY/VALUE, for the launcher's git, submodules
  and CMake's FetchContent, the player's settings unchanged. With the forced error, a fresh clone and the folder
  that had failed both get through tag and submodule. CMake's FetchContent not run so (same environment).
- The pointer brings 15 commits of the fork's main (XDK 4039, vertex blending, MSAA and FXAA apart, ultrawide). Smoke OK
  for NightFire, PGR2, PGR1 (not EoN, nor PGR1's repro: the launcher is in none of their builds; those commits are
  their sessions' own, gated there); wincheck of src/ OK. Not played on Windows yet.
- docs/releases/v1.0.2.md.

### 2026-10-30 - v1.0.0's launcher offered no update (sdk 7f29c5f)

- Harry: his v1.0.0 launcher offered no v1.0.1; he updated by hand. Both published launchers carry their versions
  (v1.0.0, v1.0.1), and launcher_cli's check acting as v1.0.0 finds v1.0.1 and offers it: the logic holds. The
  check runs once at the launcher's start and dropped any error (offline: no news), so launcher.log could not say
  whether it ran before v1.0.1 was out (a window left open from the failed build) or got no answer.
- SDK 7f29c5f: the start-up check logs what it found (the latest release and whether it is offered, or why there was
  no answer). win/main.cpp checked against mingw's headers (a planted error is caught). Smoke OK for PGR2, PGR1 and
  NightFire. It reaches players with the next release's launcher.

### 2026-10-30 - v1.0.0's launcher built only default.xbe (sdk 66a7066)

- Harry, Windows 11, both discs: the launcher's build stopped preparing: "file SHA1 failed to read file
  .../source/assets/Driving.xbe" (XbeRecomp.cmake:83), then "NightfireDriving is not built by this project". The fork
  main's launcher wrote game.json's one "xbe" alone; the multi-xbe launcher (game.json "xbes" taken from the copy,
  "exes" installed into play: a33f5fa) was only on the fork's eon branches, which EoN's releases build from.
- SDK 66a7066: a33f5fa cherry-picked onto fork main (README conflict only). launcher_cli game + xbes on both images:
  default.xbe and Driving.xbe written with their SHA-1s (USA 7e4b8f8a/8d46a023, Europe 796222e0/e0c41209). Smoke OK
  for PGR2, PGR1 and NightFire (launcher code is in none of their builds; EoN ships this commit already).
- docs/releases/v1.0.1.md; the v1.0.0 launcher builds its own tag, so the fix reaches players as v1.0.1's launcher
  (its update offer).

### 2026-10-30 - Release v1.0.0 (main ef4d3e2)

- MIT License (as EoN's); the repository made public by Harry (GitHub reports visibility public, licence MIT).
- Tag v1.0.0 on ef4d3e2: Actions run 41035237588 built NightFireLauncher.exe (2,322,432 bytes, sha256 9b667c54...)
  and published https://github.com/hk21x/nightfire-recomp/releases/tag/v1.0.0 with docs/releases/v1.0.0.md.
- Harry: the wheel's scope zoom works and resets on leaving the scope.
- Next: Harry's run of the released launcher from an empty folder; reports from players.

### 2026-10-30 - The wheel's scope zoom resets when the scope closes (pc-features)

- Harry: zoomed with the wheel, the view stayed zoomed after leaving the scope, for any weapon. The step changed the
  camera's angle (+0x104), which is lasting game state the scope never touches: 0xAAF21's continuous zoom is a free
  camera's (flag +0x50 bit 0 of [+0xB8]), not the scope's. A weapon's zoom is the player's +0x860, copied into the
  camera's divisor (+0xF4) each frame by the first person camera (0xA8BC6); 0xB7190 eases it to the target +0x864,
  kept to 1 .. +0x868 (the weapon's most, at most 50), the zoom control (slot 6) multiplying the target while held;
  leaving the scope sets it back to 1 (0xB9BD1). A notch now multiplies the target by 1.25 (down by as much), within
  those limits, so the game's easing and reset apply. Smoke OK; Harry to check in a scope.

### 2026-10-30 - The wheel's scope zoom, again (pc-features)

- Harry: the wheel did not zoom the sniper scope. The scope's zoom is continuous: 0xAAF21 adds its zoom control
  (slot 6 through 0x6CA20) times the frame's time to the camera's angle (+0x104, kept to 0.2..3.14) and sets the
  camera (0x25190); a notch's 70 ms d-pad press moved it a few frames' worth at most. Now the input thread counts
  the notches through a scope (nf::ScopeZoom) and the look hook (0xA9CF0, the game's thread) steps the angle 0.8
  times a notch up (nearer), out no further than the scope opened at, setting it through 0x25190. Not seen
  headless (no scope there). Smoke OK.
- Repo check before going public: 65 files tracked, history (58 commits, all by Harry's noreply address) holds no
  game files, generated code, secrets or local paths beyond /home/harry folder names in PROGRESS; biggest blob the
  embedded font (OFL). .gitignore now also ignores *.iso and *.xbe anywhere.

### 2026-10-30 - Keys: 1 the next weapon, 2 the next gadget; Shift and Ctrl pitch in the driving missions (pc-features)

- Harry: on foot 1 switches the weapon and 2 to the gadget: 1 is now the d-pad's right (the next weapon, as the wheel)
  and 2 Black (the next gadget, with G); the d-pad's up and down keep the arrows alone (1 and 2 were theirs), left
  keeps 3. Driving: Left Shift is the left stick's up and Left Ctrl its down, beside the arrows (pitch: Deep
  Descent). Only the defaults change: keys bound anew in settings.toml stay. README tables.

### 2026-10-30 - The USA and Europe (En,Fr,De,Es,It,Nl,Sv) discs (pc-features)

- Harry's two more images (.local/variants, unzipped; extracted with an XDVDFS reader in the scratchpad). Europe
  (En,Fr,De,Es,It,Nl,Sv): the same xbes as the supported PAL release; smoke from the .iso OK on a PAL console.
- USA: its xbes differ in the header alone (signature and certificate, regions 1), so GameInfo::same_code takes them
  and the launcher's game.json lists "NTSC (USA)". Its certificate gives an NTSC console: it plays eurocom/30_fps
  (skip_intro now hides the 30_fps EA and MGM logos too) and driving/misc.viv's NTSC\eng movies. Headless from the
  extracted files and from the .iso: MGM logo, title, attract demo, START into Driving.xbe, the Paris intro, the
  sniper tutorial, the chase, Mission Failed. Smoke's driving window (150 s) ends inside the NTSC movie after the
  tutorial (one draw a frame), so it reads FAIL there; a 240 s run plays on (about 380 draws a frame).

### 2026-10-30 - Scope zoom on the wheel, Quit Game in the main menu (pc-features)

- Mouse wheel through a scope: while player 1's view is zoomed in (its angle under 0.9 of 60 degrees: nf::Zoomed, the
  look code's own test, now src/game/mouse.cpp's ViewZoom) the wheel presses the d-pad's up and down, the NightFire
  style's gadget/zoom (the keyboard plays that style), up a notch away; out of a scope it cycles the weapon as before.
  Not tried in a scope headless (no sniper there): which way zooms in is Harry's to check.
- Quit Game: a fifth main menu item (0x1000FC05, the Codenames item's records cloned as PC Settings'), the five rows
  26 apart from 222 to 326 (two rows up ran into the logo); A calls app::Quit, its help line "Leave the game."
  Headless: down four, A: "quit asked for", Exit (0).
- Driving menus' prompts: already Enter for Select in the pause and on Mission Failed (page 5; headless, keyboard):
  the 2026-10-29 fix covers them; nothing to change.

### 2026-10-29 - The launcher's update prompts (pc-features; sdk 0b5ff49)

- Fork main 0b5ff49 (launcher-update, merged by another session after its gate; launcher only): a release launcher
  knows its tag, offers a newer release (Update the launcher, What's new, Later), builds its own tag and offers a
  rebuild that keeps saves. The check is GitHub's unauthenticated API: a private repository gets no prompt.
  release.yml needs nothing (EoN's). My own gate run: PGR2 and EoN smoke OK; PGR1's collided with the PGR1
  session's run in the same trees (frames stopped), not counted.
- docs/releases/v1.0.0.md: an Updates section, today's features; README: modern buttons, Hide HUD, pause PC Settings.
  Smoke OK.

### 2026-10-29 - Modern button pictures, Hide HUD, scope on ultrawide, the driving exe quarantined (pc-features; sdk ed5e147)

- Modern button pictures (modern_buttons, on): a controller's prompts as today's pad (EoN's shapes, sized by each
  box): coloured A B X Y on dark discs, LB/RB for White/Black, LT/RT, Menu/View, d-pads, sticks; Driving.xbe's P8
  icons take the palette's nearest colours, its LT and RT boxes now the caps' full height (rows 111-130, 152-172).
  Headless: main menu, the bot match's CONTROLS tab, Enemies Vanquished's CONTROLS tab.
- Hide HUD (hide_hud): on foot each view's 2D sprite list (0xD9410 from the 2D pass 0xDAC20, the views 0x1F661C) is
  not drawn while no menu is up; driving, the HUD draw 0xE1970 (ecx [0x23F44C]) is skipped in play. Headless: bot
  match and Enemies Vanquished clean, their pause screens drawn.
- Harry's Deep Descent "crash": Windows Defender quarantined NightfireDriving.exe (Behavior:Win32/DefenseEvasion.A!ml),
  so the handover found no exe and closed. SDK 6eb3183: the new exe is no longer started suspended and resumed (the
  handover event is named by the old instance, --handover=pid); a missing exe gets a message box.
- Harry's scope capture on 21:9: the sight's black bars are textured (32x32, x 0-81 and 559-640, full height), so
  they were squeezed and the widened view showed past them. SDK ed5e147: GameInfo::edge_bars (NightFire on) keeps
  such a bar on the frame's edge, as an untextured fill. Windows renderers only: Harry to check.
- Gate passed (EoN, PGR2, PGR1 smoke, PGR1's frame check); fork main fast-forwarded to ed5e147.
- Harry on Windows: modern button pictures, Hide HUD and the scope on ultrawide all work.
- PC Settings from the pause screen (on foot): a level's menu file (stream 834CC5D0 in the bot match; pages 4B, 42,
  4C, 07, 0C, 44, 46, 2F, 21) has no AV Options page, so the front end's AV records are kept when its file is built
  and the page is made from them into the level's file, with the pause panel (0x10000001) behind it at 16,24
  608x432. The choices list (0x10000028 index 6, type 6: ATTR 0x11 texts 0x16A-0x16C) gets PC SETTINGS (a handle
  GetText answers) and room for four (y 176, h 160); A on it with 3 chosen (0x34) opens the page, B comes back.
  Headless: bot match, START, PC SETTINGS, the page, B, the pause screen again. Driving.xbe's pause (EA's menus)
  has none yet.

### 2026-10-29 - Harry's Windows checks

- Confirmed on Windows: mission loading screens whole (sdk 8acd824), PC Settings (resolution text and applying),
  mounted guns with the mouse, achievements, the icon, the Quit game button (F1), driving at 60 frames a second
  (the vertex buffer wait costs nothing seen).
- Keypads with the left mouse button / Enter and ultrawide confirmed too.
- Release notes written (docs/releases/v1.0.0.md). Next: the first release (launcher run, GitHub release), then polish (Hide HUD, modern button pictures,
  PC Settings in the pause menu, Quit Game in the game's menu, mouse prompts in the driving menus).

### 2026-10-29 - Icon, README, launcher name (pc-features)

- Harry's NightFire PC Port artwork: the round logo as launcher/icon.ico (16 to 256 px; the launcher's and both
  exes' icon), the wide one as the README's banner (docs/img/nightfire-pc-port.webp, 1280x720).
- README rewritten from EoN's: supported release (both SHA-1s), launcher and source builds, PC Settings, on-foot and
  driving keys, features, status, troubleshooting, credits (Sour Gummy, OFL).
- launcher.bat still had the template's short name (Game): now NightFire, so it copies NightFireLauncher.exe.

### 2026-10-29 - Mission loading screens whole on Windows (pc-features; sdk 8acd824)

- Harry's F3 capture of a Driving.xbe loading screen (left half black): the picture is 64x64 P8 tiles on stage 3,
  and the first draws carried later tiles' corners (draw 1 at 384,384; headless, draw 1 is at 0,0). Headless the same
  buffer (00260C40) draws at x 64 and later at x 512 in one frame: EAGL refills its ring of three buffers a tile
  (0xF6E90) within the frame, after a lock that blocks in D3D_BlockOnNonSurfaceResource 0x16CE90, which nothing
  named; its wait (BlockOnTime) returns at once, so the render thread read the refilled buffer.
- SDK 8acd824: D3D_BlockOnNonSurfaceResource hosted as BlockUntilNotBusy (wait for the render thread when the
  buffer is still in a batch), 4831 signature from driving.toml (rescans reproduce both configs; default.xbe has none),
  a placeholder address for other builds. About 155 calls a frame in a race; they wait only on a buffer still in
  flight. Headless (no render thread) it is reached from 0xF6E90; NightFire smoke OK. Gate passed (EoN, PGR2, PGR1
  smoke, PGR1's frame check: a live race); fork main fast-forwarded to it.
- Next: Harry to check a mission loading screen, and the frame rate in a driving mission (a wait per refilled buffer).

### 2026-10-29 - Keypads on the keyboard: the left button and Enter confirm (pc-features)

- Harry: a door's keypad took its digits with Ctrl. Its confirm is the pad's A, which the keyboard has as crouch
  (C, Ctrl, the NightFire style's A). Where Bond is not the player's to move on foot (nf::PlayerFree: 0xA9CF0 has not
  turned player 1 for two frames, as for the guided missile; no menu up), the left mouse button and Enter press A too
  (src/common/menu_mouse.cpp) and A's picture is the left button (src/common/buttons.cpp). Not tried at a keypad
  headless (none reached); the bot match's play and smoke OK.

### 2026-10-29 - PC Settings: the resolution's text (pc-features)

- Harry's screenshot: Resolution showed "WUff". The guest copies of the page's strings were found again by their host
  pointer; the sizes' texts lived in a growing vector, so a new text could take an old one's address and get its copy.
  Now by contents, the sizes' texts in a deque, and every string and the menu file copy in AllocHostContiguous memory
  (AllocSystem's pages are not all readable by recompiled code). Smoke OK.

### 2026-10-29 - PC Settings: display mode, resolution, vertical sync, frame cap (pc-features)

- Harry: add display mode, resolution, VSync and the frame cap to the page. Eleven rows do not fit AV's ten, so the
  page is now made from AV's records (nf::PcPage): its page widget, title, prompt line, help box and pictures kept, its
  rows dropped, ours stacked 24 apart from y 80 from three templates (the music slider with its grey track, the
  subtitles choice, the Restore Defaults button); focus moves by position. Choices carry their values (Display Mode
  the runtime's DisplayMode numbers; Resolution "Desktop" 0 and the monitor's sizes 1.., as the settings window
  lists them; Frame Cap 30, 60, 120, 144, 240, Unlimited). Kept: the settings file, and ApplyDisplay when the mode,
  size or aspect changed. Headless: Frame Cap 60 -> 120 and Display Mode Borderless -> Fullscreen kept; the Edit
  Codename route still opens it. Smoke OK.

### 2026-10-29 - PC Settings as a page of the game's own menus (pc-features)

- Harry: the PC settings in the game's menus, as an option of Edit Codename (Codenames, a profile, its list).
- src/game/pc_settings_page.cpp: the AV Options page (0x40000031) cloned into the menu file copy the builder reads
  (pc_settings_item.cpp), its page and row widgets given our ids (0x4000FC10, 0x1000FC11..), its labels ours: Field of
  View and Mouse Sensitivity (the volume sliders, 21 steps), Invert Mouse, Skip Intro Movies, Sharp Text, Aspect Ratio
  (the choices), More Settings (Restore Defaults' button: the settings window); the rest hidden as AV hides its own.
  Driven through the dispatcher 0x8E320 as AV's handler 0x8D7A0 drives its own (0x4C entered: sliders 0x27/0x69/0x2E,
  choices 0x17, 0x10 text and value, 0x1E by value; 0x4B: read 0x40/0x35, kept to the settings file, back 0x5F;
  0x4E: the row's help line).
- Edit Codename's list (table 0x17CEC8, 7 entries) gets an eighth: its range widened as it is made (0x8C7F0, 0x51:
  0x27 0..7), the rows drawn from a copy of the table (0x7F680 called from 0x8C7F0's hook: its own direct call bypasses
  a hook of 0x7F680) with ours at 7 (label and help through a GetText hook, AV's picture), A on it opens the page. The
  copy lives in AllocHostContiguous memory: AllocSystem's pages are not readable by recompiled code (a fault at
  0xB00F30C8); before the list is made its index is a pointer the game wraps, so the copy is used for real indices
  only. kernel::CallGuest returns the guest's eax (the context is restored): reading ctx.eax after gave stale values.
- The main menu's PC Settings item opens the page too. Headless (Harry's save): Codenames -> Default -> the list's
  eighth entry "PC Settings" with its help -> the page; field of view +3 steps and Invert Mouse on kept ("PC Settings
  kept: field of view 115%, ... invert 1"). Smoke OK.

### 2026-10-29 - Achievements (pc-features)

- The progress map (agent, .local/progress_re.md; checked against Harry's decoded save and a faked Enemies Vanquished
  handover, .local/launch_ev_done.bin): FINDINGS.md "Progress". Deltas on the profile cannot tell a codename load
  from a completion (the profile is restored and the completion scored with no frame between), so each achievement is
  the Results page with mode 6 and the level done, plus its own term.
- src/common/achievement_set.cpp: the 12 missions, gold and platinum on any mission, gold and platinum on every
  mission's best, any mission and Equinox on 00 Agent, every Bond Move in a mission (ACHIEVEMENTS.md). Driving.xbe
  gets the list with a never-met condition (shown, not awarded).
- Headless: the faked Enemies Vanquished handover unlocked Enemies Vanquished, Gold Standard and Shaken, Not Stirred
  (Harry's gold there, its Bond Moves); title -> codename BOND -> mission list unlocked none. Smoke OK.

### 2026-10-29 - Driving font on Windows: replacements for linear textures (pc-features; sdk ed1796c)

- Harry's car screenshot: the objective, timer and ammo still in EA's font, though they draw from the replaced
  82E2E580. D3D12's and D3D11's BindGuestTexture took a title's replacement only for a non-linear texture; LIN_A8's
  coordinates are normalised by the guest's size (texsize), so a larger picture reads the same, and A8 decodes to
  white with alpha as the picture is. SDK ed1796c drops the condition. Gate passed (EoN, PGR2, PGR1 smoke, PGR1's
  frame check; none of them sets a replacement on a linear texture); fork main fast-forwarded to it.
- fonts.cpp: Driving.xbe treats any A8 / LIN_A8 texture with EA's glyph table behind it as a font (the table's size
  from the texture's), remembering those that are not. Smoke OK.

### 2026-10-29 - Driving.xbe's font from Sour Gummy Black too (pc-features)

- EA's font file holds its glyph table before its kerning pairs and the pixels (Driving.xbe font 82E2E580: records at
  82E2D080, 12 bytes each, code order: code u16, width, height, x u16, y u16, advance, x offset, y offset from the
  line's top; 191 codes, the printable Latin-1; then the kerning pairs' count 747 and the pairs, "Buy ERTS", the
  pixels). fonts.cpp's EaTable finds it from the texture's data (back to the 0xFF record whose pairs end just before
  the pixels, then down to the space); DrawEaFont draws each code into its rectangle on the line's baseline (the H's
  bottom), sized to the H, fitted; EA's ligature codes (^ | ~ thorn: ffi ffl ff fl) and much-shrunk glyphs keep the
  game's. Headless Paris: "Driving.xbe's font drawn from Sour Gummy Black". Smoke OK.

### 2026-10-29 - Driving pause prompts: Enter for Select, the Scroll icon (pc-features)

- Harry's car pause screenshot: Select showed the mouse (A's play key) and Scroll a stray mark. The atlas's d-pad
  cross was two pictures (a disc with up and down lit at 220,117-239,136; arrow pieces with up and down lit at
  222,137-241,156), boxed as one; now two. Driving.xbe's nf::MenuKeys: a menu up but the pause's CONTROLS tab (the
  HUD's +0x470 == 2) shows Enter and Backspace. Headless car pause: "ENTER Select, ESC Continue, arrows Next and
  Scroll". Smoke OK.

### 2026-10-29 - default.xbe's font from a real font: Sour Gummy Black (pc-features)

- Harry asked for a real font in place of the upscale and picked Google's Sour Gummy Black (SIL OFL 1.1; the static
  Black from upstream eifetx/Sour-Gummy-Fonts, third_party/fonts with OFL.txt; src/common/font_data.cpp its bytes,
  since the headless harness builds src/ alone). stb_truetype (public domain, third_party/stb) draws it.
- default.xbe's 512x64 font: three rows of slots (columns with a clear one between: 63, 62, 25), row 0 ASCII '!' to
  ']' (the menu's "S" at u 399/512 matches), row 1 '`', a-z and Latin-1, row 2 more Latin-1 (ligatures, quotes and a
  box keep the rebuilt mask; no glyph table was found in memory: none of widths, x as bytes, shorts or floats).
  DrawFont: one size (the game's H), each row's baseline (its H, x, a-grave), centred in the slot, narrowed to fit,
  smaller where it would leave its band (ascenders), the game's glyph where that is under 3/4 (accented capitals).
  NF_FONT_DUMP shows the sheet; a line laid out from it on the game's baselines reads cleanly. Driving.xbe's font
  keeps the smooth rebuild (its own layout). Smoke OK.

### 2026-10-29 - Fonts: stairs on shallow curves ironed out (pc-features)

- Harry (Windows screenshot, the objective text): still a bit stepped. Scale2x fixes 45 degree stairs only; shallower
  ones kept bumps through the Catmull-Rom field. Now Scale2x twice, then a Gaussian half a source texel wide over the
  4x mask (separable), then bilinear 2x cut at 0.5 a pixel wide (src/common/fonts.cpp). NF_FONT_DUMP: "/" one clean
  stroke, "0" a smooth oval, the letters' shapes kept. Smoke OK.
- Harry asked about drawing a real font in place of the upscale: possible (stb_truetype, an OFL font like the game's,
  each glyph fitted to its cell and baseline); not started.

### 2026-10-29 - Keyboard prompts in Driving.xbe; clean fonts in both exes (pc-features)

- Driving.xbe's prompts (the pause CONTROLS tab's icons, REF_DRAWLOG draws 513-518) come from its HUD/menu atlas, a
  256x256 P8 (swizzled indices; FNV-1a 0x96C6096C3CB2BF53) the game binds with three palettes; the icons' palette is
  the one whose entry under the A icon is green. src/common/buttons.cpp now has a layout per atlas: the Xbox
  pictures' boxes (from the atlas drawn with that palette), drawn as keys and coded to that palette's nearest
  colours, at the atlas's size (no replacement: it would not follow the palettes). On a mounted gun RT and LT show
  the mouse's buttons and the reload button R; in the car the right stick (look back) its key. Headless car pause,
  CONTROLS: Fire the left button, Handbrake SPACE, C, Gadget the right button, Steer W A S D, S, Q, W, Z.
- Fonts (src/common/fonts.cpp): default.xbe's 512x64 A8R8G8B8 and Driving.xbe's 256x108 LIN_A8 are pixel art; the
  runtime's sharp 2D (interpolation) left stepped rims. Rebuilt 8x: the mask through Scale2x twice (straight strokes
  kept, stairs made diagonals), then a Catmull-Rom field cut at its middle a pixel wide; the colour the nearest
  opaque texel's; the title's replacement (gpu::SetReplacement) wins over the sharpened one. NF_FONT_DUMP=folder
  writes each as built. Headless: both rebuilt (default 8286A000, Driving 81CDBC80 and 82F50180).
- Smoke OK.

### 2026-10-29 - Keyboard prompts on foot: the button pictures show the keys (pc-features)

- Harry: prompts should show the keys, as in EoN. default.xbe draws every prompt (menus' help lines, the pause
  CONTROLS tab, hints) from one 256x128 A8R8G8B8 swizzled atlas with the PS2 and GameCube pictures too (FNV-1a
  0xF0D6752BB8C56E3B, loaded again per level at another address). src/common/buttons.cpp (EoN's buttons.cpp and
  strokes.h adapted): while the keyboard plays, the Xbox pictures' boxes are drawn as each button's first key (in
  the front end's menus A as ENTER, B as BKSP: nf::MenuKeys; a level's pause shows the keys played, which its
  CONTROLS tab lists), the triggers and White as mouse buttons, the d-pads as arrow keys, the left stick W A S D, the
  right the mouse; written to guest memory and 4x for the renderers (gpu::SetReplacement). The HUD binds the atlas
  once, so it is also redrawn from each poll (RefreshButtons) when the look changes.
- The keyboard's style override (style 0) now holds while the keyboard plays, so the game's controls page and
  prompts name the buttons the keys play; a controller puts the chosen style back, and 0x6ED30 (a player's controls
  packed for the profile and launch data) sees the chosen style.
- Headless: the main menu's "Scroll / Select" as the arrow keys and ENTER; the bot match's CONTROLS tab with every
  picture a key or mouse button (its style text was the controller's: pad script presses are a controller's).
- Driving.xbe: its prompts are glyphs of its LIN_A8 256x108 font tinted by vertex colour (the CONTROLS tab's icons),
  not a picture atlas: not done yet.
- Smoke OK.

### 2026-10-28 - PC controls in Driving.xbe: the mounted guns, the car, the menus (pc-features)

- Harry: the on-foot controls did not reach the vehicle missions (Alpine Escape's snowmobile turret, the Paris van's
  gun, the car). Reverse engineering in .local/driving_input_re.md (angles in turns; the control files are text in
  driving/misc.viv: data\control\povcfg0-7.def for the guns, drivecfg.def for the car).
- Mounted guns (src/driving/mouse.cpp): the gun camera's update 0x857D0 (fastcall) adds the stick's rates to +0x1EC
  yaw and +0x1E8 pitch (up a smaller pitch) and clamps them (0x83190). The mouse's counts go into both before the
  call, the stick's part (+0x290/+0x294, +0x2A0/+0x2A4, ramp +0x1F0/+0x1F4) zeroed; slower zoomed (+0xB4 over 33
  degrees) and a quarter again. Auto-aim [0x2445D0] (default.xbe's option through the launch data, read only by
  0x82E00 from 0x857D0) is off for the call and its lock let go. Headless Alpine Escape with MOUSER: the turret turns.
- Car or gun: the control map's mode [0x1E4560] (1 gun, 0 car), the gun's style [0x1E4568] (nf::GunStyle). On a gun
  the keyboard's car buttons are taken off: the left button fires (RT), the right zooms (LT), R reloads (X in style 0,
  else A); the wheel cycles weapons (its style's controls). In the car: the keys from its controls page (W gas, S
  brake, A/D steer, mouse left fire, Space handbrake, E/mouse right gadget, C camera, Q/Z weapons, V look back), the
  wheel Black/White.
- Menus: the HUD's byte [[0x23F44C]+0x2B0] while an in-game menu takes input (nf::MenuUp): the mouse in Paused and
  Mission Failed. Headless Paris: pause and resume logged.
- mouse_direct in both exes now (NF_DRIVING_EXE dropped). Smoke OK.

### 2026-10-28 - Driving.xbe trap in the car mission (pc-features)

- Harry (Windows, "Enemies Vanquished", 117 s in): "indirect call to 00081D10 has no recompiled function" from
  0x8783F: a state's update (+0x14 of the 0x18-byte entries at [0x1EC368], stored by 0x78770 at 0x79405), reached
  only through that table. Added to config/driving.toml [functions]. A sweep of both xbes' code for function
  addresses stored as immediates (mov [reg+d], imm; push imm) with no recompiled function found no other.
- Headless with Harry's save (.local/hdd_harry, from his hdd.zip; codename BOND): default.xbe's NightFire item ->
  mission list -> Enemies Vanquished wrote .local/launch_enemies_vanquished.bin; 5 minutes of driving with the gas
  held (to a wreck and "Mission Failed"): no trap.
- The car's own controls page (pause, CONTROLS): A fire, B handbrake, Y camera, X gadget, d-pad left/right cycle
  weapons, left stick steer, L brake/reverse, R gas, Black/White weapon forward/back, right stick look back.
- Next: Driving.xbe's controls for the keyboard and mouse (the mounted gun's aim, auto-aim, car vs gun layout, the
  menus): being reverse-engineered (.local/driving_input_re.md).

### 2026-10-28 - Guided missile steered by the mouse; auto-aim off with the mouse (pc-features)

- Harry: a guided missile flew straight (WASD and the mouse did nothing); auto-aim locked onto chests.
- Steering something else, the player's movement update 0xA9CF0 does not turn player 1, so the direct mouse had
  nothing to drive and the right stick got nothing. After 0xDE720, two frames without 0xA9CF0's turn (and no menu
  up), the mouse plays the right stick in the controls (style 0's slots 0, 3 x and 4, 5 y, -1 to 1; 20 counts a
  frame full). Not tried headless (no guided missile in the bot match); the log says "the mouse plays the right stick".
- Auto-aim: the game's option is the byte at +1 of the player's controls (0x1FE6D1; the options page 0x8D0F0 widget
  0x1000011B; +2 in the mode 0x260018 flags); its target search 0xB9950 runs with it. While the keyboard and mouse
  play, 0xB9950 sees both off for the call (the option as set; homing weapons 'P' and 'Q' still lock on).
- Smoke OK; the bot match's direct mouse unchanged.

### 2026-10-28 - On-foot keys from the game's own controls page; the weapon wheel; zoomed mouse a quarter (pc-features)

- Harry: the mouse is excellent; zoomed in (sniper, rocket launcher) it wants a quarter the speed; E for action,
  C or Ctrl crouch, the wheel for weapons, R reload.
- The game's controls page (pause, CONTROLS tab, headless in the bot match) for every style; the style number is
  player +0xE (Classic Bond 7, GoldenEye 6, Thunderball 5, Dr. No 4, Goldfinger 3, Octopussy 2, Moonraker 1,
  NightFire 0). NightFire (the keyboard's): A crouch, X reload/action, Y jump, B next weapon, White alt. fire, Black
  next gadget, L manual aim, R fire, Back vision mode, d-pad left/right cycle weapon, up/down gadget/zoom. The old
  keys were from a guess (Space was crouch, E jump).
- Keys: C/Ctrl crouch, E/R action and reload, Space jump, Q/Backspace next weapon, middle button/F alt. fire, G next
  gadget, V/Tab vision mode, arrows/1-4 the d-pad; the wheel cycles the weapon in play (down the next), Enter selects
  in the menus (menu_mouse.cpp).
- Mouse zoomed in (the camera's angle over its zoom divisor under 0.9 of 60 degrees): a quarter again; the log says
  "zoomed in" and "zoomed out" so Harry's log shows the test works.
- Headless (KEYS, MOUSER, WHEELD in the bot match): the style override (7 -> 0) and the direct mouse logged. Smoke OK.

### 2026-10-28 - The mouse turns the view itself on foot; on-foot play headless; the snow (pc-features)

- Harry: the mouse as the right stick is no FPS mouse (dead spots, a ramp). default.xbe takes the mouse itself now
  (GameInfo::mouse_direct; NF_DRIVING_EXE keeps Driving.xbe on the stick), src/game/mouse.cpp:
  - The movement update 0xA9CF0 (the actor) writes the frame's turn at actor +0x28: radians added to the heading
    next step (right negative), after an acceleration filter (0xB7420) and the stick squared. The look update 0xB7610
    adds to the pitch at +0x838 (camera pitch = pi/2 times it) and sets +0xF0 = 1 (looked; without it the view
    eases back level). After 0xA9CF0 for player 1 the mouse's counts go straight into both, 0.0008 radians a count
    at 100% sensitivity, scaled by the camera's field of view over 60 degrees (the scope).
  - Headless: NF_MOUSE_TEST=100,60,600,619: the camera turned 0.08 a frame and pitched 0.048 a frame to 0.96 and held.
  - The dead zone rewrite (0xE76A0) is gone: the mouse no longer plays the stick on foot.
- Found by probing: getter 0x6CA20 (player, slot): slot k is the float at 0x1FE6E4 + player * 0x158 + 4k (0 turn,
  1 and 2 moving, 3-5 looking); style 0 fills the turn from the right stick. The headless Multiplayer profile turns
  with the left stick (not style 0), so the keyboard's style override stays.
- On-foot play headless: scripts/pad/mp_bots.txt (Multiplayer, one bot, Skyrail) from launch_quit_to_menu.bin.
- The snow (Harry's F3 captures): one fixed-function point-sprite draw of 4096 points with its texture on stage 3,
  stage 0 holding the last draw's (ferns, spheres, stripes). SDK branch nightfire-pointsprite: such a draw uses stage 3
  as its first stage (the Xbox gives point sprite coordinates to stage 3 alone). Gate passed (EoN, PGR2, PGR1 smoke,
  PGR1's frame check; PGR2's point sprites all have a pixel shader, PGR1 and EoN draw none): fork main ca1d0f9.
- Smoke OK.

### 2026-10-28 - The mouse past the game's stick dead zone (pc-features)

- Harry: dead spots in the mouse movement. default.xbe's poll 0xE76A0 makes each stick axis a float, -100 to 100
  (0x1620E0), takes a dead zone off it and stretches the rest (0xE79CF..); slow mouse moves fell inside it. While
  the keyboard and mouse play, controller 1's right stick floats are remade after the poll from the XInputGetState
  shorts, linear, the runtime's 20% mouse start taken off (src/game/controls.cpp). Sensitivity: the overlay's
  mouse sensitivity (10-1000%); the game's top turn rate stays its own.
- Smoke OK. Next: Harry: slow aiming, and the snow captures (F3).

### 2026-10-28 - Keyboard and mouse always as the NightFire controller style (pc-features)

- Harry (Windows): the field of view works; on foot the mouse strafed and moved Bond about, and A/D did not strafe.
- default.xbe's 0xDE720 reads the pads into each player's controls by the player's controller style (u16 at +0xE of
  a 0x158-byte player, port byte at +0x156): style 0 (NightFire) takes moving and strafing from the left stick,
  turning and looking from the right; 2 and 5 (Octopussy, Thunderball by the options' order) swap the x axes. The
  keys and mouse are laid out for style 0, so while controller 1 is on the keyboard and mouse its player is read as
  style 0 for the call (src/game/controls.cpp; the chosen style is logged); a controller keeps its style.
- Smoke OK. Next: Harry on Windows: WASD moves and strafes, the mouse turns and looks, in his save's style.

### 2026-10-28 - PC Settings in the main menu, menu mouse, sharp 2D, ultrawide and field of view (pc-features)

- PC Settings: a fourth main-menu item under NightFire / Multiplayer / Codenames, in the game's own font and focus
  (src/game/pc_settings_item.cpp). Eurocom's menus are a record stream ([0x25FB08]) the page builder 0x93960 reads:
  the builder gets a copy with the Codenames item's records cloned under our id (0x1000FC01) and label, the items moved
  up a row; A on it (dispatcher 0x8E320, message 0x4B) opens the overlay; its help line is set after 0x959E0.
  Headless: shown, focused with its help line, A logs "PC Settings opened from the main menu".
- Mouse in the menus (EoN's MenuMouse): left A, right B, the wheel the d-pad while a menu is up. default.xbe tells
  by Eurocom's menu context (player 1's page widget, 0x25F38C); Driving.xbe's EA menus are not found yet (never up).
- sharpen_2d on (text and HUD sharp at high resolutions; the overlay's sharp_2d turns it off).
- Ultrawide and fov_percent (50-150, both exes; .local/fov_re.md has the reverse engineering):
  - default.xbe (src/game/view.cpp): the D3D projection 0xDD950 and the culling planes 0xDB5C0 get the camera's
    aspect (+0x100) times the frame's width over 16:9 and its angle (+0x104) scaled, for the call alone (the scope's
    zoom and a cutscene camera read those fields back); 0x25190's pixel scales (+0xEC/+0xF0: HUD markers, aim ray)
    follow the field of view, not ultrawide (the HUD is squeezed). Not run headless: no on-foot level is reached
    (the Paris pad ends at "Mission Failed").
  - Driving.xbe (src/driving/view.cpp): 0xE46D0's horizontal angle widened in tangent and its aspect with it (the
    height unchanged); the field of view scales the game's multiplier [[0x1EBFF4]+0x58] around 0x96EB0 and the HUD
    markers 0xCE2B0. Headless Paris: NF_FOV=130 shows more of the scene; NF_ULTRAWIDE=2 (a headless override) gives a
    picture half as wide at the same height, as a 32:9 screen stretches it back. squeeze_screen_programs on: every
    driving draw is a vertex program, the HUD's 640x480 quads too.
- Smoke OK (147 draws a frame).
- Next: Harry on Windows: PC Settings from the main menu, the menus with the mouse, the layout against the game's
  control labels, aspect = fill at 21:9 or 32:9 (edge pop-in, the HUD squeezed, aim at the edges on foot:
  0xB7EB0 keeps 16:9), the field of view; the log should say "out of the menus" in play.

### 2026-10-28 - PC features begun: skip intro, mouse look, keyboard layouts (pc-features)

- skip_intro (settings, NF_SKIP_INTRO=1 headless): default.xbe's EA (25_fps/07370048.xmv) and MGM (07380048) logo
  movies reported missing through GameInfo::hide_file; the game passes over them and "Press START" is up at 15 s
  instead of 26 s; the language select (07350048, its background) stays.
- Mouse look: GameInfo::mouse_look (the mouse plays the right stick, both exes). Keyboard layouts per exe as EoN's
  (kDriving in each hooks.cpp; Driving.xbe's rebinds kept as [driving_keys]): on foot WASD, mouse fire/aim, Space,
  Ctrl, R, E, F, Q, 1-4 the d-pad, Esc/P start, Tab back; driving W accelerate (RT), S brake/reverse (A), A/D steer.
  The game's own control labels (text bank 0): Fire, Jump, Alt. Fire, Crouch, Reload/Action, Look/Strafe, Vision
  Mode, Cycle Weapon, Cycle Gadget/Zoom, Manual Aim, Next Weapon/Gadget, Pause: the layout is to be checked against
  them in game.
- The main menu's text: GetText 0x6D460 (handle = bank << 24 | n, bank bases at [[0x1FEC78] + bank * 4], the
  table at [0x215588]); main menu handles 0x148 NightFire, 0x149 Multiplayer, 0x14A Options (not shown: a hidden
  item), 0x14B Codenames. The menu itself is data-driven; its items are being traced (.local/menu_re.md), as are
  the projection and culling of both engines for ultrawide and FOV (.local/fov_re.md).

### 2026-10-28 - The SDK branch in fork main (main; sdk 41ac590)

EoN's smoke had not failed: its default length is 240 s and 150 s ends inside its intro movie (EoN's session; the
note was wrong). Gate on the nightfire tip: EoN main 2146a11 with its SDK bfbdc0a merged with nightfire SMOKE OK
(race 11736 draws a frame, /home/harry/eon-soak); PGR2 SMOKE OK (2007); PGR1 SMOKE OK (2811) and its frame check a
live race (.local/mm/after.png, draws 6156269 -> 6242839 a second apart). The recompiler change regenerates PGR2's
and PGR1's code byte for byte. Fork main fast-forwarded 0ade542 -> 41ac590 (14 commits); this repository already
pins it.

Next: PC features: skip intro, widescreen, FOV, mouse aim and menus, PC settings in the game's own menu.

### 2026-10-28 - Windows: a long session clean (main)

Harry: "a fair bit of gameplay, all worked OK": 469 s in default.xbe (his saves; the on-foot side), no errors, 60
frames a second paced, slow frames only on shader compiles (1 to 5 a 10 s window, longest 214 ms at boot).

Next: the SDK branch into fork main once EoN's smoke works again (then the launcher builds NightFire); later
missions; PC features (widescreen, FOV, skip intro) as the other ports have.

### 2026-10-28 - default.xbe trapped at 0x11234A: an instruction past the raw data (main; sdk nightfire)

Harry's next run (with his saves, so default.xbe stays in its menus) trapped after 39 s: "undecodable instruction"
at 0x11234A, from the game's 0xE83A8 (D3DX 0x111FC0, a matrix inverse). The D3DX section's raw data ends at
0x11234C, its virtual size at 0x112350: the function's last instruction, ret 0xC (C2 0C 00), has its last byte in
the zero filled tail the loader maps. sdk xbe_recomp: Decode pads with the tail's zeros, and an instruction that
starts in code may run into its own section's tail. Forza's FZFULLUI had the same trap (0x3A9AEA); PGR2, PGR1 and
EoN had none, so their code does not change. Smoke OK.

### 2026-10-28 - Menu panels: Driving.xbe's SetPalette (main; sdk nightfire)

Harry's rebuild: no crash; the scope shows the view inside it now (dim) but not yet as it should, and the menu
panels (Paused, Mission Failed) were black and white noise, also headless. Their textures are P8 (format 0x0B).
0x1669E0 stores a palette in the device's slot [+0xB48 + stage * 4] and pushes 0x41B20 + stage * 0x40 itself (EoN's
QFE 4 SetPalette does the same): D3DDevice_SetPalette, unnamed, so the host never had a palette (and logged
"MakeSpace from 00166A4E"). Named: the Mission Failed panel draws right headless. Smoke OK.

Next: the scope on Windows after this build (the readback of 2026-10-28 made the view show; brightness and the
noise pattern still to compare with the console).

### 2026-10-28 - Leaving Paris crashed; the sniper scope black (main; sdk nightfire)

Harry's Windows run reached the end of the first mission, then crashed: an access violation in the heap's free
(0x110BA6 from RtlFreeHeap) through D3DResource_Release 0x169230 from the game's teardown 0xE8600. Reproduced headless
by quitting a failed mission to the menu (render thread on or off). gdb on the releases: the object is the host's
depth buffer (header 0x00050001: one reference, no internal one), and library code at 0x16D23D had dropped the
device's hold: the frame buffer teardown 0x16D180 inside Driving.xbe's D3DDevice_Reset 0x166230, unnamed, so the
library's Reset ran on the host's buffers. Named (PGR2's 0x1C38D0 at 0.96): the host Reset runs, and the quit now
relaunches into default.xbe, which comes up on the main menu. Smoke OK.

Visual: the sniper scope is black (both renderers). Probes and traces at the scope's centre: draw 158 multiplies
the view by a texture that is near black, the 128x128 target 0x80F07000, a decaying accumulator a black quad dims
each frame and 16384 points (128 x 128) add to. The points' colour stream is the memory of the other 128x128
target 0x80F17000, which the pass before drew from the back buffer: the console reads those texels as vertex
colours; here the GPU's pixels never reached guest memory, the points added zeros and the accumulator stayed black.
sdk: before a draw each vertex stream's data is read back when the GPU drew it, and the D3D12/D3D11 read-backs
write 32 bit swizzled targets in Morton order (they skipped every swizzled one). Headless cannot show it (the null
backend has no GPU targets); wincheck and smoke OK. The icon-atlas frame in Harry's captures runs the same passes.
Ruled out on the way: linear texture coordinates as 0..1 (experiment: no change, the HUD text broke).

Next: Harry rebuilds and checks the scope, the atlas frame and the end of the mission.

### 2026-10-28 - Windows: the first mission was black; the gamma ramp (main; sdk nightfire)

Harry's first Windows build (D3D12, RX 7900 XT) boots, plays the movies and the title, relaunches into Driving.xbe
and then shows black in the first mission. His F3 capture (.local/win) has the frame drawn right (final.bmp: the
van's sniper view), so the picture was lost after the draws. The device's gamma ramps in his memory dump are zeros,
and the game's own base ramp at 0x23FC08 too: Driving.xbe reads the current ramp once with the library's
D3DDevice_GetGammaRamp 0x165E70 (copies [device + 0x259C + slot * 0x300]) and every fade is that ramp scaled
(0xE7740: base * scale + 0.5, then SetGammaRamp). The host never put a ramp in the device: its SetGammaRamp goes to
the GPU only, and the console's linear ramp was never there, so every fade was black. The reference renderer has no
gamma, so headless never showed it. sdk: CreateDevice writes the linear ramp into both slots and SetGammaRamp keeps
them current. Headless the saved base is now linear (0, 32 ... 224); smoke OK.

Next: Harry rebuilds (build.bat updates the sdk) and checks the mission shows.

### 2026-10-28 - Ready for a first Windows build (main; sdk nightfire)

- Gate on the branch: PGR2 smoke OK (2009 draws a frame), PGR1 smoke OK. EoN smoke fails at 1 draw a frame (the
  intro movie in1_palw.xmv never ends) on fork main's SDK, on its own pinned SDK 9e5900b and merged with this branch
  alike, in /home/harry/eon-soak and in EoN's own tree: an EoN problem of its own, so fork main is not moved yet.
- Windows checks: scripts/wincheck.sh clean; scripts/shadercheck.sh compiles all 41 shaders the two xbes made (4 in
  default.xbe's boot, 37 in the Paris level), 0 failed.
- build.bat is EoN's (both exes); the sdk submodule is the pushed nightfire branch's commit, so a clone builds
  without fork main. The launcher builds from fork main and cannot build this game until the merge.

### 2026-10-28 - Pad scripts and a smoke test across the two xbes (main; sdk nightfire)

PAL boot (frames every 120, no input): language select 120..600 (it moves on by itself), the EA and MGM movies to
1200, the title from 1440, "Press START" from about 1560 (24 draws a frame). scripts/pad/smoke.txt: A at 300, START
at 2000; the relaunch follows at about 34 s.

Driving.xbe, Paris (scripts/pad/paris.txt, vblanks from its own start): the sniper tutorial from about 1500 passes
on RT every 40 vblanks with auto-aim; its closing movie leads (about 5100) into the Aston Martin's chase. RT
accelerates; A brakes and reverses (A+RT reversed into the camera). Unsteered, the car meets the barriers and the
mission fails to its menu at about 7200 (A there replays the tutorial). Movie decoding makes frame times vary
between runs, so presses after a movie are timed loosely.

sdk: smoke.sh takes a second xbe (SMOKE_NEXT_GEN/NAME/PAD/SECS): the first run must relaunch, the next xbe runs on
its disk and launch data and gets the drawing checks; SMOKE_SECS. NightFire: SMOKE OK (title 24 draws a frame,
Paris 146); without START it fails ("no relaunch"); PGR1's smoke unchanged on the branch.

Next: steering through the chase (the harness's cmd file takes presses live), PGR2 / EoN smoke and PGR1's frame
check, then the branch into fork main and a Windows build.

### 2026-10-27 - The Paris mission plays headless (main; sdk nightfire)

After the intro movie Driving.xbe stopped three times, each an XDK function only it links:
1. D3D 0x16B160 read the current pixel shader's definition ([[pDevice]+0x370]+8, null with hosted shaders):
   D3DDevice_SetPixelShaderConstant (its opening is PGR2's 0x1C27C0 instruction for instruction, ret 0xC).
2. DSOUND 0x17BE1B -> 0x17BAF4 copies a DSI3DL2LISTENER: IDirectSound_SetI3DL2Listener.
3. 73 s in, a 0x15000 byte contiguous allocation failed with 64156 KB in use. XBE_MEM_LOG=2 (new, sdk) summed the
   live blocks by caller: the game's own 36 MB heap (0x10E818), its D3D resources (454 blocks, 6.9 MB), its render
   targets, and 10 MB the host made: three swap buffers and the depth buffer, as a console makes, plus a copy target
   (1.2 MB) and the DrawVerticesUP ring (4 MB) no console has. sdk: physical memory is mapped to 128 MB as on a
   development kit; the title's allocator and statistics keep the console's 64 MB and the host's own buffers (copy
   target, UP ring, scratch push) come from the part past it. Then 200 s of the mission without a trap. PGR1's
   smoke passes on the branch.

Names signed into sdk/xdk/4831 from both hand configs (.local/resign.sh); both rescans reproduce every hand name.

Next: a pad script through the mission (and the front end: START at the title went straight into mission 1 with the
"Default" profile), a smoke test across the two exes, then PGR2 / EoN smoke and PGR1's frame check before the
branch goes into fork main; a Windows build for Harry.

### 2026-10-27 - XDK 4831 named; title screen; Driving.xbe reached and plays its intro movie (main; sdk nightfire)

Named with three parallel read-only passes (D3D functions and data, the D3D layout, DSOUND), each against PGR2's 5659
bodies (instruction sequences with numbers masked, callees followed) and checked on NightFire's own instructions;
XAPI/XPP by hand. Evidence is in config/nightfire.toml, sdk/xdk/4831/layout.toml and .local/match/result_*.txt.
- D3D: 51 functions by body, render state slot and push methods (byte-identical setters told apart by slot); the
  data (pDevice 0x1117C0, RenderState 0x1119D0, PresentInterval 0x111BCC, ...) found by both D3D passes alike.
- Layout: 5659's way on every 3948/5659 difference (state numbering, Swap, frame buffer pointers, present interval
  global, D3DPRESENT_PARAMETERS to +0x40, D3DVBLANKDATA); device size 0x2C30, miniport at +0x23C0. Spot-checked.
- DSOUND: 41 functions by their chains to the leaf (4831 has no .XTLID). The scan had SetMixBins/SetMixBinVolumes
  swapped. Mix bins as 5659, headroom as 3948.
- Signed into sdk/xdk/4831 from default.xbe (137 of 252); both rescans reproduce the hand names. Driving.xbe's 100
  shared library names compared with default.xbe's by call chain (all 0.97 or more).

Boot, one stop at a time:
1. XInitDevices unnamed: the USB driver touched 0xFED00000. Named (0x15753D) with XInputOpen/Close, XDEVICE_TYPE_GAMEPAD.
2. Direct3D_CreateDevice unnamed: 0xFD001804. With the D3D names: 120 s without a trap, the title screen (REF frames).
3. START launches driving.xbe: EA's front end and driving levels are that xbe, so it is on the main path. Its XPP
   table init 0x183BF4 needed `[functions]` as default.xbe's did; DirectSoundCreateBuffer 0x17C2A0 (default.xbe does
   not link it) and IDirectSoundBuffer_SetFilter 0x17B5E0 (4831's Driving.xbe DSOUND has it, default.xbe's not) named
   by hand after APU faults.
4. Driving.xbe then hung after 60 frames in a stream loop (0x14C800): EA's file layer opened
   `NTSC\eng\paris_intro.mad`, which is not on the disc, and a failed open leaves the stream busy for good. The movies
   are in driving/misc.viv under `pal\<language>\`: the video standard default.xbe passes in the launch data
   (offset 0xE1C) was NTSC. `GameInfo::pal_console` (as EoN): default.xbe then shows the language select and the
   three XMV intro movies, which needed D3DDevice_GetRasterStatus hosted (sdk: the XMV player read the CRTC
   register 0x600808 through the GPU base); its launch data now says PAL and Driving.xbe plays the Paris intro movie.
   Tried first: disc reads paced at 6 MB/s (60 -> 240 frames, not the cause), OBJECT_PATH_NOT_FOUND for a missing
   folder (no change, reverted).

Tools: .local/drv.sh (recompile and run Driving.xbe from .local/launch_title_start_pal.bin), .local/drvstack.sh
(thread stacks of a hang), .local/tools/{seq,align,ident,xmatch,chain}.py (matching).

Next: the crash after the Paris movie in D3D 0x16B160 (Driving.xbe, reading [null + 8]; from game 0xF44A0).

### 2026-10-27 - Repository set up; first scans and first boot attempt (main; sdk nightfire)

Chosen from the two discs dumped today: NightFire is fully extracted and has the same two-xbe structure as EoN, and
its console game differs from the Gearbox PC port. (True Crime: New York City's extract stopped before default.xbe;
its default.xbe from the .iso is XDK 5849 with LTCG D3D8, 116 of 278 functions on a first scan: a later candidate.)

Made from `sdk/templates/game` with EoN's two-xbe wiring: targets `Nightfire` (default.xbe, src/game, config/
nightfire.toml) and `NightfireDriving` (Driving.xbe, src/driving, config/driving.toml), `GameInfo::images`, both exes
in launcher/game.json and release.bat, the workflow title quoted. LESSONS.md collects what PGR2, EoN, PGR1 and Forza
learned.

SDK (branch `nightfire`):
- `xdk/4831`: no signatures existed. Trial scans seeded from each build: 3948 23 functions, 5659 54, 5849 55 (5849
  has more names, the same hits); seeded from 5659 as PGR1 was. layout.toml is 5659's, marked unverified.
- xbe_symbols: default.xbe's scan named 0x15B5D5 XInputGetCapabilities by constants (0.60) though it pops 4 bytes,
  not 8, and wrote an `[abi]` placeholder the recompiler refuses. 0x15B5D5 is an XPP device type's callback (the
  descriptor 0x15731C's +8), not XInputGetCapabilities. Now, in an image without LTCG, a constants match whose ret
  size or entry registers differ is not taken. PGR2, PGR1, EoN (both) and Forza rescans are byte-identical.
- ghidra.sh: `GHIDRA_PROGRAM=driving.exe` reads a second xbe's program.

First headless runs (default.xbe, none.txt):
1. 0.024 s: indirect call to 0x157334 from 0x15757B, which walks XPP's device type table 0x157214..0x157220
   (descriptors 0x1572D8, 0x1572F0, 0x15731C; +4 init, +8 and +0xC callbacks). Only 0x157334 had no function:
   `[functions]` in config/nightfire.toml.
2. Segfault at guest 0xFED00000, the USB host controller's registers: XInitDevices is 0x15753D (allocates the 0xB4
   byte host object, runs the table's inits, starts the controller; stdcall, 2 arguments) and was unnamed, so the
   library's driver ran. The host's XInitDevices needs XPP_GamepadType and the XPP_Current/Changed/Previous layout
   keys, which are still 5659's: naming it belongs to the 4831 pass.

Push buffers (sdk/scripts/ghidra PushBufferScan): all 226 NV2A method stores are in library code, none in the game's
.text, so the game writes no commands by hand. The 0x20001 store in D3D 0x109790 is the lazy state flush's method
value, not a push buffer resource. Whether the game records or runs push buffers stays open until D3D is named.

Recompile: default.xbe 4640 functions (357 direct, 740 by gap fill, 3925 overlapping decodes); the 31 "code
addresses not made functions" at 0x40290-0x403A0 are NV2A method headers in D3D immediates, not code.

Next: Plan 1, the 4831 pass. Name D3D, DSOUND, XPP (XInitDevices 0x15753D first) in default.xbe against PGR1's 3948
and PGR2's 5659 in Ghidra, re-derive the layout, sign, rescan both xbes; then settle push buffers and boot.
