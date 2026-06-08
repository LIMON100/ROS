# Phase 2-E v2 — Turn 1 Baseline (DCN-2026-002 D-009 정합 측정)

> **작성일**: 2026-05-12
> **권원**: DCN-2026-002 D-007 (3-Tier) + D-008 (IPC 통일) + D-009 (측정 기준)
> **선행**: Phase 1+1.5+2-F+2-D 완료 (D-004 100%)
> **이 문서**: Phase 2-E 시작 시점의 IPC 부정합 baseline. 매 turn 종료 시 동일 측정 갱신, Turn 17 시점에 모두 0/100% 도달이 목표.

---

## 1. 20 Process 인벤토리 (`main.py` 검증)

| # | Process | 진입점 | Tier | 목표 패키지 | Turn |
|---|---|---|---|---|---|
| 1 | UnitreeGo2Adapter | `adapters/unitree_go2.py` | 1 | `san_unitree_driver` (신규) | 2-3 |
| 2 | RtkGnssAdapter | `adapters/rtk_gnss.py` | 1 | `san_rtk_gnss` (신규) | 4 |
| 3 | NtripClientAdapter | `adapters/ntrip_client.py` | 1 | `san_ntrip_client` (신규) | 4 |
| 4 | LteModemAdapter | `adapters/lte_modem.py` | 1 | `san_lte_redundancy::lte_modem_node` | 2-3 |
| 5 | ExternalImuAdapter | `adapters/ext_imu.py` | 1 | `san_imu_driver` (신규) | 5 |
| 6 | LrfAdapter (LiDAR) | `adapters/lrf.py` | 1 | `san_lidar::lidar_driver_node` | 7 |
| 7 | IMX678Adapter | `adapters/imx678.py` | 1 | `san_camera_imx678` (신규) | 6 |
| 8 | ThermalCameraAdapter | `adapters/thermal.py` | 1 | `san_camera_thermal` (신규) | 6 |
| 9 | LocalizationProcess | `localization/...py` | 1 | `san_slam::local_slam_node` ✅ | 통합 |
| 10 | SLAMBridgeProcess | `mapping/processes.py` | 2 | `san_slam_bridge` (신규) | 5 |
| 11 | SharedMapReceiverProcess | `mapping/processes.py` | 1 | `san_hub_slam::hub_slam_node` ✅ | 통합 |
| 12 | MapFusionProcess | `mapping/processes.py` | 1 | `san_costmap::cost_map_node` ✅ | 통합 |
| 13 | PerceptionProcess | `perception/perception_process.py` | 3 | `san_perception` (신규, rclpy) | 11-12 |
| 14 | MissionProcess | `mission/mission_process.py` | 2 | `san_mission` (신규, rclpy) | 9-10 |
| 15 | CommProcess | `comm/comm_process.py` | 2 | `san_mesh_comm` (신규) | 8 |
| 16 | SafetyProcess (Limp+Fire) | `safety/...py` | 1 | `san_role_management` + `san_fire_authorization` ✅ | 통합 |
| 17 | SwarmBridgeProcess | `swarm/swarm_bridge.py` | 2 | `san_role_management` + `san_lte_redundancy` ✅ | 통합 |
| 18 | BleControlProcess | `control/ble_control.py` | 3 | `san_ble_control` (신규, rclpy) | 13 |
| 19 | OrchestratorProcess | `control/orchestrator.py` | 3 | `squadron.launch.py` 로 대체 | 1 ✅ |
| 20 | HubUgvAdapter (hub-only) | `adapters/hub_ugv.py` | 2 | `san_hub_ugv_adapter` (신규) | 8 |

✅ = 이미 Tier 1 C++로 존재 (Phase 1+2-D 결과). 통합 = 기존 노드에 추가 의무 없음, Python 측 process 만 제거.

---

## 2. D-009 정합 측정 (baseline)

> Phase 2-E Turn 1 진입 시점 (commit `9d5fcb6`) 측정. 매 turn 종료 시 갱신.

| 항목 | 측정 명령 | 목표 (Turn 17) | **baseline (Turn 1)** |
|---|---|---|---|
| Active `multiprocessing.*` import (lines) | `grep -rn '^from multiprocessing\|^import multiprocessing' --include='*.py' .` | **0** | **17** |
| Active `multiprocessing.*` import (files) | (위 grep 의 file count) | **0** | **16** |
| `ShmPool` / `shared_memory` 사용 (files) | `grep -rln 'ShmPool\|shared_memory' --include='*.py' .` | **0** | **10** |
| `Manager().dict` / `manager.dict` 사용 (files) | `grep -rln 'Manager().dict\|manager.dict' --include='*.py' .` | **0** | **4** |
| `rclpy` import (files) | `grep -rln '^from rclpy\|^import rclpy' --include='*.py' .` | ≥ Tier 2 + 3 노드 수 | **0** |
| Tier 1 모듈의 비-C++ 파일 (test/ 제외) | 패키지별 `*.py` 카운트 | **0** | (코드 위치별 분류) |

### 2.1 16 mp-import 파일 분류

| 위치 | 파일 수 | 분류 |
|---|---|---|
| `core/` (ipc, base_process, shm_pool) | 3 | 인프라. Turn 17 archive |
| `main.py` | 1 | 부트스트랩. Turn 17 archive |
| HW adapter (`adapters/unitree_go2.py`) | 1 | Tier 1. Turn 2-3 에 C++ 전환 후 제거 |
| `streaming/nv12_pool.py` | 1 | Tier 3. Turn 6 (카메라) 시 zero-copy 로 대체 |
| `swarm/swarm_bridge.py` | 1 | Tier 2. 기존 C++ 노드와 통합 |
| `tests/` (9 files) | 9 | Tier 3. Turn 14-15 에 pytest+rclpy 회귀로 전환 |
| **합계** | **16** | |

### 2.2 10 SHM 사용 파일 분류

- 인프라: `core/shm_pool.py`, `core/__init__.py` (재수출)
- HW: `adapters/unitree_go2.py`
- Pipeline: `streaming/nv12_pool.py`, `mapping/processes.py`, `perception/perception_process.py`
- Entry: `main.py`
- Tests: `tests/test_camera_fanout.py`, `tests/test_imx_adapter_reaper.py`, `tests/test_shm_pool_multi.py`

→ Turn 6 (카메라 어댑터) + Turn 11-12 (perception) + Turn 14-15 (test 전환) 종료 시 모두 0.

### 2.3 4 Manager().dict 사용 파일

- `core/base_process.py` — metrics_dict, auth_state_proxy 부모 클래스
- `core/diag.py` — metrics 공통 인터페이스
- `main.py` — auth_state_proxy 생성
- `scripts/debug_dashboard.py` — 외부 도구

→ Turn 17 archive 시 모두 0. 대체:
- metrics → ROS 2 statistics or per-node Prometheus exporter
- auth_state_proxy → ROS 2 service (`/auth/get_state`) + topic (`/auth/state_change`)

---

## 3. Turn 1 산출물 (이 PR)

| 산출 | 경로 | 비고 |
|---|---|---|
| 패키지 스캐폴드 | `ros/src/skyautonet/combat_robot_system/san_bringup/` | ament_cmake |
| 패키지 매니페스트 | `san_bringup/package.xml` | 7개 Tier 1 패키지 exec_depend |
| 빌드 설정 | `san_bringup/CMakeLists.txt` | launch / config / systemd install |
| Squadron launch | `san_bringup/launch/squadron.launch.py` | 7+1 노드 (hub 조건부 hub_slam_node) |
| 기본 파라미터 | `san_bringup/config/squadron.yaml` | deployment_mode / robot_id / robot_role |
| systemd unit | `san_bringup/systemd/san-squadron.service` | mwan3-init dependency, ProtectSystem |
| 부트스트랩 deprecate | `main.py` 파일 헤더 | Turn 17 archive 일정 안내 |
| 정합 baseline | `docs/04_Change_Management/Phase2E_baseline_2026-05-12.md` (이 문서) | D-009 측정 + 인벤토리 |

---

## 4. squadron.launch.py 가 띄우는 노드 (현 시점, 7 packages → 8 executables)

| Package | Executable | 조건 |
|---|---|---|
| `san_role_management` | `role_management_node` | 항상 |
| `san_lte_redundancy` | `lte_node` | 항상 (mwan3_init 은 systemd ExecStartPre) |
| `san_lte_redundancy` | `lte_link_quality_node` | 항상 |
| `san_slam` | `local_slam_node` | 항상 |
| `san_costmap` | `cost_map_node` | 항상 |
| `swarm_coordinator` | `swarm_coordinator` | 항상 |
| `san_fire_authorization` | `fire_authorization_node` | 항상 |
| `san_hub_slam` | `hub_slam_node` | `robot_role:=hub` 일 때 |

→ Phase 2-E 진행과 함께 Turn 2~13 매 turn 에 신규 노드 추가.

---

## 5. 다음 Turn 진입 조건

- [ ] PR #79 (이 PR) 머지
- [ ] `colcon build --packages-up-to san_bringup` 통과 (ament_cmake only, no source)
- [ ] PATCH 문서 (이 문서) main 반영
- [ ] Turn 2 진입 — UnitreeGo2Adapter / LteModemAdapter → C++ rclcpp

---

## 6. 검증 명령 (D-009 재측정)

```bash
# 작업 디렉토리: 저장소 루트
echo "mp imports (lines):"
grep -rn '^from multiprocessing\|^import multiprocessing' --include='*.py' . | grep -v __pycache__ | wc -l

echo "mp imports (files):"
grep -rln '^from multiprocessing\|^import multiprocessing' --include='*.py' . | grep -v __pycache__ | wc -l

echo "ShmPool/shared_memory:"
grep -rln 'ShmPool\|shared_memory' --include='*.py' . | grep -v __pycache__ | wc -l

echo "Manager dict:"
grep -rln 'Manager().dict\|manager.dict' --include='*.py' . | grep -v __pycache__ | wc -l

echo "rclpy:"
grep -rln '^from rclpy\|^import rclpy' --include='*.py' . | grep -v __pycache__ | wc -l
```

| 측정 (Turn 1 종료) | 값 | Δ vs baseline | Turn 17 목표 |
|---|---|---|---|
| mp imports (lines) | 17 | 0 (스캐폴드는 mp 미사용) | 0 |
| mp imports (files) | 16 | 0 | 0 |
| SHM (files) | 10 | 0 | 0 |
| Manager dict (files) | 4 | 0 | 0 |
| rclpy (files) | 0 | 0 (Tier 2/3 노드 추가는 Turn 9+) | ≥ 5 |
