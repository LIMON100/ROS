#!/bin/bash
# board_bringup.sh — mesh 보드 1대의 FSM 군집주행 풀 bringup (센서+CAN+nav2+FSM)
# ─────────────────────────────────────────────────────────────────────────
# 사용법:  ~/combatrobot_1/ros/src/.../mesh/board_bringup.sh <robot_id> <role> [leader_id] [followers]
#   리더 s2 :  board_bringup.sh 2 leader 2 4      (robot_id=2, leader, leader_id=2, followers=4)
#   팔로워 s4:  board_bringup.sh 4 follower 2      (robot_id=4, follower, leader_id=2, followers 없음)
# 각 보드에서 실행. 네임스페이스는 s<robot_id> 로 자동(예 robot_id 2 → /s2).
#
# 포함(FSM 군집주행 필수만): 센서(IMU/GPS[C++]/LiDAR) + CAN 제어(--control 헤드리스
#   하트비트, GUI분리) + nav2/swarm(ekf/navsat/gps_to_map/costmap/controller/planner/
#   swarm_path_executor) + FSM(operation_system autonomous). command_server(태블릿)·
#   rviz 는 제외(필요시 별도).
# 종료:  board_kill.sh   (같은 폴더)
# ★ 반드시 board_kill.sh 로 완전정리 후 재기동(중복 launch 방지).
# (set -u 는 ROS setup.bash 와 충돌하므로 사용 안 함)
RID="${1:?robot_id 필요}"; ROLE="${2:?role(leader|follower) 필요}"
LID="${3:-$RID}"; FOLL="${4:-}"
NS="s${RID}"

source /opt/ros/jazzy/setup.bash
source ~/combatrobot_1/ros/install/setup.bash
export ROS_DOMAIN_ID=0
export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/mesh_dds.xml
echo "[board_bringup] ns=$NS rid=$RID leader=$LID role=$ROLE foll=$FOLL"

# ★ 중복 스택 방지(시스템정비): 기동 전 기존 스택을 항상 완전정리(멱등).
#   반복 재기동 시 옛 노드가 남아 2중 스택(2배 트래픽/CPU/discovery)이 생기던 것을 근본차단.
echo "[board_bringup] 기존 스택 정리(중복 방지)..."
#   $$ 를 넘겨 board_kill 이 이 board_bringup(및 자식)을 죽이지 않게 함(자살 방지).
bash "$(cd "$(dirname "$0")" && pwd)/board_kill.sh" "$$" >/tmp/bu_prekill.log 2>&1 || true
sleep 2

# 0) GPS primer — 콜드스타트 시 gnss 첫 오픈이 DTR 교란으로 garble 되는 것 방지.
#    python 으로 dtr/rts=False 로 먼저 열어 GPS 스트림을 안정화한 뒤 sensor.launch 기동.
python3 -c "import serial,time; s=serial.Serial('/dev/gps',921600,timeout=1); s.dtr=False; s.rts=False; time.sleep(1.2); s.read(5000); s.close()" 2>/dev/null || true

# 1) 센서 (IMU/GPS[C++ gnss_heading_node, DTR 해제]/LiDAR[use_lidar_clock:false])
ros2 launch combat_robot_nav2 sensor.launch.py robot_id:="$RID" >/tmp/bu_sensor.log 2>&1 &
sleep 16

# 2) CAN 제어 (헤드리스 하트비트 30Hz 전용스레드, 기본 DISARMED)
ros2 run combat_robot_nav2 can_reader.py --control --ros-args \
  -r /odom:=/$NS/odom -r /cmd_vel:=/$NS/cmd_vel \
  -r /can/enable:=/$NS/can/enable -r /can/estop:=/$NS/can/estop \
  -r /headlight:=/$NS/headlight \
  -p odom_frame:=$NS/odom -p base_frame:=$NS/base_footprint >/tmp/bu_can.log 2>&1 &
sleep 3

# 3) nav2 + swarm (PushRosNamespace /$NS)
#    with_can_reader:=false — CAN 은 위 2)의 --control 인스턴스가 담당(launch 내장
#    can_reader 는 무인자=GUI 모드라 헤드리스에서 즉사 + CAN 중복 오픈 위험).
#    swarm_lidar_filter 는 launch 에 포함됨(world_pos + 팀메이트 마스킹 — 수동기동 불필요).
FOLL_ARG=""; [ -n "$FOLL" ] && FOLL_ARG="formation_followers:=$FOLL"
ros2 launch combat_robot_nav2 bringup_realtime.launch.py \
  robot_ns:="$NS" robot_id:="$RID" leader_robot_id:="$LID" role:="$ROLE" $FOLL_ARG \
  formation_enable:=true formation_mode:=static use_sim_time:=false with_gnss:=true \
  with_can_reader:=false \
  >/tmp/bu_nav2.log 2>&1 &
sleep 24

# 3.5) 편대 장애물 공유 — 리더/팔로워가 서로 본 costmap lethal 점집합을 공유해
#   상대 obstacle_layer(swarm_shared, marking)에 마킹. base 프레임 재발행이라
#   obstacle_max_range 가 로봇 기준(datum 아님). /tf 는 rclpy 절대구독 → /$NS/tf remap 필수.
ros2 run combat_robot_nav2 swarm_obstacle_share_node --ros-args \
  -r __ns:=/$NS -p robot_id:=$RID \
  -r /tf:=/$NS/tf -r /tf_static:=/$NS/tf_static >/tmp/bu_obshare.log 2>&1 &
sleep 2

# 4) FSM (autonomous). command_server 제외.
YAML=~/combatrobot_1/ros/src/skyautonet/combat_robot_system/combat_robot_operation_system/config/params.autonomous.yaml
#   ★ mission/path_command remap 필수: FSM(글로벌)이 /swarm/path_command 받아
#     mission/path_command 로 fan-out 하는데, 그게 /mission/path_command(글로벌)로
#     가면 executor(/sN/mission/path_command 구독)에 안 닿음 → /sN 으로 remap.
#   ★ mission/control_command remap 동일하게 필수(누락돼 있었음): 이게 없으면
#     태블릿/FSM 의 대형(formation) 명령이 글로벌로 새서 executor 에 영원히 미도달
#     → 실보드에서 대형 전환 불가(항상 기본 나란히).
ros2 run combat_robot_operation_system combat_robot_operation_system_node --ros-args \
  --params-file "$YAML" -r swarm/mission_state:=/$NS/swarm/mission_state \
  -r /operation_state:=/$NS/operation_state \
  -r mission/path_command:=/$NS/mission/path_command \
  -r mission/control_command:=/$NS/mission/control_command >/tmp/bu_fsm.log 2>&1 &

echo "[board_bringup] 전 노드 기동됨. nav2 활성화(controller/planner active)까지 ~30s."
wait
