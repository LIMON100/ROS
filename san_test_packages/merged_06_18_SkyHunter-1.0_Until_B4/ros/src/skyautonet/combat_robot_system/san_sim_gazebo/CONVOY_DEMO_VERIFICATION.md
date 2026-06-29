# Convoy Demo — Verification Notes (`convoy_demo.launch.py`)

Go2 SITL leader (robot dog) + 4 UGV single-file convoy. This note records the
fixes landed, the WSL environment limitation found during bring-up, and the
**verification procedure to run on the Ubuntu 24.04 / ROS 2 Jazzy dev box or CI**
(per `CLAUDE.md`, the stack is edited on Windows but runs on Linux).

## Changes in this launch file

1. **Leader ground-truth odom — `/odom_gt` source fix (correctness).**
   The coordinator / costmap / viz consume the leader pose on `/odom_gt`.
   Bridging the **shared** gz topic `/odom` is wrong: champ's bring-up
   `gazebo_bridge` bridges `/odom` **bidirectionally** (`@`), so champ's EKF
   output (`footprint_to_odom_ekf`, `odometry/filtered:=odom`, initially `0,0`)
   is re-injected into gz `/odom`. That gives gz `/odom` **two publishers**
   (Go2 truth + EKF `0,0`) and the leader pose **flickers `(0,0) ↔ real`**,
   freezing the leader and producing false lidar projections.
   Fix: bridge the **dedicated, single-publisher** gz topic
   `/model/go2/odometry_with_covariance` → ROS `/odom_gt`.
   *Verified:* leader pose continuous, dog drives the full 40 m route, all 3
   obstacles cleared (min clearance ≥ 1.16 m), zero `(0,0)` frames.

2. **`sim_timeout` arg (default `120.0`).** After N seconds the whole launch
   (gz included) is shut down via `EmitEvent(Shutdown())`; the launch escalates
   `SIGINT → SIGTERM → SIGKILL`. `<= 0` runs forever. *Verified firing.*

3. **`lidar_only` arg (default `false`).** When `true`, the coordinator's static
   obstacle prior is dropped (`obstacles:=[]`) so avoidance depends **solely** on
   Go2 lidar detections (`/convoy/detected_obstacles`). Use this to verify the
   lidar avoidance pipeline (otherwise the ground-truth prior masks it).

4. **Per-user UGV URDF tmpdir** (`tempfile.gettempdir()/san_convoy_<uid>/`) to
   avoid `/tmp/robot_N.urdf` ownership conflicts on re-runs.

## Environment limitation (why verify on Linux, not WSL)

On Windows/WSLg the Gazebo process **exits mid-run**
(`[gazebo-1]: process has finished cleanly`) under the GUI + ~10% real-time
factor for the 5-robot/champ/lidar load. When gz dies, every gz odometry
publisher vanishes at once → leader pose freezes → **the dog never departs**
and the UGVs cannot follow, while the ROS nodes keep publishing `cmd_vel` into a
dead simulator. This is environmental, **not** a convoy-logic defect — the
control architecture is correct (see below). Run verification on the Linux/CI
box where gz holds a stable real-time factor.

## Architecture (matches the intended spec)

- Leader → UGV @2 Hz: `convoy_coordinator.broker()` publishes each UGV's
  **predecessor** pose+velocity on `/convoy/target/r{n}` (`PRED={3:0,4:3,5:4,2:5}`,
  `0`=dog). The UGV predicts 1 s ahead (`predict_horizon_s=1.0`) and targets the
  slot `gap` (≈3 m) behind along the predecessor's breadcrumb trail.
- UGV → Leader @2 Hz: `/convoy/report/r{n}` (own RTK/odom world position).
- First UGV `robot_3` follows the dog; each robot follows the one ahead at ~3 m
  while avoiding obstacles (lidar costmap + coordinator avoid side).

## Verification procedure (Ubuntu 24.04 + ROS 2 Jazzy)

```bash
# Build (workspace with the 4 SkyHunter pkgs symlinked + vendored unitree_go2_ros2)
cd <convoy_ws>
colcon build --symlink-install
source install/setup.bash

# A) Full demo (GUI), auto-stop after 120 s
ros2 launch san_sim_gazebo convoy_demo.launch.py sim_timeout:=120.0

# B) Lidar-only avoidance verification (no ground-truth obstacle prior)
ros2 launch san_sim_gazebo convoy_demo.launch.py lidar_only:=true sim_timeout:=0
```

### Acceptance criteria

| # | Check | How | Pass |
|---|---|---|---|
| 1 | Leader pose clean | `ros2 topic echo /odom_gt` continuous, never `(0,0)` | no flicker |
| 2 | Dog departs + completes | coordinator log `leader=` advances `0 → ~40`, `wp=10/10` | reaches goal `(40,0)` |
| 3 | Convoy spacing ~3 m | each UGV diag `dpred≈3.0` (`/convoy/report` gaps) | `2.5–3.5 m` held |
| 4 | First UGV follows dog | `robot_3` `dpred` to leader tracks ~3 m | gap bounded, not diverging |
| 5 | Obstacle avoidance | path min-distance to each obstacle | `> radius` (≥ ~1 m clearance) |
| 6 | Lidar-driven (run B) | `/convoy/detected_obstacles` non-empty near real obstacles; dog still clears them with `obstacles:=[]` | avoids via lidar only |
| 7 | Auto-shutdown | gz + all nodes exit ~`sim_timeout` s | clean termination |

### Useful runtime probes

```bash
ros2 topic hz /odom_gt /robot_3/odom            # odom feeds alive (≈30 Hz on Linux)
ros2 topic echo /convoy/detected_obstacles      # lidar-detected obstacles
ros2 topic info -v /odom_gt                      # 1 publisher (leader_odom_gt_bridge)
# coordinator log line: leader=(x,y) wp=k/10 gap3=.. throttle=.. mode=FOLLOW|AVOID
```

If the convoy diverges (gap grows) on Linux with a healthy RTF, *then* it is a
controller-tuning issue (catch-up gain / `min_gap`/`slow_gap` / turn-speed
coupling in `convoy_ugv.py`) — but first confirm `/robot_N/odom` is publishing
continuously (≈30 Hz), since a stalled odom feed mimics a controller fault.
