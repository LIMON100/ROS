# SAN-WIFI-OPSPEC-001 — Android App External Spec

**문서번호**: SAN-WIFI-OPSPEC-001
**발행일**: 2026-05-14
**Rev**: A
**관련 통제**: DCN-2026-008 v2 (BLE removal + WiFi reconnect)
**적용 대상**: SkyHunter v1.5.2 이후
**발주처**: (별도 협력사 — Android App 외부 개발)
**보안 분류**: 회사 비밀

---

## 0. 목적

SkyHunter 군집 로봇 시스템의 운용병사가 사용하는 **Android 태블릿 App** 의 외부 발주 spec.
본 spec 은 App ↔ Robot 통신 계약 (transport / topic / QoS / 재연결 / UI 요구) 을 규정하고, App 측 구현 범위 (이 spec 외부) 와 Robot 측 구현 범위 (이 spec 내부 — 이미 v1.5.2 에 통합) 를 명확히 분리한다.

---

## 1. 통신 구조

```
┌─────────────────────────────┐       ┌──────────────┐      ┌──────────────┐
│ Android App                 │       │              │      │ Leader Robot │
│ (Galaxy Tab S9 / rugged tab)│──Wi-Fi│ WiFi Router  │──LAN─│  (Go2 또는    │
│                             │  6    │ (TP-Link 등) │  /   │   Hub UGV)   │
│ - roslibjs / roslib4j       │ WPA3  │              │  6E  │              │
│ - WebSocket client          │       │              │      │ rosbridge_   │
│                             │       │              │      │ server :9090 │
└─────────────────────────────┘       └──────────────┘      └──────────────┘
```

- App 은 한 번에 1개 Robot 에 연결 (현재 Leader)
- Robot 측 연결점: `rosbridge_websocket` on port **9090** (`san_bringup/launch/squadron.launch.py`)
- Transport: **WebSocket** (rosbridge protocol v2.0)
- Encoding: **JSON** (binary 영상은 base64)

## 2. 권장 client 라이브러리

| 언어 | 라이브러리 | 비고 |
|---|---|---|
| **Kotlin native** | `roslib4j` 또는 동등한 OkHttp WebSocket | 권장 — Android 표준 |
| JavaScript / WebView | `roslibjs` | App 이 WebView UI 이면 사용 가능 |

## 3. 자동 재연결 정책 (App 측 책임)

App 은 다음 정책으로 끊김 후 자동 재연결을 수행한다:

| 항목 | 값 |
|---|---|
| `reconnect_on_close` | `true` |
| 초기 backoff | **1s** |
| Exponential factor | **2.0** |
| 최대 backoff | **30s** |
| Sequence | 1s → 2s → 4s → 8s → 16s → 30s → 30s → ... |
| 재시도 횟수 | 무한 (App 종료까지) |

재연결 성공 시 App 동작:
1. 모든 ROS 토픽 subscription 재구성 (각 토픽 다시 subscribe).
2. `session_id` 동일 유지 (`OperatorHeartbeat.session_id` 필드).
3. 마지막 발행 `command_id` (FormationCommand 등) 보존 — Robot 의 `RobotStatus.last_received_command_id` 와 echo 검증으로 중복 발행 방지.

Robot 측 grace
- `mission_node` 의 `operator_timeout` 은 **≥ 30s** 로 설정 (App reconnect burst 흡수).
  - 현 v1.5.2: `OperatorHeartbeat` 의 rclpy 소비자 구현 자체가 Phase 7 deferred. 구현 시 30s 이상으로 설정해야 한다 (DCN-2026-008 v2 D-WIFI-002 launch comment 에도 명시).

## 4. 발행 토픽 (App → Robot)

| 토픽 | 메시지 | 주기 / 이벤트 | QoS |
|---|---|---|---|
| `/operator/heartbeat` | `combat_robot_msgs/OperatorHeartbeat` | **1 Hz** (deadman) | BEST_EFFORT, KEEP_LAST(5) |
| `/operator/formation_command` | `combat_robot_msgs/FormationCommand` | event | RELIABLE |
| `/operator/manual_override` | `combat_robot_msgs/ManualOverrideCommand` | event | RELIABLE |
| `/operator/mission_state_command` | `combat_robot_msgs/MissionStateCommand` | event | RELIABLE |
| `/operator/emergency_stop` | `combat_robot_msgs/EmergencyStop` | event | **TRANSIENT_LOCAL** (durable — 끊김 후 재연결 시 즉시 전달) |

`OperatorHeartbeat.msg` 의 필드:
- `sequence: uint32` (단조 증가)
- `operator_id: string` (예: "OP-12")
- `session_id: string` (UUID — Leader 승계 후에도 유지)
- `last_command_id: uint32` (App 이 마지막 발행한 command_id)
- `tablet_battery_percent: float32` (0-100)
- `uplink_rssi_dbm: int16` (-120 ~ 0)
- `operator_alive: bool` (항상 `true`; absence = dead)
- `timestamp_ms: uint64`

## 5. 구독 토픽 (Robot → App)

| 토픽 | 메시지 | 주기 | QoS |
|---|---|---|---|
| `/swarm/fleet_status` | `combat_robot_msgs/FleetStatus` | 1 Hz | RELIABLE, KEEP_LAST(1) |
| `/swarm/threat_alert` | `combat_robot_msgs/ThreatAlert` | event | RELIABLE |
| `/robot_<id>/status` | `combat_robot_msgs/RobotStatus` | 5 Hz | BEST_EFFORT |
| `/robot_<id>/internal_camera` | `sensor_msgs/CompressedImage` | 가변 (~2 Hz) | BEST_EFFORT |

rosbridge_server 의 `topics_glob` 은 `[/operator/*, /swarm/*, /robot_*]` 로 제한된다. 위 목록 외 토픽은 외부에서 접근 불가.

## 6. 서비스 호출

**서비스 노출 안 됨.** `rosbridge_server` 의 `services_glob: "[]"` 로 모든 ROS 2 서비스가 차단되어 있다. App 은 service call 을 시도하지 않는다.

발사 인증 / 무장 해제 등은 모두 **토픽 + HMAC** 모델 (`san_fire_authorization`) 로 처리된다 — 본 spec 범위 외.

## 7. 보안

| 항목 | 현재 (v1.5.2) | 향후 (v1.6 V16-02 SROS 2) |
|---|---|---|
| Wi-Fi 인증 | **WPA3-Personal** (또는 WPA3-Enterprise) | 동일 |
| WebSocket 인증 | **없음** — closed-mesh 가정 | TLS + token (SROS 2) |
| 메시지 서명 | `EmergencyStop` / 발사 인증은 HMAC | 모든 명령 HMAC + nonce |
| 운용자 신원 | `OperatorHeartbeat.operator_id` (자체신고) | mTLS / hardware-backed token |

**현재의 보안 가정**: 운용 환경이 격리된 closed mesh 라는 전제. WiFi 라우터가 적군 침투 위협 영역과 분리된다. 이 가정이 깨지면 (예: 라우터 노출) SROS 2 도입 전까지 운영 SOP 로 보완.

## 8. UI 요구 사항

| 요소 | 요구 |
|---|---|
| 연결 상태 표시 | **CONNECTED / RECONNECTING / DISCONNECTED** 명확한 시각화 (색상 + 아이콘) |
| 마지막 응답 시각 | 마지막 `OperatorHeartbeat` echo (Robot 의 `RobotStatus.last_received_command_id`) 수신 시각 |
| 끊김 경고 | 30s 이상 disconnect 시 사용자 음성 / 진동 alert |
| Reconnect 카운터 | "재연결 중 (N회)" 정도의 디버그 정보 (개발자 모드) |
| 운용자 ID 입력 | App 첫 실행 시 `operator_id` 입력 / 저장 — `OperatorHeartbeat` 에 첨부 |

## 9. 외부 발주 가이드

- **발주처**: 별도 협력사 (스카이오토넷 영업팀이 별도 선정)
- **개발 기간 추정**: 5-7 영업일
  - Kotlin + OkHttp + roslib4j (또는 동등)
  - 자동 재연결 backoff 단순 (지수, 무한 재시도)
  - UI 4-5 화면 (Connection / Fleet / Map / Detail / Emergency)
- **사전 통합 시험**: PC 측 dummy ROS 2 publisher 로 App 의 토픽 발행/구독 검증
- **납품 결과물**:
  1. APK + AAB
  2. 소스 (Kotlin) — 인수 후 스카이오토넷 보유
  3. 통합 시험 보고서 (PC dummy 발행자 사용)
  4. 사용자 매뉴얼 (운용자 PIN 입력, 연결 절차, 끊김 시 행동)

## 10. App 측 책임 vs Robot 측 책임 (분명히 분리)

| 항목 | App (외부 발주) | Robot (v1.5.2 본 sprint 완료) |
|---|---|---|
| WebSocket 연결 | ✓ roslibjs / roslib4j | ✓ `rosbridge_server` :9090 |
| 자동 재연결 | ✓ exponential backoff | — (rosbridge 가 새 client_id 발급) |
| Subscription 재구성 | ✓ 재연결 후 다시 subscribe | — |
| Session continuity | ✓ `session_id` 유지 | (Phase 7) `mission_node` 의 OPERATOR_LOST grace ≥30s |
| 토픽 메시지 정의 | (구독자) — schema 변경 시 알림 | ✓ `combat_robot_msgs/*.msg` |
| 보안 (WPA3) | (네트워크 인증) | (네트워크 인프라) |
| 명령 HMAC | ✓ `EmergencyStop` / 발사 인증 | ✓ `san_fire_authorization` |

## 11. 검증 (PM + Sales 사전 review)

- [ ] PM 김태근 review
- [ ] Sales review (외부 발주 적합성)
- [ ] 발주처 선정 후 spec 전달
- [ ] 발주처 측 통합 시험 결과 인수

## 12. 부록 — 변경 이력

| Rev | 일자 | 변경 |
|---|---|---|
| A | 2026-05-14 | 최초 발행 — DCN-2026-008 v2 D-WIFI-003 (BLE removal 동반) |

---

— 끝 —
