#!/usr/bin/env bash
# SIM-2.3 — a target drone CHANGES HEADING ON CUE, mid-scenario.
#
# Client item 3 asks for configurable speeds, directions AND trajectories. Speed
# and heading are answered by the TARGETS table in make_swarm_world.py; a
# TRAJECTORY is a heading that changes DURING the run. S-G3 already does this to
# its single target (run_gazebo_evasive.sh: one gz topic publish on
# /model/<name>/cmd_vel, triggered on RANGE rather than on time so the turn
# always lands during tracking). This applies the same pattern to one drone_* of
# the swarm and then PROVES the turn happened.
#
# The proof is drift-independent, deliberately: heading is taken as the
# direction of MEASURED DISPLACEMENT before and after the command, so it does not
# depend on when sampling started or on the sim clock rate (the 19.4 fix-3
# lesson — assert the invariant, not the bench geometry).
#
#   PX4_BUILD=... RIPOSTE_BUILD=... MAVSDK_LIB=... \
#     bash test/gazebo/tools/run_swarm_maneuver.sh
#
# Optional: MANEUVER_TARGET (default drone_2) · TURN_VX / TURN_VY · TURN_AT_M
#           MIN_TURN_DEG (default 60) · HEADLESS=1

set -u
export LC_ALL=C
: "${PX4_BUILD:?set PX4_BUILD}"
: "${RIPOSTE_BUILD:?set RIPOSTE_BUILD}"

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
GZ_DIR=$(cd "$HERE/.." && pwd)
PYTHON=${PYTHON:-python3}

export HEADLESS=${HEADLESS:-1}
export WORLD=riposte_swarm
export TG='drone_*'
export TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-swarm-mvr.XXXXXX)}

TARGET=${MANEUVER_TARGET:-drone_2}
TURN_VX=${TURN_VX:-0.0}
TURN_VY=${TURN_VY:-0.45}
TURN_AT_M=${TURN_AT_M:-25}     # cue: engagement has closed to this range
MIN_TURN_DEG=${MIN_TURN_DEG:-60}
SAMPLE_S=${SAMPLE_S:-6}        # displacement window; long enough to beat noise
SETTLE_S=${SETTLE_S:-3}        # let the new command take effect before sampling

echo "logs: $TESTDIR"
echo "maneuver: $TARGET -> linear($TURN_VX, $TURN_VY) at range <= ${TURN_AT_M}m"

# --- world + sweep (single-instance rule) --------------------------------------
"$PYTHON" "$HERE/make_swarm_world.py" > /dev/null || exit 2
pkill -f "gz sim" 2>/dev/null; pkill -f px4 2>/dev/null
pkill -f gz_track_bridge 2>/dev/null; pkill -f riposte-obc 2>/dev/null
rm -f /dev/shm/riposte_* 2>/dev/null
sleep 8

# --- the proven scenario drives everything; we only inject ---------------------
bash "$GZ_DIR/run_gazebo_balloon.sh" > "$TESTDIR/scenario.log" 2>&1 &
SCEN_PID=$!

# pose of one model as "x y z", or empty
pose() { gz model -m "$1" -p 2>/dev/null | grep -A1 "Pose \[" | tail -1 | tr -d '[]'; }

die() { echo "$1"; kill "$SCEN_PID" 2>/dev/null; exit "$2"; }

wait_for() {  # wait_for <file> <pattern> <tries>
    for _ in $(seq 1 "$3"); do
        [ -f "$1" ] && grep -q "$2" "$1" && return 0
        kill -0 "$SCEN_PID" 2>/dev/null || return 1
        sleep 1
    done
    return 1
}

wait_for "$TESTDIR/scenario.log" "PATROL_STARTED" 180 \
    || { echo "PATROL_NEVER_STARTED"; tail -20 "$TESTDIR/scenario.log"; exit 3; }
echo "PATROL_SEEN"

# --- cue: trigger on RANGE, as S-G3 does, not on elapsed time ------------------
TRIGGERED=0
for _ in $(seq 1 120); do
    R=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" 2>/dev/null \
        | tail -1 | cut -d= -f2)
    IN=$("$PYTHON" -c "print(1 if ${R:-99} <= $TURN_AT_M else 0)")
    [ "$IN" = "1" ] && { TRIGGERED=1; break; }
    sleep 1
done
[ "$TRIGGERED" = "1" ] || die "CUE_NEVER_REACHED (range > ${TURN_AT_M}m)" 4
echo "CUE_REACHED range=${R}m"

# --- heading BEFORE ------------------------------------------------------------
P0=$(pose "$TARGET"); sleep "$SAMPLE_S"; P1=$(pose "$TARGET")
[ -n "$P0" ] && [ -n "$P1" ] || die "POSE_READ_FAILED ($TARGET)" 5

# --- inject the turn -----------------------------------------------------------
gz topic -t "/model/$TARGET/cmd_vel" -m gz.msgs.Twist \
    -p "linear: {x: $TURN_VX, y: $TURN_VY, z: 0.0}" \
    || die "TURN_CMD_FAILED" 5
echo "SWARM_MANEUVER_INJECTED target=$TARGET vel=($TURN_VX,$TURN_VY)"

# --- heading AFTER -------------------------------------------------------------
sleep "$SETTLE_S"
P2=$(pose "$TARGET"); sleep "$SAMPLE_S"; P3=$(pose "$TARGET")
[ -n "$P2" ] && [ -n "$P3" ] || die "POSE_READ_FAILED_POST" 5

TURN=$("$PYTHON" - "$P0" "$P1" "$P2" "$P3" <<'PY'
import math, sys

def v(a, b):
    ax, ay, _ = map(float, a.split())
    bx, by, _ = map(float, b.split())
    return bx - ax, by - ay
dx0, dy0 = v(sys.argv[1], sys.argv[2])
dx1, dy1 = v(sys.argv[3], sys.argv[4])
# A displacement too small to have a direction is noise, not a heading.
if math.hypot(dx0, dy0) < 0.5 or math.hypot(dx1, dy1) < 0.5:
    print("-1 0.0 0.0"); raise SystemExit
h0 = math.degrees(math.atan2(dy0, dx0))
h1 = math.degrees(math.atan2(dy1, dx1))
d = abs((h1 - h0 + 180.0) % 360.0 - 180.0)
print("%.1f %.1f %.1f" % (d, h0, h1))
PY
)
read -r TURN_DEG H_PRE H_POST <<<"$TURN"
echo "heading: ${H_PRE} deg -> ${H_POST} deg  (change ${TURN_DEG} deg)"

# --- let the scenario finish and take its verdict too --------------------------
wait "$SCEN_PID"; SCEN_RC=$?
LOG="$TESTDIR/scenario.log"
grep -q "GAZEBO_BALLOON_PASS" "$LOG" || {
    echo "SCENARIO_FAILED rc=$SCEN_RC"; tail -15 "$LOG"; exit 6; }
echo "SCENARIO_OK"
grep -m1 "^patrol:" "$LOG"
grep -m1 "DISTINCT_TARGETS" "$LOG"

[ "$TURN_DEG" = "-1" ] && { echo "TARGET_NOT_MOVING"; exit 7; }
BIG=$("$PYTHON" -c "print(1 if $TURN_DEG >= $MIN_TURN_DEG else 0)")
[ "$BIG" = "1" ] || {
    echo "TURN_TOO_SMALL (${TURN_DEG} < ${MIN_TURN_DEG} deg)"; exit 7; }

echo "SIM23_TRAJECTORY_PASS"
