#!/usr/bin/env bash
# The driving co-op test: two headless Driving.xbe machines in one session on Alpine Escape's mounted gun (src/driving/
# coop.cpp). Driving.xbe runs headless only from launch data default.xbe wrote: LAUNCH_HDD is a run folder's hdd
# whose launch data starts the mission (default .local/lvl5/1/hdd). Player 1 fires once (the shot the others copy),
# player 2 holds the trigger with their aim pushed right; the machines' views of the game (net::CheckState) must agree
# 600 (AFTER) ticks after player 2's first shot.
#   scripts/net-driving-gun.sh [seconds, default 100]
#   HOST_ENV="NF_SNAPSHOT_SAVE=1500,/tmp/s" JOIN_ENV="NF_SNAPSHOT_LOAD=1500,/tmp/s": the joiner takes the host's state
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-100}
HDD=${LAUNCH_HDD:-.local/lvl5/1/hdd}
[ -d "$HDD" ] || { echo "net-driving-gun: no launch data at $HDD"; exit 2; }
[ -d generated/driving ] || { echo "net-driving-gun: no generated/driving"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/driving -o .local/harness_next -n driving -G .local/harness_next/gen -x '/src/game/' || {
    echo "NET-DRIVING-GUN FAIL: the harness did not build"
    exit 1
  }
RUN=.local/driving-gun-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/pad"
if [ -n "${HOST_PAD:-}" ]; then cp "$HOST_PAD" "$RUN/pad/host.txt"; else printf '4000 RT 20\n' > "$RUN/pad/host.txt"; fi  # HOST_PAD=<file>: player 1's
printf "${JOIN_PAD:-4100 RT 2400\n4100 RSR 20\n}" > "$RUN/pad/join.txt"  # JOIN_PAD='\n': a baseline, no shots
GAME=$(realpath assets)
EXE=$(realpath .local/harness_next/driving)
for k in 1 2; do
  cp -r "$HDD" "$RUN/$k/hdd$([ $k = 1 ] || echo $k)"  # a second instance's disk is hdd2 (app/main.cpp)
  [ -n "${DUMP_TICKS:-}" ] && echo "$DUMP_TICKS" > "$RUN/$k/dumptick"  # earlier dumps too: "100,400,..."
  pad=$(realpath "$RUN/pad/$([ $k = 1 ] && echo host || echo join).txt")
  net=$([ $k = 1 ] && echo host || echo join,127.0.0.1)
  (
    cd "$RUN/$k"
    [ $k = 2 ] && [ -n "${DISC_RATE_2:-}" ] && export XBE_DISC_RATE=$DISC_RATE_2  # machine 2 loads slower
    export INSTANCE=$k SCRIPT="$pad" NF_NET=$net NF_NET_WAIT=30 NF_NET_PLAYERS=2 NF_NET_DELAY=2 NF_NET_PORT=${NET_PORT:-26583}
    # HOST_ENV / JOIN_ENV: "NAME=value ..." for one machine alone
    exec env $([ $k = 1 ] && echo "${HOST_ENV:-}" || echo "${JOIN_ENV:-}") "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
done
log() { cat "$RUN/$1"/logs/*.log 2>/dev/null; }
for i in $(seq "$SECS"); do log 1 | grep -q 'player 2 fires the mounted gun' && break; sleep 1; done
at=$(log 1 | grep -m1 'player 2 fires the mounted gun' | sed 's/.*session tick \([0-9]*\).*/\1/')
at=$(( ${at:-0} + ${AFTER:-600} ))  # AFTER ticks (ten seconds) of the gun on: a tick both machines reach
echo "$at" > "$RUN/1/dumptick"
echo "$at" > "$RUN/2/dumptick"
for i in $(seq 60); do [ -f "$RUN/1/mem_tick_$at.bin" ] && [ -f "$RUN/2/mem_tick_$at.bin" ] && break; sleep 1; done
sleep 2
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null

for k in 1 2; do
  echo "--- machine $k:"
  log $k | grep -hE 'net: (booting|the host)|driving co-op|\[error\]' | sed 's/^.*\] //' | head -8
done
log 1 | grep -q 'player 2 fires the mounted gun' || { echo "NET-DRIVING-GUN FAIL: player 2 never fired on the host"; exit 1; }
log 2 | grep -q 'player 2 fires the mounted gun' || { echo "NET-DRIVING-GUN FAIL: player 2 never fired on their own machine"; exit 1; }
log 2 | grep -q "player 2 has a reticle" || { echo "NET-DRIVING-GUN FAIL: player 2's machine showed no reticle"; exit 1; }
a=$RUN/1/mem_tick_$at.bin b=$RUN/2/mem_tick_$at.bin
[ -f "$a" ] && [ -f "$b" ] || { echo "NET-DRIVING-GUN FAIL: no memory of tick $at from both machines"; exit 1; }
echo "comparing tick $at"
python3 "$HERE/net-diff.py" "$a" "$b" | head -4
# the machines' views of the game (net::CheckState: the game's time, the camera) every 60 ticks decide; the xbe's
# data (the virtual dump) shows what else is apart: frame timings, stream queues and heap addresses differ harmlessly
python3 - "${a%.bin}_virtual.bin" "${b%.bin}_virtual.bin" <<'PY'
import sys
import numpy as np
a, b = (np.fromfile(p, np.uint8) for p in sys.argv[1:3])
lo, hi = 0x1B3000, 0x24C000  # Driving.xbe's data (.data, .bss)
print(f"the xbe's data: {int(np.count_nonzero(a[lo:hi] != b[lo:hi]))} bytes apart")
PY
if log 1 | grep -q 'the games differ' || log 2 | grep -q 'the games differ'; then
    log 1 | grep 'the games differ' | sed 's/^.*\] //'
    echo "NET-DRIVING-GUN FAIL: the machines' views of the game differ"; exit 1
fi
echo "NET-DRIVING-GUN OK: player 2 aimed and fired player 1's gun, the game the same on both"
