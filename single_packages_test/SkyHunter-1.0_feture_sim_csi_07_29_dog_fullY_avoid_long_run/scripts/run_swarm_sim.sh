#!/usr/bin/env bash
# One-shot launcher: 2-robot namespaced Gazebo swarm sim (leader s1 + follower s2).
#
# Brings up one shared gz world and a full per-robot stack (/s1, /s2):
# robot_state_publisher + gz spawn/bridge + robot_localization (ekf/navsat,
# shared datum) + nav2 + swarm_path_executor + command_server (role adapter).
# The follower drives a lateral formation offset of the leader's path.
#
# Usage:
#   ./scripts/run_swarm_sim.sh [--build] [-- <ros2 launch args>]
#
# Examples:
#   ./scripts/run_swarm_sim.sh                       # 2 robots
#   ./scripts/run_swarm_sim.sh --build
#   ./scripts/run_swarm_sim.sh -- num_robots:=1      # leader only
#   ./scripts/run_swarm_sim.sh -- formation_lateral_spacing_m:=3.0
#
# Inject a path/formation (separate terminal, same env):
#   PJ='{"waypoints":[{"lat":36.6101256,"lon":127.2877257},{"lat":36.6102256,"lon":127.2877257}]}'
#   ros2 topic pub -t 5 /s1/swarm/path_command combat_robot_msgs/msg/SwarmPathCommand "{command: 5, path_json: '$PJ'}"
#   ros2 topic pub -t 5 /s2/swarm/path_command combat_robot_msgs/msg/SwarmPathCommand "{command: 5, path_json: '$PJ'}"
#   # then command:1 (START) to both. command: 5=LOAD 1=START 2=STOP 3=PAUSE 4=RESUME

set -eo pipefail

THIS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${THIS_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/common.sh
source "${THIS_DIR}/lib/common.sh"

PRINT_HELP=0
parse_common_args "$@"
if [[ "${PRINT_HELP}" == "1" ]]; then
  sed -n '/^# One-shot/,/^$/p' "$0" | sed 's/^# \?//'
  exit 0
fi

setup_ros_env
# gz-transport uses UDP multicast for discovery. With no GZ_IP set it auto-picks an
# interface, and on this host it sometimes grabs the dead docker0 (NO-CARRIER) →
# "Exception sending a multicast message: Network is unreachable" → gz sensor
# publishers (NavSat/IMU/lidar) never announce → /sN/fix etc. stay empty (followers
# show DEFAULT_GPS on the tablet). Pin gz-transport to loopback for this single-host
# sim; ROS2/DDS (FastDDS profile) is unaffected. Verified loopback discovery works here.
export GZ_IP=127.0.0.1
if [[ "${DO_BUILD}" == "1" ]]; then
  build_workspace
fi
source_workspace_overlay

echo "[swarm_sim] launching 2-robot Gazebo swarm (domain ${ROS_DOMAIN_ID})..."
exec ros2 launch combat_robot_nav2 swarm_sim.launch.py "${LAUNCH_EXTRA[@]}"
