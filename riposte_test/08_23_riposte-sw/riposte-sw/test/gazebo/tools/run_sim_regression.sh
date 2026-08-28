#!/usr/bin/env bash
# Riposte Gazebo simulation regression — re-runs every test proved on 2026-08-24.
#
# Purpose: START HERE each session. It re-verifies the whole ladder before any
# new work, so a later failure is attributable to the new code and not to
# something that was already broken (riposte_sw.md §11.1, §18.9 SIM-1).
#
#   bash test/gazebo/tools/run_sim_regression.sh            # everything
#   bash test/gazebo/tools/run_sim_regression.sh 0 1 2      # ladder subset
#   bash test/gazebo/tools/run_sim_regression.sh swarm      # SIM-2 only
#   HEADLESS=1 bash .../run_sim_regression.sh               # no GUI (faster)
#
# Every stage monopolises PX4/gz/riposte on this host, so they run strictly one
# at a time with a sweep between (GAZEBO-TEST-001 §6.3 single-instance rule; a
# stray riposte-seeker or gz server silently corrupts the TrackBus seqlock).
set -u

HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
GZ_DIR=$(cd "$HERE/.." && pwd)                 # test/gazebo
REPO=$(cd "$GZ_DIR/../.." && pwd)              # riposte-sw/riposte-sw

export PX4_BUILD=${PX4_BUILD:-$HOME/riposte_sim/PX4-Autopilot/build/px4_sitl_default}
export RIPOSTE_BUILD=${RIPOSTE_BUILD:-$REPO/build-gz}
export MAVSDK_LIB=${MAVSDK_LIB:-/usr/lib}
export HEADLESS=${HEADLESS:-1}

RESULTS=""
FAILED=0

sweep() { pkill -f "gz sim" 2>/dev/null; pkill -f px4 2>/dev/null
          pkill -f gz_track_bridge 2>/dev/null; pkill -f riposte-obc 2>/dev/null
          rm -f /dev/shm/riposte_* 2>/dev/null
          sleep 12; }
        
# run <label> <expected-marker> <command...>
run() {
    local label=$1 marker=$2; shift 2
    sweep
    echo "=================== $label ==================="
    local log; log=$(mktemp /tmp/riposte-reg.XXXXXX)
    "$@" > "$log" 2>&1
    if grep -q "$marker" "$log"; then
        echo "PASS  $label"
        RESULTS="$RESULTS
PASS  $label   $(grep -oE 'range: .*|patrol: approaches=.*|DISTINCT_TARGETS=[0-9]+' "$log" | head -2 | tr '\n' ' ')"
    else
          echo "FAIL  $label   (marker '$marker' absent)"
          tail -8 "$log"
          RESULTS="$RESULTS
  FAIL  $label   log=$log"
          FAILED=$((FAILED + 1))
      fi  
  }   

  WANT=${*:-all}
  want() { case " $WANT " in *" all "*|*" $1 "*) return 0;; esac; return 1; }
  
  cd "$REPO" || exit 2

  for n in 0 1 2 3 4 5 6; do
      want "$n" || want all || continue
      case $n in
        0) M=STAGE0_ENV_PASS;;      1) M=STAGE1_BRIDGE_PASS;;
        2) M=STAGE2_OFFBOARD_PASS;; 3) M=STAGE3_STATIC_PASS;;
        4) M=STAGE4_CROSSING_PASS;; 5) M=STAGE5_EVASIVE_PASS;;
        6) M=STAGE6_SAFETY_PASS;;
      esac 
      run "ladder stage $n" "$M" env -u WORLD -u TG bash "$GZ_DIR/stages/stage${n}_"*.sh
  done
 
  # S-G4: the shipped multi-target scenario (six STATIC balloons, ground truth).
  if want balloon || want all; then
      run "S-G4 balloon (static, 6)" GAZEBO_BALLOON_PASS \
          env WORLD=riposte_balloon TG='balloon_*' \
          bash "$GZ_DIR/run_gazebo_balloon.sh"
  fi      

  # SIM-2: our generated world -- five INDEPENDENTLY MOVING target drones.
  # Regenerate first so the .sdf always matches the TARGETS table in the tool.
  if want swarm || want all; then
      python3 "$HERE/make_swarm_world.py" > /dev/null 2>&1
      run "SIM-2 swarm (moving, 5)" GAZEBO_BALLOON_PASS \
          env WORLD=riposte_swarm TG='drone_*' \
          bash "$GZ_DIR/run_gazebo_balloon.sh" 
  fi 

  sweep
  echo
  echo "########## SIM REGRESSION SUMMARY ##########"
  echo "$RESULTS"
  echo
  if [ "$FAILED" -eq 0 ]; then
      echo "SIM_REGRESSION_ALL_PASS"
  else
      echo "SIM_REGRESSION_FAILURES=$FAILED"
  fi  
  exit "$FAILED"

