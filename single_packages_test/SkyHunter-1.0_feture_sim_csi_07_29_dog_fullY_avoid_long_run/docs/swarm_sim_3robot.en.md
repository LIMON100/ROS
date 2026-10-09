# 3-Robot Swarm Driving Simulation — Run Guide & Architecture (EN)

> 한국어 문서: [`swarm_sim_3robot.md`](./swarm_sim_3robot.md)

Bring up **one leader (s1) + two followers (s2, s3)** in a single shared Gazebo
world and inject a path to reproduce **formation driving**. Everything runs on this
one host. Run all commands from the repo root (`combatrobot_1/`).

---

## 1. TL;DR

```bash
# Terminal A — bring the stack up (~40 s; leave it running)
./scripts/run_swarm_sim3.sh

# Terminal B — inject a path and start formation driving
./scripts/swarm_drive.sh

# When done — clean up
./scripts/swarm_kill.sh
```

Success = in the gz window the 3 robots drive north side-by-side in formation and all
reach the goal (`FollowPath 완료 — 미션 도착` ×3 in the launch log).

---

## 1b. Verified run scenario (this session)

Recommended flow that completes reliably on a single 16-core host:

```bash
# (on restart) clean up, confirm load<6 and shm_fast=0
./scripts/swarm_kill.sh

# Terminal A — bring up 3 robots (rviz off: lower load, watch in the gz window)
./scripts/run_swarm_sim3.sh -- use_rviz:=false

# Terminal B — default line-abreast formation drive (55 m)
./scripts/swarm_drive.sh
```

**Verified result:** s1·s2·s3 all reach the goal in ~40 s (`FollowPath 완료 — 미션 도착`),
longitudinal spacing steady at 0.3 m (no surging). Longer route:
`./scripts/swarm_drive.sh -d 90` (within the ±100 m rolling-costmap limit).

**Load reality:** on this host the drive sits at load ≈ 27 (16 cores). Dominated by
**gz sim server (physics + 3 lidars) + gz sim gui (rendering) + the per-robot Python
nodes (executor, lidar filter)** — cutting rviz / sensor rates alone does not move it
much. It still completes; for lighter runs use 2 robots or HIL split (§10).

**Known limit:** *mid-drive* formation changes (`line`/`wedge`…, §6b) can deadlock on
resume. Fixed formations (set at start with `-f`) are reliable.

---

## 2. Key concept — 3 independent stacks, not one program

This is **not** one program controlling 3 robots. One launch command spawns **three
symmetric, fully independent robot stacks**, each isolated under its own ROS
namespace (`/s1`, `/s2`, `/s3`) via `PushRosNamespace`.

```
run_swarm_sim3.sh
  └─ swarm_sim.launch.py  (num_robots:=3)
       ├─ gz_world_sim.launch.py        ← ONE shared Gazebo world + clock bridge (shared)
       ├─ robot_bringup_sim.launch.py  ns=s1  robot_id=1  role=leader     ┐
       ├─ robot_bringup_sim.launch.py  ns=s2  robot_id=2  role=follower   ├ 3 full stacks
       ├─ robot_bringup_sim.launch.py  ns=s3  robot_id=3  role=follower   ┘
       └─ rviz2 + TF-unify relays       ← ONE rviz showing all 3 (shared)
```

So with 3 robots you get **3× nav2 stacks + 3× FSM + 3× swarm_path_executor + 3×
command_server**, each a separate node instance. They never share state directly —
they communicate over DDS topics namespaced per robot (`/s1/...`, `/s2/...`,
`/s3/...`). The trigger is one command; the result is three isolated stacks.

> **Sim vs. real vehicle:** In sim, all 3 stacks run on **this one PC** sharing one gz
> world. On real vehicles each robot is a separate board running its own launch.
> (Note: the real-vehicle launch does not yet pass `robot_id`/`role` to the executor,
> so it currently comes up as a lone leader — see memory `realvehicle-swarm-bringup-pending`.)

---

## 3. Nodes launched per robot (inside each `/sN` namespace)

`robot_bringup_sim.launch.py` builds one robot's node set, wrapped in
`PushRosNamespace(ns)`. Nodes are gated by three HIL-split flags (all default `true`
→ standard single-host sim). Names below are the unprefixed names; at runtime they
appear as `/sN/<name>`.

| Group (flag) | Node | Package · executable | Purpose |
|---|---|---|---|
| always | `robot_state_publisher` | `robot_state_publisher` | URDF → TF tree |
| **body** (`launch_body`) | `ros_gz_sim/create` (spawn) | `ros_gz_sim · create` | spawn the robot into gz |
| | `gz_bridge` | `ros_gz_bridge · parameter_bridge` | cmd_vel / odom / lidar / IMU / GNSS bridge |
| **brain** (`launch_brain`) | `ekf_filter_node_odom` | `robot_localization · ekf_node` | local (odom) EKF |
| | `ekf_filter_node_map` | `robot_localization · ekf_node` | global (map) EKF |
| | `navsat_transform` | `robot_localization · navsat_transform_node` | GNSS → map, shared datum |
| | `frame_fixer` | `combat_robot_nav2 · frame_fixer.py` | force GPS odom into `sN/map` frame |
| | **`swarm_path_executor`** | `combat_robot_nav2 · swarm_path_executor` | formation slot offset, FollowPath driver |
| | `map_server` + `lifecycle_manager_map` | `nav2_map_server`, `nav2_lifecycle_manager` | static empty map in `sN/map` |
| | **nav2** (see below) | `navigation_lite.launch.py` | autonomous navigation stack |
| | `swarm_lidar_filter` | `combat_robot_nav2 · swarm_lidar_filter.py` | mask teammates from own lidar |
| **command** (`launch_command`) | **`command_server`** | `robot_server · command_server_node` | role adapter (leader/follower), tablet/app TCP·UDP |
| | **`combat_robot_operation_system`** (FSM) | `combat_robot_operation_system` | command gate before the executor |

**nav2 stack** (`navigation_lite.launch.py`, a trimmed navigation_launch — drops
route_server/waypoint_follower/smoother_server/docking to cut CPU when running N
stacks on one host):

```
controller_server · planner_server · bt_navigator · behavior_server
velocity_smoother · collision_monitor · local_costmap · global_costmap
lifecycle_manager_navigation
```

**Command gate chain** (per robot — each robot gates its own executor through its own FSM):

```
command_server → /sN/swarm/{path,control}_command
              → FSM (combat_robot_operation_system)
              → /sN/mission/{path,control}_command
              → swarm_path_executor → nav2 FollowPath → /sN/cmd_vel
```

In sim there is no detector/gun/pan-tilt hardware, so the FSM's status checks are
disabled (`checks.*=false`) and the gate is transparent pass-through.

> A `dynamic`-mode follower drives `/cmd_vel` straight from the formation controller
> and therefore skips nav2/map_server/lidar_filter (it would otherwise contend for
> cmd_vel via collision_monitor). The leader always runs nav2. Default mode is
> `static`, where every robot runs its own nav2 and follows an offset path.

**Shared nodes** (launched once by `swarm_sim.launch.py`, not per robot): the gz
world + `clock_bridge`, `rviz2`, and per-robot TF-unify relays (`/sN/tf(_static)` →
`/tf(_static)` plus an identity `map → sN/map`) so a single rviz can show all robots.

---

## 4. Prerequisites (first time)

| Item | Check / install |
|---|---|
| OS / ROS | Linux + ROS 2 (this host: **jazzy**). `gz sim --version` → Gazebo Sim 8.x |
| Workspace build | `ros/install/` must exist; rebuild only if code changed |
| Deps (once) | `cd ros && rosdep install --from-paths src --ignore-src -r -y` |

Build (only when code changed):

```bash
cd ros && colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release && cd ..
# or let the launcher build first:
./scripts/run_swarm_sim3.sh --build
```

> The DDS environment is set automatically by the scripts (both source
> `scripts/lib/common.sh`: `ROS_DOMAIN_ID=96`, `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`,
> FastDDS profile). For **manual** `ros2` commands you must replicate it — see §8.

---

## 5. Bring up the stack — Terminal A

```bash
./scripts/run_swarm_sim3.sh
```

Runs `swarm_sim.launch.py num_robots:=3`. Bring-up is **staggered**
(gz world → s1 @8 s → s2 @20 s → s3 @28 s → rviz) so each nav2 lifecycle settles in
order. **Ready** when the log shows, per robot:
`lifecycle_manager_navigation: Managed nodes are active`, plus
`command_server: [Swarm] role=leader namespace=s1`, and rviz shows all 3 robots.

Pass launch args after `--`:

```bash
./scripts/run_swarm_sim3.sh -- formation_lateral_spacing_m:=3.0   # 3 m lateral gap
./scripts/run_swarm_sim3.sh -- formation_mode:=dynamic            # legacy leader-chasing
./scripts/run_swarm_sim3.sh -- random_spawn:=true                 # scatter then form up
```

### Formation slots (auto from `robot_id`)

`slot = formationSlotOffset(robot_id)`: rank `id-1` → 1:+1(left) 2:-1(right) 3:+2 4:-2…
On a north path, `+slot = west` (screen left).

| Robot | role | slot | spawn | on a north path |
|---|---|---|---|---|
| s1 | leader | 0 (center) | (0, 0) | center |
| s2 | follower | +1 | (−spacing, 0) | left (west) |
| s3 | follower | −1 | (+spacing, 0) | right (east) |

Default lateral spacing = 2.0 m.

---

## 6. Inject a path & drive — Terminal B

After the stack reports "Managed nodes active", in a **new terminal**:

```bash
./scripts/swarm_drive.sh            # default 'go' = LOAD (3 wp, ~55 m north) → START
```

`go` does: **LOAD** (`command:5`, caches a 3-waypoint path to s1,s2,s3) → wait 2 s →
**START** (`command:1`). The leader broadcasts a formation anchor, aligns, and departs;
followers track their lateral-offset slots.

```bash
./scripts/swarm_drive.sh -d 80      # ~80 m north
./scripts/swarm_drive.sh -n 2       # only s1,s2 (for a 2-robot sim)
./scripts/swarm_drive.sh load       # LOAD only
./scripts/swarm_drive.sh start      # START
./scripts/swarm_drive.sh pause      # PAUSE
./scripts/swarm_drive.sh resume     # RESUME
./scripts/swarm_drive.sh stop       # STOP
./scripts/swarm_drive.sh -w '{"waypoints":[{"lat":36.6101,"lon":127.2877},{"lat":36.6106,"lon":127.2877}]}' go
```

`SwarmPathCommand.command` enum: `1`=START `2`=STOP `3`=PAUSE `4`=RESUME
`5`=LOAD_PATH `6`=COMPLETE. The default route runs due north from the sim GNSS datum
(`sejong.world` origin `36.61002559225, 127.28772570583`).

**Alternative — rviz click:** drop a **Nav2 Goal** on the leader (`/s1`) in rviz; the
goal_bridge converts it into a formation mission (single-host sim caveats: memory
`swarm-rviz-goal-bridge`).

---

## 6b. Change formation (incl. mid-drive)

Formation is set with `SwarmControlCommand.formation_type`, published to
`/sN/swarm/control_command` (→ FSM gate → `/sN/mission/control_command` → executor).
The initial formation is **line (횡대)**. Changing it **while driving** makes the
executor stop, re-slot the followers (NavigateToPose to the new lateral offset), and
resume the remaining path.

| Command | type | 대형 | shape |
|---|---|---|---|
| `./scripts/swarm_drive.sh line` | 0 | 횡대 (line abreast) | followers side-by-side |
| `./scripts/swarm_drive.sh column` | 1 | 종대 (column) | single file behind leader |
| `./scripts/swarm_drive.sh diamond` | 2 | 마름모 (diamond) | side escorts |
| `./scripts/swarm_drive.sh wedge` | 3 | 쐐기 (wedge / V) | back-diagonal V |

Start a drive already in a formation with `-f`:

```bash
./scripts/swarm_drive.sh -f wedge go        # form up as a wedge, then drive
```

Full demo — longer path, then switch to line-abreast mid-route:

```bash
./scripts/swarm_drive.sh -d 90 -f wedge go  # drive ~90 m as a wedge
# ...a few seconds into the drive, from the same/another terminal:
./scripts/swarm_drive.sh line               # → 횡대 (spread to line abreast on the move)
```

> ⚠️ Keep a single mission path within **±100 m** of the start: the global costmap is
> a 200×200 m rolling window (§3). For longer routes raise `width/height` in
> `nav2_params_sim.yaml`.
> ⚠️ **Mid-drive formation transitions are currently fragile (unresolved).** The
> command triggers correctly (leader stops → re-aligns → resumes), but on resume the
> leader/follower pacing can deadlock (frac-arc garbage → leader won't advance,
> followers v=0), stalling the mission partway. In a verified run a wedge→line switch
> stalled near the midpoint. Fixed formations (set at start with `-f`) are reliable.
> Background: memory `swarm-column-transition-stuck-rootcause`,
> `swarm-formation-transition-redesign`.

---

## 7. Verify

```bash
# (set the DDS env from §8 first)
ros2 topic echo --once /s1/cmd_vel geometry_msgs/msg/Twist --field linear.x   # >0 = moving
ros2 topic echo --once /s1/odom nav_msgs/msg/Odometry --field pose.pose.position
```

Expected log:

```
s1.swarm_path_executor  [Formation] leader anchor broadcast — align then depart
s2.swarm_path_executor  formation offset applied: slot=1  lateral=+2.00m
s3.swarm_path_executor  formation offset applied: slot=-1 lateral=-2.00m
sN.swarm_path_executor  FollowPath start: NNN pts, 54.8 m
sN.controller_server    Reached the goal!  →  FollowPath 완료 — 미션 도착   (×3)
```

---

## 8. Manual `ros2` — DDS environment

Outside the scripts you must match the nodes' domain/transport (mismatch → domain 0
→ you see nothing):

```bash
export ROS_DOMAIN_ID=96
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=$PWD/scripts/fastdds_profile.xml
unset CYCLONEDDS_URI
source ros/install/setup.bash
```

---

## 9. Shut down / cleanup

```bash
./scripts/swarm_kill.sh
```

⚠️ **Always shut down with this script.** Repeated restarts leave orphan launch
parents that respawn child nodes and pile up `/dev/shm/fastrtps_*` segments — causing
**environment problems that look like code bugs** (form-up freeze, mission won't move).
`swarm_kill.sh` kills parents first → clears nodes → cleans SHM → reports load.
**Before restarting, confirm `load < ~6` and `shm_fast=0`** (the script prints both).
A CPU-overloaded host starves the executor and corrupts formation measurements.

---

## 10. HIL split (`launch_body` / `launch_brain` / `launch_command`)

Each robot's stack can be split across machines (hardware-in-the-loop). All three
default `true` (= standard single-host sim):

| Flag | Nodes | Set `true` on |
|---|---|---|
| `launch_body` | gz spawn + bridge (the "body") | the host providing gz |
| `launch_brain` | ekf + nav2 + executor (the "brain") | the compute board |
| `launch_command` | command_server + FSM | host with those packages (real boards: `false`) |

`robot_state_publisher` is always included (gz spawn needs `/robot_description`;
nav2/ekf need the TF tree).

---

## 11. Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Robots/topics invisible | DDS env not set (§8) or domain mismatch |
| `/sN/fix` (GPS) empty | gz-transport grabbed a dead iface → launcher pins `GZ_IP=127.0.0.1`; on a multi-host setup use the real IP |
| START but no motion / form-up freeze | host overload (zombie stacks) → `swarm_kill.sh`, wait for load<6, relaunch |
| s3 appears late in `node list` | staggered bring-up (s3 ~28 s); wait for all |
| Mid-drive stop on repeated formation change | known issue (memory `tablet-formation-change-demo`, `swarm-column-transition-stuck-rootcause`) |
| GPS topic `echo` hangs | lazy gz bridge — use `timeout -s KILL 6 ros2 topic echo --once ...` |

---

## 12. Related files

- `scripts/run_swarm_sim3.sh` — 3-robot launcher (§5)
- `scripts/swarm_drive.sh` — path injection + drive control (§6)
- `scripts/run_swarm_sim.sh` — generic N-robot launcher (`-- num_robots:=N`)
- `scripts/swarm_kill.sh` — cleanup (§9)
- `ros/.../combat_robot_nav2/launch/swarm_sim.launch.py` — multi-robot orchestration
- `ros/.../combat_robot_nav2/launch/robot_bringup_sim.launch.py` — per-robot stack + datum + HIL flags
- `ros/.../combat_robot_nav2/launch/navigation_lite.launch.py` — trimmed nav2 stack
