#!/usr/bin/env bash
# 호스트에서 mesh 보드 swarm 뷰(TF+local costmap+경로) rviz2 실행
# 인자: 로봇 네임스페이스 (기본 s1). 예: rviz_swarm.sh s2
NS="${1:-s1}"
export DISPLAY="${DISPLAY:-:0}"
source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=~/rviz_host.xml   # 유니캐스트 initialPeers(.51/.52) - wifi 멀티캐스트 차단 우회
exec rviz2 -d ~/swarm_view.rviz \
  --ros-args -r /tf:="/${NS}/tf" -r /tf_static:="/${NS}/tf_static"
