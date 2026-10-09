#!/bin/bash
# board_kill.sh — 이 보드의 combat_nav2 관련 ROS 프로세스를 전부 확실히 종료.
# ─────────────────────────────────────────────────────────────────────────
# board_bringup.sh 로 띄운 스택(센서/CAN/nav2/FSM)을 완전정리한다.
# ★ 재기동 전 반드시 이걸로 0 확인 → 중복 launch(→map TF 간헐/과부하) 방지.
# 사용법:  ~/combatrobot_1/ros/src/.../mesh/board_kill.sh
#
# 시스템설정(정적IP/라이다IP/wifi off/CAN서비스/sysctl)·ssh 세션은 안 건드림.
set -u

# 매칭 패턴 = combat_nav2 스택 프로세스 (시스템/ssh/자기자신 제외)
PAT='board_bringup|lean_bringup|ros2 launch|ros2 run|_ros2_launch|/opt/ros/jazzy/lib/nav2|/opt/ros/jazzy/lib/robot_localization|/opt/ros/jazzy/lib/robot_state_publisher|combatrobot_1/ros/install|ekf_node|ekf_filter|gnss_heading|gps_to_map_node|frame_fixer_node|[x]sens_mti|rslidar_sdk|can_reader.py|operation_system_node|command_server_node|swarm_path_executor|swarm_lidar_filter|navsat_transform|component_container'

# ★ 호출자 제외: board_bringup 이 자기 기동 전에 board_kill 을 부를 때 자신을 죽이지
#   않도록, 제외할 PID 를 $1 로 받는다(그 PID 와 그 자식=board_kill 자신 보호).
EXCL="${1:-0}"
MYPID=$$
kill_round() {
  ps -eo pid,ppid,args \
    | grep -vE 'grep| ps -eo|sshd|board_kill' \
    | grep -E "$PAT" \
    | awk -v ex="$EXCL" -v me="$MYPID" '$1!=ex && $2!=ex && $1!=me && $2!=me {print $1}' \
    | xargs -r kill -9 2>/dev/null
}

# nav2 노드는 respawn 될 수 있으니 2회 반복 후 확인
kill_round; sleep 2; kill_round; sleep 2

REMAIN=$(ps -eo args | grep -vE 'grep|board_kill| ps -eo' | grep -cE "$PAT")
echo "board_kill: 남은 프로세스 = $REMAIN"
if [ "$REMAIN" -gt 0 ]; then
  echo "  ⚠ 아직 남음 → 한번 더 kill"
  kill_round; sleep 2
  echo "  최종 남은 = $(ps -eo args | grep -vE 'grep|board_kill| ps -eo' | grep -cE "$PAT")"
fi
