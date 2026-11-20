#!/usr/bin/env bash
# A first start's relay (BuiltInServers a list alone, as the release's): the exe knows list A, whose GET /servers
# names relay B, and nothing else (no net_master, no net_relay, no servers.txt). A host through a relay must wait for
# the list's answer and get its join code from relay B, and a joiner by the bare code must reach it there.
#   scripts/net-first-start.sh [seconds, default 60]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-60}
[ -d generated/game ] || { echo "net-first-start: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-FIRST-START FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/first-start-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
LIST_A=26631 LIST_B=26632 RELAY_B=26633
A=http://127.0.0.1:$LIST_A
printf 'relay 127.0.0.1:%s\n' "$RELAY_B" > "$RUN/servers.txt"
python3 tools/master/master.py --bind 127.0.0.1 --port $LIST_A --servers "$RUN/servers.txt" > "$RUN/a.out" 2>&1 &
python3 tools/master/master.py --bind 127.0.0.1 --port $LIST_B --relay $RELAY_B > "$RUN/b.out" 2>&1 &
for _ in $(seq 50); do curl -sf "$A/list" > /dev/null && curl -sf "http://127.0.0.1:$LIST_B/list" > /dev/null && break; sleep 0.1; done
launch() {  # $1 = machine, $2 = its port, then the env
  local k=$1 p=$2
  shift 2
  (
    cd "$RUN/$k"
    export INSTANCE=$k SCRIPT="$PAD" NF_SKIP_INTRO=1 NF_NET_PORT=$p NF_NET_NAME=Machine$k NF_NET_FULLSCREEN=0 \
      NF_NET_BUILTIN="list $A"
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
launch 1 26634 NF_NET=host NF_NET_WAIT=50 NF_NET_PLAYERS=2 NF_NET_WHO=1 NF_NET_HOST_RELAY=1
sleep 10
code=$(grep -ho 'join code NF-[A-Z0-9]*' "$RUN"/1/logs/*.log | head -1 | sed 's/join code //')
echo "the host's join code: ${code:-none}"
launch 2 26635 NF_NET="join,$code" NF_NET_WAIT=40
sleep $((SECS - 10))
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
grep -h 'net: ' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log | grep -E 'relay|list|servers|booting|differ' |
  sed 's/^.*\] //' | head -20
fail=""
[ -n "$code" ] || fail="the host got no join code"
grep -q "asking the relay at 127.0.0.1:$RELAY_B for a join code" "$RUN"/1/logs/*.log ||
  fail="${fail:-the host did not ask the relay list A names}"
grep -q "the relay at 127.0.0.1:$RELAY_B has the game" "$RUN"/2/logs/*.log ||
  fail="${fail:-the joiner did not reach the game through relay B}"
grep -q 'booting with 2 connected' "$RUN"/1/logs/*.log || fail="${fail:-the session did not start}"
grep -qi 'games differ' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log && fail="${fail:-the games differ}"
if [ -n "$fail" ]; then
  echo "NET-FIRST-START FAIL: $fail (the runs' logs: $RUN/{1,2}/logs)"
  exit 1
fi
echo "NET-FIRST-START OK: knowing list A alone, the host got $code from the relay it names, and the joiner joined there"
