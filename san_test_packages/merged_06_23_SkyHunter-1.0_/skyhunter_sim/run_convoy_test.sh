#!/usr/bin/env bash
# shellcheck disable=SC1091,SC2086,SC2206
# Runtime test of the full Go2 SITL convoy demo (leader dog + 4 UGV + convoy +
# RViz) from the workspace built by build_test_ws.sh. Mirrors
# san_sim_gazebo/launch/convoy_demo.launch.py using the built packages + vendored
# Go2 SITL. Log: /tmp/skytest_run.log
exec >/tmp/skytest_run.log 2>&1
set +e
. /opt/ros/jazzy/setup.bash
. /root/skytest_ws/install/setup.bash
unset LIBGL_ALWAYS_SOFTWARE
export GALLIUM_DRIVER=d3d12
export LD_LIBRARY_PATH=/usr/lib/wsl/lib:${LD_LIBRARY_PATH:-}
export GZ_IP=127.0.0.1
# gz must resolve UGV package://san_description/meshes or UGVs are invisible in Gazebo
export GZ_SIM_RESOURCE_PATH="${GZ_SIM_RESOURCE_PATH:-}:/root/skytest_ws/install/san_description/share"
RVIZ_CFG=/root/sky/ros/src/skyautonet/combat_robot_system/san_sim_gazebo/rviz/convoy.rviz
XACRO=/root/sky/ros/src/skyautonet/combat_robot_system/san_description/urdf/san_robot.urdf.xacro
declare -A SX=([3]=-3.0 [4]=-6.0 [5]=-9.0 [2]=-12.0)  # float: convoy_ugv spawn_x is a double param

cleanup() {
  for p in 'topic pub' 'ros2 launch' convoy_coordinator convoy_ugv convoy_viz \
           'gz sim' gz-sim sim-server rviz2 parameter_bridge ros_gz ruby \
           robot_state_publisher controller_manager; do
    pkill -9 -f "$p" 2>/dev/null
  done
  rm -f /dev/shm/fastrtps_* /dev/shm/sem.fastrtps_* 2>/dev/null
}
xy() { timeout 6 gz model -m "$1" -p 2>/dev/null | grep -oE '\-?[0-9]+\.[0-9]+(e-?[0-9]+)?' | head -2 | tr '\n' ' '; }

cleanup; sleep 3
echo "=== [1] vendored Go2 SITL (rviz off) ==="
nohup ros2 launch unitree_go2_sim unitree_go2_launch.py rviz:=false >/tmp/go2_sim.log 2>&1 &
for i in $(seq 1 50); do gz topic -l 2>/dev/null | grep -q /world/default && { echo "  gz up @${i}s"; break; }; sleep 1; done
sleep 12
echo "  go2 pose: $(xy go2)"

echo "=== [2] spawn UGV x4 (sensor-stripped) + obstacle ==="
strip_cam() { python3 -c "import re,sys;s=open(sys.argv[1]).read();s=re.sub(r'<sensor\\\\b[^>]*type=\"camera\"[^>]*>.*?</sensor>','',s,flags=re.S);open(sys.argv[1],'w').write(s)" "$1"; }
for n in 3 4 5 2; do
  ns=robot_$n
  xacro "$XACRO" robot_ns:=$ns robot_name:=$ns lidar_mode:=none >/tmp/$ns.urdf 2>/dev/null
  strip_cam /tmp/$ns.urdf
  timeout 25 ros2 run ros_gz_sim create -world default -file /tmp/$ns.urdf -name $ns -x ${SX[$n]} -y 0 -z 0.35 >/dev/null 2>&1
  sleep 2
done
OBS='<sdf version="1.8"><model name="obs_c1"><static>true</static><link name="l"><collision name="c"><geometry><cylinder><radius>0.5</radius><length>2</length></cylinder></geometry></collision><visual name="v"><geometry><cylinder><radius>0.5</radius><length>2</length></cylinder></geometry></visual></link></model></sdf>'
timeout 15 ros2 run ros_gz_sim create -world default -string "$OBS" -name obs_c0 -x 28 -y -3 -z 1 >/dev/null 2>&1  # 경로(x=28,y=-1)서 2m 비껴 안전 → 회피X, FOLLOW 추종(회피회랑 밖)
OBS2=$(echo "$OBS" | sed 's/obs_c1/obs_c1b/')
timeout 15 ros2 run ros_gz_sim create -world default -string "$OBS2" -name obs_c1 -x 20 -y 1 -z 1 >/dev/null 2>&1  # 6번째 waypoint 상 → 충돌→회피
OBS3=$(echo "$OBS" | sed 's/obs_c1/obs_c2x/')
timeout 15 ros2 run ros_gz_sim create -world default -string "$OBS3" -name obs_c2 -x 12 -y 2.5 -z 1 >/dev/null 2>&1  # 4번째 waypoint(apex) 상 → 충돌→회피
# waypoint 큰 점(녹색 구) — 계획 경로(S-curve) 표시
WPS="0 0 4 1 8 2 12 2.5 16 2 20 1 24 0 28 -1 32 -1.5 36 -1 40 0"
wparr=($WPS); k=0
for ((i=0;i<${#wparr[@]};i+=2)); do
  WPSDF='<sdf version="1.8"><model name="wp_'$k'"><static>true</static><link name="l"><visual name="v"><geometry><sphere><radius>0.05</radius></sphere></geometry><material><ambient>0.1 0.9 0.1 1</ambient><diffuse>0.1 0.9 0.1 1</diffuse></material></visual></link></model></sdf>'
  timeout 8 ros2 run ros_gz_sim create -world default -string "$WPSDF" -name wp_$k -x "${wparr[$i]}" -y "${wparr[$((i+1))]}" -z 0.05 >/dev/null 2>&1
  k=$((k+1))
done
echo "  models: $(timeout 5 gz model --list 2>/dev/null | grep -cE 'go2|robot_|obs_')"

echo "=== [3] bridge + convoy nodes + viz + rviz ==="
BARGS=""
for n in 2 3 4 5; do BARGS="$BARGS /robot_$n/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist /robot_$n/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry"; done
nohup ros2 run ros_gz_bridge parameter_bridge $BARGS >/tmp/bridge.log 2>&1 &
sleep 4
nohup ros2 run san_operator_tools convoy_coordinator --ros-args -p leader_vmax:=0.5 -p leader_wmax:=0.25 -p "obstacles:=[12.0,2.5,0.5,28.0,-3.0,0.5,20.0,1.0,0.5]" >/tmp/coord.log 2>&1 &
for n in 3 4 5 2; do
  nohup ros2 run san_operator_tools convoy_ugv --ros-args -r __ns:=/robot_$n \
    -p robot_id:=$n -p spawn_x:=${SX[$n]} -p spawn_y:=0.0 -p gap_m:=3.0 \
    -p max_linear_mps:=0.6 -p min_gap_m:=1.4 -p slow_gap_m:=2.2 >/tmp/ugv_$n.log 2>&1 &
  sleep 1
done
nohup ros2 run san_operator_tools convoy_costmap >/tmp/costmap.log 2>&1 &
nohup ros2 run san_operator_tools convoy_viz >/tmp/viz.log 2>&1 &
[ "${RVIZ:-1}" = "1" ] && nohup rviz2 -d "$RVIZ_CFG" >/tmp/rviz.log 2>&1 &
sleep 6
echo "  nodes: coord=$(pgrep -fc convoy_coordinator) ugv=$(pgrep -fc convoy_ugv) viz=$(pgrep -fc convoy_viz) rviz=$(pgrep -fc rviz2)"
echo "  viz log: $(grep -m1 'ConvoyViz UP' /tmp/viz.log || echo 'no UP')"

echo "=== [4] observe 90s + metrics ==="
xyzr() { timeout 5 gz model -m "$1" -p 2>/dev/null | grep -oE '\-?[0-9]+\.[0-9]+(e-?[0-9]+)?' | head -6 | tr '\n' ' '; }
mn() { awk -v a="$1" -v b="$2" 'BEGIN{print (b!=""&&(b+0)<(a+0))?b:a}'; }
mx() { awk -v a="$1" -v b="$2" 'BEGIN{print (b!=""&&(b+0)>(a+0))?b:a}'; }
# 실제 위치(px,py)에서 계획 경로(waypoint polyline) 까지 cross-track 편차(최단거리).
crosstrack() {
  awk -v px="$1" -v py="$2" -v wps="$WPS" 'BEGIN{
    n=split(wps,a," "); m=1e9;
    for(i=1;i+3<=n;i+=2){ x1=a[i];y1=a[i+1];x2=a[i+2];y2=a[i+3];
      dx=x2-x1;dy=y2-y1;L2=dx*dx+dy*dy;
      if(L2<1e-9){d=sqrt((px-x1)^2+(py-y1)^2)}
      else{t=((px-x1)*dx+(py-y1)*dy)/L2; if(t<0)t=0; if(t>1)t=1;
        cx=x1+t*dx;cy=y1+t*dy; d=sqrt((px-cx)^2+(py-cy)^2)}
      if(d<m)m=d }
    printf "%.2f",m }'
}
ZMIN=9; RMAX=0; GMIN=99; OMIN=99; XTMAX=0; GX=0; GWP=NA
# Gazebo sim 종료(관찰) 시간 = 120초(기본). OBS_STEPS 로 재정의 가능.
for t in ${OBS_STEPS:-30 60 90 120}; do
  sleep 30
  GP=$(xyzr go2)
  GX=$(echo "$GP" | awk '{print $1}'); GY=$(echo "$GP" | awk '{print $2}')
  GZ=$(echo "$GP" | awk '{print $3}'); GR=$(echo "$GP" | awk '{r=$4; print (r<0?-r:r)}')
  ZMIN=$(mn "$ZMIN" "$GZ"); RMAX=$(mx "$RMAX" "$GR")
  XT=$(crosstrack "$GX" "$GY"); XTMAX=$(mx "$XTMAX" "$XT")
  GWP=$(grep -E 'leader=' /tmp/coord.log | tail -1 | grep -oE 'wp=[0-9]+/[0-9]+')
  MODE=$(grep -oE 'mode=[A-Z]+' /tmp/coord.log | tail -1)
  echo "  --- t=${t}s --- go2=($GX,$GY) z=$GZ |roll|=$GR xtrack=$XT $GWP $MODE"
  PX=$GX; PY=$GY
  for n in 3 4 5 2; do
    CP=$(xyzr robot_$n); CX=$(echo "$CP" | awk '{print $1}'); CY=$(echo "$CP" | awk '{print $2}')
    [ -z "$CX" ] && continue
    GAP=$(awk -v px="$PX" -v py="$PY" -v cx="$CX" -v cy="$CY" 'BEGIN{print sqrt((px-cx)^2+(py-cy)^2)}')
    GMIN=$(mn "$GMIN" "$GAP")
    for ob in "12 2.5" "28 -3" "20 1"; do
      ox=${ob% *}; oy=${ob#* }
      OD=$(awk -v cx="$CX" -v cy="$CY" -v ox="$ox" -v oy="$oy" 'BEGIN{print sqrt((cx-ox)^2+(cy-oy)^2)}')
      OMIN=$(mn "$OMIN" "$OD")
    done
    echo "    robot_$n=($CX,$CY) gap=$GAP"
    PX=$CX; PY=$CY
  done
done

# 로봇개(리더) 연속 궤적을 coord.log(leader=(x,y) @~2Hz)에서 파싱 → 신뢰성 있는 최종 x +
# 실제 로봇개 경로 기준 장애물 최소 clearance. 30s gz-model 샘플은 빈 샘플/최근접 순간을
# 놓치므로 연속 로그가 권원. 안전장애물 (28,-3) 포함 3개 모두 확인.
LEADER_TRAJ=$(grep -oE 'leader=\([^)]*\)' /tmp/coord.log | sed 's/leader=(//; s/)//')
if [ -n "$LEADER_TRAJ" ]; then
  read -r GXT DOGOMIN <<EOF
$(echo "$LEADER_TRAJ" | awk -F, -v obs="12 2.5 28 -3 20 1" 'BEGIN{no=split(obs,o," ");om=99}
  {lastx=$1; for(i=1;i<=no;i+=2){d=sqrt(($1-o[i])^2+($2-o[i+1])^2); if(d<om)om=d}}
  END{printf "%.2f %.2f", lastx, om}')
EOF
  [ -n "$GXT" ] && GX=$GXT
  OMIN=$(mn "$OMIN" "$DOGOMIN")
  echo "  leader traj: final_x=$GX dog_obstacle_min=$DOGOMIN (samples=$(echo "$LEADER_TRAJ" | wc -l))"
fi

# 첫 UGV(robot_3) 추종 검증: 코디네이터가 로깅한 gap3(리더↔robot_3 거리) 최대값. 목표 gap=3.0,
# 캐치업으로 ~3-4 유지해야 함. 과도(>5)면 첫 UGV 낙오(추종 실패 — oside/캐치업 버그 회귀 감지).
GAP3MAX=$(grep -oE 'gap3=[0-9.]+' /tmp/coord.log | sed 's/gap3=//' | awk 'BEGIN{m=0}{if($1+0>m)m=$1}END{printf "%.2f",m}')

echo "=== [5] verdict ==="
awk -v z="$ZMIN" -v r="$RMAX" 'BEGIN{printf "  GO2 z_min=%.2f |roll|max=%.2f -> %s\n",z,r,((z+0>0.15&&r+0<0.6)?"UPRIGHT_OK":"FALL")}'
awk -v x="$GX" 'BEGIN{printf "  LEADER x=%.1f -> %s\n",x,((x+0>23)?"COMPLETE":(x+0>12?"PAST_OBS":"SHORT"))}'
echo "  leader wp=$GWP"
awk -v g="$GMIN" 'BEGIN{printf "  CONVOY min gap=%.2f -> %s\n",g,((g+0>0.8)?"NO_COLLIDE":"COLLIDE")}'
awk -v g="$GAP3MAX" 'BEGIN{printf "  FIRST-UGV follow max gap3=%.2f -> %s\n",g,((g+0<5.0)?"FOLLOW_OK":"FOLLOW_FAIL")}'
awk -v o="$OMIN" 'BEGIN{printf "  OBSTACLE min clearance=%.2f -> %s\n",o,((o+0>=1.3)?"AVOID_OK":"AVOID_FAIL")}'
# 경로 비교: 실제 Go2 궤적 vs 계획 경로 max cross-track. 회피구간(장애물옆)에선 ~측면거리,
# 그 외(평시 추종)에선 작아야 함(추종 정확도). 과도하면 gait drift → 성능 개선 필요.
awk -v x="$XTMAX" 'BEGIN{printf "  PATH max cross-track(실제 vs 계획)=%.2f m -> %s\n",x,((x+0<3.0)?"TRACK_OK":"DRIFT")}'
sleep 5
cleanup
echo "RUN_CONVOY_TEST_DONE"
