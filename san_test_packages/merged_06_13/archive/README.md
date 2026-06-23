# SkyHunter v1.5 — Python Prototype Archive

> **Status**: Archived 2026-05-12 per Phase 2-E Turn 16-17.
> **Successor**: `ros/` workspace (ROS 2 Humble, C++ Tier 1 + rclpy Tier 2/3).

---

## Why archived?

This directory contains the **v1.4 Python prototype** that proved the
architecture and validated all hardware integrations. It has been
superseded by the ROS 2 workspace under `../ros/` per **DCN-2026-002
D-007/D-008**:

- **D-007 (3-Tier policy)** — Tier 1 (HW/safety) requires C++; Tier 2
  (coordination) prefers C++ but allows rclpy; Tier 3 (application)
  prefers rclpy.
- **D-008 (IPC unification)** — All inter-process communication must
  go through ROS 2 topics; `multiprocessing.Process/Queue/shared_memory`
  is no longer permitted.
- **ADR-006 (IPC unification strategy)** — Defines the migration path,
  which this archive completes.

The prototype is **kept for**:
- Reference / historical understanding of the design evolution
- Regression comparison when validating individual ROS 2 nodes
- PoC scripts that have not yet been ROS 2-ported (e.g. some
  bench-test tools)

The prototype is **no longer**:
- Run in production
- A target for new feature work
- Considered authoritative for system behavior — see `../ros/` for canonical implementation

---

## Successor mapping

| Python prototype | ROS 2 successor | Phase 2-E Turn |
|---|---|---|
| `main.py` | `ros/.../san_bringup/launch/squadron.launch.py` | 1 |
| `adapters/unitree_go2.py` | `ros/.../san_unitree_driver/` | 2 |
| `adapters/lte_modem.py` | `ros/.../san_lte_redundancy/` | 3 |
| `adapters/rtk_gnss.py` | `ros/.../san_rtk_gnss/` | 4 |
| `adapters/ntrip_client.py` | `ros/.../san_ntrip_client/` | 4 |
| `adapters/payload_sensors.py::ExternalImuAdapter` | `ros/.../san_imu_driver/` | 5 |
| `adapters/payload_sensors.py::IMX678Adapter` | `ros/.../san_cameras/` (imx678_camera_node) | 6 |
| `adapters/payload_sensors.py::ThermalCameraAdapter` | `ros/.../san_cameras/` (thermal_camera_node) | 6 |
| `adapters/payload_sensors.py::LrfAdapter` | `ros/.../san_lidar/` (lrf_node) | 7 |
| `adapters/hub_ugv.py` | `ros/.../san_hub_orchestrator/` | 8 |
| `swarm/swarm_bridge.py` (link health) | `ros/.../san_comm_link/` | 8 |
| `mission/mission_process.py` | `ros/.../san_mission/` (rclpy) | 9-10 |
| `perception/*` | `ros/.../san_perception/` (rclpy) | 11-12 |
| `comm/ble_*.py` | `ros/.../san_ble_control/` (rclpy) | 13 |
| `core/messages.py` | `ros/.../combat_robot_msgs/msg/*.msg` | 1+ |
| `core/ipc.py`, `core/shm_pool.py` | ROS 2 DDS intra-process zero-copy | (deprecated) |
| `core/base_process.py` | rclcpp::Node + rclpy.node.Node | (deprecated) |

---

## D-009 compliance — before vs after

| Measurement | Pre-migration | Post-migration | Target |
|---|---|---|---|
| `multiprocessing.*` imports outside `archive/` | 18 | **0** | 0 |
| `ShmPool` / `shared_memory` usage outside `archive/` | 53 | **0** | 0 |
| Tier 1 HW drivers in Python outside `archive/` | many | **0** | 0 |
| ROS 2 C++ packages | 15 | **22** | 22+ |
| ROS 2 Python packages (rclpy) | 0 | **3** | — |
| Standalone test count (gtest + pytest) | 0 | **121+** | — |

All targets met.

---

## Re-running the prototype (development only)

If a developer needs to run the Python prototype for comparison:

```bash
cd archive/v15_python_prototype/
python3 main.py --config ../../config/runtime.yaml
```

Note that the prototype's `core/ipc.py` shared-memory implementation
will not coexist with a running ROS 2 squadron. Stop all `ros2 launch`
processes before running.

---

## Removal timeline

The archive is **kept indefinitely** in the source tree as part of
the project history. It does not affect production binaries since
`ros/` is the only target for `colcon build` and packaging.

If disk-space pressure arises, the archive may be moved to git LFS
or to a separate `historical-v1.4` branch in a future maintenance
window, but the migration record (this README) will remain.
