# skyhunter_navigation — Patch 2026-05-13

**Source**: SkyAutoNet integration feedback
**Author**: Kim Taegeun (taegeun.kim@skyautonet.com)
**Scope**: `skyhunter_navigation` package — EKF + Nav2 frame alignment

---

## TL;DR

The original `ekf.yaml` used a single-EKF setup with `world_frame: map`,
which causes GPS fix jumps to propagate into `base_link` velocity
estimates. This patch replaces it with the standard `robot_localization`
**dual-EKF pattern** (local + global) and adds an AMCL fallback path.

5 fixes total. **All changes are config + launch only — no source code
changes.** Backward incompatibility: the launch file now spawns 3 nodes
instead of 1, and the output topic naming follows the `robot_localization`
convention.

---

## Files Changed

| File | Action | Reason |
|------|--------|--------|
| `config/ekf.yaml` | **Removed** (kept as `.orig` for reference) | Split into local + global |
| `config/ekf_local.yaml` | **New** | EKF local node (odom → base_link, no GPS) |
| `config/ekf_global.yaml` | **New** | EKF global node (map → odom, with GPS) + navsat_transform |
| `config/nav2_params.yaml` | **Modified** | Fix `odom_topic`; add `amcl` section |
| `config/nav2_follower_params.yaml` | **Modified** | Fix `odom_topic` |
| `launch/localization.launch.py` | **Rewritten** (kept as `.orig`) | Spawn 3 EKF/navsat nodes + optional AMCL |

---

## The 5 Issues Addressed

### Issue 1 — Single EKF with `world_frame: map`

**Problem**: GPS fix jumps (RTK lock transitions, GPS resets) feed
directly into the `base_link` velocity estimate, causing pose
discontinuities.

**Fix**: Split into two EKF nodes following the `robot_localization`
dual-EKF best practice:
- `ekf_filter_node_local` — `world_frame: odom`, IMU + wheel only
  (no GPS). Provides continuous `odom → base_link` transform.
- `ekf_filter_node_global` — `world_frame: map`, IMU + wheel + GPS.
  Provides the `map → odom` transform with absolute pose.

The local node remains stable when GPS is jumpy or unavailable.

### Issue 2 — EKF publishing `map → base_link` directly

**Problem**: This bypasses the standard Nav2 TF chain
(`map → odom → base_link`), confusing the costmap and controller
servers.

**Fix**: With the dual-EKF split, the local node publishes
`odom → base_link` and the global node publishes `map → odom`,
giving Nav2 the expected canonical chain.

### Issue 3 — `odom_topic` mismatch in `nav2_params.yaml`

**Problem**: `odom_topic: odom_filtered` did not match the actual
EKF output topic (`/odometry/filtered`), so Nav2 was not receiving
EKF output.

**Fix**: Standardized to `/odometry/filtered` (with the leading
slash to make it absolute). Applied in both `nav2_params.yaml`
and `nav2_follower_params.yaml`.

### Issue 4 — Missing `amcl` section in `nav2_params.yaml`

**Problem**: No LiDAR-based localization fallback for GPS-denied
environments (indoor, pre-mapped areas).

**Fix**: Added a standard AMCL section with conservative defaults
(`alpha1-5 = 0.2`, `max_particles = 2000`, `min_particles = 500`,
`scan_topic = scan`, `transform_tolerance = 1.0`). Activated via
`use_amcl:=true` launch flag.

### Issue 5 — `broadcast_cartesian_transform: true` conflict

**Problem**: In a dual-EKF setup, the global EKF node is the
authoritative publisher of the `map → odom` transform. The
`navsat_transform_node` also broadcasting it (via
`broadcast_cartesian_transform: true`) causes a duplicate/conflict.

**Fix**: Commented out `broadcast_cartesian_transform` in
`ekf_global.yaml`. The global EKF is now the sole publisher.

---

## How to Verify

After applying this patch, run these checks to confirm the fix:

```bash
# (a) TF tree should show the canonical chain
ros2 run tf2_tools view_frames
# Expected: map → odom → base_link (not map → base_link directly)

# (b) Canonical EKF output topic
ros2 topic hz /odometry/filtered
# Expected: ~30 Hz steady

# (c) Local-only EKF output topic
ros2 topic hz /odometry/filtered/local
# Expected: ~30 Hz steady (used by downstream high-rate consumers)

# (d) AMCL stack with map server
ros2 launch skyhunter_navigation localization.launch.py \
    use_amcl:=true map:=empty_world_map.yaml
ros2 topic echo /amcl_pose --once
# Expected: pose with non-zero covariance

# (e) GPS isolation test
#     Stop publishing /gps/fix (e.g., kill the GPS bridge or use
#     'gz topic --pause /gps/fix'). Then:
ros2 topic hz /odometry/filtered/local
# Expected: still ~30 Hz (local node is GPS-free, must keep running)

ros2 topic hz /odometry/filtered
# Expected: still ~30 Hz, but pose drift will grow without GPS
#           (this is the intended graceful degradation)
```

---

## Launch Usage

### Outdoor / RTK GPS available (default)

```bash
ros2 launch skyhunter_navigation localization.launch.py
```

Brings up `ekf_local` + `ekf_global` + `navsat_transform`. AMCL stays
disabled.

### Indoor / pre-mapped environment

```bash
ros2 launch skyhunter_navigation localization.launch.py \
    use_amcl:=true map:=empty_world_map.yaml
```

Additionally brings up `map_server` + `amcl` + `lifecycle_manager`.

---

## Migration Notes for Existing Users

If you are already running the old `localization.launch.py`, the
behavior change is:

| What | Before | After |
|------|--------|-------|
| Number of EKF nodes | 1 (`ekf_filter_node`) | 2 (`ekf_filter_node_local`, `ekf_filter_node_global`) |
| Number of total nodes | 2 (EKF + navsat) | 3 (2 EKF + navsat) + 3 conditional (AMCL stack) |
| `/odometry/filtered` published by | single EKF | EKF global |
| `/odometry/filtered/local` | not published | new — published by EKF local |
| TF chain | `map → base_link` | `map → odom → base_link` |
| GPS-jump behavior | base_link velocity disturbed | base_link velocity stable (GPS isolated in global) |

If any downstream node subscribes to `/odometry/filtered` and assumes
it represents a continuous (non-jumpy) velocity source, switch it to
`/odometry/filtered/local` instead.

---

## Original Files (Kept for Reference)

To preserve the original state, the patch keeps two `.orig` copies:

- `config/ekf.yaml.orig` — original single-EKF config
- `launch/localization.launch.py.orig` — original launch file

These can be deleted once the new setup is confirmed working.

---

## Contact

Questions or feedback: **taegeun.kim@skyautonet.com**
