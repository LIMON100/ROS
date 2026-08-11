source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0; export RMW_IMPLEMENTATION=rmw_fastrtps_cpp; export FASTRTPS_DEFAULT_PROFILES_FILE=~/mesh_dds.xml
B=~/combatrobot_1/ros/src/skyautonet/combat_robot_system
# === 정리 ===
for p in '[b]ringup_realtime' '[s]warm_path_executor' '[e]kf_node' '[c]ontroller_server' '[p]lanner_server' '[b]t_navigator' '[b]ehavior_server' '[s]moother_server' '[m]ap_server' '[n]avsat_transform' '[l]ifecycle_manager' '[c]ostmap' '[x]sens_mti_node' '[r]slidar_sdk_node' '[f]rame_fixer' '[r]obot_state_publisher' '[g]nss_heading' '[c]ommand_server_node' 'topic_tools'; do pkill -9 -f "$p" 2>/dev/null; done
sleep 3
EKF=$B/combat_nav2/combat_robot_nav2/config/ekf.yaml
cp -n $EKF ${EKF}.bak 2>/dev/null; sed -i 's#imu0: /gps/heading_imu#imu0: /imu/data#g' $EKF
XY=$B/combat_nav2/bluespace_ai_xsens_ros_mti_driver/param/xsens_mti_node.yaml
# === s2 swarm: xsens + lidar(loopback) + nav2 ===
ros2 run bluespace_ai_xsens_mti_driver xsens_mti_node --ros-args --params-file $XY -r /imu/data:=/s2/imu/data -p frame_id:=s2/imu_link >/tmp/s2_xsens.log 2>&1 &
sleep 2
FASTRTPS_DEFAULT_PROFILES_FILE=/tmp/lidar_local.xml ros2 launch rslidar_sdk start.py >/tmp/s2_lidar.log 2>&1 &
sleep 4
ros2 launch combat_robot_nav2 bringup_realtime.launch.py robot_ns:=s2 robot_id:=2 leader_robot_id:=1 formation_mode:=static formation_enable:=true use_sim_time:=false >/tmp/s2_bringup.log 2>&1 &
sleep 30
# === GPS -> /s2/fix ===
ros2 run combat_robot_nav2 gnss_heading_node --ros-args -p port:=/dev/gps -p baud:=921600 -p antenna_yaw_offset_deg:=90.0 -r /fix:=/s2/fix -r /gps/heading_imu:=/s2/gps/heading_imu -r /vel:=/s2/vel -r /edge_heading:=/s2/edge_heading >/tmp/s2_gnss.log 2>&1 &
sleep 4
# === command_server (follower) ===
ros2 run robot_server command_server_node --ros-args -p robot_id:=2 -p role:=follower -p position_topic:=/s2/fix >/tmp/s2_cmdsrv.log 2>&1 &
echo "board2 full started"
sleep 5
echo "nodes: $(ros2 node list 2>/dev/null | grep -c /s2/)"
