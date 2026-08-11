#!/usr/bin/env bash
# 이 호스트(robot_server·nav만 빌드됨)에서 robot_server(command_server + rtsp_server)만
# 메인 combat_robot.launch.xml 로 띄운다.
# 기본 launch 는 use_operator/use_detector/use_teleop/use_trigger/use_controller 가 true 라
# 미빌드 패키지(operation_system/camera/human_detector/pan_tilt 등)를 찾다가 실패하므로,
# 그 그룹들을 모두 끄고 use_robot_server 만 켠다.
set -e
source /opt/ros/jazzy/setup.bash
source /home/skyauto-csi/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=96
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=/home/skyauto-csi/combatrobot_1/scripts/fastdds_profile.xml
unset CYCLONEDDS_URI

ROBOT_ID="${1:-1}"

exec ros2 launch combat_robot_launch combat_robot.launch.xml \
  use_robot_server:=true \
  use_detector:=false use_operator:=false use_controller:=false \
  use_teleop:=false use_trigger:=false use_camera_motor:=false \
  use_sensor:=false use_gnss:=false use_visualization:=false \
  use_swarm_coordinator:=false \
  robot_id:="${ROBOT_ID}" deployment_mode:=production \
  params_overlay_file:=/home/skyauto-csi/combatrobot_1/ros/install/robot_server/share/robot_server/config/rtsp_server.yaml
