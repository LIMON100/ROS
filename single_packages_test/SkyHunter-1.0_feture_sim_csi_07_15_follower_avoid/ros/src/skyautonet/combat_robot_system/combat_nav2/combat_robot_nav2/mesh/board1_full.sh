source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0; export RMW_IMPLEMENTATION=rmw_fastrtps_cpp; export FASTRTPS_DEFAULT_PROFILES_FILE=~/mesh_dds.xml
B=~/combatrobot_1/ros/src/skyautonet/combat_robot_system
# === 정리 ===
for p in '[b]ringup_realtime' '[s]warm_path_executor' '[e]kf_node' '[c]ontroller_server' '[p]lanner_server' '[b]t_navigator' '[b]ehavior_server' '[s]moother_server' '[m]ap_server' '[n]avsat_transform' '[l]ifecycle_manager' '[c]ostmap' '[x]sens_mti_node' '[r]slidar_sdk_node' '[f]rame_fixer' '[r]obot_state_publisher' '[g]nss_heading' '[c]ommand_server_node' '[o]peration_system_node' 'topic_tools'; do pkill -9 -f "$p" 2>/dev/null; done
sleep 3
EKF=$B/combat_nav2/combat_robot_nav2/config/ekf.yaml
cp -n $EKF ${EKF}.bak 2>/dev/null; sed -i 's#imu0: /gps/heading_imu#imu0: /imu/data#g' $EKF
XY=$B/combat_nav2/bluespace_ai_xsens_ros_mti_driver/param/xsens_mti_node.yaml
# === s1 swarm: xsens + lidar(loopback) + nav2 ===
ros2 run bluespace_ai_xsens_mti_driver xsens_mti_node --ros-args --params-file $XY -r /imu/data:=/s1/imu/data -p frame_id:=s1/imu_link >/tmp/s1_xsens.log 2>&1 &
sleep 2
FASTRTPS_DEFAULT_PROFILES_FILE=/tmp/lidar_local.xml ros2 launch rslidar_sdk start.py >/tmp/s1_lidar.log 2>&1 &
sleep 4
ros2 launch combat_robot_nav2 bringup_realtime.launch.py robot_ns:=s1 robot_id:=1 leader_robot_id:=1 formation_followers:=2 formation_mode:=static formation_enable:=true use_sim_time:=false path_command_topic:=swarm/path_command >/tmp/s1_bringup.log 2>&1 &
sleep 30
# === GPS -> /s1/fix (gnss 직접 발행, relay 불필요 - board2와 동일 방식) ===
ros2 run combat_robot_nav2 gnss_heading_node --ros-args -p port:=/dev/gps -p baud:=921600 -p antenna_yaw_offset_deg:=90.0 -r /fix:=/s1/fix -r /gps/heading_imu:=/s1/gps/heading_imu -r /vel:=/s1/vel -r /edge_heading:=/s1/edge_heading >/tmp/s1_gnss.log 2>&1 &
sleep 4
# === command_server (leader, /s1/fix 구독) ===
ros2 run robot_server command_server_node --ros-args -p robot_id:=1 -p role:=leader -p position_topic:=/s1/fix >/tmp/s1_cmdsrv.log 2>&1 &
sleep 3
# === FSM 자율주행 (복원): executor가 swarm/path_command 직결로 path 받아 mission_state.total_waypoints 발행,
#     FSM 이 그 값을 operation_state 로 전달(state.cpp 1391). 태블릿 임무상태 + START 게이트 둘 다 정상화.
CFG=~/combatrobot_1/ros/install/combat_robot_operation_system/share/combat_robot_operation_system/config/params.autonomous.yaml
ros2 run combat_robot_operation_system combat_robot_operation_system_node --ros-args --params-file $CFG -r swarm/mission_state:=/s1/swarm/mission_state >/tmp/s1_fsm.log 2>&1 &
echo "board1 full started (FSM + direct path_command)"
sleep 5
echo "nodes: $(ros2 node list 2>/dev/null | grep -c /s1/)"
