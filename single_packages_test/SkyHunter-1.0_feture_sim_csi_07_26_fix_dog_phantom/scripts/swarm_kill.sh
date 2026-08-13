#!/bin/bash
# ============================================================================
# swarm_kill.sh — Gazebo+nav2 멀티로봇 sim 스택 완전 정리 (단일호스트 sim 전용)
# ============================================================================
# 왜 필요한가:
#   · 재시작을 반복하면 launch 부모(run_swarm_sim.sh / ros2 launch)가 살아남아
#     자식 노드를 재spawn → 스택이 누적 → load average 50+ → executor CPU 기아 →
#     미션이 안 움직이거나 form-up 이 freeze (코드 버그처럼 보이지만 환경 문제!).
#   · 죽은 launch 가 /dev/shm/fastrtps_* SHM 세그먼트를 남김 → ~1500개 누적 시
#     SHM 고갈 → 새 executor 가 discovery 실패 → 미션이 아예 시작 안 됨.
#
# 반드시 dangerouslyDisableSandbox 로 실행 (sandbox 가 kill 신호를 조용히 삼킴).
#
# 절차: ① launch 부모 먼저 kill(재spawn 차단) ② 전체 sim 노드 kill(반복)
#       ③ 노드 0 확인되면 /dev/shm FastDDS 세그먼트 정리 ④ load 보고
# 사용 후 load < ~6/16, shm_fast 0 확인하고 재기동할 것.
# ============================================================================

SELF=$$

# ① launch 부모 먼저 (자식 재spawn 차단)
PARENTS=$(ps -eo pid,cmd | grep -E "run_swarm_sim\.sh|ros2 launch combat|bin/ros2 launch|combat_robot_nav2.*launch\.py" | grep -v grep | grep -vw "$SELF" | awk '{print $1}')
[ -n "$PARENTS" ] && kill -9 $PARENTS 2>/dev/null
sleep 2

# ② 전체 sim 노드 (패턴을 이 파일 안에 둬서 자기 cmdline 과 self-match 안 함)
PATS='swarm_path_executor|gz sim|gz-sim|gz_tools|[r]uby|ros_gz|component_container|nav2_|ekf_node|navsat_transform|robot_state_publisher|command_server|combat_robot_operation|tf2_ros|static_transform|gz-transport|topic_tools|/relay |rviz2|map_server|lifecycle_manager|swarm_lidar_filter|frame_fixer|gnss_heading|controller_server|planner_server|bt_navigator|behavior_server|velocity_smoother|collision_monitor|goal_bridge|/opt/ros/jazzy/lib'
for i in 1 2 3 4; do
  pkill -9 -f "$PATS" 2>/dev/null
  sleep 1.5
done

echo "=== remaining sim procs ==="
pgrep -af "$PATS" | grep -v swarm_kill | head

# ③ 노드 0 일 때만 SHM 정리 (살아있는 노드 segment 를 지우면 안 됨)
if [ "$(pgrep -fc 'swarm_path_executor|gz sim')" -eq 0 ]; then
  ls /dev/shm/ 2>/dev/null | grep -iE 'fastrtps|fastdds|sem.fast|datasharing' | xargs -r -I{} rm -f /dev/shm/{}
  echo "=== /dev/shm FastDDS 세그먼트 정리 완료 ==="
else
  echo "=== 노드 잔존 → SHM 정리 건너뜀(다시 실행할 것) ==="
fi

echo "=== shm_fast=$(ls /dev/shm/ 2>/dev/null | grep -icE 'fastrtps|fastdds|sem.fast')  loadavg=$(cat /proc/loadavg) ==="
echo "=== 재기동 전 load<6 / shm_fast=0 확인할 것 ==="
