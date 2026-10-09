#!/usr/bin/env bash
# Target-board system dependency installer.
# Hailo driver/runtime + libmodbus + GStreamer + rosdep-driven ROS deps.
# Run once on the RK3588 target. ROS2 Humble must already be installed.

set -e

# --- Hailo accelerator (human_detector) ---------------------------------
# Place hailort_*.deb and hailort-pcie-driver_*.deb under ~/Downloads first.
cd ~/Downloads
sudo apt update
sudo apt install -y linux-headers-"$(uname -r)"
sudo dpkg -i hailort_*.deb hailort-pcie-driver_*.deb
hailortcli scan

# --- libmodbus (teleop_controller chassis driver, Modbus RTU) -----------
# Picked up by teleop_controller's CMake; not declared in any package.xml,
# so rosdep won't install it.
sudo apt install -y libmodbus-dev

# --- GStreamer (camera_interface + robot_server RTSP pipeline) ----------
# Used via pkg-config (gstreamer-1.0 / gstreamer-app-1.0 /
# gstreamer-rtsp-server-1.0) — not declared in any package.xml.
# Runtime element coverage: appsrc/appsink/queue/videoconvert/videoscale
# (base), rtph264pay (good), h264parse / jpegparse (bad).
# - mpph264enc (RK3588 hardware encoder) is provided by the Rockchip
#   MPP gstreamer plugin from the board image, not from apt here.
# - libcamerasrc is provided by the libcamera submodule under ros/src/.
sudo apt install -y \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  libgstreamer-plugins-bad1.0-dev \
  libgstrtspserver-1.0-dev \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-tools \
  gstreamer1.0-rtsp

# --- ROS2 workspace dependencies (rosdep) -------------------------------
# Reads <depend> entries from each package.xml under ros/src/ and apt-
# installs the matching system packages (image_transport, cv_bridge,
# opencv, lifecycle_msgs, …).
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/.." && pwd)

sudo apt install -y python3-rosdep
if [ ! -f /etc/ros/rosdep/sources.list.d/20-default.list ]; then
  sudo rosdep init
fi
rosdep update
rosdep install --from-paths "$REPO_ROOT/ros/src" --ignore-src -r -y

# --- (Removed — PC-side monitoring tools, see README §6) ----------------
# python3-pyqt5 / ros-humble-rqt[-common-plugins,-image-view] /
# ros-humble-compressed-image-transport are not used by any node in this
# repo (rqt is for developer-PC monitoring with matching ROS_DOMAIN_ID).
# Install them on the developer PC if you want rqt views, not here.
