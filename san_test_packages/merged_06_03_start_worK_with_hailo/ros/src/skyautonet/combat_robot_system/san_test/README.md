# san_test

Integrated regression testing (Tier 3 per [[ADR-008]]) — Gate-1
acceptance scenarios for SkyHunter v1.5.3.

## Scope

This package hosts the **L5-26~33** scenarios: cross-module tests
that exercise the v1.5.2 + v1.5.3 stack (RTK, Nav2, RTH, mission BT,
E-Stop response, Gate-1 demo) as a connected system. It complements
the **per-package** gtests (which test individual modules in
isolation) and the **`san_l5_regression`** package (which runs the
**S18-1~6** Hub-Deputy / Leader succession scenarios from
SAN-TST-INT-001 v1.5 §11).

| Suite | Scenarios | Focus | DCN |
|---|---|---|---|
| `san_test` (this) | L5-26~33 | Gate-1 acceptance | DCN-2026-022 |
| `san_l5_regression` | S18-1~6 | Hub-Deputy / Leader succession | DCN-2026-001 D-006 |

## Two ways to run

### A. via colcon test (build-time gtest)
```bash
colcon test --packages-select san_test --ctest-args -R test_l5_scenarios
colcon test-result --verbose
```
- Uses the standard `ament_add_gtest` machinery.
- Tests that require a live ROS graph (most of them) will
  `GTEST_SKIP()` with a clear reason — CI passes as "skipped".

### B. via the runner inside a live launch (JUnit XML for CI)
```bash
ros2 launch san_test gate1_regression.launch.xml use_sim_time:=true
# After ~30s stabilization, runner executes.
# Output: /tmp/gate1_regression_results.xml (JUnit XML)
```
- Launch file brings up the deputy + rth + nav2 + mission stack.
- The runner waits 30s, then runs the same `L5_*` filter.
- Tests EXECUTE for real (no skip) because the dependencies are up.

## Scenarios

| ID | Title | Prereq | Status |
|---|---|---|---|
| L5_26 | Deputy boot ≤ 90 s | san_bringup, full launch | ✅ implemented |
| L5_27 | RTK lock + heading accuracy < 1° | san_rtk_gnss | ✅ implemented |
| L5_28 | /local_costmap/costmap ≥ 10 Hz | san_costmap | ✅ implemented |
| L5_29 | Nav2 waypoint follow ±1 m | san_nav2 | ✅ implemented |
| L5_30 | RTH ±2 m accuracy | san_rth (PR #177) | ✅ implemented |
| L5_31 | E-Stop response ≤ 200 ms | san_role_management | ✅ implemented |
| L5_32 | Mission BT loop complete | san_mission | ✅ implemented |
| L5_33 | Gate-1 demo E2E | DCN-2026-016 gate_demo_orchestrator | ⏳ STUB — DCN-016 not yet implemented |

## CI integration status

The DCN-022 spec listed `.github/workflows/gate1-regression.yml`, but
that conflicts with commit `14dd940` (chore(ci): remove all GitHub
Actions workflows). Per the C1 countermeasure agreed in this session's
audit, CI integration is **DEFERRED** and the workflow file is NOT
created. The runner is fully usable locally and from any external CI
(GitLab, Jenkins, etc.) that consumes the JUnit XML.

## Refs

- DCN-2026-022 — this DCN
- `[[ADR-008]]` — Tier 3 (test/tool, C++ preferred for CI integration)
- DCN-2026-017 — `/rth` action (L5_30 backend, landed via PR #177 + #181)
- DCN-2026-016 — gate_demo_orchestrator (L5_33 prerequisite, future)
- SAN-TST-INT-001 v1.5 §11 — sister suite (S18) in `san_l5_regression`
- commit `14dd940` — CI workflow removal (D-118 conflict source)
