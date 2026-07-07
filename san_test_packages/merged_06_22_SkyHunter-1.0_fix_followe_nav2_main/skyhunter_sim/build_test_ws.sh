#!/usr/bin/env bash
# shellcheck disable=SC1091
# Build the convoy demo entirely from REPO sources (validates the vendoring):
# vendored Go2 SITL (third_party) + san_description + san_operator_tools, fresh
# overlay on /opt/ros/jazzy only (NOT go2_ws). Output -> /root/skytest_ws.
# Log: /tmp/skytest_build.log
exec >/tmp/skytest_build.log 2>&1
set +e
. /opt/ros/jazzy/setup.bash
ln -sfn "/mnt/c/Users/Taegeun Kim/Google_Drive/CLAUDE/SkyHunter-1.0" /root/sky
TP=/root/sky/ros/src/third_party
CRS=/root/sky/ros/src/skyautonet/combat_robot_system
mkdir -p /root/skytest_ws
cd /root/skytest_ws || exit 1
echo "===== colcon build (repo sources) ====="
colcon build --symlink-install \
  --base-paths "$TP" "$CRS" \
  --packages-select champ_msgs champ champ_base unitree_go2_description unitree_go2_sim \
                    combat_robot_msgs san_description san_operator_tools \
  2>&1 | tail -40
echo "===== verify ====="
. /root/skytest_ws/install/setup.bash 2>/dev/null
for p in unitree_go2_sim san_description san_operator_tools champ_base; do
  echo "  $p: $(ros2 pkg prefix "$p" 2>/dev/null || echo MISSING)"
done
echo "  convoy execs: $(ros2 pkg executables san_operator_tools 2>/dev/null | grep -c convoy)"
echo "BUILD_TEST_DONE"
