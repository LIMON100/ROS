#!/usr/bin/env bash
# Riposte UAS-tracking EVASIVE-TARGET test in Gazebo (S-G3, gz-sim 8 / Harmonic).
#
# Same stack as run_gazebo_closure.sh, but mid-tracking the target is
# commanded a sharp (>90 deg) turn via the gz VelocityControl runtime topic
# (/model/target/cmd_vel). Automated form of the formerly-manual S-G3.
#
# Pass shapes (doc RIPOSTE-GAZEBO-TEST-001 S-G3):
#  a) RECONVERGENCE: PN re-converges after the maneuver and reaches contact
#     range (<= CONTACT_M), then the system ends safe (commanded disengage or
#     safety auto-disengage back to READY).
#  b) SAFE EXIT: no re-convergence, but SafetyMonitor auto-disengages to READY
#     (safe-side exit, e.g. SM-7 track stale / SM-8 timebox).
#
# Requirements: identical to run_gazebo_closure.sh
#   PX4_BUILD, RIPOSTE_BUILD, PYTHON (pymavlink); optional MAVSDK_LIB, HEADLESS=1
set -u
export LC_ALL=C # sort -n on float ranges is locale-sensitive
: "${PX4_BUILD:?set PX4_BUILD to a gz-capable px4_sitl build dir}"
: "${RIPOSTE_BUILD:?set RIPOSTE_BUILD to a RIPOSTE_WITH_MAVSDK=ON,RIPOSTE_WITH_GZ=ON build}"
PYTHON=${PYTHON:-python3}
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SITL_DIR="$SCRIPT_DIR/../sitl"
TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-gz-ev.XXXXXX)}
mkdir -p "$TESTDIR"
[ -n "${MAVSDK_LIB:-}" ] && export LD_LIBRARY_PATH=$MAVSDK_LIB:${LD_LIBRARY_PATH:-}
export GZ_SIM_RESOURCE_PATH="$SCRIPT_DIR/worlds:${GZ_SIM_RESOURCE_PATH:-}"
echo "logs: $TESTDIR"

EVADE_AT_M=12        # command the turn when tracking closes to this range —
                     # margin above CONTACT_M so contact cannot land in the
                     # command-to-turn latency and pass the test untested
EVADE_MAX_WAIT_S=20  # give up waiting for the threshold after this long
EVADE_WINDOW_S=25    # post-maneuver observation window
EVADE_SKIP_N=2       # bridge log lines (~1 s at its 1 Hz cadence) to exclude
                     # right after the command: VelocityControl takes ~0.5-1.5 s
                     # to actually turn the target, so those samples still
                     # reflect the PRE-maneuver geometry
CONTACT_M=1.5
# sharp turn + speed-up vs S-G2 initial (-1.5, 0.6): heading change > 120 deg
EVADE_VEL_X=0.9
EVADE_VEL_Y=-2.4

PX4_PID=""
BRIDGE_PID=""
OBC_PID=""

cleanup() {
    # Kill the PIDs this run started first so a concurrent unrelated session's
    # processes are spared as far as practical; the pattern pkill below is the
    # fallback for children we did not spawn directly (gz sim is a PX4 child)
    # and for anything that outlived its parent.
    for pid in "$OBC_PID" "$BRIDGE_PID" "$PX4_PID"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    pkill -f "gz_track_bridge" 2>/dev/null
    pkill -f "bin/px4" 2>/dev/null
    pkill -f "gz sim" 2>/dev/null
    pkill -f "riposte-obc" 2>/dev/null
    # A leftover seeker would be a second WRITER on the TrackBus shm alongside
    # the bridge, silently corrupting the seqlock.
    pkill -f "riposte-seeker" 2>/dev/null
    rm -f /tmp/riposte-obc.sock
}
trap cleanup EXIT
# Startup sweep (documented single-instance assumption: this test owns the
# host's PX4/gz/riposte processes and clears any stale leftovers).
cleanup
sleep 1

cat > "$TESTDIR/riposte-gz.ini" <<EOF
[obc]
connection_url = udpin://0.0.0.0:14540
source = guidance
operator_token = sitl-test-token
cmd_socket = /tmp/riposte-obc.sock
rt_priority = 0
cpu_affinity = -1
[safety]
vmax_h = 8.0
vmax_v = 3.0
geofence_r = 300.0
alt_min = 1.0
alt_max = 120.0
engage_timebox_s = 90.0
EOF

# --- 1. PX4 SITL + Gazebo -------------------------------------------------------
# PX4 rcS sources rootfs/gz_env.sh which force-overrides PX4_GZ_WORLDS to the
# PX4 tree, so the world must be reachable there: link it in.
if [ -f "$PX4_BUILD/rootfs/gz_env.sh" ]; then
    # shellcheck disable=SC1091
    # PX4 v1.17+의 gz_env.sh 는 GZ_SIM_SYSTEM_PLUGIN_PATH 를 참조하는데, 이
    # 스크립트는 set -u 라 미정의 변수 참조가 치명 오류가 된다 (v1.15.4 에는
    # 없던 참조 — PX4 버전 호환). 소싱 전에 빈 기본값을 보장한다.
    export GZ_SIM_SYSTEM_PLUGIN_PATH="${GZ_SIM_SYSTEM_PLUGIN_PATH:-}"
    . "$PX4_BUILD/rootfs/gz_env.sh"
    [ -d "${PX4_GZ_WORLDS:-}" ] \
        && ln -sf "$SCRIPT_DIR/worlds/riposte_closure.sdf" "$PX4_GZ_WORLDS/"
fi
cd "$PX4_BUILD" || exit 2
rm -rf eeprom parameters*.bts log
[ "${HEADLESS:-0}" = "1" ] && export HEADLESS=1
PX4_SYS_AUTOSTART=4001 PX4_SIM_MODEL=gz_x500 PX4_GZ_WORLD=riposte_closure \
    ./bin/px4 -d -s etc/init.d-posix/rcS > "$TESTDIR/px4.log" 2>&1 &
PX4_PID=$!
for _ in $(seq 1 45); do
    grep -q "Startup script returned successfully" "$TESTDIR/px4.log" && break
    sleep 1
done
grep -q "Startup script returned successfully" "$TESTDIR/px4.log" \
    || { echo "PX4_START_FAILED"; tail -20 "$TESTDIR/px4.log"; exit 2; }
echo "PX4_GZ_UP"

# --- 2. gz_track_bridge ----------------------------------------------------------
"$RIPOSTE_BUILD/gz_track_bridge" riposte_closure x500_0 target \
    > "$TESTDIR/bridge.log" 2>&1 &
BRIDGE_PID=$!
for _ in $(seq 1 15); do
    grep -q "range=" "$TESTDIR/bridge.log" && break
    sleep 1
done
grep -q "range=" "$TESTDIR/bridge.log" \
    || { echo "BRIDGE_NO_TRACK"; tail -10 "$TESTDIR/bridge.log"; exit 2; }
echo "BRIDGE_TRACKING"

# --- 3. OBC (guidance) -----------------------------------------------------------
"$RIPOSTE_BUILD/riposte-obc" "$TESTDIR/riposte-gz.ini" > "$TESTDIR/obc.log" 2>&1 &
OBC_PID=$!
for _ in $(seq 1 30); do
    grep -q "state=READY" "$TESTDIR/obc.log" && break
    kill -0 $OBC_PID 2>/dev/null || { echo "OBC_DIED"; tail -20 "$TESTDIR/obc.log"; exit 3; }
    sleep 1
done
grep -q "state=READY" "$TESTDIR/obc.log" || { echo "OBC_READY_TIMEOUT"; exit 3; }
echo "OBC_READY"

# --- 4. arm + takeoff --------------------------------------------------------------
"$PYTHON" "$SITL_DIR/arm_takeoff.py" > "$TESTDIR/gcs.log" 2>&1
grep -q "READY_FOR_ENGAGE" "$TESTDIR/gcs.log" \
    || { echo "TAKEOFF_FAILED"; tail -20 "$TESTDIR/gcs.log"; exit 4; }
echo "AIRBORNE"

# --- 5. engage ---------------------------------------------------------------------
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token > /dev/null
for _ in $(seq 1 15); do
    grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" && break
    sleep 1
done
grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" \
    || { echo "ENGAGE_FAILED"; tail -30 "$TESTDIR/obc.log"; exit 5; }
R0=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -1 | cut -d= -f2)
echo "TRACKING_STARTED range0=${R0:-?}m"

# --- 6. mid-tracking evasive maneuver ------------------------------------------------
# Trigger on range, not time: the turn must happen DURING tracking, before
# contact (closing speed varies run to run).
TRIGGERED=0
for _ in $(seq 1 $((EVADE_MAX_WAIT_S * 2))); do
    R_NOW=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -1 | cut -d= -f2)
    IN_RANGE=$("$PYTHON" -c "print(1 if ${R_NOW:-99} <= ${EVADE_AT_M} else 0)" 2>/dev/null)
    [ "$IN_RANGE" = "1" ] && { TRIGGERED=1; break; }
    sleep 0.5
done
[ "$TRIGGERED" = "1" ] || { echo "EVADE_THRESHOLD_NEVER_REACHED"; exit 6; }
N_PRE=$(grep -c "range=" "$TESTDIR/bridge.log")
gz topic -t /model/target/cmd_vel -m gz.msgs.Twist \
    -p "linear: {x: ${EVADE_VEL_X}, y: ${EVADE_VEL_Y}, z: 0.0}" \
    || { echo "EVADE_CMD_FAILED"; exit 6; }
R_EV=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -1 | cut -d= -f2)
echo "EVASIVE_COMMANDED at range=${R_EV:-?}m vel=(${EVADE_VEL_X},${EVADE_VEL_Y})"

sleep "$EVADE_WINDOW_S"

# --- 7. pass criteria ---------------------------------------------------------------
# Start the post-maneuver slice at N_PRE + 1 + EVADE_SKIP_N: the first
# EVADE_SKIP_N samples after the command land in the command-to-turn latency
# window and still reflect pre-maneuver geometry — contact there would let the
# test pass without any turn having been tested.
RMIN_POST=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" \
    | tail -n "+$((N_PRE + 1 + EVADE_SKIP_N))" \
    | cut -d= -f2 | sort -n | head -1)
R1=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -1 | cut -d= -f2)
echo "post-maneuver range: ${R_EV:-?}m -> ${R1:-?}m (min ${RMIN_POST:-?}m) over ${EVADE_WINDOW_S}s (first ${EVADE_SKIP_N} post-cmd samples excluded)"

RECONV=$("$PYTHON" -c "print(1 if ${RMIN_POST:-99} <= ${CONTACT_M} else 0)" 2>/dev/null)
if [ "$RECONV" = "1" ]; then
    echo "EVASIVE_RECONVERGENCE_CONTACT rmin=${RMIN_POST}m"
    if tail -3 "$TESTDIR/obc.log" | grep -q "state=OFFBOARD_ACTIVE"; then
        RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
            "$RIPOSTE_BUILD/test/riposte-engage" disengage > /dev/null
        for _ in $(seq 1 15); do
            tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" && break
            sleep 1
        done
        tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" \
            || { echo "DISENGAGE_FAILED"; exit 7; }
        echo "DISENGAGED_READY"
    else
        grep -q "DISENGAGING -> READY" "$TESTDIR/obc.log" \
            || { echo "NO_SAFE_EXIT_AFTER_CONTACT"; exit 7; }
        echo "SAFE_EXIT_OK"
    fi
elif grep -q "safety violation" "$TESTDIR/obc.log" \
        && grep -q "DISENGAGING -> READY" "$TESTDIR/obc.log"; then
    echo "EVASIVE_SAFE_EXIT $(grep -oE "mask=0x[0-9a-f]+" "$TESTDIR/obc.log" | tail -1)"
else
    echo "NO_RECONVERGENCE_NO_SAFE_EXIT"
    tail -10 "$TESTDIR/obc.log"
    exit 6
fi
echo "GAZEBO_EVASIVE_PASS"
