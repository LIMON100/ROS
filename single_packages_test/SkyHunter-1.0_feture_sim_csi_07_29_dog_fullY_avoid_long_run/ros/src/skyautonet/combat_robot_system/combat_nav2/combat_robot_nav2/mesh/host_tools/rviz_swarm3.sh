#!/bin/bash
# rviz_swarm3.sh — 호스트에서 s2(리더)+s3+s4(팔로워) 통합 rviz.
#   각 로봇 ns tf(/sN/tf)를 공용 /tf 로 병합(relay) + s2/map ↔ s3/map ↔ s4/map 정적 브리지.
#   Fixed Frame = s2/map. 각 로봇 costmap/plan/slot마커, 글로벌 leader_ref_path 표시.
#
# 맵 브리지 오프셋: 리더 datum 브로드캐스트(/swarm/datum)로 맵원점이 대체로 정합(gap~1.4m).
#   어긋나 보이면 S3_TX/S3_TY, S4_TX/S4_TY 로 ENU 평행이동 보정.
#     예:  S4_TX=1.2 S4_TY=-0.5 ~/rviz_swarm3.sh
#
# ★ full-swarm 주행 중 호스트 뷰는 wifi 부하다. 갱신 rate 낮음(정상). 필요할 때만.
set -m
source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0 RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/rviz_host.xml    # peers .12/.13/.14
export DISPLAY=:0
export XAUTHORITY=$(ls -t /run/user/1000/.mutter-Xwaylandauth.* 2>/dev/null | head -1)

S3_TX="${S3_TX:-0}"; S3_TY="${S3_TY:-0}"
S4_TX="${S4_TX:-0}"; S4_TY="${S4_TY:-0}"
CFG="${CFG:-$HOME/swarm_view3.rviz}"

cleanup_old() {
  pkill -9 -f "topic_tools.*relay /s[234]/tf"        2>/dev/null
  pkill -9 -f "static_transform_publisher.*s[34]/map" 2>/dev/null
  pkill -9 -f "rviz2 -d $CFG"                          2>/dev/null
}
cleanup_old; sleep 1

echo "[rviz_swarm3] fixed=s2/map  bridge s3=($S3_TX,$S3_TY) s4=($S4_TX,$S4_TY)  cfg=$CFG"

HELPERS=()
for ns in s2 s3 s4; do
  ros2 run topic_tools relay /$ns/tf /tf              & HELPERS+=($!)
  ros2 run topic_tools relay /$ns/tf_static /tf_static & HELPERS+=($!)
done
ros2 run tf2_ros static_transform_publisher --x "$S3_TX" --y "$S3_TY" --z 0 \
  --roll 0 --pitch 0 --yaw 0 --frame-id s2/map --child-frame-id s3/map & HELPERS+=($!)
ros2 run tf2_ros static_transform_publisher --x "$S4_TX" --y "$S4_TY" --z 0 \
  --roll 0 --pitch 0 --yaw 0 --frame-id s2/map --child-frame-id s4/map & HELPERS+=($!)

trap 'kill "${HELPERS[@]}" 2>/dev/null; cleanup_old' EXIT INT TERM
sleep 2
rviz2 -d "$CFG"    # exec 아님 → 종료 후 trap 이 relay/브리지 정리
