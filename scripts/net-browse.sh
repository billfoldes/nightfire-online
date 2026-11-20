#!/usr/bin/env bash
# The LAN browser's test (F1's Online page, net::FindGames): a host waits for a second player, a third machine with
# no session looks for games (NF_NET_FIND=1 logs each change of its list), and a joiner comes 20 s in. The browser
# must hear the host waiting (1 of 2), then playing and full once the joiner is in.
# STREAM=1: the browsing machine is in streamer mode (net_default_names): the host is Game and a number, not its name.
#   scripts/net-browse.sh [seconds, default 70]
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-70}
[ -d generated/game ] || { echo "net-browse: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-BROWSE FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/browse-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2" "$RUN/3"
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/none.txt)
launch() {  # $1 = machine, then the env
  local k=$1
  shift
  (
    cd "$RUN/$k"
    # not the game's port (26565): a real session on this PC must not hear the test machines, nor they it
    export INSTANCE=$k SCRIPT="$PAD" NF_SKIP_INTRO=1 NF_NET_PORT=${NET_PORT:-26579} NF_NET_NAME=Machine$k
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
launch 1 NF_NET=host NF_NET_WAIT=60 NF_NET_PLAYERS=2
launch 3 NF_NET_FIND=1 NF_NET_DEFAULT_NAMES=${STREAM:-0}
sleep 20
launch 2 NF_NET=join,127.0.0.1 NF_NET_WAIT=45
sleep $((SECS - 20))
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null
found=$(grep -h 'net: found' "$RUN"/3/logs/*.log | sed 's/^.*\] //')
echo "$found"
host='Machine1'
[ "${STREAM:-0}" = 1 ] && host='Game [0-9]\{4\}'
if echo "$found" | grep -q "$host at 127.0.0.1, waiting for players, 1 of 2" &&
   echo "$found" | grep -q "$host at 127.0.0.1, playing, 2 players, full" &&
   { [ "${STREAM:-0}" != 1 ] || ! echo "$found" | grep -q Machine1; }; then
  echo "NET-BROWSE OK: the browser heard the host waiting, then its game under way"
  exit 0
fi
echo "NET-BROWSE FAIL: the browser's list is not as expected (the runs' logs: $RUN/{1,2,3}/logs)"
exit 1
