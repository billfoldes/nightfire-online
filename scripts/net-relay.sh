#!/usr/bin/env bash
# The relay's test (net_relay, net_host_by 1; tools/master/master.py --relay): a list server with its relay on this
# PC, a host through the relay that lists its game, a browser that must find it by its join code (no address), and a
# joiner 20 s in that joins by the code the host's log shows. The session must run through the relay: the host's
# joiner is a relay connection, and the two play the same game (no "games differ"). Both have net_direct on, so after
# the relay puts them in touch they must connect straight; DIRECT=0 turns the joiner's off, and then neither may.
#   [DIRECT=0] scripts/net-relay.sh [seconds, default 80]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-80}
[ -d generated/game ] || { echo "net-relay: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-RELAY FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/relay-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
LIST_PORT=${LIST_PORT:-26592}
RELAY_PORT=${RELAY_PORT:-26593}
MASTER=http://127.0.0.1:$LIST_PORT
python3 tools/master/master.py --bind 127.0.0.1 --port "$LIST_PORT" --relay "$RELAY_PORT" > "$RUN/master.out" 2>&1 &
for _ in $(seq 50); do curl -sf "$MASTER/list" > /dev/null && break; sleep 0.1; done
launch() {  # $1 = machine, $2 = its port, then the env
  local k=$1 p=$2
  shift 2
  (
    cd "$RUN/$k"
    export INSTANCE=$k SCRIPT="$PAD" NF_SKIP_INTRO=1 NF_NET_PORT=$p NF_NET_NAME=Machine$k NF_NET_MASTER=$MASTER \
      NF_NET_RELAY=127.0.0.1:$RELAY_PORT NF_NET_FULLSCREEN=0
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
launch 1 26584 NF_NET=host NF_NET_WAIT=60 NF_NET_PLAYERS=2 NF_NET_PUBLIC=1 NF_NET_HOST_RELAY=1
launch 3 26585 NF_NET_FIND=1
sleep 10
curl -s "$MASTER/list" > "$RUN/list.txt"
code=$(grep -ho 'join code NF-[A-Z0-9]*' "$RUN"/1/logs/*.log | head -1 | sed 's/join code //')
echo "the host's join code: ${code:-none}"
sleep 10
DIRECT=${DIRECT:-1}
launch 2 26586 NF_NET="join,$code" NF_NET_WAIT=45 NF_NET_DIRECT="$DIRECT" NF_NET_DIRECT_CUT="${CUT:-}"
sleep $((SECS - 20))
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
echo "the list at 10 s:"
cat "$RUN/list.txt"
found=$(grep -h 'net: found' "$RUN"/3/logs/*.log | sed 's/^.*\] //')
echo "$found"
grep -h 'net: ' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log | grep -E 'relay|joined|booting|differ' | sed 's/^.*\] //' | head -20
fail=""
[ -n "$code" ] || fail="the host got no join code"
grep -q "$code" "$RUN/list.txt" && ! grep -q '127.0.0.1' <(cut -f1 "$RUN/list.txt") || fail="${fail:-the list does not show the code alone}"
echo "$found" | grep -q "Machine1 at $code@127.0.0.1:$RELAY_PORT, waiting for players, 1 of 2 - through a relay" ||
  fail="${fail:-the browser did not find the game by its code}"
grep -q 'the relay at .* has the game' "$RUN"/2/logs/*.log || fail="${fail:-the joiner did not reach the game through the relay}"
grep -q 'booting with 2 connected' "$RUN"/1/logs/*.log || fail="${fail:-the session did not start}"
grep -qi 'games differ' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log && fail="${fail:-the games differ}"
grep -h 'straight\|direct connection\|in touch' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log | sed 's/^.*\] //'
if [ -n "${CUT:-}" ]; then
  grep -q 'connected straight to the host' "$RUN"/2/logs/*.log || fail="${fail:-the joiner did not connect straight}"
  grep -q 'direct connection to the host stopped' "$RUN"/2/logs/*.log || fail="${fail:-the joiner did not go back to the relay}"
  grep -q 'direct connection to the joiner on relay connection 1 stopped' "$RUN"/1/logs/*.log || fail="${fail:-the host did not go back to the relay}"
  how="straight, then the relay again once the joiner's door shut at ${CUT} s"
elif [ "$DIRECT" = 1 ]; then
  grep -q 'connected straight to the joiner' "$RUN"/1/logs/*.log || fail="${fail:-the host did not connect straight}"
  grep -q 'connected straight to the host' "$RUN"/2/logs/*.log || fail="${fail:-the joiner did not connect straight}"
  grep -q 'stopped working both ways' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log &&
    fail="${fail:-the direct connection dropped}"
  how="then straight"
else
  grep -q 'in touch' "$RUN"/1/logs/*.log "$RUN"/2/logs/*.log && fail="${fail:-the relay gave out an address the joiner did not allow}"
  how="the relay alone (the joiner's Connect directly off)"
fi
if [ -z "$fail" ]; then
  echo "NET-RELAY OK: the host got $code, the list showed it with no address, the joiner played through the relay, $how"
  exit 0
fi
echo "NET-RELAY FAIL: $fail (the runs' logs: $RUN/{1,2,3}/logs, the list: $RUN/list.txt)"
exit 1
