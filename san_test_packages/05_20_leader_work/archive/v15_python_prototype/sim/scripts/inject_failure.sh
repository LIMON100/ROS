#!/usr/bin/env bash
# inject_failure.sh -- controlled failure injection for UC tests (Phase D).

set -euo pipefail

usage() {
    cat <<EOF
Usage: $0 <failure_type> [args]

Failure types:
  leader_kill <robot_name>       Kill leader process (UC-6)
  wifi_drop <duration_s>         WiFi mesh down for N seconds (UC-4)
  rtk_loss [duration_s]          RTK signal loss (UC-5)
  geofence_intrusion             Move robot toward boundary (UC-10)
  battery_critical <robot_name>  Force battery <10% (UC-8 RTH)
  comm_storm                     Saturate WiFi6 (UC-4 degrade)
  follower_drop <robot_name>     T4 single follower (UC-11 rollback)

Output: log to /tmp/failure_inject.log + JSON event file.
EOF
    exit 1
}

if [[ $# -eq 0 ]]; then
    usage
fi

FAILURE=$1
shift

LOGFILE=/tmp/failure_inject.log
mkdir -p "$(dirname "$LOGFILE")"

log() {
    echo "[$(date -u +%Y-%m-%dT%H:%M:%SZ)] $*" | tee -a "$LOGFILE"
}

case "$FAILURE" in
    leader_kill)
        ROBOT="${1:-robot1}"
        log "INJECT: kill leader process on $ROBOT"
        docker exec phase_d_robot_stack pkill -f "robot_id=1" || true
        log "INJECT: leader killed at $(date -u +%s%3N) ms"
        ;;

    wifi_drop)
        DURATION="${1:-10}"
        log "INJECT: WiFi mesh drop for ${DURATION}s"
        docker exec phase_d_gazebo iptables -A OUTPUT -d 224.0.0.0/4 -j DROP \
            || true
        sleep "$DURATION"
        docker exec phase_d_gazebo iptables -D OUTPUT -d 224.0.0.0/4 -j DROP \
            || true
        log "INJECT: WiFi restored"
        ;;

    rtk_loss)
        DURATION="${1:-30}"
        log "INJECT: RTK signal loss for ${DURATION}s"
        docker exec phase_d_gazebo ros2 topic pub --once \
            /sim/rtk_quality std_msgs/msg/String "data: 'NONE'" || true
        sleep "$DURATION"
        docker exec phase_d_gazebo ros2 topic pub --once \
            /sim/rtk_quality std_msgs/msg/String "data: 'FIXED'" || true
        log "INJECT: RTK restored"
        ;;

    geofence_intrusion)
        log "INJECT: send waypoint outside geofence"
        docker exec phase_d_gazebo ros2 topic pub --once \
            /robot1/mission_command sensor_msgs/msg/NavSatFix \
            "{latitude: 38.0, longitude: 128.0}" || true
        ;;

    battery_critical)
        ROBOT="${1:-robot1}"
        log "INJECT: force $ROBOT battery to 8%"
        docker exec phase_d_robot_stack ros2 topic pub --once \
            "/$ROBOT/sim/battery_pct" std_msgs/msg/Float32 "data: 8.0" || true
        ;;

    comm_storm)
        log "INJECT: WiFi6 saturation 30s"
        docker exec phase_d_gazebo bash -c \
            "iperf3 -c 224.0.0.1 -u -b 200M -t 30 &" || true
        ;;

    follower_drop)
        ROBOT="${1:-robot3}"
        log "INJECT: $ROBOT enters T4 (50s lag)"
        docker exec phase_d_robot_stack ros2 topic pub --once \
            "/$ROBOT/sim/inject_lag" std_msgs/msg/Float32 "data: 50.0" || true
        ;;

    *)
        echo "ERROR: unknown failure type: $FAILURE" >&2
        usage
        ;;
esac

EVENT_JSON="/tmp/failure_event_$(date +%s).json"
cat > "$EVENT_JSON" <<EOF
{
  "type": "$FAILURE",
  "ts_unix_ms": $(date +%s%3N),
  "args": "$*"
}
EOF
log "INJECT: event recorded -> $EVENT_JSON"
