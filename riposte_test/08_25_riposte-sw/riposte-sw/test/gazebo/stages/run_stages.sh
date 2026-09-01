#!/usr/bin/env bash
# run_stages.sh — staged Gazebo bring-up ladder for the Riposte UAS-tracking
# stack. Runs the development-stage verifications in order, each isolating one
# subsystem, so a regression tells you WHICH layer broke:
#
#   0 env       world / PX4 / sensors / target motion      (no riposte, no MAVSDK)
#   1 bridge    gz_track_bridge pose->TrackBus + staleness    (no flight)
#   2 offboard  FSM + SafetyMonitor lifecycle (hover, no tgt) (full flight)
#   3 static    PN convergence on a pinned target (S-G1)      (full flight)
#   4 crossing  moving-target tracking (S-G2)                  (full flight)
#   5 evasive   mid-tracking turn (S-G3)                       (full flight)
#   6 safety    SM-7 track-stale auto-egress (fault inject)   (full flight)
#
# Usage:
#   PX4_BUILD=… RIPOSTE_BUILD=… [MAVSDK_LIB=…] [HEADLESS=1] \
#     run_stages.sh [stages]
#     stages: space/comma list or range, e.g. "0 1 2", "0-3", "all" (default).
#
# Each stage runs as its own process (fresh sim, own TESTDIR). The ladder stops
# at the first failing stage (a lower stage failing makes higher stages
# meaningless). Set KEEP_GOING=1 to run every requested stage regardless.
set -u
DIR=$(cd "$(dirname "$0")" && pwd)
export LC_ALL=C
: "${PX4_BUILD:?set PX4_BUILD}"; : "${RIPOSTE_BUILD:?set RIPOSTE_BUILD}"

declare -A NAME=(
  [0]=stage0_env [1]=stage1_bridge [2]=stage2_offboard [3]=stage3_static
  [4]=stage4_crossing [5]=stage5_evasive [6]=stage6_safety)
ORDER=(0 1 2 3 4 5 6)

# --- parse requested stages ----------------------------------------------------
req=${*:-all}
req=${req//,/ }
if [ "$req" = "all" ]; then
    STAGES=("${ORDER[@]}")
elif [[ "$req" =~ ^([0-6])-([0-6])$ ]]; then
    STAGES=(); for ((s=BASH_REMATCH[1]; s<=BASH_REMATCH[2]; s++)); do STAGES+=("$s"); done
else
    STAGES=($req)
fi

ROOT=${TESTROOT:-$(mktemp -d /tmp/riposte-ladder.XXXXXX)}
mkdir -p "$ROOT"
echo "ladder logs: $ROOT"
echo "stages: ${STAGES[*]}"
echo

declare -A RESULT
overall=0
for s in "${STAGES[@]}"; do
    scr="$DIR/${NAME[$s]}.sh"
    [ -f "$scr" ] || { echo "?? unknown stage $s"; continue; }
    echo "==================== STAGE $s (${NAME[$s]#stage*_}) ===================="
    TESTDIR="$ROOT/stage$s" bash "$scr" 2>&1 | tee "$ROOT/stage$s.out"
    rc=${PIPESTATUS[0]}
    if [ "$rc" -eq 0 ]; then RESULT[$s]="PASS"; else RESULT[$s]="FAIL(rc=$rc)"; overall=1; fi
    echo "---- stage $s: ${RESULT[$s]} ----"; echo
    if [ "$rc" -ne 0 ] && [ "${KEEP_GOING:-0}" != "1" ]; then
        echo "stopping at first failure (set KEEP_GOING=1 to continue)"; break
    fi
done

echo "==================== LADDER SUMMARY ===================="
for s in "${STAGES[@]}"; do printf "  stage %s %-9s : %s\n" "$s" "${NAME[$s]#stage*_}" "${RESULT[$s]:-skipped}"; done
[ "$overall" -eq 0 ] && echo "LADDER_ALL_PASS" || echo "LADDER_HAD_FAILURES"
exit "$overall"
