#!/bin/bash
# 한 rviz에 s1(리더)+s2(팔로워) 동시 표시.
# 두 로봇은 각자 GPS datum으로 독립 map 원점 → s1/map, s2/map 분리 트리.
# 둘 다 ENU(동일 방향)라 평행이동만 이으면 됨. datum 오프셋 = fromLL(s2 datum) in s1/map.
# TX/TY: /s2/toLL(0,0,0)->위경도 -> /s1/fromLL -> s1/map 좌표.
set -m
source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0 RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/rviz_host.xml
export DISPLAY=:0
export XAUTHORITY=$(ls /run/user/1000/.mutter-Xwaylandauth.* 2>/dev/null | head -1)

TX="${TX:--0.11792}"; TY="${TY:--1.38102}"
CFG="${CFG:-$HOME/swarm_view2.rviz}"

# s1/map -> s2/map 정적 브리지(회전0)
ros2 run tf2_ros static_transform_publisher \
  --x "$TX" --y "$TY" --z 0 --roll 0 --pitch 0 --yaw 0 \
  --frame-id s1/map --child-frame-id s2/map &

# 네임스페이스 tf를 공용 /tf 로 병합
ros2 run topic_tools relay /s1/tf /tf &
ros2 run topic_tools relay /s2/tf /tf &
ros2 run topic_tools relay /s1/tf_static /tf_static &
ros2 run topic_tools relay /s2/tf_static /tf_static &
sleep 2

# 통합 뷰(Fixed Frame s1/map, s1+s2 costmap/plan). 기본 /tf 사용(remap 없음).
exec rviz2 -d "$CFG"
