# SkyHunter 1.0

> ## ⚠️ Maintainer transition in progress (2026-05-15)
>
> 원작자(tkim1310@gmail.com)가 active maintenance에서 물러나는 시점.
> v1.5.2 사이클 머지 완료 후 23개 stale PR이 다음 maintainer 인계 대기 중.
>
> **새로 합류하시는 분께**:
> 1. 인계 트래킹 이슈 **[#175](https://github.com/adasone/SH_Unitree_Patrol/issues/175)** 부터 읽어주세요 (pinned). 남은 PR 분류 + 권장 처리 절차 + close 코멘트 템플릿 포함.
> 2. PR 목록에서 `stale` 라벨 필터로 23개 일괄 확인 가능.
> 3. 코드/문서 인덱스는 아래 "현재 구현 현황" 표 참조. 최근 머지 history는 `git log --oneline 2b99e74` (v1.5.2 사이클 종료 시점 HEAD).

8대 편대 (1 Leader + 1 Hub UGV + 1 Deputy UGV + 5 follower) 의 자율 군집
운용을 위한 ROS 2 Humble 스택. v1.4 의 **Hub-Deputy 이중화 + 4-tier
Leader 승계 + Limp Mode** 위에 v1.5 부터는 **3-Tier IPC 통일 정책
(DCN-2026-002)**, **HMAC + Two-key 발사 인증 (DCN-2026-001 D-004)**,
**5-Tier Follower FSM (SDD §6.2)**, **Behavior-Tree Mission**, **Gazebo
sim + Nav2 + Dual-EKF + AMCL** 가 통합되었습니다.

**Latest release:** [`v1.5.5`](https://github.com/adasone/SkyHunter-1.0/releases/tag/v1.5.5) — 2026-05-24
(v1.5.4 cross-audit 21 fixes + Gate-1 CI workflow + DCN-2026-024
WiFi auth gate. 이전 v1.5.4 (6 DCN integration), v1.4.0 (2026-05-12).)

## v1.5.4 sprint 변경 사항 (2026-05-24 진행 중)

운용자 + Gate-1 ROS 통합 + BLE 잔재 cleanup. 자세한 내역은
[`CHANGELOG.md`](CHANGELOG.md) 의 `[Unreleased]` 섹션 참조.

| DCN | 변경 | 영향 모듈 |
|---|---|---|
| DCN-2026-016 | **Gate-1 demo ROS integration** — `/gate1/start_demo` (Trigger) + `/gate1/demo_status` (OperationState 5 s) + `/emergency_stop` (EmergencyStop) → `/rth` fallback + 60 s phase watchdog | `san_operation_control` (+ `gate1_demo.launch.xml`, 6 gtest) |
| DCN-2026-018 | **Fire simulator co-located** — `/swarm/fire/authorization_response` 구독 → 정렬 평가 (2° tol) → `/swarm/fire/result` (`FireResult`) 발행. 가공된 가짜 무기 발사 없음 | `san_fire_authorization::fire_simulator_node` (5 gtest) |
| DCN-2026-019 | **MC ACK/retransmit protocol** — wire-level `MCMessage`/`MCAck` (seq + crc32 + 16-deep sliding window), receiver + sender (500 ms ack timeout, 3 retry, `/mc/timeout` escalation) | `san_operation_control::mc_{protocol,sender}_node` + `combat_robot_msgs/{MCMessage,MCAck}.msg` (8 gtest) |
| DCN-2026-020 | **MC stress test scenario** — 1 kHz + drop/dup/reorder 합성 noise + p99 < 50 ms KPP 측정 + CSV/JUnit 출력 | `san_l5_regression::McStressScenario` (3 gtest) |
| DCN-2026-022 | **L5_26~L5_33 Gate-1 acceptance suite** — 8 시나리오 (Deputy boot / RTK / Costmap / Nav2 / RTH / E-Stop / Mission BT / Gate-1 E2E) + JUnit XML emitter + `.github/workflows/gate1-regression.yml` CI workflow | `san_l5_regression::*` (6 gtest) |
| DCN-2026-023 v2 | **BLE 완전 제거** (PIN auth + archive 포함) — `requires_pin` 필드 / `_pin_authenticated` / `set_pin_authenticated()` 등 dead code 모두 제거. App 연동은 전적으로 WiFi (rosbridge_server) | `san_mission` / `docs/PDR/{PKG,MSG,ARCH}-001` / `archive/v15_python_prototype/` (6 file rm) |

**v1.5.4 sprint Phase 4 INTEGRATION 완료** — 6/6 DCN main 머지. Phase 5
(validation) + Phase 6 (v1.5.4 tag + GitHub release) 후속.

## v1.5.1 / DCN-2026-003 변경 사항 (2026-05-13)

v1.5 PDR-prep 위에 운용자 요청 4건 적용:

| D | 변경 | 영향 모듈 |
|---|---|---|
| D-001 | **영상 스트리밍 HD 다운사이징 = 2 Mbps** (기존 1.5 Mbps) | `san_hub_comm`, `san_video_sender`, `combat_robot_msgs/VideoStreamRequest.msg` |
| D-002 | **Cost map 1 Hz → 2 Hz** layer refresh (10 Hz callback 활용도 10% → 20%) | `san_costmap` |
| D-003 | **C++ `human_detector` 완전 적용**: 카메라 sub + DetectionArray pub + RK3588 NPU 실 추론 (Airys V6.13.5 `rknn_detector_board.cpp` 포팅) | `human_detector` |

**Airys V6.13.5 reference 포팅 내용**:
- 3-head YOLOv5 디코드 (stride 8/16/32, anchor 3× per head)
- `sigmoid + anchor decode + objectness × class confidence`
- `thread_local` scratch buffer NMS (heap 할당 0)
- bbox 원본 해상도 자동 rescale
- `HAVE_RKNN` 미정의 시 graceful fallback (host build OK)


## 현재 구현 현황

| 단계 | 범위 | 핵심 산출물 | 상태 |
|---|---|---|---|
| **v1.4 GA** (#49~#63) | Hub-Deputy 이중화 + 4-tier Leader + Limp Mode | 12 packages | ✅ |
| **v1.5 Phase 2-D** (#73~#76) | DCN-2026-001 D-004 — HMAC-SHA256 + Two-key + audit log | `san_fire_authorization` (46 unit tests) | ✅ |
| **v1.5 Phase 2-E** (#79~#88, #97) | DCN-2026-002 — 3-Tier IPC 통일 (no `multiprocessing`, ROS 2 IPC 의무) | 14-node squadron launch + 11 신규 패키지 | ✅ |
| **PDR-prep** (#90~#95, #97) | san_formation / san_surveillance / san_follower_tier / san_reroute_planner / Mission BT Fallback + IDS 메시지 6건 | SDD §6.1/§6.2/§6.4/§7/§8 | ✅ |
| **PDR-7** (#100) | `combat_robot_msgs` IDS 80% → 94% (6 신규 메시지) | — | ✅ |
| **Limon sim** (#98~#99) | Gazebo + URDF + 운용자 도구 + Nav2/Dual-EKF/AMCL/Maps/Comm sim | 39 packages (현재 40개) | ✅ |
| **CI/CD** (#89, #97) | TST S20 + Sanitizers (ASAN/UBSAN) + Coverage + standalone-pytest | `.github/workflows/` | ✅ |
| **Phase 7** (#129, 진행 중) | clock-skew anchor + lifecycle service success + tier dt + mission pose lock | 4 medium-tier residual | 🟡 |
| **Phase 7 deferred** (#130~#134, 진행 중) | CI baseline + HubSlam PNG-outside-mutex + S20-3/4/5 strict + 동적 producer discovery + 정적분석 hardening 6건 | — | 🟡 |

## 8대 편대 ID 매핑

| robot_id | 역할 | SBC 구성 | 비고 |
|---|---|---|---|
| S1 | Leader (Unitree Go2) | 단일 | Android app 직접 통신 |
| **S2** | **Hub UGV** | **듀얼 RK3588J** | Leader 승계 2순위, LTE 1차, SLAM aggregation, GStreamer SRT relay |
| **S3** | **Deputy UGV ★v1.4** | **듀얼 RK3588J** (Hub 와 동일) | Leader 승계 1순위, Hub 인수 1순위 |
| S4-S8 | Follower UGV | 단일 | 1~5 대 가변, 배터리 최대 = 3 순위 Leader 후보 |

## v1.4 정책 (계승)

### 4-tier Leader 승계
| 우선순위 | 후보 | 조건 |
|---|---|---|
| 1순위 ★ | Deputy UGV (S3) | SBC 둘 다 정상 + 배터리 ≥ 20 % |
| 2순위 | Hub UGV (S2) | Deputy 불능 + SBC 정상 |
| 3순위 ★ | 배터리 최대 follower | Deputy + Hub 모두 불능 |
| 4순위 (Limp) | RTB / 임무 종료 | 전 robot 배터리 < 10 % |

Split-brain 방지: `leader_term`, `hub_term` 단조 증가. 낮은 term 의
announcement 는 stale 로 무시.

### Hub-Deputy 이중화
Hub UGV 작동 불능 시 Deputy UGV (S3) 가 LTE 게이트웨이 / SLAM
aggregation / GStreamer SRT relay 를 lifecycle activate 로 모두
인수합니다.

### Limp Mode (Hub + Deputy 모두 불능)
- ❌ LTE 외부 원격 접속, ❌ SLAM 글로벌 통합
- ✅ Android app 이 Wi-Fi 6 mesh 직접 접속 → 생존 군집 전체 통제
- ✅ 사격/타격 명령 — mesh HMAC-SHA256 인증 (v1.5 Phase 2-D 강화)
- ✅ 영상 — GStreamer UDP/SRT 가 Hub relay 대신 Android IP 로 직접 송신
- ⏸ 복잡 임무 (대형 변경, AI 추론) 만 일시 중단

### LTE-driven 영상 auto-rate (PHASE 5b)
RSRP 기반 4-grade 매핑으로 active stream quality 자동 조정. Demotion 즉시,
Promotion 은 hold-tick hysteresis (기본 5 sample). Concurrency-driven
thumbnail 모드 (4 + 동시 stream) 가 link-quality 정책보다 우선.

## v1.5 신규 정책

### 3-Tier IPC 통일 (DCN-2026-002 + ADR-006)
- **Tier 1 (C++ HW driver)** — IMU / RTK GNSS / NTRIP / LRF / Camera /
  Unitree SDK / LTE Modem / Comm uplink. `rclcpp` lifecycle node, 실
  HW 미존재 시 3-layer compile/link/runtime gate stub.
- **Tier 2 (C++ 동작 로직)** — formation / surveillance / hub
  orchestrator / fire authorization / reroute planner / role
  management.
- **Tier 3 (Python rclpy)** — perception (AI inference seam) / mission
  (Behavior Tree) / operator tools.
  ※ DCN-2026-008 + DCN-2026-023 v2 로 `san_ble_control` 완전 삭제 —
  App 연동은 전적으로 WiFi (rosbridge_server) 사용.
- **금지**: `multiprocessing`, `subprocess.Popen` 으로 ROS 우회. 모든
  IPC 는 ROS 2 토픽 / 서비스 / 액션.

### HMAC + Two-key 발사 인증 (DCN-2026-001 D-004)
모든 사격 명령은 다음을 통과해야 fire authorized:
1. **HMAC-SHA256** 서명 검증 (mesh shared secret, /etc/san/mesh_secret.bin)
2. **Nonce sliding window** 재사용 방지 (2 s × 65536-entry 시간 앵커)
3. **Two-key arming** — KEY1_TARGET_TAP → KEY2_CONFIRM (5 s timeout)
4. **Target binding** — KEY1↔KEY2 동일 target 강제 (mismatch 시 거부)
5. **Audit log** — JSON Lines + sha256 chain + UUIDv4 (fsync 후 응답)

### 5-Tier Follower FSM (SDD §6.2)
- T0 PREDICTIVE_TRACK — 예측 정상
- T1 NORMAL — 정상 follow
- T1.5 AUTO_REROUTE — 장애물 회피 (KPP-2: 장애물 감지 1 tick 내 진입)
- T2 CATCH_UP — δ > 1.5·d0
- T3 HARD_CATCH_UP — δ > 2.0·d0
- T4 BREADCRUMB_RECOVERY — δ > 4.0·d0 또는 60 s comm timeout

### Behavior-Tree Mission (SDD §6.1)
Sequence / Selector / Fallback / Decorator + Action / Condition 노드.
mission 정책 = patrol / advance / hold / RTB / threat_response.
ThreatAlert priority 큐 + manual_override 락 (RLock).

## 코드 구조 (39 packages — DCN-2026-008 + DCN-2026-023 v2 로 `san_ble_control` 제거)

```
ros/src/skyautonet/combat_robot_system/
├── combat_robot_msgs/              # IDS 메시지 (PDR-7: 94% coverage)
├── combat_robot_operation_system/  # 5-tier deployment_mode + DEVELOPER_AUTH gate
├── san_bringup/                    # squadron.launch.py (14-node + 다중 robot ns)
├── san_description/                # URDF 자산
├── san_sim_gazebo/                 # Limon Gazebo sim
├── san_sim_gazebo_helpers/         # sim helper packages
│
│ ── Tier 1: C++ HW drivers ───────────────────────────
├── san_imu_driver/                 # binary frame parser (B0~B4)
├── san_rtk_gnss/                   # NMEA + RTCM (stub-safe NaN/Inf)
├── san_ntrip_client/               # RTCM caster client
├── san_lidar/                      # LRF + GroundSegmenter
├── san_cameras/                    # V4L2 (compile-gate stub)
├── san_unitree_driver/             # Unitree SDK2 (cmd_vel watchdog)
├── san_lte_redundancy/             # mwan3 (libuci/libubus)
├── san_comm/                       # Wi-Fi 6 mesh
├── san_comm_link/                  # link health + WiFi6↔LTE failover
├── san_comm_msgs/                  # 통신 메시지 별도 패키지
├── san_comm_sim/                   # comm 시뮬레이션
│
│ ── Tier 2: C++ 동작 로직 ────────────────────────────
├── san_fire_authorization/         # HMAC + Two-key + Audit (D-004) + fire_simulator_node (DCN-018)
├── san_formation/                  # 9 formation + Hungarian (SDD §7)
├── san_surveillance/               # Pan-Tilt + threat (SDD §8)
├── san_follower_tier/              # 5-Tier FSM + anti-flap (SDD §6.2)
├── san_reroute_planner/            # T1.5 cost map 회피 (SDD §6.4, KPP-2)
├── san_role_management/            # Hub-Deputy + 4-tier Leader + Limp Mode
├── san_hub_orchestrator/           # SwarmAggregator + ThreatAggregator (clock anchored)
├── san_hub_comm/                   # GStreamer SRT relay + auto-rate
├── san_hub_slam/                   # 8-robot delta merge + dynamic discovery
├── san_video_sender/               # Follower UDP sender
├── san_slam/                       # local SLAM delta producer
├── san_costmap/                    # 4-layer Nav2 cost map
├── san_localization/               # EKF + AMCL
├── san_nav2/                       # Nav2 stack 통합
├── san_operation_control/          # DEMO sequencer + command_echo + Gate-1 ROS (DCN-016)
├── swarm_coordinator/              # 군집 조정
│
│ ── Tier 3: Python rclpy ─────────────────────────────
├── san_perception/                 # AI inference seam (stub-safe)
├── san_mission/                    # Behavior Tree + MissionContext
├── san_operator_tools/             # waypoint_sender + formation_switcher
│
│ ── 검증 ──────────────────────────────────────────────
├── san_integration_tests/          # TST S20 시리즈 (1~9 + 7b)
├── san_l5_regression/              # L5 시나리오 회귀
└── human_detector/                 # RK3588 / Hailo8 / stub
```

배포: `infra/docker/sbc{1,2}/` 에 Hub UGV 듀얼 SBC 용 multi-stage Docker
compose stack + CycloneDDS NIC binding XML.

## 정책 (전 PHASE 공통)

- **Shell 호출 0 건** — `system()`, `popen()`, `gst-launch-1.0` 금지.
  모든 외부 통합은 C API (libuci / libubus / GStreamer C API /
  lifecycle services / D-Bus).
- **Bash 스크립트 0 건** — 배포 helper 외 `*.sh` 없음.
- **multiprocessing 금지** (v1.5 DCN-2026-002) — Python 코드도 ROS 2
  IPC 만 사용. fork/spawn 으로 다중 작업자 분리는 ROS 2 노드 분리로.
- **Test seam** — 각 컴포넌트는 `injectForTest` 또는 `processXxxForTest`
  로 단위 테스트 가능. GTest + synthetic data generator.

## 빌드

```bash
cd ros

# 전체 빌드 (39 packages)
colcon build --symlink-install

# 핵심 패키지만
colcon build --packages-up-to \
    san_role_management san_hub_comm san_lte_redundancy \
    san_hub_slam san_operation_control san_fire_authorization \
    san_follower_tier san_reroute_planner san_mission \
    san_bringup

source install/setup.bash
colcon test --packages-select san_role_management san_fire_authorization \
                              san_hub_slam san_follower_tier
```

### 8-robot squadron 실행

```bash
ros2 launch san_bringup squadron.launch.py
# 또는 단일 robot
ros2 launch san_bringup squadron.launch.py robot_id:=2 robot_role:=hub
```

### Gazebo sim

```bash
ros2 launch san_sim_gazebo limon_squadron.launch.py
```

Hub UGV 듀얼 SBC Docker 배포는 [`docs/deployment/hub_dual_sbc.md`](docs/deployment/hub_dual_sbc.md),
LTE auto-rate 운용은 [`docs/deployment/lte_auto_rate.md`](docs/deployment/lte_auto_rate.md)
참조. CI 운영은 [`docs/CI_GUIDE.md`](docs/CI_GUIDE.md).

## CI 워크플로 (`.github/workflows/`)

| Workflow | 트리거 | 내용 |
|---|---|---|
| `ros2-ci.yml` | PR / push | colcon build + test (Humble, ubuntu-22.04) |
| `standalone-tests.yml` | PR | C++ standalone gtest + Python standalone pytest |
| `integration-tests.yml` | PR | TST S20 시리즈 (1~9 + 7b) |
| `regression.yml` | PR | S15 시나리오 회귀 |
| `kpp.yml` | PR | KPP-1 / KPP-2 검증 |
| `coverage.yml` | PR | gcov + lcov + pytest-cov |
| `sanitizers.yml` | weekly + release/** | ASAN + UBSAN (formation / surveillance / follower_tier / reroute_planner) |
| `lint.yml` | PR | clang-format + ruff + yamllint |
| `arm64.yml` | release | cross-build for RK3588J |

## 검증 시나리오 매핑

| 시나리오 | 의미 | 검증 위치 |
|---|---|---|
| S15-2 | Pan-Tilt detection KPP | `san_surveillance/test/` |
| S15-3 | Hub 듀얼 SBC 부분 운용 (Case A/B/C) | `test_hub_health.cpp` |
| S15-5 | LTE 변동 시 영상 auto-rate | `test_auto_rate_controller.cpp` + 실로봇 KPP |
| S15-6 | 다중 영상 thumbnail 다운그레이드 | `test_thumbnail_downgrade.cpp` |
| S16-1~6 | DEMO 6-phase + lab_test 확장 | `test_demo_sequencer.cpp` |
| S18-1~6 | 4-tier Leader 승계 + Limp Mode | `san_role_management/test/*` |
| **S20-1** | Squadron stability (5 min, 모든 nodes UP) | `test_s20_1_squadron_stability.py` |
| **S20-2** | Leader failover 시간 측정 | `test_s20_2_leader_failover.py` |
| **S20-3** | RTCM chain (NTRIP→RTK_FIX) | `test_s20_3_rtcm_chain.py` |
| **S20-4** | Fire auth end-to-end (HMAC + Two-key) | `test_s20_4_fire_auth.py` |
| **S20-5** | Comm link WiFi6↔LTE failover | `test_s20_5_comm_link_failover.py` |
| **S20-6** | E2E latency (operator → robot ack) | `test_s20_6_e2e_latency.py` |
| **S20-7/7b** | Mission BT boot + priority injection | `test_s20_7*_mission_bt_*.py` |
| **S20-8** | KPP-2 E2E (장애물 → T1.5 1-tick) | `test_s20_8_kpp2_e2e.py` |
| **S20-9** | T4 recovery (breadcrumb → comm restore) | `test_s20_9_t4_recovery.py` |

## 잔여 / 후속

코드 레벨 통합 완료된 v1.5 PDR-prep 위에서, 다음 항목은 실하드웨어 단계
(L4/L5 Sprint 통합) 에서 검증합니다.

- L5 실로봇 8 대 4 시간 회귀 시험
- KPP 측정 보고서 (FHD 1.5 Mbps E2E p99, 3 stream concurrency, KPP-1/KPP-2)
- 실 Hailo8 모듈 통합 KPP
- `pose_graph_optimizer` 의 g2o full population (현재 sentinel만)
- `combat_robot_msgs::FormationCommand` ↔ `FormationStatus` enum 통일 (breaking schema, 별도 PR)

## 참조 문서

### v1.5 (현재)
- SAN-SDD-SWARM-001 v1.5 (Hub-Deputy + Limp Mode + Behavior Tree + 5-Tier FSM)
- SAN-SDD-SUR-001 v1.5 (GStreamer SRT + cost map + SLAM aggregation)
- SAN-IDS-CMD-001 v1.5 (`FireAuthorizationRequest/Response`, `ThreatAlert`,
  `LeaderRoleAnnouncement`, `HubRoleAnnouncement`, `SlotAssignment`,
  `FollowerTargetMessage`, `TierStatusChange`, `CommLinkStatus`,
  `FleetStatus`, ...)
- SAN-TST-INT-001 v1.5 (S20 시리즈 9 개 시나리오 + 7b)
- SAN-OPS-SOP-001 v1.5

### DCN / ADR
- DCN-2026-001 D-004 — Limp Mode 발사 정책 Option A (HMAC + Two-key + Audit)
- DCN-2026-002 + ADR-006 — 3-Tier IPC 통일
- ADR-005 — 영상 SRT pull mode
- ADR-004 — Hub-Deputy lifecycle activate

### PDR 산출물
- SAN-PDR-* Rev.A (7 평가 산출물, 2026-05-08)

각 PHASE / 패키지의 ROS 2 코드는 위 문서의 §을 인용합니다.
