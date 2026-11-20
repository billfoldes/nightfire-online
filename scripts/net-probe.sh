#!/usr/bin/env bash
# Online play's determinism probe (docs/online.md): two headless machines, the same pad script into a Multiplayer
# Arena match with a bot (scripts/pad/mp_bots.txt), the game paused at the same point, and their whole memories
# compared. A session runs the same match on every machine from the same input stream (src/common/net.cpp), so the
# same input twice must be the same game twice; anything that differs here is a desync waiting for an online match.
#   Usage: scripts/net-probe.sh [seconds, default 240; at least 240, the pause is at about 195]
#   Needs the game in assets/ and its code generated: scripts/linux-tools.sh, scripts/recompile.sh.
# The pad script starts from the main menu: each run folder gets .local/launch_quit_to_menu.bin as its launch.bin
# (a run's hard disk with that launch data, as scripts/harness.sh's KEEP_HDD=1 keeps it).
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)   # absolute: the script cd's to the repo root below
cd "$HERE/.."
SECS=${1:-240}
if [ "$SECS" -lt 240 ]; then echo "net-probe: give it at least 240 s (the pause is at about 195)"; exit 2; fi
[ -d generated/game ] || { echo "net-probe: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
PAD=.local/net-probe-pad.txt
RUN=.local/run
mkdir -p .local
cp scripts/pad/mp_bots.txt "$PAD"
echo "11600 START 20" >> "$PAD"  # mp_bots.txt's last press is 11100 A: the match is under way, START pauses it

CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-PROBE FAIL: the harness did not build (the error above)"
    exit 1
  }
[ -x .local/harness/game ] || {
  echo "NET-PROBE FAIL: no harness exe at .local/harness/game"
  exit 1
}

# two machines, each its own run folder; a launch-data page each, and instance 2's hard disk instance 1's
mkdir -p "$RUN/1" "$RUN/2"
if [ -f .local/launch_quit_to_menu.bin ]; then
  cp .local/launch_quit_to_menu.bin "$RUN/1/launch.bin"
  cp .local/launch_quit_to_menu.bin "$RUN/2/launch.bin"
else
  echo "net-probe: no .local/launch_quit_to_menu.bin; the pad script starts from the main menu, so it may not reach the match"
fi
[ -d "$RUN/1/hdd" ] && [ ! -d "$RUN/2/hdd" ] && cp -r "$RUN/1/hdd" "$RUN/2/hdd"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PADOBJ=$(realpath "$PAD")
pids=()
for k in 1 2; do
  (
    cd "$RUN/$k"
    export INSTANCE=$k SCRIPT="$PADOBJ" NF_SKIP_INTRO=1
    exec "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
  pids+=($!)
done

echo "net-probe: two machines playing the same match; pausing and comparing at $((SECS - 10)) s"
sleep $((SECS - 10))
touch "$RUN/1/memdump" "$RUN/2/memdump"  # the next frame each, paused: the same standing state
sleep 10
kill "${pids[@]}" 2>/dev/null
wait 2>/dev/null

report() {  # what to compare: a machine's newest memory dump, physical (the game's) or the whole virtual space
  ls -t "$RUN/$1"/mem_*_phys.bin 2>/dev/null | head -1
}
a=$(report 1)
b=$(report 2)
[ -n "$a" ] && [ -n "$b" ] || {
  echo "NET-PROBE FAIL: no memory dump (the runs' logs: $RUN/{1,2}/logs)"
  for k in 1 2; do
    echo "--- machine $k, $RUN/$k/run.out:"
    tail -5 "$RUN/$k/run.out" 2>/dev/null
    echo "--- machine $k, the log's last lines:"
    tail -5 "$RUN/$k/logs/"*.log 2>/dev/null
  done
  exit 1
}
if ! command -v python3 >/dev/null; then
  echo "NET-PROBE FAIL: python3 is needed for the comparison (scripts/net-diff.py)"
  exit 1
fi
rc=0
python3 "$HERE/net-diff.py" "$a" "$b" || rc=$?
if [ "$rc" -eq 0 ]; then
  echo "NET-PROBE OK: the same input twice is the same game twice ($(md5sum "$a" | cut -d' ' -f1))"
elif [ "$rc" -eq 2 ]; then
  echo "NET-PROBE OK except the audio ring: the same input twice is the same game twice"
elif [ "$rc" -eq 3 ]; then
  echo "NET-PROBE OK in the known classes (the ring, the ambient drift - docs/online.md): the same game twice"
else
  echo "NET-PROBE FAIL: the two machines' games differ (a desync an online session would have hit; docs/online.md)"
  exit 1
fi
