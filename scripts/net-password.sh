#!/usr/bin/env bash
# A host's password (net_who Only people I tell, net_password): the host's game asks for one, a browser (NF_NET_FIND=1, and asked at the host's
# address as a favourite is, NF_NET_FIND_ASK) must hear it locked, a joiner with the wrong password must be refused
# and boot alone, and one with the right password must be given controller 2.
#   scripts/net-password.sh [seconds, default 75]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-75}
PORT=${NET_PORT:-26583}
[ -d generated/game ] || { echo "net-password: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-PASSWORD FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/password-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3" "$RUN/4"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
launch() {  # $1 = machine, then the env
  local k=$1
  shift
  (
    cd "$RUN/$k"
    # not the game's port (26565): a real session on this PC must not hear the test machines, nor they it
    export INSTANCE=$k SCRIPT="$PAD" NF_SKIP_INTRO=1 NF_NET_PORT=$PORT NF_NET_NAME=Machine$k
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
launch 1 NF_NET=host NF_NET_WAIT=70 NF_NET_PLAYERS=2 NF_NET_WHO=2 NF_NET_PASSWORD=moonraker
launch 3 NF_NET_FIND=1 NF_NET_FIND_ASK=127.0.0.1:$PORT
sleep 12
launch 2 NF_NET=join,127.0.0.1 NF_NET_WAIT=30 NF_NET_JOIN_PASSWORD=goldfinger
sleep 18
launch 4 NF_NET=join,127.0.0.1 NF_NET_WAIT=40 NF_NET_JOIN_PASSWORD=moonraker
sleep $((SECS - 30))
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
log() { cat "$RUN/$1"/logs/*.log 2>/dev/null; }
found=$(log 3 | grep 'net: found' | sed 's/^.*\] //')
echo "$found"
fail=0
echo "$found" | grep -q 'Machine1 at 127.0.0.1, waiting for players, 1 of 2.*(locked)' ||
  { echo "the browser did not hear the host's game locked"; fail=1; }
log 1 | grep -q 'asked to join with the wrong password; refused' ||
  { echo "the host did not refuse the wrong password"; fail=1; }
log 2 | grep -q 'has a password, and this machine gave another; booting without a session' ||
  { echo "the wrong password's joiner did not boot alone"; fail=1; }
log 2 | grep -q 'gives this one controller' && { echo "the wrong password's joiner was given a controller"; fail=1; }
log 4 | grep -q 'gives this one controller 2' ||
  { echo "the right password's joiner was not given controller 2"; fail=1; }
if [ $fail = 0 ]; then
  echo "NET-PASSWORD OK: the locked game is heard locked, the wrong password refused, the right one plays"
  exit 0
fi
echo "NET-PASSWORD FAIL (the runs' logs: $RUN/{1,2,3,4}/logs)"
exit 1
