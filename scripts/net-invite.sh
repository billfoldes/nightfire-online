#!/usr/bin/env bash
# Only people I tell through the relay (net_who 2, net_host_by 1; tools/master/master.py --relay): the host's game
# goes on the list with its code withheld, a browser hears it locked by its code, a joiner by this network's address
# with the wrong code is refused, and two with the code play: one through the relay (NF-code@relay), one by the
# address with the code as its password.
#   scripts/net-invite.sh [seconds, default 90]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-90}
[ -d generated/game ] || { echo "net-invite: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-INVITE FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/invite-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3" "$RUN/4" "$RUN/5"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
LIST_PORT=${LIST_PORT:-26594}
RELAY_PORT=${RELAY_PORT:-26595}
HOST_PORT=26596
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
launch 1 $HOST_PORT NF_NET=host NF_NET_WAIT=80 NF_NET_PLAYERS=3 NF_NET_WHO=2 NF_NET_HOST_RELAY=1
launch 3 26597 NF_NET_FIND=1
sleep 10
curl -s "$MASTER/list" > "$RUN/list.txt"
code=$(grep -ho 'join code NF-[A-Z0-9]*' "$RUN"/1/logs/*.log | head -1 | sed 's/join code //')
echo "the host's join code: ${code:-none}"
launch 2 26598 NF_NET=join,127.0.0.1:$HOST_PORT NF_NET_WAIT=30 NF_NET_JOIN_PASSWORD=NF-ZZZZZZ
sleep 10
launch 4 26599 NF_NET="join,$code" NF_NET_WAIT=50
launch 5 26600 NF_NET=join,127.0.0.1:$HOST_PORT NF_NET_WAIT=50 NF_NET_JOIN_PASSWORD="${code,,}"
sleep $((SECS - 20))
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
echo "the list at 10 s:"
cat "$RUN/list.txt"
found=$(grep -h 'net: found' "$RUN"/3/logs/*.log | sed 's/^.*\] //')
echo "$found"
log() { cat "$RUN/$1"/logs/*.log 2>/dev/null; }
fail=""
[ -n "$code" ] || fail="the host got no join code"
grep -qP '^-\t0\t.*\t-\t127\.0\.0\.1:'"$RELAY_PORT"'$' "$RUN/list.txt" && ! grep -q "$code" "$RUN/list.txt" ||
  fail="${fail:-the list does not show the game with its code withheld}"
echo "$found" | grep -q "Machine1 at #[0-9]*@127.0.0.1:$RELAY_PORT, .*by its code.*(locked by its code) (relay)" ||
  fail="${fail:-the browser did not hear the game locked by its code}"
log 1 | grep -q 'asked to join with the wrong join code; refused' || fail="${fail:-the host did not refuse the wrong code}"
log 2 | grep -q 'asks for its join code, and this machine gave another; booting without a session' ||
  fail="${fail:-the wrong code's joiner did not boot alone}"
log 2 | grep -q 'gives this one controller' && fail="${fail:-the wrong code's joiner was given a controller}"
log 4 | grep -q 'the relay at .* has the game' || fail="${fail:-the relay's joiner did not reach the game}"
log 4 | grep -q 'gives this one controller' || fail="${fail:-the relay's joiner was not given a controller}"
log 5 | grep -q 'gives this one controller' || fail="${fail:-the code as a password did not join}"
log 1 | grep -q 'booting with 3 connected' || fail="${fail:-the session did not start with 3}"
grep -qi 'games differ' "$RUN"/1/logs/*.log "$RUN"/4/logs/*.log "$RUN"/5/logs/*.log && fail="${fail:-the games differ}"
if [ -z "$fail" ]; then
  echo "NET-INVITE OK: listed with $code withheld, heard locked, the wrong code refused, the code joins through the relay and by address"
  exit 0
fi
echo "NET-INVITE FAIL: $fail (the runs' logs: $RUN/{1,2,3,4,5}/logs, the list: $RUN/list.txt)"
exit 1
