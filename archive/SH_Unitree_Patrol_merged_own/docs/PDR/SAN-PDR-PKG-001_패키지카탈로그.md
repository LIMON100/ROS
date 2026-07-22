# SAN v1.5 — 패키지 카탈로그

> **문서 ID**: SAN-PDR-PKG-001 Rev.A
> **목적**: 32 패키지 인벤토리 + Tier 분류 + SDD/IDS 매핑

---

## 1. 패키지 분포 요약

| Tier | 패키지 수 | 언어 | 목적 |
|---|---|---|---|
| **T0 Infrastructure** | 4 | mixed | 메시지 / 부트스트랩 |
| **T1 Drivers / IO** | 13 | C++ (rclcpp) | HW driver + 저수준 |
| **T2 Coordination** | 6 | C++ (rclcpp) + Py | 군집 행동 (SDD §6-§8) |
| **T3 Application** | 4 | C++ + Py (rclpy) | 임무 BT + 안전 |
| **T4 Test** | 2 | mixed | TST S20 + 회귀 |
| **합계** | **32** | | |

DCN-2026-002 D-007 3-Tier 정합 — Tier 2 rclpy 4개 (san_mission, san_perception, san_ble_control, san_slam_fusion).

---

## 2. Tier 0 — Infrastructure (4)

| 패키지 | 역할 | SDD/IDS |
|---|---|---|
| `combat_robot_msgs` | **36 message 정의** | IDS-CMD v1.5 전체 |
| `combat_robot_operation_system` | 운용 시스템 메타 | §2 |
| `san_bringup` | **squadron.launch.py** (4 GroupAction: always_on / hub_only / leader_only / **follower_only**) | §1 |
| `swarm_coordinator` | 군집 최상위 코디네이션 | §3 |

---

## 3. Tier 1 — Drivers / IO (13)

| 패키지 | 역할 | HW |
|---|---|---|
| `san_unitree_driver` | Unitree Go2 SDK 통합 (Leader 전용) | Unitree Go2 |
| `san_rtk_gnss` | RTK 기반 위치 (F9P + RTCM3) | u-blox F9P |
| `san_ntrip_client` | NTRIP RTCM3 클라이언트 | 인터넷 |
| `san_imu_driver` | IMU 9-DoF 수집 | ICM-42688P |
| `san_cameras` | EO + Thermal dual cam | IMX678 + LWIR |
| `san_lidar` | 3D LiDAR wrapper | **Robosense E1** (HW 구성도 정합) |
| `san_video_sender` | GStreamer 영상 송신 (SBC #2) | — |
| `san_slam` | Local SLAM | — |
| `san_hub_slam` | Aggregated SLAM (Hub 측, 30-60s) | — |
| `san_comm_link` | 통신 링크 (LTE/WiFi6 abstraction) | — |
| `san_comm` | 저수준 통신 | — |
| `san_hub_comm` | Hub 통신 (mesh / LTE bridge) | — |
| `san_costmap` | **4-layer Local Cost Map (1Hz, SDD §6.4)** | — |
| `san_perception` | YOLO + DeepSORT (rclpy Tier 2) | — |
| `san_ble_control` | BLE 컨트롤러 (rclpy Tier 2) | — |
| `human_detector` | Detection / DetectionArray | — |

---

## 4. Tier 2 — Coordination / Swarm (6)

| 패키지 | 역할 | SDD §, 메시지 | 신설 시점 |
|---|---|---|---|
| `san_role_management` | D-005 Modified Raft + Hub 승계 | §11 / LeaderRoleAnnouncement | Phase 2-E |
| `san_hub_orchestrator` | D-007 3-Tier 분배 | §5 / HubRoleAnnouncement | Phase 2-E |
| `san_lte_redundancy` | 삼중화 통신 (WiFi6 + LTE + LoRa) | §5 / LTERoleAnnouncement | Phase 2-E |
| **`san_formation`** | **Hungarian + 9 대형 + 4 preset** | **§7 / FollowerTargetMessage, SlotAssignment, BreadcrumbBroadcast, FormationStatus** | **P0-1 (PDR)** |
| **`san_surveillance`** | **360° sector + PanTilt 4 mode** | **§8 / SurveillanceSectorAssignment, PanTiltCommand** | **P0-2 (PDR)** |
| **`san_follower_tier`** | **6-state FSM (T0/T1/T1.5/T2/T3/T4)** | **§6.2 / TierStatusChange** | **PDR-2** |
| **`san_reroute_planner`** | **Tier 1.5 cost map ±2m 우회 (KPP-2)** | **§6.4 / CostMapUpdate** | **PDR-5** |

---

## 5. Tier 3 — Application (Mission + Safety) (4)

| 패키지 | 역할 | SDD §, 권원 |
|---|---|---|
| **`san_mission`** (rclpy Tier 2) | **Mission BT Fallback root (SDD §6.1)** | **§6.1 — PDR-4** |
| `san_fire_authorization` | HMAC + 2-key + Audit Fire Auth | §10 / FireAuthorization / **D-004 100%** |
| `san_operation_control` | Fire Auth orchestrator + 한계 모드 | §10 |
| `san_slam_fusion` (rclpy Tier 2) | Local SLAM fusion | §9 |

---

## 6. Tier 4 — Test & Integration (2)

| 패키지 | 역할 | 시나리오 수 |
|---|---|---|
| `san_integration_tests` | TST S20 launch_testing | **9** (1-6 기존 + 7/8/9 PDR-6) |
| `san_l5_regression` | L5 회귀 (deployment_mode=bench/dev) | — |

---

## 7. PDR 준비 단계 신설/확장 패키지 — 종합

| 패키지 | 신설 / 확장 | 핵심 산출 | 테스트 |
|---|---|---|---|
| **san_formation** (신설) | P0-1 | Hungarian + 9 대형 + 4 preset | 16 standalone gtest |
| **san_surveillance** (신설) | P0-2 | Sector allocator + PanTilt 4 mode | 17 standalone gtest |
| **san_follower_tier** (신설) | PDR-2 | TierFsm 6-state | 16 standalone gtest |
| san_formation (확장) | PDR-3 | Leader velocity 1초 예측 | 기존 16 회귀 ✅ |
| san_mission (확장) | PDR-4 | Fallback root + ExtendedMissionContext | 15 신규 standalone pytest |
| **san_reroute_planner** (신설) | PDR-5 | Cost path checker + lateral evasion (★ KPP-2) | 13 standalone gtest |
| san_integration_tests (확장) | PDR-6 | TST S20-7/8/9 (★ KPP-2 자동화) | 3 launch_test |
| `combat_robot_msgs` (확장) | 전 단계 | +7 메시지 (CostMapUpdate, FollowerTargetMessage, BreadcrumbBroadcast, FormationStatus, SlotAssignment, SurveillanceSectorAssignment, PanTiltCommand, TierStatusChange) | — |

→ PDR 준비 단계 신설 6개 + 확장 4개 = **10 변경 영역**, **77 신규 테스트**

---

## 8. 패키지 의존성 표 (주요 경로만)

```
mission_node (Tier 3, rclpy)
  ├─ depends on combat_robot_msgs (Tier 0)
  └─ runtime exec: san_mission BT Fallback root

formation_node (Tier 2, C++)
  ├─ depends on combat_robot_msgs
  ├─ publishes  → FollowerTargetMessage, SlotAssignment, BreadcrumbBroadcast, FormationStatus
  └─ subscribes ← FormationCommand, RobotStatus

surveillance_node (Tier 2, C++)
  ├─ publishes  → SurveillanceSectorAssignment, PanTiltCommand
  └─ subscribes ← FormationStatus, ThreatAlert

tier_node (Tier 2, C++)
  ├─ publishes  → TierStatusChange
  └─ subscribes ← FollowerTargetMessage, RobotStatus, SlotAssignment, ~/obstacle_on_path

reroute_node (Tier 2, C++)              ★ KPP-2 owner
  ├─ publishes  → ~/obstacle_on_path, /cmd_vel
  └─ subscribes ← CostMapUpdate, FollowerTargetMessage, RobotStatus, TierStatusChange
```

Soft Kill 제외 영향: san_perception/san_surveillance 가 RF 재머 토픽 게시하지 않음. JammingCommand 메시지는 정의 유지하나 publisher 없음.
