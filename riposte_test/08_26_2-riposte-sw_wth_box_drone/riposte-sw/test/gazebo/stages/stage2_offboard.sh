#!/usr/bin/env bash
# Stage 2 — 오프보드 수명주기 (FSM + safety, NO guidance/target).
#
# Verifies the flight-control CONTROL PLANE in the gz environment, decoupled
# from perception/guidance: the OBC runs a HOVER setpoint source (zero velocity,
# station-keeping), so the run exercises only:
#   arm/takeoff -> engage -> OFFBOARD_ACTIVE -> hold -> disengage -> READY,
# with the SafetyMonitor watching. A clean hover must NOT trip any violation.
# If Stage 2 fails but Stage 0/1 pass, the fault is in FcuLink / the FSM / the
# safety monitor — not perception or guidance.
set -u
. "$(dirname "$0")/gz_lib.sh"
gz_setup
gz_arm_ladder

gz_write_ini "$TESTDIR/obc.ini" hover
gz_start_px4 || exit $?
# No bridge needed: the hover source ignores the track bus. (Bridge omitted so a
# missing TrackBus cannot mask an FCU/FSM fault.)
gz_start_obc "$TESTDIR/obc.ini" || exit $?
gz_arm_takeoff || exit $?
gz_engage || exit $?
gz_follow_camera "$OWNSHIP"   # GUI only (GUI_FOLLOW=1); no-op headless

gz_log "holding hover 8 s under SafetyMonitor…"
sleep 8

tail -3 "$TESTDIR/obc.log" | grep -q "state=OFFBOARD_ACTIVE" \
    || { echo "DROPPED_OUT_OF_OFFBOARD during benign hover"; tail -15 "$TESTDIR/obc.log"; exit 6; }
if grep -q "safety violation" "$TESTDIR/obc.log"; then
    echo "UNEXPECTED_VIOLATION during hover"; grep "safety violation" "$TESTDIR/obc.log"; exit 6
fi
gz_ok "held OFFBOARD_ACTIVE 8 s, zero violations"

gz_disengage || exit $?
echo "STAGE2_OFFBOARD_PASS"
