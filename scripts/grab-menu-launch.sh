#!/usr/bin/env bash
# The main-menu launch data, made by playing the game (thirty seconds of your own hands beat every scripted
# attempt at its scripted content). Start this, then play normally: boot the game, press START at the title,
# pause the first level, QUIT TO MAIN MENU. The moment the game hands that menu to its next exe it writes the
# launch page, and this catches it (it lives for under a second) - then quit the game so its disk is quiet and
# press Enter, and the disk beside the page is taken too. net-loopback uses both to boot both machines straight
# to the main menu.
#   scripts/grab-menu-launch.sh <your game's folder (the one with Nightfire.exe and hdd)>
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
DIR=${1:?usage: scripts/grab-menu-launch.sh <your game folder, the one with Nightfire.exe and hdd>}
[ -d "$DIR" ] || { echo "grab-menu-launch: no folder $DIR"; exit 2; }
mkdir -p .local
echo "grab-menu-launch: watching $DIR for the relaunch's launch page"
echo "play the game now: boot, START at the title, pause the first level, QUIT TO MAIN MENU"
for i in $(seq 1 7200); do  # twelve minutes of watching, at 50 ms
  f=$(find "$DIR" -name launch.bin -print -quit 2>/dev/null)
  if [ -n "$f" ]; then
    cp "$f" .local/menu-launch.bin
    echo "grab-menu-launch: the launch page is captured (.local/menu-launch.bin)"
    read -p "now quit the game completely so its disk is quiet, then press Enter: "
    d=$(dirname "$f")
    rm -rf .local/menu-hdd
    cp -r "$d" .local/menu-hdd
    echo "MENU-LAUNCH OK: .local/menu-launch.bin and .local/menu-hdd (from $d)"
    echo "scripts/net-loopback.sh boots both machines straight to the main menu with them now"
    exit 0
  fi
  sleep 0.05
done
echo "grab-menu-launch: twelve minutes, no launch page came - is that the right game folder?"
exit 1
