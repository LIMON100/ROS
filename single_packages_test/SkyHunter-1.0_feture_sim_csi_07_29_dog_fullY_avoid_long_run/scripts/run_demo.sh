#!/usr/bin/env bash
# One-shot launcher: env setup → (optional build) → demo deployment.
#
# Demo mode runs the full FSM with deployment_mode:=demo so that any
# operator command (RECON/PROTECT_GENERAL/ASSAULT/...) triggers the
# fixed demo sequence: forward → scan → (aim + fire per queued target)
# → reverse → IDLE. Tuning lives in
#   ros/src/skyautonet/combat_robot_system/combat_robot_operation_system/config/params.demo.yaml
# and is selected automatically by the deployment_mode arg.
#
# All real-hardware subsystems are on by default. Disable any one with
# the corresponding use_* arg passed after --.
#
# Usage:
#   ./scripts/run_demo.sh [--build] [--skip-permissions] [-- <ros2 launch args>]
#
# Examples:
#   ./scripts/run_demo.sh
#   ./scripts/run_demo.sh --build
#   ./scripts/run_demo.sh -- use_detector:=false use_teleop:=false
#   ./scripts/run_demo.sh -- robot_id:=2 use_swarm_coordinator:=true swarm_role:=follower leader_robot_id:=1

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

# Visualization (ImGui overlay) is on by default — let it find the local
# X server on the firefly target when launched over SSH (no DISPLAY set).
export DISPLAY="${DISPLAY:-:0}"

echo "[launch] combat_robot.launch.xml deployment_mode:=demo use_visualization:=true (DISPLAY=${DISPLAY})"
exec ros2 launch combat_robot_launch combat_robot.launch.xml \
  deployment_mode:=demo \
  use_visualization:=true \
  "${LAUNCH_EXTRA[@]}"
