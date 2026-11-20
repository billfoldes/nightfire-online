#!/usr/bin/env bash
# The rejoin test: two headless machines in a co-op level (as scripts/net-coop.sh), the joiner leaves (as closing
# its window does), then a new joiner on the same PC asks to play: the host offers it the seat, it boots alone,
# loads the session's game at its snapshot point (src/common/net.cpp's kRejoin) and plays player 2 again. Both
# machines draw split screen (net_fullscreen off), so their whole memories match; they are compared some ticks
# after the return.
#   scripts/net-rejoin.sh [seconds the new machine plays after its return, default 40]
#   LEAVE_AT=<seconds> when the joiner leaves (default 140; the level is up about 110 s in)
#   LEAVE_HOW=kill kills the joiner instead (the host gives the seat up after 6 s of silence)
#   NF_NET_DROP=<percent> the new machine's and the host's packets dropped (the state's chunks too)
#   RELAY=1 the session through a relay (tools/master/master.py --relay) on this PC, joined by the host's code
#   THIRD=1 a third machine (player 3) plays throughout: it learns the tick player 2 is back from (kBack), and its
#   memory is compared too
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
AFTER=${1:-40}
LEVEL=${LEVEL:-7000002}
LEAVE_AT=${LEAVE_AT:-140} LEAVE_HOW=${LEAVE_HOW:-leave}
[ -d generated/game ] || { echo "net-rejoin: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-REJOIN FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/rejoin-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3" "$RUN/4"
THIRD=${THIRD:-0}
PLAYERS=$((THIRD ? 3 : 2))
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
launch() {  # $1 = machine, $2 = pad, then the env
  local k=$1 pad=$2
  shift 2
  (
    cd "$RUN/$k"
    export INSTANCE=$k SCRIPT="$pad" NF_SKIP_INTRO=1 NF_SKIP_TUTORIAL=0 NF_LEVEL=$LEVEL NF_NET_PORT=${NET_PORT:-26577} \
      NF_NET_FULLSCREEN=0
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
log() { cat "$RUN/$1"/logs/*.log 2>/dev/null; }
leave=
[ "$LEAVE_HOW" = leave ] && leave="NF_NET_LEAVE=$LEAVE_AT"
JOIN_AT=127.0.0.1
relay=
if [ "${RELAY:-0}" = 1 ]; then  # RELAY=1: the session through a relay on this PC, joined by the host's code
  RELAY_PORT=${RELAY_PORT:-26594}
  python3 tools/master/master.py --bind 127.0.0.1 --port $((RELAY_PORT + 1)) --relay "$RELAY_PORT" > "$RUN/master.out" 2>&1 &
  relay="NF_NET_RELAY=127.0.0.1:$RELAY_PORT"
fi
launch 1 "$(realpath scripts/pad/level.txt)" NF_NET=host NF_NET_WAIT=45 NF_NET_PLAYERS=$PLAYERS NF_NET_DELAY=2 NF_NET_CHARACTER=13 \
  NF_NET_NAME=Host ${NF_NET_DROP:+NF_NET_DROP=$NF_NET_DROP} ${relay:+$relay NF_NET_HOST_RELAY=1}
if [ -n "$relay" ]; then
  for i in $(seq 30); do log 1 | grep -q 'join code NF-' && break; sleep 0.5; done
  JOIN_AT=$(log 1 | grep -o 'join code NF-[A-Z0-9]*' | head -1 | sed 's/join code //')
  [ -n "$JOIN_AT" ] || { kill $(jobs -p) 2>/dev/null; echo "NET-REJOIN FAIL: the host got no join code from the relay"; exit 1; }
  echo "net-rejoin: through the relay, join code $JOIN_AT"
fi
launch 2 "$(realpath scripts/pad/none.txt)" NF_NET=join,$JOIN_AT NF_NET_WAIT=45 NF_NET_CHARACTER=4 NF_NET_NAME=Joiner $leave $relay
pid2=$!
if [ "$THIRD" = 1 ]; then
  sleep 1  # the second machine asks first: it is player 2
  launch 4 "$(realpath scripts/pad/none.txt)" NF_NET=join,$JOIN_AT NF_NET_WAIT=45 NF_NET_CHARACTER=7 NF_NET_NAME=Third $relay
fi
echo "net-rejoin: host and joiner into level $LEVEL; the joiner leaves ($LEAVE_HOW) at about $LEAVE_AT s"
if [ "$LEAVE_HOW" = kill ]; then sleep "$LEAVE_AT"; kill -9 $pid2; else wait $pid2; fi
for i in $(seq 60); do log 1 | grep -q 'net: player 2 left' && break; sleep 1; done
log 1 | grep -q 'net: player 2 left' || { kill $(jobs -p) 2>/dev/null; echo "NET-REJOIN FAIL: the host never saw the joiner leave"; exit 1; }
echo "net-rejoin: $(log 1 | grep -h 'net: player 2 left' | sed 's/^.*\] //')"
launch 3 "$(realpath scripts/pad/none.txt)" NF_NET=join,$JOIN_AT NF_NET_WAIT=45 NF_NET_CHARACTER=4 NF_NET_NAME=Joiner $relay \
  ${NF_NET_DROP:+NF_NET_DROP=$NF_NET_DROP}
for i in $(seq 180); do log 3 | grep -qE 'net: back in the session|cannot load|did not load' && break; sleep 1; done
back=$(log 3 | grep -h 'net: back in the session' | sed 's/^.*\] //')
[ -n "$back" ] || {
  kill $(jobs -p) 2>/dev/null
  wait 2>/dev/null
  log 1 | grep -hE 'net: |\[error\]' | sed 's/^.*\] //' | tail -8
  log 3 | grep -hE 'net: |\[error\]' | sed 's/^.*\] //' | tail -8
  echo "NET-REJOIN FAIL: the new machine did not get back in (the runs' logs: $RUN/{1,3}/logs)"
  exit 1
}
from=$(echo "$back" | sed 's/.*tick \([0-9]*\).*/\1/')
at=$((from + AFTER * 60))
echo "$at" > "$RUN/1/dumptick"
echo "$at" > "$RUN/3/dumptick"
[ "$THIRD" = 1 ] && echo "$at" > "$RUN/4/dumptick"
echo "net-rejoin: $back; comparing at tick $at"
for i in $(seq $((AFTER * 3 + 60))); do
  [ -f "$RUN/1/mem_tick_$at.bin" ] && [ -f "$RUN/3/mem_tick_$at.bin" ] &&
    { [ "$THIRD" = 0 ] || [ -f "$RUN/4/mem_tick_$at.bin" ]; } && break
  sleep 1
done
sleep 2
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null

for k in 1 3 $([ "$THIRD" = 1 ] && echo 4); do
  echo "--- machine $k:"
  log $k | grep -hE 'net: (player|a machine|controller|the host at|asking|the session|back in)|co-op player|snapshot|\[error\]' |
    sed 's/^.*\] //' | head -20
done
a=$RUN/1/mem_tick_$at.bin b=$RUN/3/mem_tick_$at.bin
[ -f "$a" ] && [ -f "$b" ] || { echo "NET-REJOIN FAIL: no memory of tick $at from both machines (one stalled?)"; exit 1; }
log 1 | grep -q 'co-op player 2 is back' && log 3 | grep -q 'co-op player 2 is back' ||
  { echo "NET-REJOIN FAIL: player 2 did not come back into the level on both machines"; exit 1; }
same() {  # $1 $2 memories: the same game (net-diff.py's verdicts 0, 2 and 3)
  local rc=0
  python3 "$HERE/net-diff.py" "$1" "$2" || rc=$?
  [ "$rc" -eq 0 ] || [ "$rc" -eq 2 ] || [ "$rc" -eq 3 ]
}
same "$a" "$b" || { echo "NET-REJOIN FAIL: the host and the rejoined machine differ at tick $at"; exit 1; }
if [ "$THIRD" = 1 ]; then
  c=$RUN/4/mem_tick_$at.bin
  [ -f "$c" ] || { echo "NET-REJOIN FAIL: no memory of tick $at from the third machine"; exit 1; }
  log 4 | grep -q 'net: player 2 rejoins the session at tick' || { echo "NET-REJOIN FAIL: the third machine never heard of the return"; exit 1; }
  echo "--- the host against the third machine:"
  # two machines that played the whole level apart in co-op differ in a few blocks of one place (PROGRESS: "co-op
  # leaves the two machines' memories apart in the same places", open); the rejoined machine took the host's copy
  same "$a" "$c" || python3 - "$a" "$c" <<'PY' || { echo "NET-REJOIN FAIL: the host and the third machine differ at tick $at"; exit 1; }
import sys
a, b = (open(p, 'rb').read() for p in sys.argv[1:3])
C = 4096
blocks = [c for c in range(0x100000, min(len(a), len(b)), C) if a[c:c + C] != b[c:c + C]]
near = [c for c in blocks if c - blocks[0] < 0x40000]
if len(blocks) <= 32 and len(blocks) - len(near) <= 2:
    print(f"only co-op's own difference: {len(blocks)} blocks from 0x{blocks[0]:x}")
    sys.exit(0)
sys.exit(1)
PY
fi
echo "NET-REJOIN OK: the new machine took player 2 back and the $PLAYERS games are the same at tick $at"
exit 0
