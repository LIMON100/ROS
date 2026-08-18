#!/bin/bash
# 한 rviz 에 s2(리더)+s4(팔로워) 동시 표시. 각자 GPS datum 독립 map → s2/map, s4/map.
# 브리지 s2/map->s4/map 평행이동 = 두 datum GPS 차(ENU). 기본값은 현재 GPS 기준 근사.
set -m
source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0 RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/can_host.xml
export DISPLAY=:0
export XAUTHORITY=$(ls /run/user/1000/.mutter-Xwaylandauth.* 2>/dev/null | head -1)

TX="${TX:--1.074}"; TY="${TY:-1.293}"   # s4/map 원점 in s2/map (East,North)
CFG="${CFG:-$(dirname "$0")/swarm_view_s2s4.rviz}"

ros2 run tf2_ros static_transform_publisher \
  --x "$TX" --y "$TY" --z 0 --roll 0 --pitch 0 --yaw 0 \
  --frame-id s2/map --child-frame-id s4/map &

ros2 run topic_tools relay /s2/tf /tf &
ros2 run topic_tools relay /s4/tf /tf &
ros2 run topic_tools relay /s2/tf_static /tf_static &
ros2 run topic_tools relay /s4/tf_static /tf_static &
sleep 2
exec rviz2 -d "$CFG"
