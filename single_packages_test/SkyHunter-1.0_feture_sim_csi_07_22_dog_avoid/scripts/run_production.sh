#!/usr/bin/env bash
# One-shot launcher: env setup → (optional build) → production deployment.
#
# Production deployment_mode runs the standard FSM:
#   RECON → MOVE, PROTECT_GENERAL → SURVEILLANCE, ASSAULT → ASSAULT_STATE, etc.
# (No fixed demo sequence override.) Hardware permission setup is on by
# default — the script chmod 666's /dev/ttyPTZ /dev/ttyTELEOP /dev/ttyAMA3
# /dev/hailo0 if present, skipping any that are absent.
#
# All real-hardware subsystems are on by default. Disable any one with
# the corresponding use_* arg passed after --.
#
# Usage:
#   ./scripts/run_production.sh [--build] [--skip-permissions] [-- <ros2 launch args>]
#
# Examples:
#   ./scripts/run_production.sh
#   ./scripts/run_production.sh --build
#   ./scripts/run_production.sh -- use_swarm_coordinator:=true swarm_role:=leader robot_id:=1
#   ./scripts/run_production.sh -- use_swarm_coordinator:=true swarm_role:=follower robot_id:=2 leader_robot_id:=1
#   ./scripts/run_production.sh -- use_sensor:=true use_visualization:=true

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
if [[ "${DO_BUILD}" == "1" ]]; then
  build_workspace
fi
source_workspace_overlay

if [[ "${SKIP_PERMS}" != "1" ]]; then
  setup_hw_permissions
fi

echo "[launch] combat_robot.launch.xml deployment_mode:=production"
exec ros2 launch combat_robot_launch combat_robot.launch.xml \
  deployment_mode:=production \
  "${LAUNCH_EXTRA[@]}"
