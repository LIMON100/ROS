#!/usr/bin/env bash
# One-shot launcher: env setup → (optional build) → office-test launch.
#
# Bench/desk run with dummy data — no pan/tilt, gun, or chassis hardware
# is required. The dummy node drives a scenario script and most subsystem
# toggles are off by default (use_detector/use_pan_tilt/use_teleop/use_trigger).
#
# Usage:
#   ./scripts/run_test.sh [--build] [--skip-permissions] [-- <ros2 launch args>]
#
# Examples:
#   ./scripts/run_test.sh
#   ./scripts/run_test.sh --build
#   ./scripts/run_test.sh -- scenario:=attack_fire dummy_loop:=false
#   ./scripts/run_test.sh -- use_foxglove_bridge:=true

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

# Visualization (ImGui overlay) is on by default so the dummy-scenario
# detector/FSM transitions are visible. Default DISPLAY=:0 so it works
# over SSH against the firefly target's local monitor.
export DISPLAY="${DISPLAY:-:0}"

echo "[launch] combat_robot_test.launch.xml use_visualization:=true (dummy data, no HW required, DISPLAY=${DISPLAY})"
exec ros2 launch combat_robot_launch combat_robot_test.launch.xml \
  use_visualization:=true \
  "${LAUNCH_EXTRA[@]}"
