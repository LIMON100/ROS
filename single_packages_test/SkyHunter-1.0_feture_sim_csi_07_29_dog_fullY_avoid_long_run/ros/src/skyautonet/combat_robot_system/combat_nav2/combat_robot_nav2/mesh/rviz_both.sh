#!/usr/bin/env bash
# 호스트에서 s1+s2 두 로봇을 한 rviz에 (TF+local costmap+경로). 리더(s1) datum 기준 병합.
# 사용법: rviz_both.sh [xoff yoff]  (s1/map->s2/map 오프셋, 기본 GPS 계산값)
XOFF="${1:--1.40}"   # East (s1/map x)
YOFF="${2:-1.29}"    # North (s1/map y)
export DISPLAY="${DISPLAY:-:0}"
source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=~/rviz_host.xml   # 유니캐스트 .51/.52 (wifi 멀티캐스트 차단 우회)

# 이전 브리지/relay 정리 (self-kill 방지 브래킷)
pkill -9 -f "[s]tatic_transform_publisher.*s2/map" 2>/dev/null
pkill -9 -f "topic_tools/[r]elay /s" 2>/dev/null
sleep 1

# 리더 datum 기준 s1/map -> s2/map 정적 브리지
setsid ros2 run tf2_ros static_transform_publisher --x "$XOFF" --y "$YOFF" --z 0 \
  --frame-id s1/map --child-frame-id s2/map >/tmp/tfbridge.log 2>&1 < /dev/null &
# 네임스페이스 TF 병합: /s{1,2}/tf -> /tf , /s{1,2}/tf_static -> /tf_static
setsid ros2 run topic_tools relay /s1/tf /tf >/tmp/relay_s1tf.log 2>&1 < /dev/null &
setsid ros2 run topic_tools relay /s2/tf /tf >/tmp/relay_s2tf.log 2>&1 < /dev/null &
setsid ros2 run topic_tools relay /s1/tf_static /tf_static >/tmp/relay_s1tfs.log 2>&1 < /dev/null &
setsid ros2 run topic_tools relay /s2/tf_static /tf_static >/tmp/relay_s2tfs.log 2>&1 < /dev/null &
sleep 3
exec rviz2 -d ~/swarm_view2.rviz
