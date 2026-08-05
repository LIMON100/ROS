#!/usr/bin/env bash
# One-shot launcher: 3-robot namespaced Gazebo swarm sim (leader s1 + followers s2,s3).
#
# Thin wrapper over swarm_sim.launch.py with num_robots:=3. Brings up one shared gz
# world and a full per-robot stack for /s1 /s2 /s3:
#   robot_state_publisher + gz spawn/bridge + robot_localization (ekf/navsat,
#   shared datum) + nav2 + swarm_path_executor + command_server (role adapter).
#
# Formation slots (auto, must match executor formationSlotOffset):
#   s1 leader  → center      spawn (0, 0)
#   s2 follower→ +1 (left)   spawn (-spacing, 0)
#   s3 follower→ -1 (right)  spawn (+spacing, 0)
# On a north-bound path: +slot = west (screen left), -slot = east (screen right).
#
# Usage:
#   ./scripts/run_swarm_sim3.sh                      # 3 robots, static formation
#   ./scripts/run_swarm_sim3.sh --build              # rebuild workspace first
#   ./scripts/run_swarm_sim3.sh -- formation_lateral_spacing_m:=3.0
#   ./scripts/run_swarm_sim3.sh -- formation_mode:=dynamic
#
# This launcher only brings the stack UP (it blocks in the foreground). To make the
# robots DRIVE, open a SECOND terminal and run the path injector:
#   ./scripts/swarm_drive.sh                         # LOAD + START to s1,s2,s3
# See docs/swarm_sim_3robot.md for the full runbook.

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
# interface and on some hosts grabs a dead docker0 (NO-CARRIER) → gz sensor
# publishers (NavSat/IMU/lidar) never announce → /sN/fix stays empty. Pin gz-transport
# to loopback for this single-host sim; ROS2/DDS (FastDDS profile) is unaffected.
export GZ_IP=127.0.0.1
if [[ "${DO_BUILD}" == "1" ]]; then
  build_workspace
fi
source_workspace_overlay

# Force 3 robots, but let the user override anything else they pass after `--`.
# If they explicitly pass num_robots:=N after `--`, that later value wins.
echo "[swarm_sim3] launching 3-robot Gazebo swarm (domain ${ROS_DOMAIN_ID})..."
echo "[swarm_sim3] to drive: open another terminal → ./scripts/swarm_drive.sh"
exec ros2 launch combat_robot_nav2 swarm_sim.launch.py num_robots:=3 "${LAUNCH_EXTRA[@]}"
