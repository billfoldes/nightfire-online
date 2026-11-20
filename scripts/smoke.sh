#!/usr/bin/env bash
# shared: sdk/scripts/smoke.sh (this game's values in scripts/game.env)
# the route is the first boot's: START at the title relaunches into Driving.xbe's Paris, which skip_tutorial skips
export NF_SKIP_TUTORIAL=0
# and its pad script is timed for the logo movies, which skip_intro (on by default) skips
export NF_SKIP_INTRO=0
exec "$(dirname "$0")/../sdk/scripts/smoke.sh" "$@"
