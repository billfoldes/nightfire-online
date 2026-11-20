#!/usr/bin/env bash
# The driving rejoin test: two headless Driving.xbe machines in one session (as scripts/net-driving-gun.sh), the
# joiner leaves (as closing its window does), then a new Driving.xbe joiner on the same launch data asks to play: the
# host offers it the seat, it boots into the mission alone, loads the session's game at the main loop's snapshot
# point (src/driving/clock.cpp) and plays player 2 again. The host's and the new machine's memories are compared
# some ticks after the return: the machines' views of the game (net::CheckState) must agree.
#   scripts/net-driving-rejoin.sh [seconds the new machine plays after its return, default 30]
#   LAUNCH_HDD (default .local/lvl5/1/hdd, Alpine Escape), LEAVE_AT=<seconds> (default 90), LEAVE_HOW=kill
#   FROM_DEFAULT=1: the new machine starts default.xbe as a player does, with no launch data on its disk: the host
#   sends it the mission's (net.cpp's kLaunch) and it relaunches; its Driving.xbe is then started on that disk here
#   (a harness relaunch has no exe beside it to go to)
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
AFTER=${1:-30}
HDD=${LAUNCH_HDD:-.local/lvl5/1/hdd}
LEAVE_AT=${LEAVE_AT:-90} LEAVE_HOW=${LEAVE_HOW:-leave}
[ -d "$HDD" ] || { echo "net-driving-rejoin: no launch data at $HDD"; exit 2; }
[ -d generated/driving ] || { echo "net-driving-rejoin: no generated/driving"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/driving -o .local/harness_next -n driving -G .local/harness_next/gen -x '/src/game/' || {
    echo "NET-DRIVING-REJOIN FAIL: the harness did not build"
    exit 1
  }
FROM_DEFAULT=${FROM_DEFAULT:-0}
if [ "$FROM_DEFAULT" = 1 ]; then
  CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
    bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
      echo "NET-DRIVING-REJOIN FAIL: the default.xbe harness did not build"
      exit 1
    }
fi
RUN=.local/driving-rejoin-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3"
GAME=$(realpath assets)
EXE=$(realpath .local/harness_next/driving)
printf '\n' > "$RUN/none.txt"
launch() {  # $1 = machine, then the env (EXE_K=<exe> another exe, KEEP_DISK=1 its disk as it is)
  local k=$1
  shift
  [ "${KEEP_DISK:-0}" = 1 ] || cp -r "$HDD" "$RUN/$k/hdd$([ $k = 1 ] || echo $k)"
  (
    cd "$RUN/$k"
    export INSTANCE=$k SCRIPT=$(realpath ../none.txt) NF_NET_WAIT=30 NF_NET_PORT=${NET_PORT:-26589}
    exec env "$@" "${EXE_K:-$EXE}" "$GAME"
  ) >> "$RUN/$k/run.out" 2>&1 &
}
log() { cat "$RUN/$1"/logs/*.log 2>/dev/null; }
leave=
[ "$LEAVE_HOW" = leave ] && leave="NF_NET_LEAVE=$LEAVE_AT"
launch 1 NF_NET=host NF_NET_PLAYERS=2 NF_NET_DELAY=2
launch 2 NF_NET=join,127.0.0.1 $leave
pid2=$!
echo "net-driving-rejoin: host and joiner into the mission; the joiner leaves ($LEAVE_HOW) at about $LEAVE_AT s of play"
if [ "$LEAVE_HOW" = kill ]; then sleep "$LEAVE_AT"; kill -9 $pid2; else wait $pid2; fi
for i in $(seq 60); do log 1 | grep -q 'net: player 2 left' && break; sleep 1; done
log 1 | grep -q 'net: player 2 left' || { kill $(jobs -p) 2>/dev/null; echo "NET-DRIVING-REJOIN FAIL: the host never saw the joiner leave"; exit 1; }
echo "net-driving-rejoin: $(log 1 | grep -h 'net: player 2 left' | sed 's/^.*\] //')"
if [ "$FROM_DEFAULT" = 1 ]; then
  cp -r "$HDD" "$RUN/3/hdd3" && rm -f "$RUN/3/hdd3/launch.bin"
  KEEP_DISK=1 EXE_K=$(realpath .local/harness/game) launch 3 NF_NET=join,127.0.0.1 NF_SKIP_INTRO=1
  pid3=$!
  for i in $(seq 60); do log 3 | grep -q 'relaunching into it' && break; sleep 1; done
  log 3 | grep -q 'relaunching into it' || {
    kill $(jobs -p) 2>/dev/null
    log 3 | grep -hE 'net: |\[error\]' | sed 's/^.*\] //' | tail -6
    echo "NET-DRIVING-REJOIN FAIL: default.xbe never relaunched into the host's Driving.xbe"
    exit 1
  }
  wait $pid3 2>/dev/null
  echo "net-driving-rejoin: $(log 3 | grep -h 'relaunching into it' | sed 's/^.*\] //')"
  cmp -s "$RUN/3/hdd3/launch.bin" "$HDD/launch.bin" || { kill $(jobs -p) 2>/dev/null; echo "NET-DRIVING-REJOIN FAIL: the launch data written is not the mission's"; exit 1; }
  KEEP_DISK=1 launch 3 NF_NET=join,127.0.0.1
else
  launch 3 NF_NET=join,127.0.0.1
fi
for i in $(seq 240); do log 3 | grep -qE 'net: back in the session|cannot load|did not load' && break; sleep 1; done
back=$(log 3 | grep -h 'net: back in the session' | sed 's/^.*\] //')
[ -n "$back" ] || {
  kill $(jobs -p) 2>/dev/null
  wait 2>/dev/null
  log 1 | grep -hE 'net: |snapshot|\[error\]' | sed 's/^.*\] //' | tail -8
  log 3 | grep -hE 'net: |snapshot|\[error\]' | sed 's/^.*\] //' | tail -8
  echo "NET-DRIVING-REJOIN FAIL: the new machine did not get back in (the runs' logs: $RUN/{1,3}/logs)"
  exit 1
}
from=$(echo "$back" | sed 's/.*tick \([0-9]*\).*/\1/')
at=$((from + AFTER * 30))  # a tick is two frames here
echo "$at" > "$RUN/1/dumptick"
echo "$at" > "$RUN/3/dumptick"
echo "net-driving-rejoin: $back; comparing at tick $at"
for i in $(seq $((AFTER * 3 + 60))); do [ -f "$RUN/1/mem_tick_$at.bin" ] && [ -f "$RUN/3/mem_tick_$at.bin" ] && break; sleep 1; done
sleep 2
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
for k in 1 3; do
  echo "--- machine $k:"
  log $k | grep -hE 'net: (player|controller|the host at|asking|the session|back in)|snapshot: (saved|loaded)|\[error\]' |
    sed 's/^.*\] //' | head -12
done
a=$RUN/1/mem_tick_$at.bin b=$RUN/3/mem_tick_$at.bin
[ -f "$a" ] && [ -f "$b" ] || { echo "NET-DRIVING-REJOIN FAIL: no memory of tick $at from both machines (one stalled?)"; exit 1; }
# the machines' views of the game (net::CheckState: the game's time, the camera) every 60 ticks decide; the xbe's
# data (the virtual dump) shows what else is apart: frame timings, stream queues and heap addresses differ harmlessly
python3 - "${a%.bin}_virtual.bin" "${b%.bin}_virtual.bin" <<'PY'
import sys
import numpy as np
a, b = (np.fromfile(p, np.uint8) for p in sys.argv[1:3])
lo, hi = 0x1B3000, 0x24C000  # Driving.xbe's data (.data, .bss)
print(f"the xbe's data: {int(np.count_nonzero(a[lo:hi] != b[lo:hi]))} bytes apart")
PY
if log 1 | grep -q 'the games differ' || log 3 | grep -q 'the games differ'; then
    log 1 | grep 'the games differ' | sed 's/^.*\] //'
    echo "NET-DRIVING-REJOIN FAIL: the machines' views of the game differ"; exit 1
fi
echo "NET-DRIVING-REJOIN OK: the new machine took player 2 back, the game the same on both at tick $at"
