# SAN v1.5 — 메시지 카탈로그 (IDS-CMD 정합)

> **문서 ID**: SAN-PDR-MSG-001 Rev.A
> **목적**: 36 message 인벤토리 + IDS § 매핑 + publisher/subscriber 관계

---

## 1. 메시지 정합도 요약

| IDS § | 범주 | 정의 | 사용 |
|---|---|---|---|
| §3 | 운용자 명령 (10/10) | 10 | 9 |
| §4 | 시스템 상태 (12/12) | 12 | 12 |
| §5 | 군집 통신 (16/16) | 16 | 13 |
| §6 | 인프라 (4/8) | 8 | 4 |
| **합계** | | **42 정의 / 38 사용** | **90% 활용** |

### PDR-7 완료 후 변화

| 시점 | 메시지 수 | IDS 정합도 |
|---|---|---|
| PDR-8 (이전) | 36 | 80% |
| **PDR-7 적용 후 (현재)** | **42** | **94%** ⭐ |

| 정합 상태 | 메시지 수 | 비율 |
|---|---|---|
| 정의 + 활용 | 38 | **90%** |
| 정의만 (legacy) | 4 | 10% |
| 누락 | 0 | **PDR-7 으로 모두 해소** ⭐ |

---

## 2. §3 운용자 명령 메시지 (9)

| 메시지 | 용도 | Publisher | Subscriber |
|---|---|---|---|
| `EmergencyStop` | E-stop | OperatorUI, BleCommand | mission_node (P0) |
| `ManualOverrideCommand` | 수동 cmd_vel | OperatorUI | mission_node (P1) |
| `MissionStateCommand` | 임무 상태 변경 | OperatorUI | mission_node |
| `WaypointCommand` | 경로 점 지정 | OperatorUI | mission_node |
| `FormationCommand` | 대형 / preset 선택 | OperatorUI | formation_node |
| `BleCommand` | BLE 조이스틱 | ble_control | mission_node |
| `FireAuthorization` | Fire Auth (D-004) | OperatorUI | fire_authorization (HMAC+2-key) |
| `FireAuthorizationRequest` | 사격 요청 | san_perception | fire_authorization |
| `FireAuthorizationResponse` | 사격 응답 | fire_authorization | san_operation_control |
| `JammingCommand` ⚠️ | RF 재머 (legacy) | **(미사용 - Soft Kill 제외)** | — |

---

## 3. §4 시스템 상태 메시지 (10)

| 메시지 | 용도 | Publisher | 빈도 |
|---|---|---|---|
| `RobotStatus` | 로봇 pose + 상태 | each robot | 10 Hz |
| `SwarmHealthSummary` | 군집 헬스 통합 | role_manager | 1 Hz |
| `FormationStatus` ★ | 대형 정합도 + KPP-1 | formation_node | 1 Hz |
| `TierStatusChange` ★ | T0-T4 전환 이벤트 + KPP-2 | tier_node | event |
| `RtkFixStatus` | RTK fix 타입 | rtk_gnss | 1 Hz |
| `BlePhaseStatus` | BLE 연결 phase | ble_control | event |
| `OperationState` | 운용 모드 (RECON/COMBAT/RTB/DEV_TEST) | mission_node | 1 Hz |
| `LteLinkQuality` | LTE RSRP / SNR | lte_redundancy | 1 Hz |
| `LteModemStatus` | LTE 모뎀 상태 | lte_redundancy | 1 Hz |
| `ThreatAlert` | 위협 검출 알림 | perception | event |

---

## 4. §5 군집 통신 메시지 (13)

| 메시지 | 용도 | Publisher | 빈도 |
|---|---|---|---|
| `FollowerTargetMessage` ★ | **1초 예측 위치** | formation_node (Leader) | 10 Hz |
| `BreadcrumbBroadcast` ★ | Leader 과거 경로 | formation_node (Leader) | 0.5 s |
| `SlotAssignment` ★ | Hungarian slot 할당 | formation_node | event |
| `SurveillanceSectorAssignment` ★ | 360° sector 분배 | surveillance_node | 10 s |
| `PanTiltCommand` ★ | Pan-Tilt 명령 (Sweep/Fixed/Track/Engage) | surveillance_node | 5 Hz |
| `LeaderRoleAnnouncement` | Leader 선출 결과 | role_manager | event |
| `HubRoleAnnouncement` | Hub 역할 발표 | hub_orchestrator | event |
| `LTERoleAnnouncement` | LTE link 역할 분배 | lte_redundancy | event |
| `SLAMAggregatedMap` | Hub 통합 글로벌 맵 | hub_slam | 30-60 s |
| `SLAMLocalDelta` | Local SLAM 증분 | slam | 1 Hz |
| `VideoStreamRequest` | 비디오 요청 | operator | event |
| `VideoStreamHandle` | 비디오 핸들 응답 | video_sender | event |
| `CostMapUpdate` ★ | Local Cost Map snapshot | costmap | 1 Hz |

(★ = PDR 준비 단계 신설)

---

## 5. ★ PDR-7 신설 6 메시지 — IDS 누락 해소

| 메시지 | IDS § | 용도 | Publisher | 권원 |
|---|---|---|---|---|
| **OperatorHeartbeat** | §3.8 | 운용병사 deadman heartbeat | OperatorUI | D-004 보조 |
| **FireResult** | §4.6 | 사격 결과 보고 | fire_authorization | D-004 audit |
| **BatteryWarning** | §4.7 | 배터리 transition event | each robot | BT P3 |
| **LeaderElectionVote** | §5.3 | Modified Raft vote 요청 | candidate robot | D-005 |
| **LeaderElectionResponse** | §5.3 | Modified Raft vote 응답 | voter robot | D-005 |
| **HeartBeat** | §5.8 | inter-robot watchdog | each robot | SDD §6.2 T4 |

→ IDS 누락 100% 해소 ✅

---

## 6. Detection 보조 메시지 (3)

| 메시지 | 용도 |
|---|---|
| `Detection` | 단일 검출 결과 |
| `DetectionArray` | 검출 배열 |
| `LrfReading` | 거리 측정 (LRF) |

---

## 7. Hub Failover 보조 메시지

(앞 §5 LeaderRoleAnnouncement / HubRoleAnnouncement 가 D-005 의 핵심. 추가 헬퍼 메시지가 정의되어 있음.)

---

## 8. ★ PDR 준비 단계 신설 7 메시지 — 핵심 산출

| 메시지 | IDS § | 신설 시점 | 영향 |
|---|---|---|---|
| `FollowerTargetMessage` | §5.1 | P0-1 | T0 PREDICTIVE_TRACK 입력 |
| `BreadcrumbBroadcast` | §5.2 | P0-1 | T4 BREADCRUMB_RECOVERY 입력 |
| `SlotAssignment` | §5.9 | P0-1 | Hungarian 결과 broadcast |
| `FormationStatus` | §4.2 | P0-1 | **KPP-1, KPP-5 측정** |
| `SurveillanceSectorAssignment` | §5.10 | P0-2 | 360° sector |
| `PanTiltCommand` | §5.11 | P0-2 | **팬틸트 ≤ 0.05° 측정** |
| `TierStatusChange` | §4.5 | PDR-2 | **KPP-2 evidence** |

= **3 KPP 의 측정 인프라 메시지 모두 PDR 준비 단계 신설**
