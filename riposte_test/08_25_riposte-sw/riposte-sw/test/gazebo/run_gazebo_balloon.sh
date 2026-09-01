#!/usr/bin/env bash
# Riposte BALLOON TRIAL rehearsal in Gazebo (gz-sim 8 / Harmonic).
#
# Flies the outdoor balloon-trial profile (RIPOSTE-DUALEO-REQ-001 T-1..T-5) in
# simulation, so the patrol behaviour and the SM-10 geofence are exercised end to
# end before anyone books a range or trains a detector:
#
#   Gazebo balloon truth -> gz_track_bridge (multi-target + FOV model) -> TrackBus
#     -> BalloonPatrolSource -> SafetyMonitor.clamp -> MAVSDK Offboard -> PX4(gz x500)
#
# What it proves, and what it does not:
#   PROVES  the patrol finds targets by yawing, flies at them, counts them as
#           serviced, moves on to a DIFFERENT one, and turns back at the fence —
#           and that SM-10 never has to fire, because the behaviour keeps the
#           vehicle inside on its own.
#   DOES NOT prove anything about detection. The bridge substitutes truth poses
#           for perception, exactly as it does in the closure tests; whether
#           a 25 cm balloon is actually detectable is a P1/HEF question.
#
# Requirements:
#   PX4_BUILD      PX4-Autopilot build with gz support (make px4_sitl gz_x500 once)
#   RIPOSTE_BUILD  riposte-sw build configured -DRIPOSTE_WITH_MAVSDK=ON
#                                               -DRIPOSTE_WITH_GZ=ON
#   PYTHON         python3 with pymavlink
# Optional: MAVSDK_LIB (libmavsdk.so.3 dir), HEADLESS=1 (no gz GUI),
#           WINDOW_S (patrol observation window, default 90)
set -u
export LC_ALL=C
: "${PX4_BUILD:?set PX4_BUILD to a gz-capable px4_sitl build dir}"
: "${RIPOSTE_BUILD:?set RIPOSTE_BUILD to a RIPOSTE_WITH_MAVSDK=ON,RIPOSTE_WITH_GZ=ON build}"
PYTHON=${PYTHON:-python3}
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
SITL_DIR="$SCRIPT_DIR/../sitl"
TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-balloon.XXXXXX)}
mkdir -p "$TESTDIR"
[ -n "${MAVSDK_LIB:-}" ] && export LD_LIBRARY_PATH=$MAVSDK_LIB:${LD_LIBRARY_PATH:-}
export GZ_SIM_RESOURCE_PATH="$SCRIPT_DIR/worlds:${GZ_SIM_RESOURCE_PATH:-}"
echo "logs: $TESTDIR"

WORLD=${WORLD:-riposte_balloon}
WINDOW_S=${WINDOW_S:-90}   # a patrol needs time to service more than one balloon
FENCE_M=50                 # T-1: the 50 x 50 m box, centred on the engage point
MIN_SERVICED=2             # must reach one balloon and then go find another

PX4_PID=""
BRIDGE_PID=""
OBC_PID=""
HB_PID=""

cleanup() {
    for pid in "$HB_PID" "$OBC_PID" "$BRIDGE_PID" "$PX4_PID"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    pkill -f "gcs_heartbeat.py" 2>/dev/null
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
cleanup
sleep 1

# --- 0. OBC config: the balloon profile ---------------------------------------
# safety.geofence_r is set WIDER than the polygon on purpose: SM-3 must not be
# what stops the vehicle here. The point of the run is that the patrol behaviour
# keeps it inside the 50 m box, with SM-10 as the untouched hard limit behind it.
cat > "$TESTDIR/balloon-gz.ini" <<EOF
[obc]
connection_url = udpin://0.0.0.0:14540
source = balloon
operator_token = sitl-test-token
cmd_socket = /tmp/riposte-obc.sock
rt_priority = 0
cpu_affinity = -1
[fence]
side_m = $FENCE_M
[patrol]
alt_m               = 5.0
approach_speed_mps  = 3.0
search_yaw_rate_rps = 0.6
soft_margin_m       = 10.0
turn_margin_m       = 3.0
reach_range_m       = 3.0
visited_cooldown_s  = 45.0
turn_dwell_s        = 2.0
[safety]
vmax_h = 5.0
vmax_v = 2.0
geofence_r = 80.0
alt_min = 1.0
alt_max = 20.0
engage_timebox_s = 600.0
EOF

# --- 1. PX4 SITL + Gazebo ------------------------------------------------------
if [ -f "$PX4_BUILD/rootfs/gz_env.sh" ]; then
    # shellcheck disable=SC1091
    # PX4 v1.17+ gz_env.sh references GZ_SIM_SYSTEM_PLUGIN_PATH and this script
    # runs under `set -u`, so guarantee an empty default before sourcing.
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

# --- 2. bridge in multi-target mode -------------------------------------------
# The trailing '*' selects multi-target + the range/FOV sensor model, so a
# balloon is only reported while the camera is pointing at it.
GZ_BRIDGE_MAX_RANGE_M=60 GZ_BRIDGE_HFOV_DEG=60 GZ_BRIDGE_VFOV_DEG=34 \
    "$RIPOSTE_BUILD/gz_track_bridge" "$WORLD" x500_0 "${TG:-balloon_*}" \
    > "$TESTDIR/bridge.log" 2>&1 &
BRIDGE_PID=$!
for _ in $(seq 1 15); do
    grep -q "targets=" "$TESTDIR/bridge.log" && break
    sleep 1
done
grep -q "targets=" "$TESTDIR/bridge.log" \
    || { echo "BRIDGE_NO_FEED"; tail -10 "$TESTDIR/bridge.log"; exit 2; }
echo "BRIDGE_UP"

# --- 3. OBC (balloon patrol) ---------------------------------------------------
"$RIPOSTE_BUILD/riposte-obc" "$TESTDIR/balloon-gz.ini" > "$TESTDIR/obc.log" 2>&1 &
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
grep -q "READY_FOR_ENGAGE" "$TESTDIR/gcs.log" \
    || { echo "TAKEOFF_FAILED"; tail -20 "$TESTDIR/gcs.log"; exit 4; }
echo "AIRBORNE"

# --- 4b. keep the GCS link alive for the rest of the run -----------------------
# arm_takeoff.py's heartbeat dies with it, and PX4 v1.17 treats GCS loss as a
# failsafe: Hold, then RTL and land. Over a 90 s patrol window that lands the
# vehicle mid-test and reads exactly like the patrol giving up (observed before
# this was added). The real aircraft has a permanent SiK link, so standing one
# in is the realistic configuration, not a workaround. Started only AFTER
# arm_takeoff.py exits so the two never share the GCS port.
"$PYTHON" "$SITL_DIR/gcs_heartbeat.py" > "$TESTDIR/heartbeat.log" 2>&1 &
HB_PID=$!
for _ in $(seq 1 20); do
    grep -q "linked to sys" "$TESTDIR/heartbeat.log" && break
    sleep 1
done
grep -q "linked to sys" "$TESTDIR/heartbeat.log" \
    || { echo "GCS_HEARTBEAT_FAILED"; tail -5 "$TESTDIR/heartbeat.log"; exit 4; }
echo "GCS_LINK_UP"

# --- 5. engage -> patrol -------------------------------------------------------
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token > /dev/null
for _ in $(seq 1 15); do
    grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" && break
    sleep 1
done
grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" \
    || { echo "ENGAGE_FAILED"; tail -30 "$TESTDIR/obc.log"; exit 5; }
grep -q "SM-10 fence" "$TESTDIR/obc.log" \
    || { echo "NO_FENCE_AT_ENGAGE"; grep -i fence "$TESTDIR/obc.log"; exit 5; }
echo "PATROL_STARTED (window ${WINDOW_S}s)"

sleep "$WINDOW_S"

# --- 6. pass criteria ----------------------------------------------------------
# The patrol log is the record of what the behaviour actually did.
SERVICED=$(grep -c "APPROACH -> TURN_AWAY (balloon reached)" "$TESTDIR/obc.log")
FENCE_TURNS=$(grep -c "APPROACH -> TURN_AWAY (fence boundary reached)" "$TESTDIR/obc.log")
APPROACHES=$(grep -c "SEARCH -> APPROACH" "$TESTDIR/obc.log")
echo "patrol: approaches=$APPROACHES serviced=$SERVICED fence_turns=$FENCE_TURNS"

# (a) It has to actually service balloons and then go find others — a run that
#     locks onto one target forever is the failure this scenario exists to catch.
[ "$SERVICED" -ge "$MIN_SERVICED" ] \
    || { echo "TOO_FEW_SERVICED (want >= $MIN_SERVICED)"; tail -40 "$TESTDIR/obc.log"; exit 6; }

# (b) Distinct targets: re-servicing the same balloon over and over would satisfy
#     (a) without demonstrating T-5 at all. The bridge logs the primary id.
DISTINCT=$(grep -oE "primary=[0-9]+" "$TESTDIR/bridge.log" | sort -u | wc -l)
[ "$DISTINCT" -ge 2 ] \
    || { echo "ONLY_ONE_TARGET_ENGAGED (distinct=$DISTINCT)"; exit 6; }
echo "DISTINCT_TARGETS=$DISTINCT"

# (c) SM-10 must never have fired: the behaviour layer is supposed to keep the
#     vehicle inside on its own, so the hard limit firing means it failed.
if grep -qE "violation mask=0x[0-9a-f]*2[0-9a-f]{2}" "$TESTDIR/obc.log"; then
    echo "SM10_FIRED (behaviour failed to hold the boundary)"
    grep "violation" "$TESTDIR/obc.log"; exit 6
fi
if grep -q "safety violation" "$TESTDIR/obc.log"; then
    echo "UNEXPECTED_VIOLATION"; grep "violation" "$TESTDIR/obc.log"; exit 6
fi

# (d) Still flying the patrol at the end of the window.
tail -3 "$TESTDIR/obc.log" | grep -q "state=OFFBOARD_ACTIVE" \
    || { echo "NOT_ACTIVE_AFTER_WINDOW"; tail -20 "$TESTDIR/obc.log"; exit 6; }
echo "PATROL_OK"

# --- 7. disengage --------------------------------------------------------------
RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
    "$RIPOSTE_BUILD/test/riposte-engage" disengage > /dev/null
for _ in $(seq 1 15); do
    tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" && break
    sleep 1
done
tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" \
    || { echo "DISENGAGE_FAILED"; exit 7; }
echo "DISENGAGED_READY"
echo "GAZEBO_BALLOON_PASS"
