#!/usr/bin/env bash
# shellcheck disable=SC1091,SC2086,SC2206,SC2046
# Convoy parameter sweep: paths x leader speeds. Relaunch the Go2 SITL per config
# (clean state), spawn UGV x4 + obstacle (at mid waypoint) + waypoint dot markers,
# run the convoy, sample metrics, append a row to /tmp/convoy_sweep.csv.
#   Usage: convoy_sweep.sh [START_IDX END_IDX]   (default: all 20)
# Log: /tmp/convoy_sweep.log    Results: /tmp/convoy_sweep.csv
exec >/tmp/convoy_sweep.log 2>&1
set +e
. /opt/ros/jazzy/setup.bash
. /root/skytest_ws/install/setup.bash
unset LIBGL_ALWAYS_SOFTWARE
export GALLIUM_DRIVER=d3d12
export LD_LIBRARY_PATH=/usr/lib/wsl/lib:${LD_LIBRARY_PATH:-}
export GZ_IP=127.0.0.1
export GZ_SIM_RESOURCE_PATH="${GZ_SIM_RESOURCE_PATH:-}:/root/skytest_ws/install/san_description/share"
XACRO=/root/sky/ros/src/skyautonet/combat_robot_system/san_description/urdf/san_robot.urdf.xacro
GAIT=/root/skytest_ws/install/unitree_go2_sim/share/unitree_go2_sim/config/gait/gait.yaml
CSV=/tmp/convoy_sweep.csv

# generous gait ceiling so the coordinator's leader_vmax is the effective limiter
[ -f "$GAIT" ] && sed -i 's/max_linear_velocity_x : .*/max_linear_velocity_x : 1.2/; s/max_angular_velocity_z : .*/max_angular_velocity_z : 0.8/' "$GAIT"

# paths (float waypoints "x y x y ...") + obstacle at a mid waypoint
declare -a PNAME=(straight scurve zigzag lturn)
declare -A PWPS=(
  [straight]="0.0 0.0 4.0 0.0 8.0 0.0 12.0 0.0 16.0 0.0"
  [scurve]="0.0 0.0 2.0 0.5 4.0 1.2 6.0 2.0 8.0 2.5 10.0 2.0 12.0 1.2 14.0 0.5 16.0 0.0"
  [zigzag]="0.0 0.0 3.0 1.2 6.0 -1.0 9.0 1.2 12.0 -1.0 15.0 0.0"
  [lturn]="0.0 0.0 4.0 0.0 8.0 0.0 8.0 4.0 8.0 8.0"
)
declare -A POBS=( [straight]="8.0 0.0" [scurve]="8.0 2.5" [zigzag]="9.0 1.2" [lturn]="8.0 0.0" )
SPEEDS=(0.3 0.45 0.6 0.75 0.9)
declare -A SX=([3]=-3.0 [4]=-6.0 [5]=-9.0 [2]=-12.0)

cleanup() {
  for p in 'topic pub' 'ros2 launch' unitree_go2_launch convoy_coordinator convoy_ugv \
           convoy_viz parameter_bridge ros_gz 'gz sim' gz-sim sim-server ruby \
           robot_state_publisher controller_manager spawner ekf_node rviz2; do
    pkill -9 -f "$p" 2>/dev/null
  done
  pkill -9 gz 2>/dev/null
  rm -f /dev/shm/fastrtps_* /dev/shm/sem.fastrtps_* 2>/dev/null
}
xyz() { timeout 5 gz model -m "$1" -p 2>/dev/null | grep -oE '\-?[0-9]+\.[0-9]+(e-?[0-9]+)?' | head -3 | tr '\n' ' '; }
mn() { awk -v a="$1" -v b="$2" 'BEGIN{print (b!=""&&(b+0)<(a+0)?b:a)}'; }

CFG=()
for p in "${PNAME[@]}"; do for s in "${SPEEDS[@]}"; do CFG+=("$p|$s"); done; done
S=${1:-0}; E=${2:-$((${#CFG[@]} - 1))}
[ -f "$CSV" ] || echo "idx,path,speed,leader_wp,reached,go2_z_min,min_gap,min_obsdist,result" > "$CSV"

for idx in $(seq "$S" "$E"); do
  cfg="${CFG[$idx]}"; path="${cfg%|*}"; spd="${cfg#*|}"
  wps="${PWPS[$path]}"; obs="${POBS[$path]}"; ox="${obs% *}"; oy="${obs#* }"
  arr=($wps); npairs=$(( ${#arr[@]} / 2 ))
  echo "##### idx=$idx path=$path speed=$spd obstacle=($ox,$oy) wps=$npairs #####"
  cleanup; sleep 5

  nohup ros2 launch unitree_go2_sim unitree_go2_launch.py rviz:=false >/tmp/sw_go2.log 2>&1 &
  for i in $(seq 1 55); do gz topic -l 2>/dev/null | grep -q /world/default && { echo "  gz up @${i}s"; break; }; sleep 1; done
  sleep 18   # CHAMP 컨트롤러 완전 초기화 대기(clean boot 신뢰성)

  for n in 3 4 5 2; do
    ns=robot_$n
    xacro "$XACRO" robot_ns:=$ns robot_name:=$ns lidar_mode:=none >/tmp/$ns.urdf 2>/dev/null
    python3 -c "import re,sys;s=open(sys.argv[1]).read();open(sys.argv[1],'w').write(re.sub(r'<sensor\\\\b[^>]*type=\"camera\"[^>]*>.*?</sensor>','',s,flags=re.S))" /tmp/$ns.urdf
    timeout 20 ros2 run ros_gz_sim create -world default -file /tmp/$ns.urdf -name $ns -x ${SX[$n]} -y 0 -z 0.35 >/dev/null 2>&1
    sleep 1
  done
  OBSSDF='<sdf version="1.8"><model name="obs_c1"><static>true</static><link name="l"><collision name="c"><geometry><cylinder><radius>0.5</radius><length>2</length></cylinder></geometry></collision><visual name="v"><geometry><cylinder><radius>0.5</radius><length>2</length></cylinder></geometry><material><ambient>0.85 0.15 0.15 1</ambient><diffuse>0.85 0.15 0.15 1</diffuse></material></visual></link></model></sdf>'
  timeout 12 ros2 run ros_gz_sim create -world default -string "$OBSSDF" -name obs_c1 -x $ox -y $oy -z 1 >/dev/null 2>&1
  for ((k=0;k<npairs;k++)); do
    wx=${arr[$((k*2))]}; wy=${arr[$((k*2+1))]}
    WSDF='<sdf version="1.8"><model name="wp_'$k'"><static>true</static><link name="l"><visual name="v"><geometry><sphere><radius>0.05</radius></sphere></geometry><material><ambient>0.1 0.9 0.1 1</ambient><diffuse>0.1 0.9 0.1 1</diffuse></material></visual></link></model></sdf>'
    timeout 8 ros2 run ros_gz_sim create -world default -string "$WSDF" -name wp_$k -x $wx -y $wy -z 0.05 >/dev/null 2>&1
  done

  BARGS=""
  for n in 2 3 4 5; do BARGS="$BARGS /robot_$n/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist /robot_$n/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry"; done
  nohup ros2 run ros_gz_bridge parameter_bridge $BARGS >/tmp/sw_bridge.log 2>&1 &
  sleep 4
  wpcsv=$(echo $wps | tr ' ' ',')
  ugv_vmax=$(awk -v s=$spd 'BEGIN{print s+0.3}')
  nohup ros2 run san_operator_tools convoy_coordinator --ros-args \
    -p leader_vmax:=$spd -p leader_wmax:=0.35 -p "waypoints:=[$wpcsv]" -p "obstacles:=[$ox,$oy,0.5]" \
    >/tmp/sw_coord.log 2>&1 &
  for n in 3 4 5 2; do
    nohup ros2 run san_operator_tools convoy_ugv --ros-args -r __ns:=/robot_$n \
      -p robot_id:=$n -p spawn_x:=${SX[$n]} -p spawn_y:=0.0 -p gap_m:=3.0 \
      -p max_linear_mps:=$ugv_vmax -p min_gap_m:=1.4 -p slow_gap_m:=2.2 >/tmp/sw_ugv_$n.log 2>&1 &
    sleep 1
  done

  # Log-based metrics (fast): leader pos + wp from sw_coord.log, UGV gap (dpred) +
  # positions from sw_ugv_*.log; only go2_z (fall) needs a gz query, once per chunk.
  fx=${arr[$(((npairs-1)*2))]}; fy=${arr[$(((npairs-1)*2+1))]}
  zmin=9; reached=0
  for _ in $(seq 1 10); do   # up to ~100s; exit when leader near final or fell
    sleep 10
    gzz=$(timeout 5 gz model -m go2 -p 2>/dev/null | grep -oE '\-?[0-9]+\.[0-9]+' | sed -n '3p')
    zmin=$(mn "$zmin" "$gzz")
    lp=$(grep -oE 'leader=\(-?[0-9.]+,-?[0-9.]+\)' /tmp/sw_coord.log 2>/dev/null | tail -1 | sed 's/leader=(//;s/)//')
    lx=$(echo "$lp" | cut -d, -f1); ly=$(echo "$lp" | cut -d, -f2)
    [ -n "$lx" ] && awk -v lx=$lx -v ly=$ly -v fx=$fx -v fy=$fy 'BEGIN{exit !(sqrt((lx-fx)^2+(ly-fy)^2)<1.2)}' && { reached=1; break; }
    awk -v z="$zmin" 'BEGIN{exit !((z+0)<0.12)}' && break
  done
  lastwp=$(grep -oE 'wp=[0-9]+/[0-9]+' /tmp/sw_coord.log | tail -1)
  gmin=$(grep -hoE 'dpred=[0-9.]+' /tmp/sw_ugv_*.log 2>/dev/null | grep -oE '[0-9.]+$' | sort -n | head -1)
  [ -z "$gmin" ] && gmin=9
  omin=$( { grep -hoE 'own=\(-?[0-9.]+,-?[0-9.]+\)' /tmp/sw_ugv_*.log 2>/dev/null | sed 's/own=(//;s/)//';
            grep -hoE 'leader=\(-?[0-9.]+,-?[0-9.]+\)' /tmp/sw_coord.log 2>/dev/null | sed 's/leader=(//;s/)//'; } \
          | awk -F, -v ox="$ox" -v oy="$oy" 'BEGIN{m=999}{d=sqrt(($1-ox)^2+($2-oy)^2); if(d<m)m=d}END{printf "%.2f",m}')
  [ -z "$omin" ] && omin=9
  res=PASS
  awk -v z=$zmin 'BEGIN{exit !((z+0)<0.15)}' && res=FALL
  [ "$res" = PASS ] && awk -v g=$gmin 'BEGIN{exit !((g+0)<0.8)}' && res=COLLIDE
  [ "$res" = PASS ] && [ "$reached" = 0 ] && res=INCOMPLETE
  echo "$idx,$path,$spd,${lastwp:-NA},$reached,$zmin,$gmin,$omin,$res" >> "$CSV"
  echo "  RESULT idx=$idx $path spd=$spd wp=${lastwp:-NA} reached=$reached zmin=$zmin gmin=$gmin omin=$omin -> $res"
  cleanup; sleep 3
done
echo "SWEEP_DONE ($(grep -c , "$CSV") rows incl header)"
