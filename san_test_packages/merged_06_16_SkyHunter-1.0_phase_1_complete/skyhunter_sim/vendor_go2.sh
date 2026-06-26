#!/usr/bin/env bash
# shellcheck disable=SC2044
# Re-vendor the Go2 SITL (unitree_go2_ros2 + CHAMP) into the repo.
# Usage: vendor_go2.sh [SRC_WORKING_COPY]   (default: /root/go2_ws/src/unitree_go2_ros2)
# Dest = ros/src/third_party/. Excludes .git + *.bak/*.sensorbak so the SkyHunter
# working state (odom_gt, gait, sensors-off) is vendored without backup clutter.
set -e
SRC="${1:-/root/go2_ws/src/unitree_go2_ros2}"
DEST=/root/sky/ros/src/third_party
mkdir -p "$DEST"
rm -rf "$DEST/unitree_go2_ros2"
if command -v rsync >/dev/null 2>&1; then
  rsync -a --exclude='.git' --exclude='*.bak' --exclude='*.sensorbak' "$SRC" "$DEST/"
else
  cp -r "$SRC" "$DEST/unitree_go2_ros2"
  rm -rf "$DEST/unitree_go2_ros2/.git"
  find "$DEST/unitree_go2_ros2" \( -name '*.bak' -o -name '*.sensorbak' \) -delete
fi
echo "vendored: $(du -sh "$DEST/unitree_go2_ros2" | cut -f1), $(find "$DEST/unitree_go2_ros2" -type f | wc -l) files"
echo "see SKYHUNTER_PROVENANCE.md for upstream + license + SkyHunter mods"
