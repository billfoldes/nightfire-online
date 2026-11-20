#!/usr/bin/env bash
# What moves the pause menu's cursor, asked of the game itself (docs/online.md): one offline machine boots, plays
# the Paris tutorial into the chase, pauses - and then each candidate button is pressed in turn through the
# harness's cmd file (read live by the pad script), a screenshot kept after each. The images say which button,
# if any, moves the cursor; every theory about this menu has been a guess so far. START opens the pause but
# d-pad, X and the left stick have all left the cursor on CONTINUE.
#   scripts/net-menu-probe.sh     (~2.5 minutes; the images in .local/menu-probe-run/probe_*.tga)
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
[ -d generated/game ] || { echo "net-menu-probe: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
[ -d generated/driving ] || {
  echo "net-menu-probe: generating the driving code (once)"
  cmake --build build/linux --target recompile_NightfireDriving || { echo "NET-MENU-PROBE FAIL: cannot generate generated/driving"; exit 1; }
}
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)"
CXX="$CXX" bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || exit 1
CXX="$CXX" bash sdk/harness/build.sh -r . -g generated/driving -o .local/harness -n NightfireDriving -G .local/harness/gen-driving -x '/src/game/' || exit 1

RUN=.local/menu-probe-run
rm -rf "$RUN"
mkdir -p "$RUN"
PROBEPAD=$(realpath scripts/pad/menu-probe.txt)
EXE=$(realpath .local/harness/game)
GAME=$(realpath assets)
(
  cd "$RUN"
  export SCRIPT="$PROBEPAD" NF_SKIP_INTRO=1
  exec "$EXE" "$GAME"
) > "$RUN/run.out" 2>&1 &
pid=$!

echo "net-menu-probe: the pause opens about 97 s in; probing from 110 s"
sleep 110
touch "$RUN/snap"
sleep 3
cp "$RUN/snap_1.tga" "$RUN/probe_0_baseline.tga" 2>/dev/null
try() {  # $1 = the cmd's buttons, $2 = the image's name
  echo "$1 120" > "$RUN/cmd"
  sleep 5
  touch "$RUN/snap"
  sleep 3
  cp "$RUN/snap_1.tga" "$RUN/probe_$2.tga" 2>/dev/null
  echo "net-menu-probe: $2 tried - probe_$2.tga"
}
try "LSD" 1_left_stick_down
try "RSD" 2_right_stick_down
try "DOWN" 3_dpad_down
try "X" 4_x
try "UP" 5_dpad_up
try "Y" 6_y
kill $pid 2>/dev/null
wait 2>/dev/null
echo
echo "the images, in order: $RUN/probe_*.tga (0 is the menu before any press)"
echo "which one first shows the cursor off CONTINUE - or none of them?"
