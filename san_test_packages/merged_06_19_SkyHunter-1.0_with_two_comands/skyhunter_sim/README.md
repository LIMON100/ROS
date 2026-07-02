# skyhunter_sim — Go2 SITL convoy demo test harness

Dev/test scripts for the **Go2 SITL convoy demo** (robot dog leader + 4 UGV
single-file convoy). These reproduce and validate the demo that ships as
`san_sim_gazebo/launch/convoy_demo.launch.py` + the `san_operator_tools` convoy
nodes + the vendored Go2 SITL under `ros/src/third_party/unitree_go2_ros2/`.

> These run on **Ubuntu 24.04 / ROS 2 Jazzy / Gazebo Harmonic** with a GPU
> (WSL2 D3D12 passthrough works). They are test orchestration helpers, not part
> of the runtime stack.

## Prereq

A no-space symlink to the repo (RViz mesh loader + space-free ament paths):

```bash
ln -sfn "/mnt/c/Users/<you>/.../SkyHunter-1.0" /root/sky
```

## Scripts

| Script | Purpose |
|---|---|
| `build_test_ws.sh` | Build the demo from repo sources only (vendored Go2 SITL + `san_description` + `san_operator_tools`) into `/root/skytest_ws` — validates the vendoring builds under Jazzy. |
| `run_convoy_test.sh` | Run the full demo (Go2 SITL leader + 4 UGV + convoy nodes + RViz) from `/root/skytest_ws` and report leader/UGV poses, convoy gaps, RViz viz topic health, fall check. |
| `vendor_go2.sh` | Re-vendor the Go2 SITL from a `unitree_go2_ros2` working copy into `ros/src/third_party/` (excludes `.git`/`*.bak`). |

## Run

```bash
# (PowerShell, per test policy) fresh state:  wsl --terminate Ubuntu-24.04
bash skyhunter_sim/build_test_ws.sh        # ~1.5 min ; tail /tmp/skytest_build.log
bash skyhunter_sim/run_convoy_test.sh      # ~2 min   ; tail /tmp/skytest_run.log
```

Or, once the **full** workspace is built (incl. `san_sim_gazebo` + nav2 deps):

```bash
ros2 launch san_sim_gazebo convoy_demo.launch.py        # one command: SITL + UGV + convoy + RViz
```

## Expected (validated 2026-06-20)

Leader walks the planned waypoint path 8/8 (dx ≈ 15 m, no fall), 4 UGVs hold a
~3.5 m single-file convoy (no collision), obstacle clearance ≥ 2.25 m, and RViz
shows the planned path, leader track, robot/chain markers, and obstacle.
