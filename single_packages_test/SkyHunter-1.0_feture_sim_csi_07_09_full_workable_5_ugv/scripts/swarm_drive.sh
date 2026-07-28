#!/usr/bin/env bash
# swarm_drive.sh — inject a path into a running swarm sim, drive, and change formation.
#
# Publishes to every robot's /sN/swarm/{path,control}_command (→ FSM gate →
# /sN/mission/* → swarm_path_executor). Pairs with run_swarm_sim3.sh / run_swarm_sim.sh.
# Run from a SECOND terminal, after the sim reports all nav2 "Managed nodes active".
#
# SwarmPathCommand.command:   1 START  2 STOP  3 PAUSE  4 RESUME  5 LOAD_PATH  6 COMPLETE
# SwarmControlCommand.formation_type (대형):
#   0 line(횡대, 나란히)   1 column(종대, 일렬)   2 diamond(마름모)   3 wedge(쐐기 V)
#   (초기 대형은 line=횡대. 주행 중 바꾸면 executor 가 정지→재정렬→재개.)
#
# Usage:
#   ./scripts/swarm_drive.sh                 # go: LOAD default N-S path + START (3 robots)
#   ./scripts/swarm_drive.sh -d 120          # longer path, ~120 m north
#   ./scripts/swarm_drive.sh -n 2            # 2 robots (s1,s2)
#   ./scripts/swarm_drive.sh -f wedge go     # start the drive in WEDGE formation
#   ./scripts/swarm_drive.sh load|start|stop|pause|resume
#   # --- mid-drive formation change (run while driving) ---
#   ./scripts/swarm_drive.sh line            # → 횡대 (line abreast)
#   ./scripts/swarm_drive.sh column          # → 종대 (column)
#   ./scripts/swarm_drive.sh wedge|diamond   # → 쐐기 / 마름모
#   ./scripts/swarm_drive.sh -w '{"waypoints":[{"lat":36.6101,"lon":127.2877}, ...]}' go
#
# The default route runs north from the sim GNSS datum (sejong.world origin).

set -eo pipefail

THIS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${THIS_DIR}/.." && pwd)"
# shellcheck source=scripts/lib/common.sh
source "${THIS_DIR}/lib/common.sh"

# ---- defaults --------------------------------------------------------------
NUM_ROBOTS=3
DIST_NORTH_M=55            # default route length (north) in metres
DATUM_LAT=36.61002559225   # sejong.world origin (must match robot_bringup_sim datum)
DATUM_LON=127.28772570583
PATH_JSON=""               # if set, overrides the generated default path
FORMATION=""               # with go/start: form up in this formation before START
ACTION="go"

# line=0 column=1 diamond=2 wedge=3  (executor FORMATION_* / SwarmControlCommand.formation_type)
formation_code() {
  case "$1" in
    line|abreast|횡대)  echo 0 ;;
    column|종대)        echo 1 ;;
    diamond|마름모)     echo 2 ;;
    wedge|쐐기)         echo 3 ;;
    *) echo "-1" ;;
  esac
}

# ---- arg parse -------------------------------------------------------------
while [[ $# -gt 0 ]]; do
  case "$1" in
    -n|--num)       NUM_ROBOTS="$2"; shift 2 ;;
    -d|--dist)      DIST_NORTH_M="$2"; shift 2 ;;
    -w|--waypoints) PATH_JSON="$2"; shift 2 ;;
    -f|--formation) FORMATION="$2"; shift 2 ;;
    go|load|start|stop|pause|resume) ACTION="$1"; shift ;;
    line|abreast|column|diamond|wedge|횡대|종대|마름모|쐐기) ACTION="formation"; FORMATION="$1"; shift ;;
    -h|--help) sed -n '/^# swarm_drive/,/^$/p' "$0" | sed 's/^# \?//'; exit 0 ;;
    *) echo "Unknown arg: $1 (see --help)" >&2; exit 2 ;;
  esac
done

setup_ros_env >/dev/null
source_workspace_overlay >/dev/null

# ---- build the default path if none was supplied ---------------------------
# 1 deg latitude ~= 111_320 m. Three waypoints: ~10 m, mid, full distance north.
if [[ -z "${PATH_JSON}" ]]; then
  read -r W1 W2 W3 < <(awk -v lat="${DATUM_LAT}" -v lon="${DATUM_LON}" -v d="${DIST_NORTH_M}" 'BEGIN{
    mlon=1.0/(111320.0*cos(lat*3.14159265/180.0));
    printf "%.7f %.7f %.7f\n", lon+10*mlon, lon+(d*0.5)*mlon, lon+d*mlon }')
  PATH_JSON=$(printf '{"waypoints":[{"lat":%s,"lon":%s},{"lat":%s,"lon":%s},{"lat":%s,"lon":%s}]}' \
              "${DATUM_LAT}" "${W1}" "${DATUM_LAT}" "${W2}" "${DATUM_LAT}" "${W3}")
fi
NUM_WP=3

pub() { # $1=command code  $2=extra-fields  (SwarmPathCommand → /sN/swarm/path_command)
  local cmd="$1" extra="$2"
  for ((i=1; i<=NUM_ROBOTS; i++)); do
    ros2 topic pub -t 3 "/s${i}/swarm/path_command" \
      combat_robot_msgs/msg/SwarmPathCommand "{command: ${cmd}${extra}}" >/dev/null 2>&1
  done
}

pubform() { # $1=formation name  (SwarmControlCommand → /sN/swarm/control_command → FSM → executor)
  local name="$1" ft
  ft="$(formation_code "${name}")"
  if [[ "${ft}" == "-1" ]]; then
    echo "Unknown formation: ${name} (line|column|diamond|wedge)" >&2; exit 2
  fi
  echo "[swarm_drive] FORMATION → ${name} (type=${ft}) → s1..s${NUM_ROBOTS}"
  for ((i=1; i<=NUM_ROBOTS; i++)); do
    ros2 topic pub -t 3 "/s${i}/swarm/control_command" \
      combat_robot_msgs/msg/SwarmControlCommand "{formation_type: ${ft}, formation_number: 1}" >/dev/null 2>&1
  done
}

do_load() {
  echo "[swarm_drive] LOAD ${NUM_WP} waypoints → s1..s${NUM_ROBOTS}"
  echo "[swarm_drive]   path: ${PATH_JSON}"
  pub 5 ", num_waypoints: ${NUM_WP}, path_json: '${PATH_JSON}'"
}
do_start()  { echo "[swarm_drive] START → s1..s${NUM_ROBOTS}"; pub 1 ""; }
do_stop()   { echo "[swarm_drive] STOP → s1..s${NUM_ROBOTS}";  pub 2 ""; }
do_pause()  { echo "[swarm_drive] PAUSE → s1..s${NUM_ROBOTS}"; pub 3 ""; }
do_resume() { echo "[swarm_drive] RESUME → s1..s${NUM_ROBOTS}"; pub 4 ""; }

case "${ACTION}" in
  go)        do_load
             [[ -n "${FORMATION}" ]] && { sleep 1; pubform "${FORMATION}"; }
             sleep 2; do_start ;;
  load)      do_load ;;
  start)     [[ -n "${FORMATION}" ]] && { pubform "${FORMATION}"; sleep 1; }
             do_start ;;
  stop)      do_stop ;;
  pause)     do_pause ;;
  resume)    do_resume ;;
  formation) pubform "${FORMATION}" ;;
esac

echo "[swarm_drive] done (action=${ACTION}, robots=${NUM_ROBOTS}${FORMATION:+, formation=${FORMATION}})"
