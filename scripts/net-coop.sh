#!/usr/bin/env bash
# The co-op campaign test: two headless machines meeting over loopback, one session, fresh disks, the first boot's
# START taking them into a campaign level (NF_LEVEL, src/game/hooks.cpp) with a character each (net_character:
# 0 the mission's, 1 Bond, 2..29 the multiplayer characters). The level's load registers the chosen characters
# (src/game/characters.cpp); both players spawn wearing them, run in lockstep, and the machines' whole memories are
# compared near the end, as scripts/net-loopback.sh does for a match.
#   scripts/net-coop.sh [seconds, default 220] [host's character, default 13] [joiner's, default 4]
#   LEVEL=<hex level id> another level (default 7000002, the second mission's first level). Characters 13 and 4
#   have the most textures stored as aliases of another pack's (characters.cpp's Rebuild)
#   JOINER_STARTS=1 the joiner's controller takes the menus and presses START, then crouches in the level
#   (scripts/pad/level-crouch.txt): each player reads their own controller and shows their own stance icon;
#   JOIN_SCRIPT=scripts/pad/level-strafe.txt with it steps sideways instead
#   HOST_SCRIPT=scripts/pad/level-quit.txt the host's controller pauses and quits instead; JOIN_SCRIPT without
#   JOINER_STARTS the joiner's pad while the host's starts
#   HOST_NAME / JOIN_NAME: the machines' net_name (one word), over their heads to the other;
#   JOIN_DEFAULT_NAMES=1 the joiner draws them as Player N (net_default_names)
#   LEAVER=1|2 LEAVE_AT=<seconds> that machine leaves the session then (NF_NET_LEAVE, as closing its window does);
#   LEAVE_HOW=kill kills it instead (a crash or a lost connection): the other must play on to the end
#   HOST_ENV / JOIN_ENV: more VAR=value words for that machine alone; HOST_UDATA / JOIN_UDATA: a folder of saves
#   on that machine's disk (as a PC's hdd/E/UDATA/45410026)
#   DUMP_TICKS="t1,t2,..." both machines write their memory at those session ticks too (mem_tick_<t>.bin and
#   _image.bin in their run folders: the same point of the game on both)
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
cd "$HERE/.."
SECS=${1:-220}
HC=${2:-13}
JC=${3:-4}
LEVEL=${LEVEL:-7000002}
[ "$SECS" -ge 150 ] || { echo "net-coop: give it at least 150 s (the level is up about 110 s in)"; exit 2; }
[ -d generated/game ] || { echo "net-coop: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-COOP FAIL: the harness did not build (the error above)"
    exit 1
  }

RUN=.local/coop-run
rm -rf "$RUN"
mkdir -p "$RUN/1" "$RUN/2"
# HOST_UDATA / JOIN_UDATA: a folder of saves (the title's E:\UDATA\45410026, its profile folders) on that machine's
# disk before boot
for k in 1 2; do
  src=$([ $k = 1 ] && echo "${HOST_UDATA:-}" || echo "${JOIN_UDATA:-}")
  [ -n "$src" ] || continue
  dst="$RUN/$k/$([ $k = 1 ] && echo hdd || echo hdd2)/E/UDATA/45410026"
  mkdir -p "$dst" && cp -r "$src"/. "$dst"/
done
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
launch() {  # $1 = machine, $2 = pad, then the env
  local k=$1 pad=$2
  shift 2
  (
    cd "$RUN/$k"
    # not the game's port (26565): a real session on this PC must not hear the test machines, nor they it
    export INSTANCE=$k SCRIPT="$pad" NF_SKIP_INTRO=1 NF_SKIP_TUTORIAL=0 NF_LEVEL=$LEVEL NF_NET_PORT=${NET_PORT:-26575}
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
HOST_PAD=$(realpath "${HOST_SCRIPT:-scripts/pad/level.txt}") JOIN_PAD=$(realpath "${JOIN_SCRIPT:-scripts/pad/none.txt}")
[ "${JOINER_STARTS:-0}" = 1 ] && HOST_PAD=$(realpath scripts/pad/none.txt) JOIN_PAD=$(realpath "${JOIN_SCRIPT:-scripts/pad/level-crouch.txt}")
LEAVER=${LEAVER:-0} LEAVE_AT=${LEAVE_AT:-150} LEAVE_HOW=${LEAVE_HOW:-leave}
leave1= leave2=
[ "$LEAVE_HOW" = leave ] && [ "$LEAVER" = 1 ] && leave1="NF_NET_LEAVE=$LEAVE_AT"
[ "$LEAVE_HOW" = leave ] && [ "$LEAVER" = 2 ] && leave2="NF_NET_LEAVE=$LEAVE_AT"
launch 1 "$HOST_PAD" NF_NET=host NF_NET_WAIT=45 NF_NET_PLAYERS=2 NF_NET_DELAY=2 NF_NET_CHARACTER=$HC ${HOST_NAME:+NF_NET_NAME=$HOST_NAME} $leave1 ${HOST_ENV:-}
pid1=$!
launch 2 "$JOIN_PAD" NF_NET=join,127.0.0.1 NF_NET_WAIT=45 NF_NET_CHARACTER=$JC ${JOIN_NAME:+NF_NET_NAME=$JOIN_NAME} ${JOIN_DEFAULT_NAMES:+NF_NET_DEFAULT_NAMES=1} $leave2 ${JOIN_ENV:-}
pid2=$!
[ -n "${DUMP_TICKS:-}" ] && echo "$DUMP_TICKS" > "$RUN/1/dumptick" && echo "$DUMP_TICKS" > "$RUN/2/dumptick"
if [ "$LEAVER" != 0 ]; then
  [ "$LEAVE_HOW" = kill ] && { sleep "$LEAVE_AT"; kill -9 "$( [ "$LEAVER" = 1 ] && echo $pid1 || echo $pid2 )"; }
  other=$((3 - LEAVER))
  echo "net-coop: machine $LEAVER leaves ($LEAVE_HOW) at about $LEAVE_AT s; machine $other plays on to $SECS s"
  sleep $((SECS - $( [ "$LEAVE_HOW" = kill ] && echo "$LEAVE_AT" || echo 0 )))
  kill $(jobs -p) 2>/dev/null
  wait 2>/dev/null
  grep -hE 'net: (player . left|the host is gone|leaving)' "$RUN"/*/logs/*.log | sed 's/^.*\] //'
  last=$(grep -h 'renderer: frame' "$RUN/$other/logs/"*.log | tail -1)
  echo "machine $other: ${last#*] }"
  t=$(echo "$last" | sed 's/^\[ *\([0-9]*\).*/\1/')
  if grep -qE 'net: (player . left|the host is gone)' "$RUN/$other/logs/"*.log && [ "${t:-0}" -ge $((SECS - 5)) ]; then
    echo "NET-COOP OK: machine $other played on after machine $LEAVER left"
    exit 0
  fi
  echo "NET-COOP FAIL: machine $other did not play on after machine $LEAVER left"
  exit 1
fi

echo "net-coop: two machines from the title into level $LEVEL, characters $HC and $JC; comparing at $((SECS - 10)) s"
sleep $((SECS - 25))
touch "$RUN/1/snap" "$RUN/2/snap"
sleep 7
# a dump lands on the next 30th frame after its file appears, so a machine half a second behind dumps a frame earlier:
# a few dumps each, and the newest frame both have is compared
for i in 1 2 3 4 5 6; do
  touch "$RUN/1/memdump" "$RUN/2/memdump"
  sleep 3
done
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null

ok=true
for k in 1 2; do
  echo "--- machine $k:"
  grep -hE 'net: booting with|NF_LEVEL|co-op character|co-op player . read|side steps|hands:|model set|not in this level|\[error\]' \
    "$RUN/$k/logs/"*.log 2>/dev/null | sed 's/^.*\] //' | head -20
  for ch in $HC $JC; do
    [ "$ch" -ge 2 ] || continue
    grep -q "co-op character $ch (.*registered" "$RUN/$k/logs/"*.log 2>/dev/null || {
      echo "machine $k did not register character $ch"
      ok=false
    }
  done
  ls "$RUN/$k"/snap_*.tga 2>/dev/null
done
frame=$(comm -12 <(ls "$RUN/1" | grep -E '^mem_[0-9]+_phys\.bin$' | sort) \
  <(ls "$RUN/2" | grep -E '^mem_[0-9]+_phys\.bin$' | sort) | sed 's/mem_\([0-9]*\)_.*/\1/' | sort -n | tail -1)
[ -n "$frame" ] || { echo "NET-COOP FAIL: no memory dump of the same frame (the runs' logs: $RUN/{1,2}/logs)"; exit 1; }
a=$RUN/1/mem_${frame}_phys.bin
b=$RUN/2/mem_${frame}_phys.bin
echo "comparing frame $frame"
frames1=$(grep 'renderer: frame' "$RUN/1/logs/"*.log 2>/dev/null | tail -1 | sed 's/^.*renderer:/renderer:/')
frames2=$(grep 'renderer: frame' "$RUN/2/logs/"*.log 2>/dev/null | tail -1 | sed 's/^.*renderer:/renderer:/')
echo "machine 1: $frames1"
echo "machine 2: $frames2"
rc=0
python3 "$HERE/net-diff.py" "$a" "$b" || rc=$?
[ "$ok" = true ] || { echo "NET-COOP FAIL: a chosen character was not registered"; exit 1; }
if [ "$rc" -eq 0 ] || [ "$rc" -eq 2 ]; then
  echo "NET-COOP OK: two machines played level $LEVEL as characters $HC and $JC and ended the same game"
elif [ "$rc" -eq 3 ] && [ -n "$frames1" ] && [ "${frames1%%,*}" = "${frames2%%,*}" ]; then
  echo "NET-COOP OK WITH THE KNOWN CLASSES: the level stayed in lockstep, the wall clock's bytes differing (docs/online.md)"
else
  echo "NET-COOP FAIL: the machines desynced (docs/online.md)"
  exit 1
fi
