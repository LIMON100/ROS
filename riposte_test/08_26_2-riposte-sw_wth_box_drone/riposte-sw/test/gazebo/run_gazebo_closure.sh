#!/usr/bin/env bash
# Riposte UAS-tracking CLOSURE test in Gazebo (gz-sim 8 / Harmonic).
#
# Full guidance loop against a physically-simulated, MOVING target:
#   Gazebo target truth -> gz_track_bridge -> TrackBus(shm)
#     -> GuidanceSource(PN) -> SafetyMonitor.clamp -> MAVSDK Offboard -> PX4(gz x500)
#
# Unlike the SIH tracking test (synthetic target), the target is a real gz model
# and the pass criterion is RANGE CLOSURE: the ownship measurably closes on the
# target while OFFBOARD_ACTIVE with zero safety violations.
#
# Requirements:
#   PX4_BUILD      PX4-Autopilot build with gz support (make px4_sitl gz_x500 once)
#                  e.g. ~/PX4-Autopilot/build/px4_sitl_default
#   RIPOSTE_BUILD  riposte-sw build configured -DRIPOSTE_WITH_MAVSDK=ON
#                                               -DRIPOSTE_WITH_GZ=ON
#   PYTHON         python3 with pymavlink
# Optional: MAVSDK_LIB (libmavsdk.so.3 dir), HEADLESS=1 (no gz GUI)
set -u
export LC_ALL=C # sort -n on float ranges is locale-sensitive
: "${PX4_BUILD:?set PX4_BUILD to a gz-capable px4_sitl build dir}"
: "${RIPOSTE_BUILD:?set RIPOSTE_BUILD to a RIPOSTE_WITH_MAVSDK=ON,RIPOSTE_WITH_GZ=ON build}"
PYTHON=${PYTHON:-python3}
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SITL_DIR="$SCRIPT_DIR/../sitl"
TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-gz.XXXXXX)}
mkdir -p "$TESTDIR"
[ -n "${MAVSDK_LIB:-}" ] && export LD_LIBRARY_PATH=$MAVSDK_LIB:${LD_LIBRARY_PATH:-}
export GZ_SIM_RESOURCE_PATH="$SCRIPT_DIR/worlds:${GZ_SIM_RESOURCE_PATH:-}"
echo "logs: $TESTDIR"

WINDOW_S=${WINDOW_S:-20}
MIN_CLOSE_M=5   # ownship must reduce range to the target by at least this much
WORLD=${WORLD:-riposte_closure}
TG=${TG:-target}

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
    pkill -f "gcs_heartbeat.py" 2>/dev/null
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

# --- 1. PX4 SITL + Gazebo (x500 spawned into riposte_closure world) ----------
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
        && ln -sf "$SCRIPT_DIR/worlds/$WORLD.sdf" "$PX4_GZ_WORLDS/"
fi
cd "$PX4_BUILD" || exit 2
rm -rf eeprom parameters*.bts log
[ "${HEADLESS:-0}" = "1" ] && export HEADLESS=1
PX4_SYS_AUTOSTART=4001 PX4_SIM_MODEL=gz_x500 PX4_GZ_WORLD=$WORLD \
    ./bin/px4 -d -s etc/init.d-posix/rcS > "$TESTDIR/px4.log" 2>&1 &
PX4_PID=$!
for _ in $(seq 1 45); do
    grep -q "Startup script returned successfully" "$TESTDIR/px4.log" && break
    sleep 1
done
grep -q "Startup script returned successfully" "$TESTDIR/px4.log" \
    || { echo "PX4_START_FAILED"; tail -20 "$TESTDIR/px4.log"; exit 2; }
echo "PX4_GZ_UP"

# --- 2. gz_track_bridge: Gazebo truth -> TrackBus ------------------------------
"$RIPOSTE_BUILD/gz_track_bridge" "$WORLD" x500_0 "$TG" \
    > "$TESTDIR/bridge.log" 2>&1 &
BRIDGE_PID=$!
for _ in $(seq 1 15); do
    grep -q "range=" "$TESTDIR/bridge.log" && break
    sleep 1
done
grep -q "range=" "$TESTDIR/bridge.log" \
    || { echo "BRIDGE_NO_TRACK"; tail -10 "$TESTDIR/bridge.log"; exit 2; }
echo "BRIDGE_TRACKING"

# --- 3. OBC (guidance) ---------------------------------------------------------
"$RIPOSTE_BUILD/riposte-obc" "$TESTDIR/riposte-gz.ini" > "$TESTDIR/obc.log" 2>&1 &
OBC_PID=$!
for _ in $(seq 1 30); do
    grep -q "state=READY" "$TESTDIR/obc.log" && break
    kill -0 $OBC_PID 2>/dev/null || { echo "OBC_DIED"; tail -20 "$TESTDIR/obc.log"; exit 3; }
    sleep 1
done
grep -q "state=READY" "$TESTDIR/obc.log" || { echo "OBC_READY_TIMEOUT"; exit 3; }
echo "OBC_READY"

# --- 4. arm + takeoff (GCS stand-in) -------------------------------------------
"$PYTHON" "$SITL_DIR/arm_takeoff.py" > "$TESTDIR/gcs.log" 2>&1
HB="$SITL_DIR/gcs_heartbeat.py"
"$PYTHON" "$HB" > "$TESTDIR/heartbeat.log" 2>&1 &
sleep 3
grep -q "READY_FOR_ENGAGE" "$TESTDIR/gcs.log" \
    || { echo "TAKEOFF_FAILED"; tail -20 "$TESTDIR/gcs.log"; exit 4; }
echo "AIRBORNE"

# --- 5. engage -> closure ----------------------------------------------------
# Capture the bridge.log range-line count BEFORE engaging: R0/RMIN below must be
# computed from post-engage lines only, otherwise a pre-engage crossing of the
# target near the grounded ownship can fake CONTACT.
N_ENG=$(grep -c "range=" "$TESTDIR/bridge.log")
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token > /dev/null
for _ in $(seq 1 15); do
    grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" && break
    sleep 1
done
grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" \
    || { echo "ENGAGE_FAILED"; tail -30 "$TESTDIR/obc.log"; exit 5; }
R0=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -n "+$((N_ENG + 1))" \
    | head -1 | cut -d= -f2)
echo "CLOSURE_STARTED range0=${R0:-?}m"

sleep "$WINDOW_S"

# --- 6. pass criteria ----------------------------------------------------------
# Two success shapes:
#  a) CONTACT: PN closed to physical-closure range (<= CONTACT_M). The gz
#     collision jolts the EKF, SafetyMonitor fires and the FSM must auto-
#     disengage to READY (safe-side exit) — that IS the mission profile.
#  b) CLOSURE: no contact inside the window; require >= MIN_CLOSE_M closure,
#     still OFFBOARD_ACTIVE and zero violations, then commanded disengage.
CONTACT_M=1.5
R1=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -1 | cut -d= -f2)
# RMIN over the post-engage phase only (lines after N_ENG, see step 5).
RMIN=$(grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -n "+$((N_ENG + 1))" \
    | cut -d= -f2 | sort -n | head -1)
echo "range: ${R0:-?}m -> ${R1:-?}m (min ${RMIN:-?}m post-engage) over ${WINDOW_S}s"

CONTACT=$("$PYTHON" -c "print(1 if ${RMIN:-99} <= ${CONTACT_M} else 0)" 2>/dev/null)
if [ "$CONTACT" = "1" ]; then
    echo "CLOSURE_REACHED rmin=${RMIN}m"
    if tail -3 "$TESTDIR/obc.log" | grep -q "state=OFFBOARD_ACTIVE"; then
        # survived contact still engaged: command disengage below
        NEED_DISENGAGE=1
    else
        grep -q "DISENGAGING -> READY" "$TESTDIR/obc.log" \
            || { echo "NO_SAFE_EXIT_AFTER_CONTACT"; exit 6; }
        echo "SAFE_EXIT_OK"
        NEED_DISENGAGE=0
    fi
else
    CLOSED=$("$PYTHON" -c "print(1 if (${R0:-0}-${R1:-0}) >= ${MIN_CLOSE_M} else 0)" 2>/dev/null)
    [ "$CLOSED" = "1" ] || { echo "NO_RANGE_CLOSURE"; exit 6; }
    tail -3 "$TESTDIR/obc.log" | grep -q "state=OFFBOARD_ACTIVE" \
        || { echo "NOT_ACTIVE_AFTER_WINDOW"; exit 6; }
    if grep -q "safety violation" "$TESTDIR/obc.log"; then
        echo "UNEXPECTED_VIOLATION"; grep "violation" "$TESTDIR/obc.log"; exit 6
    fi
    echo "RANGE_CLOSURE_OK"
    NEED_DISENGAGE=1
fi

# --- 7. disengage (unless safety already returned us to READY) ------------------
if [ "$NEED_DISENGAGE" = "1" ]; then
    RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
        "$RIPOSTE_BUILD/test/riposte-engage" disengage > /dev/null
    for _ in $(seq 1 15); do
        tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" && break
        sleep 1
    done
    tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" \
        || { echo "DISENGAGE_FAILED"; exit 7; }
    echo "DISENGAGED_READY"
fi
echo "GAZEBO_CLOSURE_PASS"
