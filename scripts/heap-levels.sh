#!/usr/bin/env bash
# The level heap's free space as each level starts (src/game/heap.cpp's "level X starts with" line), headless:
#   scripts/heap-levels.sh <players 1..4> [level hex ...]   1 plays offline, more meet in a co-op session over
#   loopback (net_character 12 each); no levels: the campaign's 19 on-foot levels. A level that never starts in 90 s
#   prints "none". Each level takes about 20 s; the runs are in .local/heap-runs
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
P=${1:?players 1..4}
shift
LEVELS=${*:-7000001 7000002 7000003 7000004 7000005 7000006 7000007 7000008 7000009 700000A 700000B 700000C 700000D \
7000011 7000012 7000013 7000014 7000015 7000016}
[ -d generated/game ] || { echo "heap-levels: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' \
  > /dev/null || { echo "heap-levels: the harness did not build"; exit 1; }
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
PAD=$(realpath scripts/pad/level.txt)
NONE=$(realpath scripts/pad/none.txt)
PORT=$((26700 + P * 10))  # not the game's port, and apart for runs of different player counts at once
for L in $LEVELS; do
  RUN=.local/heap-runs/p$P-$L
  rm -rf "$RUN"
  pids=()
  for k in $(seq 1 "$P"); do
    mkdir -p "$RUN/$k"
    net=()
    if [ "$P" != 1 ]; then
      if [ "$k" = 1 ]; then net=(NF_NET=host NF_NET_WAIT=60 NF_NET_PLAYERS=$P NF_NET_DELAY=2); else net=(NF_NET=join,127.0.0.1 NF_NET_WAIT=60); fi
      net+=(NF_NET_CHARACTER=12)
    fi
    pad=$([ "$k" = 1 ] && echo "$PAD" || echo "$NONE")
    (cd "$RUN/$k"; export INSTANCE=$k SCRIPT="$pad" NF_SKIP_INTRO=1 NF_SKIP_TUTORIAL=0 NF_LEVEL=$L NF_NET_PORT=$PORT
     exec env "${net[@]}" "$EXE" "$GAME") > "$RUN/$k/run.out" 2>&1 &
    pids+=($!)
  done
  id=$(printf '%08X' $((16#$L)))
  for t in $(seq 1 90); do
    sleep 1
    n=$(cat "$RUN"/*/logs/*.log 2>/dev/null | grep -c "level $id starts with")
    [ "$n" -ge "$P" ] && break
  done
  kill "${pids[@]}" 2>/dev/null
  wait 2>/dev/null
  line=$(grep -h "level $id starts with" "$RUN"/1/logs/*.log 2>/dev/null | head -1 | sed 's/^.*starts with //')
  more=$(grep -h "more enemies beside" "$RUN"/1/logs/*.log 2>/dev/null | head -1 | sed 's/^.*co-op: //')
  echo "players $P level $id: ${line:-none} ${more:+[$more]}"
done
