#!/usr/bin/env bash
# Riposte OBC × PX4 SITL(SIH) smoke test — SAD-001 §12 stage 2:
#   PX4 up -> OBC CONNECTING->READY -> arm+takeoff (GCS stand-in)
#   -> engage (token) -> PRESTREAM -> offboard confirmed -> OFFBOARD_ACTIVE
#   -> disengage -> DISENGAGING -> READY
#
# Required environment:
#   PX4_BUILD      px4_sitl_default build dir (contains bin/px4 and etc/)
#                  e.g. ~/PX4-Autopilot/build/px4_sitl_default
#   RIPOSTE_BUILD  riposte-sw build dir configured with -DRIPOSTE_WITH_MAVSDK=ON
#   PYTHON         python with pymavlink installed (default: python3)
# Optional:
#   MAVSDK_LIB     dir containing libmavsdk.so.3 if not on the system path
set -u
: "${PX4_BUILD:?set PX4_BUILD to the px4_sitl_default build dir}"
: "${RIPOSTE_BUILD:?set RIPOSTE_BUILD to a RIPOSTE_WITH_MAVSDK=ON build dir}"
PYTHON=${PYTHON:-python3}
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-sitl.XXXXXX)}
mkdir -p "$TESTDIR"
[ -n "${MAVSDK_LIB:-}" ] && export LD_LIBRARY_PATH=$MAVSDK_LIB:${LD_LIBRARY_PATH:-}
echo "logs: $TESTDIR"

PX4_PID=""
OBC_PID=""

cleanup() {
    # Kill the PIDs this run started first so a concurrent unrelated session's
    # processes are spared as far as practical; the pattern pkill is the
    # fallback for leftovers.
    for pid in "$OBC_PID" "$PX4_PID"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    pkill -f "bin/px4" 2>/dev/null
    pkill -f riposte-obc 2>/dev/null
    rm -f /tmp/riposte-obc.sock
}
trap cleanup EXIT
# Startup sweep (documented single-instance assumption: this test owns the
# host's PX4/riposte processes and clears any stale leftovers).
cleanup
sleep 1

# --- 1. PX4 SITL (SIH quadx), headless daemon --------------------------------
cd "$PX4_BUILD" || exit 2
rm -rf eeprom parameters*.bts log # clean previous SITL state
PX4_SIM_MODEL=sihsim_quadx PX4_SIMULATOR=sihsim \
    ./bin/px4 -d -s etc/init.d-posix/rcS \
    > "$TESTDIR/px4.log" 2>&1 &
PX4_PID=$!
echo "px4 pid=$PX4_PID"

for _ in $(seq 1 30); do
    grep -q "Startup script returned successfully" "$TESTDIR/px4.log" && break
    kill -0 $PX4_PID 2>/dev/null || { echo "PX4_DIED"; tail -20 "$TESTDIR/px4.log"; exit 2; }
    sleep 1
done
grep -q "Startup script returned successfully" "$TESTDIR/px4.log" \
    || { echo "PX4_START_TIMEOUT"; tail -20 "$TESTDIR/px4.log"; exit 2; }
echo "PX4_UP"

# --- 2. riposte-obc (MAVSDK) ---------------------------------------------------
"$RIPOSTE_BUILD/riposte-obc" "$SCRIPT_DIR/riposte-sitl.ini" > "$TESTDIR/obc.log" 2>&1 &
OBC_PID=$!
echo "obc pid=$OBC_PID"

for _ in $(seq 1 30); do
    grep -q "state=READY" "$TESTDIR/obc.log" && break
    kill -0 $OBC_PID 2>/dev/null || { echo "OBC_DIED"; tail -20 "$TESTDIR/obc.log"; exit 3; }
    sleep 1
done
grep -q "state=READY" "$TESTDIR/obc.log" \
    || { echo "OBC_READY_TIMEOUT"; tail -30 "$TESTDIR/obc.log"; exit 3; }
echo "OBC_READY"

# --- 3. arm + takeoff (GCS stand-in on :14550) ----------------------------------
"$PYTHON" "$SCRIPT_DIR/arm_takeoff.py" > "$TESTDIR/gcs.log" 2>&1
grep -q "READY_FOR_ENGAGE" "$TESTDIR/gcs.log" \
    || { echo "TAKEOFF_FAILED"; tail -20 "$TESTDIR/gcs.log"; exit 4; }
echo "AIRBORNE"

# --- 4. engage ------------------------------------------------------------------
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token
for _ in $(seq 1 15); do
    grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" && break
    sleep 1
done
grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" \
    || { echo "ENGAGE_FAILED"; tail -30 "$TESTDIR/obc.log"; exit 5; }
echo "OFFBOARD_ACTIVE"
sleep 5 # hold offboard hover for a few control seconds

# --- 5. disengage ----------------------------------------------------------------
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" disengage
for _ in $(seq 1 15); do
    tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" && break
    sleep 1
done
tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" \
    || { echo "DISENGAGE_FAILED"; tail -30 "$TESTDIR/obc.log"; exit 6; }
echo "DISENGAGED_READY"
echo "SITL_TEST_PASS"
