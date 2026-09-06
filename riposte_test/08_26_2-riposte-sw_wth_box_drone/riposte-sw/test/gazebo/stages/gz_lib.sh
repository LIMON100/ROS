#!/usr/bin/env bash
# gz_lib.sh — shared bring-up scaffolding for the staged Gazebo verification
# ladder (test/gazebo/stages/). SOURCE this from a stageN_*.sh script; it
# provides the building blocks (PX4+gz, bridge, OBC, arm/takeoff, engage,
# disengage, fault injection) with PID tracking and a safe cleanup trap.
#
# Contract for callers:
#   Required env : PX4_BUILD (gz-capable px4_sitl build), RIPOSTE_BUILD
#                  (riposte-sw build with -DRIPOSTE_WITH_MAVSDK=ON
#                   -DRIPOSTE_WITH_GZ=ON).
#   Optional env : MAVSDK_LIB (libmavsdk.so.3 dir), PYTHON (default python3),
#                  HEADLESS=1 (no gz GUI), GUI_FOLLOW=1 (GUI top-down follow),
#                  TESTDIR (log dir; default a fresh mktemp).
#   Each gz_* helper echoes a marker and returns non-zero on failure so the
#   stage can `gz_start_px4 || exit $?`. gz_cleanup runs on EXIT.
#
# The pattern pkill in gz_cleanup is safe here: the sourcing process' command
# line is `bash stageN_*.sh` (no "gz sim" substring), so it never self-matches
# the way an inline interactive `pkill -f "gz sim"` would.
set -u

# --- resolve paths -------------------------------------------------------------
GZ_LIB_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
GZ_DIR=$(cd "$GZ_LIB_DIR/.." && pwd)               # test/gazebo
SITL_DIR=$(cd "$GZ_DIR/../sitl" && pwd)            # test/sitl (arm_takeoff.py …)
WORLD_SDF="$GZ_DIR/worlds/riposte_closure.sdf"
WORLD_NAME=riposte_closure
OWNSHIP=x500_0
TARGET=target

PYTHON=${PYTHON:-python3}
export LC_ALL=C   # sort -n / numeric parsing must be locale-stable

# Tracked PIDs this run started (killed first in cleanup).
PX4_PID=""; BRIDGE_PID=""; OBC_PID=""

gz_die() { echo "FAIL: $*"; exit "${2:-1}"; }
gz_ok()  { echo "OK: $*"; }
gz_log() { echo "-- $*"; }

# --- environment + world -------------------------------------------------------
gz_setup() {
    : "${PX4_BUILD:?set PX4_BUILD to a gz-capable px4_sitl build dir}"
    : "${RIPOSTE_BUILD:?set RIPOSTE_BUILD to a RIPOSTE_WITH_MAVSDK+GZ build}"
    [ -n "${MAVSDK_LIB:-}" ] && export LD_LIBRARY_PATH="$MAVSDK_LIB:${LD_LIBRARY_PATH:-}"
    export GZ_SIM_RESOURCE_PATH="$GZ_DIR/worlds:${GZ_SIM_RESOURCE_PATH:-}"
    TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-gzstage.XXXXXX)}
    mkdir -p "$TESTDIR"
    echo "logs: $TESTDIR"
    # PX4 rcS sources rootfs/gz_env.sh which force-overrides PX4_GZ_WORLDS to
    # the PX4 tree, so the world must be reachable there: link it in.
    if [ -f "$PX4_BUILD/rootfs/gz_env.sh" ]; then
        # shellcheck disable=SC1091
        # PX4 v1.17+의 gz_env.sh 는 GZ_SIM_SYSTEM_PLUGIN_PATH 를 참조하는데, 이
        # 스크립트는 set -u 라 미정의 변수 참조가 치명 오류가 된다 (v1.15.4 에는
        # 없던 참조 — PX4 버전 호환). 소싱 전에 빈 기본값을 보장한다.
        export GZ_SIM_SYSTEM_PLUGIN_PATH="${GZ_SIM_SYSTEM_PLUGIN_PATH:-}"
        . "$PX4_BUILD/rootfs/gz_env.sh"
        [ -d "${PX4_GZ_WORLDS:-}" ] && ln -sf "$WORLD_SDF" "$PX4_GZ_WORLDS/"
    fi
}

gz_cleanup() {
    for pid in "$OBC_PID" "$BRIDGE_PID" "$PX4_PID"; do
        [ -n "$pid" ] && kill "$pid" 2>/dev/null
    done
    pkill -f "gz_track_bridge" 2>/dev/null
    pkill -f "bin/px4" 2>/dev/null
    pkill -f "gz sim" 2>/dev/null
    pkill -f "riposte-obc" 2>/dev/null
    pkill -f "riposte-seeker" 2>/dev/null   # would be a 2nd TrackBus WRITER
    rm -f /tmp/riposte-obc.sock
}

# gz_arm_ladder: install the cleanup trap and clear any stale leftovers. The
# startup sweep documents the single-instance assumption: a stage owns the
# host's PX4/gz/riposte processes for its duration.
gz_arm_ladder() {
    trap gz_cleanup EXIT
    gz_cleanup
    sleep 1
}

# --- OBC config ----------------------------------------------------------------
# gz_write_ini <path> <source> [timebox_s]
gz_write_ini() {
    local path=$1 source=${2:-guidance} timebox=${3:-90.0}
    cat > "$path" <<EOF
[obc]
connection_url = udpin://0.0.0.0:14540
source = $source
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
engage_timebox_s = $timebox
EOF
}

# --- component bring-up (each blocks until ready or fails) ----------------------
gz_start_px4() {
    cd "$PX4_BUILD" || gz_die "PX4_BUILD not a dir" 2
    rm -rf eeprom parameters*.bts log
    [ "${HEADLESS:-0}" = "1" ] && export HEADLESS=1
    # GUI follow needs Qt on WSLg via xcb; harmless elsewhere.
    [ "${GUI_FOLLOW:-0}" = "1" ] && { export QT_QPA_PLATFORM=${QT_QPA_PLATFORM:-xcb}; export DISPLAY=${DISPLAY:-:0}; }
    PX4_SYS_AUTOSTART=4001 PX4_SIM_MODEL=gz_x500 PX4_GZ_WORLD="$WORLD_NAME" \
        ./bin/px4 -d -s etc/init.d-posix/rcS > "$TESTDIR/px4.log" 2>&1 &
    PX4_PID=$!
    local i
    for i in $(seq 1 60); do
        grep -q "Startup script returned successfully" "$TESTDIR/px4.log" && break
        kill -0 "$PX4_PID" 2>/dev/null || { echo "PX4_DIED"; tail -20 "$TESTDIR/px4.log"; return 2; }
        sleep 1
    done
    grep -q "Startup script returned successfully" "$TESTDIR/px4.log" \
        || { echo "PX4_START_TIMEOUT"; tail -20 "$TESTDIR/px4.log"; return 2; }
    gz_ok "PX4+gz up ($([ "${HEADLESS:-0}" = 1 ] && echo headless || echo GUI))"
}

gz_start_bridge() {
    "$RIPOSTE_BUILD/gz_track_bridge" "$WORLD_NAME" "$OWNSHIP" "$TARGET" \
        > "$TESTDIR/bridge.log" 2>&1 &
    BRIDGE_PID=$!
    local i
    for i in $(seq 1 15); do grep -q "range=" "$TESTDIR/bridge.log" && break; sleep 1; done
    grep -q "range=" "$TESTDIR/bridge.log" \
        || { echo "BRIDGE_NO_TRACK"; tail -10 "$TESTDIR/bridge.log"; return 2; }
    gz_ok "bridge tracking (TrackBus WRITER)"
}

# gz_start_obc <ini>
gz_start_obc() {
    local ini=$1
    "$RIPOSTE_BUILD/riposte-obc" "$ini" > "$TESTDIR/obc.log" 2>&1 &
    OBC_PID=$!
    local i
    for i in $(seq 1 30); do
        grep -q "state=READY" "$TESTDIR/obc.log" && break
        kill -0 "$OBC_PID" 2>/dev/null || { echo "OBC_DIED"; tail -20 "$TESTDIR/obc.log"; return 3; }
        sleep 1
    done
    grep -q "state=READY" "$TESTDIR/obc.log" || { echo "OBC_READY_TIMEOUT"; return 3; }
    gz_ok "OBC READY"
}

gz_arm_takeoff() {
    "$PYTHON" "$SITL_DIR/arm_takeoff.py" > "$TESTDIR/gcs.log" 2>&1
    grep -q "READY_FOR_ENGAGE" "$TESTDIR/gcs.log" \
        || { echo "TAKEOFF_FAILED"; tail -20 "$TESTDIR/gcs.log"; return 4; }
    gz_ok "airborne (READY_FOR_ENGAGE)"
}

gz_engage() {
    RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
        "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token > /dev/null
    local i
    for i in $(seq 1 15); do grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" && break; sleep 1; done
    grep -q "state=OFFBOARD_ACTIVE" "$TESTDIR/obc.log" \
        || { echo "ENGAGE_FAILED"; tail -30 "$TESTDIR/obc.log"; return 5; }
    gz_ok "engaged (OFFBOARD_ACTIVE)"
}

# gz_disengage: commanded disengage; expect READY. Returns non-zero if the FSM
# is not back in READY (some stages expect an *auto* disengage instead — those
# check obc.log directly rather than calling this).
gz_disengage() {
    RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
        "$RIPOSTE_BUILD/test/riposte-engage" disengage > /dev/null
    local i
    for i in $(seq 1 15); do tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" && break; sleep 1; done
    tail -5 "$TESTDIR/obc.log" | grep -q "state=READY" || { echo "DISENGAGE_FAILED"; return 7; }
    gz_ok "disengaged (READY)"
}

# --- helpers -------------------------------------------------------------------
# gz_target_vel <x> <y> <z> : command the target's VelocityControl.
gz_target_vel() {
    gz topic -t "/model/$TARGET/cmd_vel" -m gz.msgs.Twist \
        -p "linear: {x: $1, y: $2, z: $3}" 2>/dev/null
}

# gz_target_place <x> <y> <z> : stop the target and teleport it to a known
# world pose (UserCommands set_pose). Used by the static-target stage so boot
# drift does not leave it at an arbitrary range.
gz_target_place() {
    gz_target_vel 0 0 0
    gz service -s "/world/$WORLD_NAME/set_pose" --reqtype gz.msgs.Pose \
        --reptype gz.msgs.Boolean --timeout 3000 \
        --req "name: \"$TARGET\", position: {x: $1, y: $2, z: $3}" >/dev/null 2>&1
    gz_target_vel 0 0 0
}

# gz_pause / gz_resume : freeze/step the sim (used to test bridge staleness).
gz_pause()  { gz service -s "/world/$WORLD_NAME/control" --reqtype gz.msgs.WorldControl \
                 --reptype gz.msgs.Boolean --timeout 3000 --req 'pause: true'  >/dev/null 2>&1; }
gz_resume() { gz service -s "/world/$WORLD_NAME/control" --reqtype gz.msgs.WorldControl \
                 --reptype gz.msgs.Boolean --timeout 3000 --req 'pause: false' >/dev/null 2>&1; }

# gz_follow_camera <target> : GUI top-down follow (no-op if not GUI). The follow
# p-gain is a hardcoded 0.01 in gz-sim 8 (not exposed), so the high +z offset
# gives a wide top-down footprint that keeps the target framed despite the lag.
gz_follow_camera() {
    [ "${GUI_FOLLOW:-0}" = "1" ] || return 0
    local target=$1 i
    for i in $(seq 1 20); do
        gz service -s /gui/follow --reqtype gz.msgs.StringMsg --reptype gz.msgs.Boolean \
            --timeout 2000 --req "data: \"$target\"" >/dev/null 2>&1
        gz service -s /gui/follow/offset --reqtype gz.msgs.Vector3d --reptype gz.msgs.Boolean \
            --timeout 2000 --req 'x: -2.0, y: 0.0, z: 18.0' >/dev/null 2>&1
        gz topic -e -t /gui/currently_tracked -n 1 2>/dev/null | grep -q "FOLLOW" && break
        sleep 1
    done
    gz topic -e -t /gui/currently_tracked -n 1 2>/dev/null | grep -q "FOLLOW" \
        && gz_ok "camera follows $target from above" || gz_log "follow not confirmed"
}

# gz_range_after <startline> : min range over bridge.log lines after <startline>.
gz_range_after() {
    grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -n "+$(($1 + 1))" \
        | cut -d= -f2 | sort -n | head -1
}
gz_range_last()  { grep -oE "range=[0-9.]+" "$TESTDIR/bridge.log" | tail -1 | cut -d= -f2; }
gz_range_count() { grep -c "range=" "$TESTDIR/bridge.log"; }
