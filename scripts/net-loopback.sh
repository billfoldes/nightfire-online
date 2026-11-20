#!/usr/bin/env bash
# The multiplayer match test (docs/online.md): two headless machines meeting over loopback, one session, both
# booting straight to the main menu (the bootstrap's launch data - scripts/net-bootstrap.sh, run first if it is
# missing), both players' pads pressing scripts/pad/mp-match.txt - mp_bots.txt's proven walk through
# Multiplayer's join, codename, ready and scenario screens - and the match with its bot running in lockstep.
# One START once the match is up, and the machines' whole memories compared: the verdict everything before
# was building to.
#   scripts/net-loopback.sh [seconds, default 270; at least 240, the match starts about 185 s in]
#   The harness audio stays wall-clocked (its tick-paced mode gridlocks stream-heavy screens); the comparison
#   is scripts/net-diff.py's: the kernel's clock megabyte is named, one contiguous run reads as the audio ring.
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)   # absolute: the script cd's to the repo root below
cd "$HERE/.."
SECS=${1:-270}
[ "$SECS" -ge 240 ] || { echo "net-loopback: give it at least 240 s (the match starts about 185 s in)"; exit 2; }
[ -d generated/game ] || { echo "net-loopback: no generated/game; run scripts/linux-tools.sh, then scripts/recompile.sh"; exit 2; }
if [ ! -f .local/menu-launch.bin ]; then
  echo "NET-LOOPBACK FAIL: no .local/menu-launch.bin"
  echo "the fastest way to make it: play your own game while this watches -"
  echo "  scripts/grab-menu-launch.sh <your game folder>   (boot, START, pause the level, QUIT TO MAIN MENU)"
  echo "(scripts/net-bootstrap.sh can also try it headlessly, but the game's scripted content makes that a fight)"
  exit 2
fi
CXX="$(command -v ccache >/dev/null && echo 'ccache clang++' || echo clang++)" \
  bash sdk/harness/build.sh -r . -g generated/game -o .local/harness -n game -G .local/harness/gen-game -x '/src/driving/' || {
    echo "NET-LOOPBACK FAIL: the harness did not build (the error above)"
    exit 1
  }
[ -x .local/harness/game ] || { echo "NET-LOOPBACK FAIL: no harness exe at .local/harness/game"; exit 1; }

RUN=.local/loop-run
rm -rf "$RUN"   # a fresh run: the machines' disks seeded with the bootstrap's, its launch data in place
mkdir -p "$RUN/1" "$RUN/2"
for k in 1 2; do
  h="$RUN/$k/hdd"
  [ "$k" = 2 ] && h="$RUN/$k/hdd2"
  cp -r .local/menu-hdd "$h"
  cp .local/menu-launch.bin "$h/launch.bin"
done
GAME=$(realpath assets)
EXE=$(realpath .local/harness/game)
# split-screen, so the machines draw the same frames and the whole memory compares: net_fullscreen (on by default)
# has each draw its own player's view, its frame buffers and push buffers apart by design (NF_NET_FULLSCREEN=1 to
# run it so; the comparison then differs there alone)
export NF_NET_FULLSCREEN=${NF_NET_FULLSCREEN:-0}
launch() {  # $1 = machine, $2 = pad, then the env
  local k=$1 pad=$2
  shift 2
  (
    cd "$RUN/$k"
    # not the game's port (26565): a real session on this PC must not hear the test machines, nor they it
    export INSTANCE=$k SCRIPT="$pad" NF_SKIP_INTRO=1 NF_NET_TRACE=1 NF_NET_PORT=${NET_PORT:-26575}
    exec env "$@" "$EXE" "$GAME"
  ) > "$RUN/$k/run.out" 2>&1 &
}
# HOST_NAME / JOIN_NAME: the machines' net_name (one word); HOST_HANDICAP / JOIN_HANDICAP: their net_handicap (0..8);
# JOIN_CHANGE=tick,name,handicap: the joiner changes both at that tick of the session (NF_NET_CHANGE)
# HOST_PAD / JOIN_PAD: other pads for the two (scripts/pad/mp-match-stay.txt and mp-quit.txt: the joiner quits the match)
# HOST_PLAYERS: the host's net_players (seats; 2 by default: a larger one starts with the two when its HOST_WAIT ends);
# HOST_ENV: more of the host's settings (NF_NET_BOT_CHARACTER=12 ...)
launch 1 "$(realpath "${HOST_PAD:-scripts/pad/mp-match.txt}")" NF_NET=host NF_NET_WAIT=${HOST_WAIT:-45} NF_NET_PLAYERS=${HOST_PLAYERS:-2} NF_NET_DELAY=2 ${HOST_ENV:-} ${HOST_NAME:+NF_NET_NAME=$HOST_NAME} ${HOST_HANDICAP:+NF_NET_HANDICAP=$HOST_HANDICAP}
launch 2 "$(realpath "${JOIN_PAD:-scripts/pad/mp-match.txt}")" NF_NET=join,127.0.0.1 NF_NET_WAIT=45 ${JOIN_NAME:+NF_NET_NAME=$JOIN_NAME} ${JOIN_HANDICAP:+NF_NET_HANDICAP=$JOIN_HANDICAP} ${JOIN_CHANGE:+NF_NET_CHANGE=$JOIN_CHANGE}

echo "net-loopback: two machines meeting at the main menu, into Multiplayer and a bot match; comparing at $((SECS - 10)) s"
sleep $((SECS - 30))
touch "$RUN/1/snap" "$RUN/2/snap"  # the reference renderer's next frames: the match, both machines
sleep 15
touch "$RUN/1/memdump" "$RUN/2/memdump"
sleep 10
kill $(jobs -p) 2>/dev/null
wait 2>/dev/null

for k in 1 2; do
  echo "--- machine $k's session and match:"
  grep -E 'net: booting with|of the menus|a menu is up|relaunching into' "$RUN/$k/logs/"*.log 2>/dev/null | head -8
  grep 'renderer: frame' "$RUN/$k/logs/"*.log 2>/dev/null | tail -2
  echo "--- machine $k's screen at $((SECS - 30)) s:"
  ls "$RUN/$k"/snap_*.tga 2>/dev/null
done
a=$(ls -t "$RUN/1"/mem_*_phys.bin 2>/dev/null | head -1)
b=$(ls -t "$RUN/2"/mem_*_phys.bin 2>/dev/null | head -1)
[ -n "$a" ] && [ -n "$b" ] || {
  echo "NET-LOOPBACK FAIL: no memory dump (the runs' logs: $RUN/{1,2}/logs)"
  for k in 1 2; do
    echo "--- machine $k, $RUN/$k/run.out:"
    tail -8 "$RUN/$k/run.out" 2>/dev/null
    echo "--- machine $k, the log's last lines:"
    tail -8 "$RUN/$k/logs/"*.log 2>/dev/null
  done
  exit 1
}
if ! command -v python3 >/dev/null; then
  echo "NET-LOOPBACK FAIL: python3 is needed for the comparison (scripts/net-diff.py)"
  exit 1
fi
# the machines' own evidence: their frame and draw counts, the whole run in one line each (the timestamp and
# thread before them are the machines' own - stripped, or no two would ever match)
frames1=$(grep 'renderer: frame' "$RUN/1/logs/"*.log 2>/dev/null | tail -1 | sed 's/^.*renderer:/renderer:/')
frames2=$(grep 'renderer: frame' "$RUN/2/logs/"*.log 2>/dev/null | tail -1 | sed 's/^.*renderer:/renderer:/')
echo "machine 1: $frames1"
echo "machine 2: $frames2"
lockstep_draws=false
[ "$frames1" = "$frames2" ] && [ -n "$frames1" ] && lockstep_draws=true
rc=0
python3 "$HERE/net-diff.py" "$a" "$b" || rc=$?
if [ "$rc" -eq 0 ]; then
  echo "NET-LOOPBACK OK: two machines played one multiplayer match in session and ended the same game"
elif [ "$rc" -eq 2 ]; then
  echo "NET-LOOPBACK OK except the audio ring: two machines played one multiplayer match and ended the same game"
elif [ "$rc" -eq 3 ] && [ "$lockstep_draws" = true ]; then
  echo "NET-LOOPBACK OK WITH THE KNOWN CLASSES: the match stayed in lockstep (frames and draws identical),"
  echo "the wall clock's ring and ambient bytes differing (docs/online.md)"
else
  echo "NET-LOOPBACK FAIL: the match desynced (docs/online.md)"
  exit 1
fi
