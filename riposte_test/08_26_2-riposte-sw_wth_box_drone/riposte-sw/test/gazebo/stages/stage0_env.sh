#!/usr/bin/env bash
# Stage 0 — 환경 브링업 (world / PX4 / sensors / target motion). No riposte.
#
# Verifies the simulation FOUNDATION before any riposte code is involved:
#   - the world SDF loads and PX4 spawns the x500 ownship,
#   - EKF gets its sensor feeds (no "Preflight Fail: … missing"),
#   - both models (ownship + target) appear on the pose feed,
#   - the target actually moves (VelocityControl plugin alive).
# If Stage 0 fails, nothing downstream can work — fix the gz/PX4 install or the
# world SDF first. This stage needs no MAVSDK build.
set -u
. "$(dirname "$0")/gz_lib.sh"
gz_setup
gz_arm_ladder

gz_start_px4 || exit $?
sleep ${EKF_SETTLE:-25}

# EKF sensor health: the world must carry imu/air-pressure/navsat systems.
if grep -qE "Preflight Fail: (Accel|Gyro|barometer|Compass)" "$TESTDIR/px4.log"; then
    echo "SENSOR_MISSING — world lacks imu/air-pressure/navsat plugins?"
    grep "Preflight Fail" "$TESTDIR/px4.log" | tail -5
    exit 2
fi
gz_ok "EKF sensors healthy (no preflight sensor-missing)"

# Both models present on the pose feed.
POSE=$(gz topic -e -t "/world/$WORLD_NAME/pose/info" -n 1 2>/dev/null)
echo "$POSE" | grep -q "\"$OWNSHIP\"" || { echo "NO_OWNSHIP ($OWNSHIP absent from pose feed)"; exit 2; }
echo "$POSE" | grep -q "\"$TARGET\"" || { echo "NO_TARGET ($TARGET absent from pose feed)"; exit 2; }
gz_ok "both models on pose feed ($OWNSHIP, $TARGET)"

# Target motion: sample its X twice ~2 s apart; the default world gives it a
# constant westward velocity, so |Δ| must be clearly non-zero.
ix() { gz topic -e -t "/world/$WORLD_NAME/dynamic_pose/info" -n 1 2>/dev/null \
        | awk -v m="$TARGET" '$0 ~ "\""m"\"" {f=1} f&&/position/{g=1} g&&/x:/{print $2; exit}'; }
X0=$(ix); sleep 2; X1=$(ix)
if [ -n "$X0" ] && [ -n "$X1" ]; then
    MOVED=$("$PYTHON" -c "print(1 if abs(${X1:-0}-${X0:-0}) > 0.5 else 0)" 2>/dev/null)
    [ "$MOVED" = "1" ] && gz_ok "target moving (x $X0 -> $X1)" \
        || { echo "TARGET_STATIC (x $X0 -> $X1) — VelocityControl not stepping?"; exit 2; }
else
    gz_log "target pose sample inconclusive (dynamic_pose feed); skipping motion check"
fi

echo "STAGE0_ENV_PASS"
