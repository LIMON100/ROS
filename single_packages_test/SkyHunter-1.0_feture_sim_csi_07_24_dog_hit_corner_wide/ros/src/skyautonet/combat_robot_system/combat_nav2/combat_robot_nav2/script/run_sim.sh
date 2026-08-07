#!/usr/bin/env bash
# run_sim.sh — 보드 없이 s1/s2 두 대를 mock_board로 시뮬레이션(데이터 송수신 테스트용).
# 사용법:
#   run_sim.sh            # s1(0,0) + s2(-2,0) mock 기동 (정지)
#   run_sim.sh move       # 둘 다 heading 방향으로 전진
#   run_sim.sh rviz       # mock + 호스트 merged rviz(rviz_both) 함께
# 종료: run_sim.sh stop
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
MESH="$(cd "$HERE/../mesh" && pwd)"
MODE="${1:-}"

export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-0}"
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
unset FASTRTPS_DEFAULT_PROFILES_FILE
source /opt/ros/jazzy/setup.bash

kill_all() {
  pkill -9 -f "[m]ock_board.py" 2>/dev/null
  pkill -9 -f "[s]tatic_transform_publisher.*s2/map" 2>/dev/null
  pkill -9 -f "topic_tools/[r]elay /s" 2>/dev/null
  pkill -9 -x rviz2 2>/dev/null
}

if [ "$MODE" = "stop" ]; then kill_all; echo "sim stopped"; exit 0; fi
kill_all; sleep 1

MV=false; [ "$MODE" = "move" ] && MV=true
# s1 = 리더 (0,0) 북동(45°), s2 = 팔로워 (-2,0) 동일 heading
setsid python3 "$HERE/mock_board.py" --ros-args -p ns:=s1 -p spawn_x:=0.0  -p spawn_y:=0.0 -p heading_deg:=45 -p move:=$MV >/tmp/mock_s1.log 2>&1 < /dev/null &
setsid python3 "$HERE/mock_board.py" --ros-args -p ns:=s2 -p spawn_x:=-2.0 -p spawn_y:=0.0 -p heading_deg:=45 -p move:=$MV >/tmp/mock_s2.log 2>&1 < /dev/null &
sleep 2
echo "mock s1/s2 기동 (move=$MV). 토픽: /s1/*, /s2/*"

if [ "$MODE" = "rviz" ]; then
  # 두 mock을 한 rviz에: s1/map->s2/map 브리지는 sim에선 고정 (-2,0)
  DISPLAY="${DISPLAY:-:0}" bash "$MESH/rviz_both.sh" -2.0 0.0
fi
