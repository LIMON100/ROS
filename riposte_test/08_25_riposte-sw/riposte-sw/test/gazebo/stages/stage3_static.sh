#!/usr/bin/env bash
# Stage 3 — 정지 대상 유도 (S-G1: hover target, simplest guidance convergence).
#
# First stage with the FULL chain (bridge -> GuidanceSource PN -> SafetyMonitor
# -> Offboard). The target is stopped and pinned at a known pose so the only
# thing under test is whether PN converges on a STATIONARY target — no lead, no
# maneuver. If Stage 3 fails but Stage 1/2 pass, the fault is in the guidance
# law or the bridge->guidance coupling.
set -u
. "$(dirname "$0")/gz_lib.sh"
gz_setup
gz_arm_ladder

CONTACT_M=1.5
CLOSE_M=5   # min range reduction to count as convergence if no contact

gz_write_ini "$TESTDIR/obc.ini" guidance
gz_start_px4 || exit $?
# Pin the target as a static target ~12 m out before it drifts on its default
# crossing velocity.
gz_target_place 12 0 3
gz_ok "target pinned static at (12,0,3)"
gz_start_bridge || exit $?
gz_start_obc "$TESTDIR/obc.ini" || exit $?
gz_arm_takeoff || exit $?
[ "${GUI_FOLLOW:-0}" = "1" ] && gz_follow_camera "$OWNSHIP"

# Re-pin just before engage (kills any residual drift accumulated during takeoff).
gz_target_place 12 0 3
N_ENG=$(gz_range_count)
gz_engage || exit $?
R0=$(gz_range_after "$N_ENG")
gz_log "static-target tracking, range0=${R0:-?}m — observing 20 s"
sleep 20

RMIN=$(gz_range_after "$N_ENG")
R1=$(gz_range_last)
echo "-- range: ${R0:-?}m -> ${R1:-?}m (min ${RMIN:-?}m)"
CONTACT=$("$PYTHON" -c "print(1 if ${RMIN:-99} <= ${CONTACT_M} else 0)" 2>/dev/null)
if [ "$CONTACT" = "1" ]; then
    gz_ok "converged to contact on static target (min ${RMIN}m)"
    grep -q "DISENGAGING -> READY" "$TESTDIR/obc.log" || gz_disengage || exit $?
else
    CLOSED=$("$PYTHON" -c "print(1 if (${R0:-0}-${RMIN:-0}) >= ${CLOSE_M} else 0)" 2>/dev/null)
    [ "$CLOSED" = "1" ] || { echo "NO_CONVERGENCE on static target"; tail -15 "$TESTDIR/obc.log"; exit 6; }
    grep -q "safety violation" "$TESTDIR/obc.log" && { echo "UNEXPECTED_VIOLATION"; grep "safety violation" "$TESTDIR/obc.log"; exit 6; }
    gz_ok "closed ≥${CLOSE_M}m on static target, no violation"
    gz_disengage || exit $?
fi
echo "STAGE3_STATIC_PASS"
