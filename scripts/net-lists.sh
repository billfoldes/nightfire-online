#!/usr/bin/env bash
# Several server lists and relays (net_master, net_relay with commas; GET /servers): list A names list B and relay B
# in its --servers file, a dead list and a dead relay come first. The host must get its code from relay B after the
# dead one, and be on list B (the dead list said once); a browser asking the dead list, A and B must find it there by its code; a joiner by the
# bare code, asking relay A (which has no such game) first, must reach it through relay B; and the browser's servers.txt
# beside its exe (the harness's: its run folder) must name list B and relay B, as list A told; and a browser whose
# only list is the dead one must find the game on list B all the same, from the servers.txt an earlier start kept.
#   scripts/net-lists.sh [seconds, default 80]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-80}
[ -d generated/game ] || { echo "net-lists: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-LISTS FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/lists-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3" "$RUN/4"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
LIST_A=26611 RELAY_A=26612 LIST_B=26613 RELAY_B=26614 DEAD_LIST=26615 DEAD_RELAY=26616
A=http://127.0.0.1:$LIST_A B=http://127.0.0.1:$LIST_B DEAD=http://127.0.0.1:$DEAD_LIST
printf 'list %s  # the other list\nrelay 127.0.0.1:%s\n' "$B" "$RELAY_B" > "$RUN/servers.txt"
python3 tools/master/master.py --bind 127.0.0.1 --port $LIST_A --relay $RELAY_A --servers "$RUN/servers.txt" > "$RUN/a.out" 2>&1 &
python3 tools/master/master.py --bind 127.0.0.1 --port $LIST_B --relay $RELAY_B > "$RUN/b.out" 2>&1 &
for _ in $(seq 50); do curl -sf "$A/list" > /dev/null && curl -sf "$B/list" > /dev/null && break; sleep 0.1; done
launch() {  # $1 = machine, $2 = its port, then the env
  local k=$1 p=$2
  shift 2
  (
    cd "$RUN/$k"
    export INSTANCE=$k SCRIPT="$PAD" NF_SKIP_INTRO=1 NF_NET_PORT=$p NF_NET_NAME=Machine$k NF_NET_FULLSCREEN=0
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
launch 1 26617 NF_NET=host NF_NET_WAIT=60 NF_NET_PLAYERS=2 NF_NET_WHO=1 NF_NET_HOST_RELAY=1 \
  NF_NET_MASTER="$DEAD,$A,$B" NF_NET_RELAY="127.0.0.1:$DEAD_RELAY,127.0.0.1:$RELAY_B"
launch 3 26618 NF_NET_FIND=1 NF_NET_MASTER="$DEAD,$A,$B"
# its only list dead, and list B in the servers.txt an earlier start kept
printf 'list %s\n' "$B" > "$RUN/4/servers.txt"
launch 4 26620 NF_NET_FIND=1 NF_NET_MASTER="$DEAD"
sleep 12
curl -s "$B/list" > "$RUN/list.txt"
code=$(grep -ho 'join code NF-[A-Z0-9]*' "$RUN"/1/logs/*.log | head -1 | sed 's/join code //')
echo "the host's join code: ${code:-none}"
sleep 8
launch 2 26619 NF_NET="join,$code" NF_NET_WAIT=45 NF_NET_MASTER="$DEAD,$A,$B" NF_NET_RELAY="127.0.0.1:$RELAY_A,127.0.0.1:$RELAY_B"
sleep $((SECS - 20))
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
echo "list B at 12 s:"
cat "$RUN/list.txt"
found=$(grep -h 'net: found' "$RUN"/3/logs/*.log | sed 's/^.*\] //')
echo "$found"
grep -h 'net: ' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log "$RUN"/3/logs/*.log |
  grep -E 'relay|list|servers|booting|differ' | sed 's/^.*\] //' | head -30
echo "servers.txt:"
cat "$RUN/3/servers.txt" 2>/dev/null
fail=""
[ -n "$code" ] || fail="the host got no join code"
grep -q "asking the relay at 127.0.0.1:$RELAY_B instead" "$RUN"/1/logs/*.log ||
  fail="${fail:-the host did not go on from the dead relay to relay B}"
[ "$(grep -c 'game list at http://127.0.0.1:'$DEAD_LIST' did not take' "$RUN"/1/logs/*.log)" = 1 ] ||
  fail="${fail:-the dead list is not said once}"
grep -q "$code" "$RUN/list.txt" || fail="${fail:-list B does not show the game of the host}"
echo "$found" | grep -q "Machine1 at $code@127.0.0.1:$RELAY_B, waiting for players, 1 of 2 - through a relay" ||
  fail="${fail:-the browser did not find the game on list B}"
grep -q "net: found Machine1 at $code@" "$RUN"/4/logs/*.log ||
  fail="${fail:-a browser whose list is dead did not find the game on the list servers.txt kept}"
grep -qx "list $B" "$RUN/4/servers.txt" || fail="${fail:-a list that answered was dropped from servers.txt}"
grep -q "the relay at 127.0.0.1:$RELAY_B has the game" "$RUN"/2/logs/*.log ||
  fail="${fail:-the joiner did not reach the game through relay B}"
grep -q 'booting with 2 connected' "$RUN"/1/logs/*.log || fail="${fail:-the session did not start}"
grep -qi 'games differ' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log && fail="${fail:-the games differ}"
grep -qx "list $B" "$RUN/3/servers.txt" 2>/dev/null && grep -qx "relay 127.0.0.1:$RELAY_B" "$RUN/3/servers.txt" ||
  fail="${fail:-servers.txt does not keep what list A named}"
if [ -z "$fail" ]; then
  echo "NET-LISTS OK: past the dead list and relay, the host on list B by relay B's code $code, found and joined there"
  exit 0
fi
echo "NET-LISTS FAIL: $fail (the runs' logs: $RUN/{1,2,3}/logs)"
exit 1
