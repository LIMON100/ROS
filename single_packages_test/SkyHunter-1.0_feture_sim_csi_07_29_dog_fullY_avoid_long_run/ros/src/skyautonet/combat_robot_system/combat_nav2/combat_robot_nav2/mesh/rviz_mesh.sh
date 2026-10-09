#!/bin/bash
# 호스트에서 mesh 편대(s2 리더 + s4 팔로워) 통합 rviz.
# 각 로봇의 네임스페이스 tf(/sN/tf)를 공용 /tf 로 병합 + s2/map<->s4/map 정적 브리지.
# rviz 는 Fixed Frame s2/map, 각 로봇 costmap/plan/footprint 를 이름표(sN)로 표시.
#
# ★ relay/브리지는 '여러 로봇 TF 를 한 rviz 에 합칠 때만' 필요. 한 로봇만 볼 땐
#   rviz2 -d cfg --ros-args -r /tf:=/s2/tf -r /tf_static:=/s2/tf_static 로 relay 없이 가능.
# ★ full-swarm 주행 중엔 호스트 뷰가 mesh 부하이므로 필요할 때만 띄울 것.
#
# 사용법:  ~/rviz_mesh.sh            (기본 s2+s4)
#   S4_TX / S4_TY : s2/map->s4/map 오프셋(GPS datum 차이, ENU 평행이동)
set -m
source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0 RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/rviz_host.xml
export DISPLAY=:0
export XAUTHORITY=$(ls /run/user/1000/.mutter-Xwaylandauth.* 2>/dev/null | head -1)

S4_TX="${S4_TX:-0}"; S4_TY="${S4_TY:-0}"
CFG="${CFG:-$HOME/swarm_view_mesh.rviz}"

# ★ 시스템정비 ①: 기동 전 이 뷰의 옛 helper(relay/브리지/rviz)를 항상 정리(멱등).
#   반복 실행으로 orphan relay·충돌 static_transform_publisher 가 누적돼 mesh 를
#   flooding 하던 것을 근본차단.
cleanup_old() {
  pkill -9 -f "topic_tools.*relay /s[0-9]*/tf"     2>/dev/null
  pkill -9 -f "static_transform_publisher.*s4/map" 2>/dev/null
  pkill -9 -f "rviz2 -d $CFG"                       2>/dev/null
}
cleanup_old
sleep 1

echo "[rviz_mesh] fixed=s2/map  s2/map->s4/map=($S4_TX,$S4_TY)  cfg=$CFG"

# 병합 helper 를 백그라운드로 띄우고 PID 추적
HELPERS=()
for ns in s2 s4; do
  ros2 run topic_tools relay /$ns/tf /tf & HELPERS+=($!)
  ros2 run topic_tools relay /$ns/tf_static /tf_static & HELPERS+=($!)
done
ros2 run tf2_ros static_transform_publisher \
  --x "$S4_TX" --y "$S4_TY" --z 0 --roll 0 --pitch 0 --yaw 0 \
  --frame-id s2/map --child-frame-id s4/map & HELPERS+=($!)

# ★ 시스템정비 ②: rviz 종료(창닫기/kill) 시 helper 를 반드시 같이 종료(orphan 방지).
#   exec 를 쓰지 않고 trap 으로 정리 → 종료 신호가 와도 relay/브리지가 안 남는다.
trap 'kill "${HELPERS[@]}" 2>/dev/null; cleanup_old' EXIT INT TERM

sleep 2
rviz2 -d "$CFG"   # exec 아님 → 종료 후 trap 이 helper 정리
