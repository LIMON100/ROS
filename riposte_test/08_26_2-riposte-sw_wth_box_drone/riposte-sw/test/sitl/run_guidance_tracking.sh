#!/usr/bin/env bash
# Guidance-source tracking test against PX4 SITL (SIH) — SAD-001 §12-2.
#
# Full perception->guidance chain in the loop:
#   SyntheticCamera -> SyntheticDetector -> Tracker -> TargetEstimator
#   -> TrackBus(shm) -> GuidanceSource(PN) -> SafetyMonitor.clamp -> PX4
#
# Asserts: sustained OFFBOARD_ACTIVE tracking for the whole window with zero
# safety violations, real displacement (> 20 m), and observed horizontal speed
# below the SM-4 clamp ceiling (guidance commands 6 m/s, clamp is 5 m/s).
#
# Environment: same as run_sitl_test.sh (PX4_BUILD, RIPOSTE_BUILD, PYTHON,
# optional MAVSDK_LIB).
set -u
: "${PX4_BUILD:?set PX4_BUILD to the px4_sitl_default build dir}"
: "${RIPOSTE_BUILD:?set RIPOSTE_BUILD to a RIPOSTE_WITH_MAVSDK=ON build dir}"
PYTHON=${PYTHON:-python3}
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-tracking.XXXXXX)}
mkdir -p "$TESTDIR"
[ -n "${MAVSDK_LIB:-}" ] && export LD_LIBRARY_PATH=$MAVSDK_LIB:${LD_LIBRARY_PATH:-}
echo "logs: $TESTDIR"

WINDOW_S=15
MIN_DISP_M=20

PX4_PID=""
SEEKER_PID=""
OBC_PID=""

cleanup() {
    # Kill the PIDs this run started first so a concurrent unrelated session's
    # processes are spared as far as practical; the pattern pkill is the
    # fallback for leftovers.
    for pid in "$OBC_PID" "$SEEKER_PID" "$PX4_PID"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    pkill -f "bin/px4" 2>/dev/null
    pkill -f riposte-obc 2>/dev/null
    pkill -f riposte-seeker 2>/dev/null
    rm -f /tmp/riposte-obc.sock
}
trap cleanup EXIT
# Startup sweep (documented single-instance assumption: this test owns the
# host's PX4/riposte processes and clears any stale leftovers).
cleanup
sleep 1

cat > "$TESTDIR/riposte-guidance.ini" <<EOF
[obc]
connection_url = udpin://0.0.0.0:14540
source = guidance
operator_token = sitl-test-token
cmd_socket = /tmp/riposte-obc.sock
rt_priority = 0
cpu_affinity = -1
[safety]
vmax_h = 5.0
vmax_v = 2.0
geofence_r = 300.0
alt_min = 1.0
alt_max = 120.0
engage_timebox_s = 60.0
EOF

# --- 1. PX4 SITL (SIH quadx) --------------------------------------------------
cd "$PX4_BUILD" || exit 2
rm -rf eeprom parameters*.bts log
PX4_SIM_MODEL=sihsim_quadx PX4_SIMULATOR=sihsim \
    ./bin/px4 -d -s etc/init.d-posix/rcS > "$TESTDIR/px4.log" 2>&1 &
PX4_PID=$!
for _ in $(seq 1 30); do
    grep -q "Startup script returned successfully" "$TESTDIR/px4.log" && break
    sleep 1
done
grep -q "Startup script returned successfully" "$TESTDIR/px4.log" \
    || { echo "PX4_START_FAILED"; tail -20 "$TESTDIR/px4.log"; exit 2; }
echo "PX4_UP"

# --- 2. seeker (synthetic pipeline -> TrackBus) ---------------------------------
# The SIL profile must be passed EXPLICITLY: seeker.synthetic defaults to false
# (production) so a real deployment can never fall back to synthetic detections
# (P0-01 fail-closed). With no config at all the seeker correctly refuses to
# start, which is why this scenario writes a SIL ini for it.
{
    echo "[seeker]"
    echo "synthetic = true"
    echo "width     = 1280"
    echo "height    = 720"
    echo "record    = false"
} > "$TESTDIR/seeker-sil.ini"
"$RIPOSTE_BUILD/riposte-seeker" "$TESTDIR/seeker-sil.ini" > "$TESTDIR/seeker.log" 2>&1 &
SEEKER_PID=$!
for _ in $(seq 1 15); do
    grep -q "track=yes" "$TESTDIR/seeker.log" && break
    kill -0 $SEEKER_PID 2>/dev/null || { echo "SEEKER_DIED"; exit 2; }
    sleep 1
done
grep -q "track=yes" "$TESTDIR/seeker.log" \
    || { echo "SEEKER_NO_TRACK"; tail -10 "$TESTDIR/seeker.log"; exit 2; }
echo "SEEKER_TRACKING"

# --- 3. OBC (guidance source) ---------------------------------------------------
"$RIPOSTE_BUILD/riposte-obc" "$TESTDIR/riposte-guidance.ini" > "$TESTDIR/obc.log" 2>&1 &
OBC_PID=$!
for _ in $(seq 1 30); do
    grep -q "state=READY" "$TESTDIR/obc.log" && break
    kill -0 $OBC_PID 2>/dev/null || { echo "OBC_DIED"; tail -20 "$TESTDIR/obc.log"; exit 3; }
    sleep 1
done
grep -q "state=READY" "$TESTDIR/obc.log" \
    || { echo "OBC_READY_TIMEOUT"; tail -20 "$TESTDIR/obc.log"; exit 3; }
echo "OBC_READY"

# --- 4. arm + takeoff -------------------------------------------------------------
# Higher than the default 15 m: the synthetic target's elevation oscillates, so
# the tracking descends as well as climbs. From 15 m that descent reached SM-3's
# altitude floor and ended the control session before the observation window closed
# — the floor working correctly on a scenario with no vertical room.
TAKEOFF_ALT_M=${TAKEOFF_ALT_M:-40} "$PYTHON" "$SCRIPT_DIR/arm_takeoff.py" \
    > "$TESTDIR/gcs.log" 2>&1
grep -q "READY_FOR_ENGAGE" "$TESTDIR/gcs.log" \
    || { echo "TAKEOFF_FAILED"; tail -20 "$TESTDIR/gcs.log"; exit 4; }
echo "AIRBORNE"

# --- 5. engage -> track ------------------------------------------------------------
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token > /dev/null
for _ in $(seq 1 15); do
    grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" && break
    sleep 1
done
grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" \
    || { echo "ENGAGE_FAILED"; tail -30 "$TESTDIR/obc.log"; exit 5; }
echo "TRACKING_STARTED"

# Observe displacement + speed for the window (also SM-4 clamp in flight).
"$PYTHON" "$SCRIPT_DIR/tracking_monitor.py" "$WINDOW_S" "$MIN_DISP_M" \
    > "$TESTDIR/tracking.log" 2>&1
cat "$TESTDIR/tracking.log"
grep -q "TRACKING_OK" "$TESTDIR/tracking.log" || { echo "TRACKING_FAILED"; exit 6; }

# Still actively engaged, and zero safety violations during the window.
tail -3 "$TESTDIR/obc.log" | grep -q "state=OFFBOARD_ACTIVE" \
    || { echo "NOT_ACTIVE_AFTER_WINDOW"; tail -10 "$TESTDIR/obc.log"; exit 6; }
if grep -q "safety violation" "$TESTDIR/obc.log"; then
    echo "UNEXPECTED_VIOLATION"; grep "violation" "$TESTDIR/obc.log"; exit 6
fi
echo "TRACKING_SUSTAINED"

# --- 6. disengage -------------------------------------------------------------------
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" disengage > /dev/null
for _ in $(seq 1 15); do
    tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" && break
    sleep 1
done
tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" \
    || { echo "DISENGAGE_FAILED"; tail -20 "$TESTDIR/obc.log"; exit 7; }
echo "DISENGAGED_READY"
echo "GUIDANCE_TRACKING_PASS"
