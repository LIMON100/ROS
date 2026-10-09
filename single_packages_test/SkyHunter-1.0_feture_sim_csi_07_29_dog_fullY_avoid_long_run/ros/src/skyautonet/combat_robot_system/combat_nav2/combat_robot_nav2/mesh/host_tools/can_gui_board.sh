#!/bin/bash
# can_gui_board.sh <sN> [sM ...]
#   보드-로컬 can_reader GUI 를 호스트 화면(ssh -X)에 '이름표(sN)' 달아 띄운다.
#   - 프로세스는 보드(CAN 물림) → ARM/E-stop 이 보드-로컬이라 wifi 열화와 무관하게 안전.
#   - 창 제목은 can_reader.py 가 CAN_LABEL 환경변수를 읽어 "TinS-17 [sN] Control" 로 표시.
#   - 각 보드의 headless can_reader(--control)는 CAN0 을 점유하므로, GUI 기동 전 종료한다.
#   예:  ~/can_gui_board.sh s2 s3 s4      (세 창을 s2/s3/s4 라벨로)
#        ~/can_gui_board.sh s2            (s2 만)
set -u
XAUTH=$(ls -t /run/user/1000/.mutter-Xwaylandauth.* 2>/dev/null | head -1)
ENV='source /opt/ros/jazzy/setup.bash; source ~/combatrobot_1/ros/install/setup.bash; export ROS_DOMAIN_ID=0; export RMW_IMPLEMENTATION=rmw_fastrtps_cpp; export FASTRTPS_DEFAULT_PROFILES_FILE=$HOME/mesh_dds.xml'

launch_one() {
  local NS="$1"
  local RA="--ros-args -r /odom:=/$NS/odom -r /cmd_vel:=/$NS/cmd_vel -r /can/enable:=/$NS/can/enable -r /can/estop:=/$NS/can/estop -r /headlight:=/$NS/headlight -p odom_frame:=$NS/odom -p base_frame:=$NS/base_footprint"
  echo "[$NS] 기존 can_reader(headless/GUI) 종료(CAN0 확보) ..."
  ssh -o ControlPath=none -o ConnectTimeout=8 "mesh_$NS" 'pkill -f "[c]an_reader.py"; sleep 1' 2>/dev/null
  echo "[$NS] GUI 기동(ssh -X, 라벨=$NS) ..."
  DISPLAY=:0 XAUTHORITY="$XAUTH" setsid ssh -Y -C \
    -o ControlPath=none -o ControlMaster=no \
    -o ServerAliveInterval=8 -o ServerAliveCountMax=4 -o ConnectTimeout=12 \
    "mesh_$NS" "export CAN_LABEL=$NS; $ENV; exec ros2 run combat_robot_nav2 can_reader.py $RA" \
    >/tmp/cangui_${NS}.log 2>&1 &
  echo "[$NS] launched (log: /tmp/cangui_${NS}.log)"
}

[ $# -ge 1 ] || { echo "사용법: $0 <sN> [sM ...]   예: $0 s2 s3 s4"; exit 1; }
for ns in "$@"; do launch_one "$ns"; sleep 2; done
echo "완료 — 호스트 화면에 각 보드 라벨(sN) can_reader 창. 기본 DISARMED, Enable Control 로 ARM."
