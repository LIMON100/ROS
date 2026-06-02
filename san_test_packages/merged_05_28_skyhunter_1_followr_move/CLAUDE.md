# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

SkyHunter 1.0 is a ROS 2 Humble stack for autonomous swarm operation of an 8-robot
formation (1 Leader + 1 Hub UGV + 1 Deputy UGV + 5 followers). It is a defense R&D
deliverable (26-2차 신속시범사업, ㈜스카이오토넷). Specifications are tracked as
formal documents under `docs/` (SDD / IDS / TST + DCN / ADR); code cites those `§`
sections as authority ("권원"). When behavior and a doc disagree, the doc usually wins —
check the cited DCN/ADR before changing safety- or interface-level logic.

The ROS workspace lives under `ros/src/skyautonet/combat_robot_system/` (~43 packages).
This repo is edited on Windows, but **the stack builds and runs on Ubuntu 22.04 +
ROS 2 Humble** — `colcon`/`ros2`/`source` commands below run on Linux (CI containers or
a Linux dev box), not on the Windows host. Use `bash`, not PowerShell, for the build/test
scripts.

## Build, test, lint

All paths are relative to `ros/` unless noted.

```bash
cd ros

# Full build (all packages)
colcon build --symlink-install

# Core packages only (faster iteration)
colcon build --packages-up-to \
    san_role_management san_hub_comm san_lte_redundancy san_hub_slam \
    san_operation_control san_fire_authorization san_follower_tier \
    san_reroute_planner san_mission san_bringup

# Run tests for selected packages (must source first)
source install/setup.bash
colcon test --packages-select san_role_management san_fire_authorization
colcon test-result --verbose          # show individual test outcomes
```

### Fast pre-PR feedback (no ROS 2 needed)

Standalone test runners compile and run only the **pure-logic** tests — they auto-skip
any test that `#include`s `rclcpp/rclcpp.hpp` (those run in full `colcon test` instead).
Run from the **repo root**:

```bash
bash .github/scripts/run_standalone_gtest.sh
bash .github/scripts/run_standalone_pytest.sh
```

### Lint / format (mirrors CI)

```bash
pip install pre-commit && pre-commit install   # one-time
pre-commit run --all-files                      # manual full run
```

Hooks: `clang-format` **pinned to v14** (C++), `ruff` + `ruff-format` (Python,
`--select=E,F,W,I,N,UP --ignore=E501,N999`), plus hygiene (trailing whitespace,
LF line endings, `check-added-large-files --maxkb=500`). Lint scope is restricted to
`ros/src/skyautonet/.../*.{cpp,hpp,h,py}`. If clang-format complains, run
`clang-format -i <file>` (v14) or `pre-commit run clang-format --all-files`.

### Run the swarm / sim

```bash
ros2 launch san_bringup squadron.launch.py                       # 8-robot squadron
ros2 launch san_bringup squadron.launch.py robot_id:=2 robot_role:=hub  # single robot
ros2 launch san_sim_gazebo limon_squadron.launch.py              # Gazebo sim
```

## Hard architectural rules (enforced, easy to violate)

These are project-wide policies (DCN-2026-002 / ADR-006). Violating them breaks the
design intent even if code compiles:

- **No shell-outs.** `system()`, `popen()`, `subprocess.Popen`, `gst-launch-1.0` are
  forbidden. All external integration goes through C APIs (libuci / libubus / GStreamer
  C API / lifecycle services / D-Bus).
- **No `multiprocessing`.** Python code uses ROS 2 IPC only. Parallel workers = separate
  ROS 2 nodes, never `fork`/`spawn`.
- **All IPC is ROS 2** topics / services / actions. Do not route data around DDS.
- **No new bash scripts** outside deployment helpers (`infra/`).
- **BLE is fully removed** (DCN-2026-023 v2). App integration is exclusively WiFi via
  `rosbridge_server`. Do not reintroduce BLE/PIN auth code.

## 3-Tier IPC structure

Packages are organized by tier (the tier dictates language and node style):

- **Tier 1 — C++ HW drivers** (`san_imu_driver`, `san_rtk_gnss`, `san_ntrip_client`,
  `san_lidar`, `san_cameras`, `san_unitree_driver`, `san_lte_redundancy`, `san_comm*`).
  `rclcpp` lifecycle nodes. When real hardware is absent they fall back via a **3-layer
  compile/link/runtime gate stub** (e.g. `HAVE_RKNN` undefined → host build still works).
- **Tier 2 — C++ behavior logic** (`san_fire_authorization`, `san_formation`,
  `san_surveillance`, `san_follower_tier`, `san_reroute_planner`, `san_role_management`,
  `san_hub_orchestrator`, `san_hub_comm`, `san_hub_slam`, `san_costmap`, `san_localization`,
  `san_nav2`, `san_operation_control`, `swarm_coordinator`, ...).
- **Tier 3 — Python rclpy** (`san_perception` AI inference seam, `san_mission` Behavior
  Tree, `san_operator_tools`).

`combat_robot_msgs` / `san_comm_msgs` hold the IDS message definitions; `san_bringup`
holds `squadron.launch.py`.

### Key subsystem behaviors

- **Role management** — 4-tier Leader succession (Deputy S3 → Hub S2 → max-battery
  follower → Limp Mode) + Hub-Deputy redundancy. Split-brain guarded by monotonic
  `leader_term` / `hub_term`; lower-term announcements are stale and ignored.
- **Fire authorization** — every fire command must pass HMAC-SHA256 (mesh shared secret),
  nonce sliding-window replay guard, Two-key arming (KEY1_TARGET_TAP → KEY2_CONFIRM, 5 s
  timeout, same-target binding), then a fsync'd JSON-Lines audit log with sha256 chain.
- **Follower FSM** — 5 tiers T0..T4 (predictive → normal → catch-up → hard catch-up →
  breadcrumb recovery), with auto-reroute (T1.5) on obstacle within 1 tick (KPP-2).
- **Mission** — Behavior Tree (Sequence/Selector/Fallback/Decorator) with a ThreatAlert
  priority queue and a `manual_override` RLock.

## Testing conventions

- **Test seam pattern:** components expose `injectForTest` / `processXxxForTest` so logic
  is unit-testable without hardware or a live ROS graph.
- **Keep pure-logic tests free of `rclcpp.hpp`** (C++) and `rclpy` (Python). Tests that
  pull in ROS headers are silently skipped by the fast standalone runners and only run in
  the slower full `colcon test`. A standalone test must include only local package headers,
  and the `.cpp` sources it pulls in must also be ROS-free.
- Registering a test: C++ `ament_add_gtest(test_xxx test/test_xxx.cpp src/xxx.cpp)`;
  Python via auto-discovery. **No CI change needed** — the standalone scripts auto-scan
  every `san_*/test/` tree on the next push.
- Integration/acceptance scenarios live in `san_integration_tests` (TST **S20-1..S20-9**)
  and `san_l5_regression` (**L5_26..L5_33** Gate-1 suite). Names like `S20-4`, `L5_30`,
  `KPP-2` refer to numbered scenarios defined in the `docs/` TST documents.

## CI workflows (`.github/workflows/`)

Only four workflows currently exist (README lists more, historical):

| File | Trigger | Purpose |
|---|---|---|
| `coverage.yml` | push / PR / dispatch | gcov + lcov (C++) and pytest-cov (Python) |
| `gate1-regression.yml` | PR / dispatch | L5_26~L5_33 Gate-1 acceptance suite |
| `sanitizers.yml` | weekly schedule / push / dispatch | ASAN + UBSAN |
| `main.yml` | (consolidated entry) | Gate-1 Regression aggregation |

Coverage/sanitizer steps that use the `source` builtin must run under `bash` (not `sh`).

## Change governance

Changes flow through **DCN** (Design Change Notice, e.g. `DCN-2026-024`) and **ADR**
(Architecture Decision Record) documents under `docs/04_Change_Management/` and
`docs/05_Supplementary/`. Commits/PRs reference the DCN number. `CHANGELOG.md` accumulates
the `[Unreleased]` section per sprint. Per-phase engineering notes are in
`ros/src/skyautonet/combat_robot_system/PHASE*_NOTES.md`.

## Deployment

Hub UGV runs dual RK3588J SBCs via the multi-stage Docker stacks in
`infra/docker/sbc1/` and `infra/docker/sbc2/` (+ CycloneDDS NIC-binding XML under
`infra/docker/cyclonedds/`). Mesh/router config is under `infra/openwrt/`. ARM64
cross-build targets RK3588J (`requirements-arm64.txt`).
