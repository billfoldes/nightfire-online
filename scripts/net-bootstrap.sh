#!/usr/bin/env bash
# Makes the main-menu launch data, once (the upstream dev's .local/launch_quit_to_menu.bin, made rather than
# borrowed): one OFFLINE machine boots the game, picks its language, lets the front end start the campaign's
# first level (scripts/pad/bootstrap-quit.txt pauses it and quits to the main menu), and the launch data page
# that relaunch writes is what boots the game straight to the main menu. net-loopback seeds both machines with
# it and its saved disk, and no run ever needs the title, the front end or the campaign level again.
#   scripts/net-bootstrap.sh     (~2 minutes; needs the game in assets/, generated/game, generated/driving)
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
[ -d generated/game ] || { echo "net-bootstrap: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
[ -d generated/driving ] || {
  echo "net-bootstrap: generating the driving code (once)"
  cmake --build build/linux --target recompile_NightfireDriving || { echo "NET-BOOTSTRAP FAIL: cannot generate generated/driving"; exit 1; }
}
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)"
CXX="$CXX" bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
  echo "NET-BOOTSTRAP FAIL: the game harness did not build"; exit 1
}
CXX="$CXX" bash sdk/harness/build.sh -r . -g generated/driving -o .local/harness -n NightfireDriving -G .local/harness/gen-driving -x '/src/game/' || {
  echo "NET-BOOTSTRAP FAIL: the driving harness did not build"; exit 1
}

RUN=.local/bootstrap-run
rm -rf "$RUN"
mkdir -p "$RUN"
BOOTPAD=$(realpath scripts/pad/bootstrap-quit.txt)
EXE=$(realpath .local/harness/game)
GAME=$(realpath assets)
(
  cd "$RUN"
  export SCRIPT="$BOOTPAD" NF_SKIP_INTRO=1
  exec "$EXE" "$GAME"
) > "$RUN/run.out" 2>&1 &
pid=$!

# the relaunch writes the launch data page and the next boot consumes it within moments: watched at 50 ms,
# each generation snapshotted as it appears (generation 0: into the front end; generation 1: the main menu)
gen=0
for i in $(seq 1 4000); do
  kill -0 $pid 2>/dev/null || break
  if [ -f "$RUN/hdd/launch.bin" ] && ! cmp -s "$RUN/hdd/launch.bin" "$RUN/last_launch.bin" 2>/dev/null; then
    cp "$RUN/hdd/launch.bin" "$RUN/launch_$gen.bin"
    cp "$RUN/hdd/launch.bin" "$RUN/last_launch.bin"
    echo "net-bootstrap: launch data #$gen captured"
    gen=$((gen + 1))
  fi
  [ $i -eq 2400 ] && touch "$RUN/snap"   # at two minutes: the reference renderer's next frame, what screen it is on
  sleep 0.05
done
kill $pid 2>/dev/null
wait 2>/dev/null

if [ "$gen" -lt 2 ]; then
  echo "NET-BOOTSTRAP FAIL: $gen launch data page(s) captured, the quit's menu handoff never came"
  echo "(the flow, from the run's logs:)"
  grep -E 'a menu is up|of the menus|relaunching into|HalReturnToFirmware|hidden|open d' "$RUN/logs/"*.log 2>/dev/null | head -12
  tail -4 "$RUN/run.out" 2>/dev/null
  ls "$RUN"/snap_*.tga 2>/dev/null | sed 's/^/(the screen at two minutes: /' | sed 's/$/ )/'
  exit 1
fi
cp "$RUN/launch_1.bin" .local/menu-launch.bin
rm -rf .local/menu-hdd
cp -r "$RUN/hdd" .local/menu-hdd
echo "NET-BOOTSTRAP OK: .local/menu-launch.bin boots the game straight to the main menu"
