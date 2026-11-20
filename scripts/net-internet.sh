#!/usr/bin/env bash
# The internet game list's test (net_master, net_who Anyone; tools/master/master.py): a list server on this PC, a host
# that lists its game and waits for a second player, a browser on another port (so no LAN query of its reaches the
# host: it knows the game from the list alone) and a joiner 20 s in that joins by the list's address and port.
# The browser must hear the host waiting (1 of 2), then playing and full.
#   scripts/net-internet.sh [seconds, default 70]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-70}
[ -d generated/game ] || { echo "net-internet: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-INTERNET FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/internet-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
LIST_PORT=${LIST_PORT:-26590}
MASTER=http://127.0.0.1:$LIST_PORT
python3 tools/master/master.py --bind 127.0.0.1 --port "$LIST_PORT" > "$RUN/master.out" 2>&1 &
for _ in $(seq 50); do curl -sf "$MASTER/list" > /dev/null && break; sleep 0.1; done
launch() {  # $1 = machine, $2 = its port, then the env
  local k=$1 p=$2
  shift 2
  (
    cd "$RUN/$k"
    export INSTANCE=$k SCRIPT="$PAD" NF_SKIP_INTRO=1 NF_NET_PORT=$p NF_NET_NAME=Machine$k NF_NET_MASTER=$MASTER
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
launch 1 26601 NF_NET=host NF_NET_WAIT=60 NF_NET_PLAYERS=2 NF_NET_PUBLIC=1
launch 3 26602 NF_NET_FIND=1
sleep 10
curl -s "$MASTER/list" > "$RUN/list.txt"
sleep 10
launch 2 26603 NF_NET=join,127.0.0.1:26601 NF_NET_WAIT=45
sleep $((SECS - 20))
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
echo "the list at 10 s:"
cat "$RUN/list.txt"
found=$(grep -h 'net: found' "$RUN"/3/logs/*.log | sed 's/^.*\] //')
echo "$found"
listed=$(grep -c 'on the internet game list' "$RUN"/1/logs/*.log)
if [ "$listed" -ge 1 ] && grep -q "Machine1" "$RUN/list.txt" &&
   grep -q 'waits for its players with no time limit' "$RUN"/1/logs/*.log &&
   grep -q 'the server list reached this game at 127.0.0.1 UDP 26601 from outside' "$RUN"/1/logs/*.log &&
   echo "$found" | grep -q 'Machine1 at 127.0.0.1:26601, waiting for players, 1 of 2' &&
   echo "$found" | grep -q 'Machine1 at 127.0.0.1:26601, playing, 2 players, full'; then
  echo "NET-INTERNET OK: the host went on the list, the browser found it there, the joiner joined by its address"
  exit 0
fi
echo "NET-INTERNET FAIL: not as expected (the runs' logs: $RUN/{1,2,3}/logs, the list: $RUN/list.txt)"
exit 1
