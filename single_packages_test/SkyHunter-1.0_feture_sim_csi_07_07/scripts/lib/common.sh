#!/usr/bin/env bash
# Shared env-setup helpers for the run_*.sh launchers.
#
# Do NOT enable `set -u` here or in callers: /opt/ros/humble/setup.bash
# references unbound AMENT_TRACE_SETUP_FILES and aborts under nounset.

ROS_DISTRO_DEFAULT="${ROS_DISTRO_DEFAULT:-humble}"

setup_ros_env() {
  local distro="${ROS_DISTRO:-${ROS_DISTRO_DEFAULT}}"
  local ros_setup="/opt/ros/${distro}/setup.bash"
  if [[ ! -f "${ros_setup}" ]]; then
    echo "[env] ERROR: ${ros_setup} not found. Is ROS2 ${distro} installed?" >&2
    return 1
  fi
  # shellcheck disable=SC1090
  source "${ros_setup}"

  export ROS_DOMAIN_ID="${ROS_DOMAIN_ID:-96}"
  export RCUTILS_COLORIZED_OUTPUT="${RCUTILS_COLORIZED_OUTPUT:-1}"

  # FastDDS 전송 프로파일 자동 로드.
  # 대용량 BestEffort 토픽(LiDAR PointCloud 등)이 소켓 버퍼 부족으로 드롭되지
  # 않도록 socket buffer 를 16MB 로 명시한다. (scripts/fastdds_profile.xml 참조;
  # 커널 버퍼는 scripts/sysctl/60-dds-socket-buffers.conf 로 별도 영구화.)
  # 이미 외부에서 지정했으면 존중하고, 파일이 있을 때만 export 한다.
  local this_dir profile
  this_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
  profile="${FASTRTPS_DEFAULT_PROFILES_FILE:-${this_dir}/../fastdds_profile.xml}"
  if [[ -f "${profile}" ]]; then
    export FASTRTPS_DEFAULT_PROFILES_FILE="${profile}"
  else
    echo "[env] WARN: FastDDS profile not found at ${profile} (using FastDDS defaults)" >&2
  fi

  echo "[env] ROS_DISTRO=${distro} ROS_DOMAIN_ID=${ROS_DOMAIN_ID} FASTDDS_PROFILE=${FASTRTPS_DEFAULT_PROFILES_FILE:-<none>}"
}

source_workspace_overlay() {
  local overlay="${REPO_ROOT}/ros/install/setup.bash"
  if [[ ! -f "${overlay}" ]]; then
    echo "[env] ERROR: ${overlay} not found. Re-run with --build, or build manually first." >&2
    return 1
  fi
  # shellcheck disable=SC1090
  source "${overlay}"
  echo "[env] workspace overlay sourced: ${overlay}"
}

build_workspace() {
  echo "[build] colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release"
  (
    cd "${REPO_ROOT}/ros"
    colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release
  )
}

# Best-effort permission setup for serial / accelerator devices.
# Each device is independently skipped if absent, so this is safe on dev
# boxes without the full hardware kit.
setup_hw_permissions() {
  local devs=(/dev/ttyPTZ /dev/ttyTELEOP /dev/ttyAMA3 /dev/hailo0)
  local touched=0
  for d in "${devs[@]}"; do
    if [[ -e "${d}" ]]; then
      if sudo -n chmod 666 "${d}" 2>/dev/null || sudo chmod 666 "${d}"; then
        echo "[perm] chmod 666 ${d}"
        touched=$((touched + 1))
      else
        echo "[perm] WARN: chmod ${d} failed (continuing)" >&2
      fi
    fi
  done
  if [[ "${touched}" == "0" ]]; then
    echo "[perm] no hardware devices present — assuming dummy/test setup"
  fi
}

# Common arg parser shared by run_*.sh.
# Sets DO_BUILD, SKIP_PERMS, LAUNCH_EXTRA (array) in the caller's scope.
# Usage: parse_common_args "$@"
parse_common_args() {
  DO_BUILD=0
  SKIP_PERMS=0
  LAUNCH_EXTRA=()
  while [[ $# -gt 0 ]]; do
    case "$1" in
      --build) DO_BUILD=1; shift ;;
      --skip-permissions|--no-perms) SKIP_PERMS=1; shift ;;
      --) shift; LAUNCH_EXTRA=("$@"); return 0 ;;
      -h|--help) PRINT_HELP=1; shift ;;
      *) echo "Unknown arg: $1 (pass launch args after --)" >&2; return 2 ;;
    esac
  done
}
