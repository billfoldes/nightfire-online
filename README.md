# 007: NightFire Online

007: NightFire's Xbox multiplayer and campaign, played online: the console's split-screen matches and the campaign
in co-op, with every player on their own PC.

> [!IMPORTANT]
> **Built on Harry's [nightfire-recomp](https://github.com/hk21x/nightfire-recomp).** The PC port itself (the
> recompiled game, its graphics, sound, controls, PC settings and achievements) is his work; NightFire Online adds
> online play on top of it. For the single-player port alone, and for its controls and graphics options, see his.

> [!NOTE]
> No game files are included. You need your own copy of the game: the European (PAL) or the USA Xbox disc.

## What NightFire Online adds

- **Online multiplayer** - the console's own matches (its modes, maps and bots, up to 10 players and bots in all)
  with each player on their own PC, full-screen. Every PC runs the same match from everyone's input (lockstep), so
  no game state is sent ([how it works](docs/online.md)).
- **Campaign co-op** - up to 4 players through the on-foot missions, each with their own view and name tag; a
  downed player respawns beside a teammate, and only the last one down fails the mission. The driving missions with
  a second player on the car's guns, torpedoes and turrets. More and tougher enemies for each extra player, and
  friendly fire, all on by default and the host's to change.
- **Server Browser** - games on your network and on the internet list, with their ping, players and map.
- **Join codes and a relay server** - a host gets a short code to share; no port needs forwarding and no player sees
  another's IP address.
- **Hosting** - who can join (anyone, only people you tell, or no list at all) and a password; a player who drops can
  take their seat back; the host's QUIT takes everyone back to the menus together; and **Restart the session for
  everyone** for a session that is stuck.
- **Your name and character** - shown to the other players, and a streamer mode that hides typed names and IP
  addresses.

## Getting Started

The launcher sets the game up on your PC from your own disc: no Visual Studio, no Git and no command line, nothing
installed in Windows and no admin rights needed. A release holds the runtime and a compiler, and none of the game's
code: that is compiled on your PC from your disc.

**You need:**
- Windows 10 or 11, 64 bit, and a GPU with Direct3D 12 or Direct3D 11.
- Your own copy of the game (PAL or USA): an `.iso` image of your disc, or a folder with the disc's files.
- Room for your copy of the game and an internet connection. Your copy takes 7.3 GB when the launcher has to copy
  your disc image (from another drive, or on a drive such as exFAT); nothing more when it can link to the image on
  the same NTFS drive, or to a folder of the disc's files.

**Steps:**
1. Download `NightFireOnlineLauncher.exe` from the [latest release](https://github.com/billfoldes/nightfire-online/releases/latest).
2. Put it in a folder of its own (for example `C:\Games\NightFireOnline`) and run it. Everything it makes goes in a
   `NightFireOnline` folder beside it, so it does not touch an install of Harry's port. Windows may warn about an
   unrecognised app: choose **More info**, then **Run anyway**.
3. **Your game:** choose your disc image or the folder with the disc's files. The launcher checks that it is the
   supported release.
4. Press **Get the game ready**. The launcher downloads the package and compiles the game's code from your disc.
5. Press **Play**. When a new version is out, the launcher offers **Update**: every player in a game needs the same
   version.

Everything is under the `NightFireOnline` folder: `play\` holds the game, its `settings.toml`, `hdd\` (saves) and
`logs\`; `launcher.log` is the launcher's log. To remove it all, delete the launcher and that folder (keep
`play\hdd` if you want your saves).

## Playing Online

Online play is set up in the settings window: press **F1** in the game. A session starts before the game boots, so
hosting or joining restarts the game to its connection screen; every PC then boots together.

**Hosting a game** (F1, **Online**):
1. Tick **Host online play**.
2. **How players reach me:** *A join code through a relay* for players on the internet (no port forwarding, no IP
   addresses shown), or *My IP address (direct)*, the quickest way, for players on your network or with the UDP port
   (26565) forwarded on your router.
3. **Players:** how many to wait for, 2 to 10. A match adds the bots the game's bot menu asks for, up to 10 in all.
4. **Who can join:** *Not listed* (only your network's Server Browsers see it, and anyone you give the code or
   address to), *Anyone* (in every Server Browser), or *Only people I tell* (listed with a lock: they need your code
   or password).
5. Press **Restart online play**. The connection screen shows your join code (**C** copies it) and the players as
   they arrive. The game starts once everyone is in, or press **Enter** or **Start** to start with fewer.

The co-op rows under it (Friendly fire, More enemies, Tougher enemies) and **Everyone quits with the host** are the
host's, for every player, and change during a game. **Restart the session for everyone** is there for a host whose
session is stuck: every PC restarts and meets again, and progress since the last save is lost.

**Joining a game** (F1, **Server Browser**): pick a game and press **Connect**, or type a host's join code or address
in the **Address or join code** box and press its **Connect**. The game restarts and joins. Joining switches itself off after that session, so the next
start plays alone.

**About you** (F1, Online): **My name**, **My character** and **My health handicap** reach the others at once or at the
next match; **Streamer mode** hides typed names and IP addresses on your screen.

**Through a relay, straight when it can:** a game by join code starts through the relay, then each joiner and the host
try to reach each other directly (**Connect directly when possible**, under Servers, on by default). When both have it
on and their routers let it through, the match's packets go straight between them, with no relay distance added, and
the relay only carries the games that need it. Each PC then knows the other's IP address (it is still never shown).
Either one turning it off keeps their packets on the relay.

**Hosting by IP address:** the game asks your router to forward its UDP port (**Open my port on the router (UPnP)**,
under Advanced, on by default). Routers with UPnP switched off need the port forwarded by hand.

### Self-hosting a list or relay

The Server Browser's list and the relay are one small Python program, `tools/master/master.py` (a Cloudflare Worker
can serve the list too). Anyone can run them on a VPS or an always-on PC, and every copy of the game learns of new
ones from the lists it already knows: see [tools/master/README.md](tools/master/README.md).

## Building from Source (developers)

**You need:** Windows 10 or 11 (64 bit), [Git](https://git-scm.com/download/win), Visual Studio 2022 (Community is
fine) with **Desktop development with C++** (`build.bat` offers to add **C++ Clang tools for Windows** and **C++ CMake
tools for Windows**), about 3 GB free, and your disc's files.

1. Clone the repository (the SDK is in it, in `sdk\`):
   ```
   git clone https://github.com/billfoldes/nightfire-online.git
   cd nightfire-online
   ```
2. Put your game files in the `assets` folder: `assets\default.xbe` with `Driving.xbe` and the game's folders
   (`eurocom`, `driving` ...) beside it, or your disc's `.iso`.
3. Run `build.bat`, then `out\build\release\Nightfire.exe`.

On Linux, `scripts/build-win.sh` cross-compiles the Windows executables, and `scripts/release-package.sh` then makes a
release's package (the runtime and the compiler, no game code) in `.local/release`. A `v*` tag builds the launcher on
GitHub and drafts its release; the package is attached to it before it is published. Game code is never uploaded.
Progress is in [PROGRESS.md](PROGRESS.md), and how online play works in [docs/online.md](docs/online.md).

## Troubleshooting

- **A problem online:** open an [issue](https://github.com/billfoldes/nightfire-online/issues) with the game's log from
  `NightFireOnline\play\logs` from every PC in the game if you can.
- **Can't connect by IP address:** players on the internet need the host's UDP port (26565 by default) forwarded on
  its router; a join code through the relay needs no forwarding.
- **The launcher failing:** send `NightFireOnline\launcher.log`. Press the button again after a network problem: it
  carries on where it stopped.
- **"The game could not start NightfireDriving.exe"** when a driving mission starts: Windows Security may have
  quarantined it (Protection history: allow it on your device); get the game ready again with the game closed.
- **A short stutter** the first time a new area or effect appears: its shaders are being compiled, once.
- **A first boot again:** delete `NightFireOnline\play\hdd`. This deletes your saves.

## License

The port's own code is under the MIT License ([LICENSE](LICENSE)), Harry's and this project's; the SDK and the
third-party parts under theirs (`sdk/LICENSE`, `sdk/THIRD_PARTY.txt`, `third_party/`). The game is not included: its
code is recompiled from your own disc on your PC.

## Credits

- Harry's [nightfire-recomp](https://github.com/hk21x/nightfire-recomp), the PC port this project is built on: the
  recompiled game, its PC features and the launcher.
- [Xbox XBE Recomp](https://github.com/TGP482/XboxXBE-Recomp), the recompiler and runtime it is built on.
- [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) for the overridable function layout and instruction hooks.
- [Zydis](https://github.com/zyantific/zydis) for decoding x86 instructions, [toml++](https://github.com/marzer/tomlplusplus) for the configs, [Dear ImGui](https://github.com/ocornut/imgui) for the settings overlay and [SDL](https://github.com/libsdl-org/SDL) for the window, input and controllers.
- [Sour Gummy](https://github.com/eifetx/Sour-Gummy-Fonts) by the Sour Gummy Project Authors, under the SIL Open Font
  License (`third_party/fonts/OFL.txt`), and [stb_truetype](https://github.com/nothings/stb) for drawing it.
