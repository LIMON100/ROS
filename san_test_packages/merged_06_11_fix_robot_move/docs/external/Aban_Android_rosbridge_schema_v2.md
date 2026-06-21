# SkyHunter Android App ↔ ROS Bridge Schema v2.0 / Aban 발주 사양

> **Document Owner**: 김태근 (PM, ㈜스카이오토넷)
> **Date**: 2026-05-23
> **Vendor**: Aban Co. (Android 운영병사 단말 개발)
> **DCN**: DCN-2026-021
> **Mock implementation**: `ros/src/.../san_external_mocks/` (이 repo 내)
> **Previous version**: v1.0 (v1.5.1 baseline)
> **Bilingual note**: Korean is the source of truth; English summary
> follows each major section for Aban developer reference.

---

## 1. Connection / 연결

  - **Protocol**: rosbridge_websocket v2.0 (WS, not WSS in tactical
    field — operate over WPA3-secured EasyMesh, TLS handled at link
    layer)
  - **Subnet**: 192.168.50.0/24 (EasyMesh, WiFi 6 + WPA3)
  - **WPA3 credentials**: 별도 전달 (operations 핸드오프 시점)

### 1.1 IP / port allocation per robot

Port = **9090 + robot_id** (DCN-2026-014 v2 D-041 다중-robot localhost
충돌 회피 룰). 실제 운용에서는 각 robot 이 별도 host 이므로 offset 은
무해; localhost dev/sim 에서는 충돌 회피에 essential.

| Robot | robot_id | IP | rosbridge port |
|---|---|---|---|
| Leader Go2 | 1 | 192.168.50.10 | **9091** |
| Hub UGV SBC #1 | 2 (sbc_id=1) | 192.168.50.20 | **9092** |
| Hub UGV SBC #2 | 2 (sbc_id=2) | 192.168.50.21 | **9092** (same robot_id) |
| Deputy UGV | 3 | 192.168.50.30 | **9093** |
| Follower 1-5 | 4-8 | 192.168.50.40-44 | **9094-9098** |
| Aban tablet | — | 192.168.50.100 | (client) |

> **Aban 권고**: 평상 시 **Hub UGV SBC #1 (9092)** 에 연결. SBC #1
> failover 시 SBC #2 도 같은 robot_id/port 로 publish 하므로 IP 만
> 192.168.50.21 로 전환하면 됨.

**English**: Default WebSocket endpoint = `ws://192.168.50.20:9092`
(Hub UGV SBC #1). Failover to `ws://192.168.50.21:9092` (SBC #2) on
Hub primary loss. Dev = `ws://localhost:9091`.

---

## 2. Topics to Subscribe (Android ← robot)

### 2.1 v1.5.2 production-ready topics (8)

| Topic | Type | Rate | QoS | UI element |
|---|---|---|---|---|
| `/diagnostics/robot_status_audit` | `diagnostic_msgs/DiagnosticArray` | 1 Hz | Reliable | Audit panel |
| `/diagnostics/hub_slam_audit` | `diagnostic_msgs/DiagnosticArray` | 1 Hz | Reliable | SLAM panel |
| `/diagnostics/hub_slam_loop_closure` | `diagnostic_msgs/DiagnosticArray` | ~0.1 Hz (every `loop_closure_period_sec`, default 10 s) | Reliable | SLAM panel — inter-robot alignment status |
| `/swarm/threat_alert_raw` | `combat_robot_msgs/ThreatAlert` | event-driven | Reliable | Threat banner (raw) |
| `/swarm/threat_alert_consensus` | `combat_robot_msgs/ThreatAlert` | event-driven | Reliable | Confirmed threat banner |
| `/rtk_gnss_node/heading` | `sensor_msgs/Imu` | 5 Hz | Best Effort | Heading indicator |
| `/hub_internal/sbc1/heartbeat` | `combat_robot_msgs/HeartBeat` ⚠ | 1 Hz | Reliable | SBC #1 status |
| `/hub_internal/sbc2/heartbeat` | `combat_robot_msgs/HeartBeat` ⚠ | 1 Hz | Reliable | SBC #2 status |
| `/swarm/poses` | `geometry_msgs/PoseArray` | 10 Hz | Reliable | Robot positions on map |

⚠ **Schema note** — 메시지 타입 이름은 **`HeartBeat`** (CamelCase 가 단어 사이 대문자). 일반적인 영어 표기 `Heartbeat` 와 다름. C++ 에서는 `combat_robot_msgs::msg::HeartBeat`, Python 에서는 `from combat_robot_msgs.msg import HeartBeat` 로 import.

#### HeartBeat 주요 필드 (UI 가 사용)

| Field | Type | 의미 |
|---|---|---|
| `header.stamp` | `builtin_interfaces/Time` | 발행 시점 |
| `robot_id` | uint32 | 어느 로봇 (Hub = 2) |
| `sequence` | uint32 | 단조 증가 — packet loss 추적 |
| `role` | uint8 | `ROLE_LEADER`(0) / `ROLE_HUB`(1) / `ROLE_FOLLOWER`(2) |

> **Aban 권고**: SBC #1 vs #2 구분은 토픽 이름으로만 결정 (메시지
> 내부의 robot_id 는 둘 다 2). Topic-level routing.

### 2.2 v1.5.3 future topics (2 — 백엔드 미구현, mock 만 발행)

다음 2개 topic 은 **현재 backend 없음**. Aban 은 mock 으로 UI 를 미리
빌드할 수 있으나, 실제 운영 환경에서는 아래 DCN landing 까지 데이터
없음:

| Topic | Type | Rate | QoS | Backend DCN | 예정 |
|---|---|---|---|---|---|
| `/gate1/demo_status` | `std_msgs/String` | 0.2 Hz | Reliable | DCN-2026-016 (gate_demo_orchestrator) | Gate-1 직전 |
| `/gun_trigger/simulated_fire_result` ⚠ | `combat_robot_msgs/FireResult` ⚠ | event | Reliable | (이미 sim 가능 — FireAuthorization → FireResult 체인) | 즉시 가능 |

⚠ **Schema note** — optionA 초안의 `FireEvent` 는 실재하지 않음. 실제
타입은 **`FireResult`** (사격 후 보고). 토픽 이름도 그에 맞추어
`/gun_trigger/simulated_fire_result` 로 변경. `combat_robot_msgs::msg::FireResult` 사용.

#### FireResult 주요 필드

| Field | Type | 의미 |
|---|---|---|
| `header.stamp` | `builtin_interfaces/Time` | 보고 시점 |
| `robot_id` | uint32 | 사격한 로봇 |
| `command_id` | uint32 | FireAuthorization.command_id echo (audit join key) |
| `result` | uint8 | `RESULT_SUCCESS`(0) / `RESULT_MISS`(1) / `RESULT_MALFUNCTION`(2) / `RESULT_ABORTED`(3) / `RESULT_OUT_OF_RANGE`(4) / `RESULT_NO_AUTHORIZATION`(5) |
| `rounds_fired` | uint32 | 발사 발수 |
| `target_id` | uint32 | san_perception tracker ID |
| `distance_to_target_m` | float32 | |
| `confidence` | float32 | 0.0 ~ 1.0 |
| `authorization_chain` | string | D-004 HMAC chain (audit) |

---

## 3. Topics to Publish (Android → robot)

| Topic | Type | UI 컨트롤 | 백엔드 DCN | 상태 |
|---|---|---|---|---|
| `/attack_permission` | `std_msgs/String` | Two-key 승인 ("APPROVE" / "DENY") | (TBD) | 🟡 future |
| `/emergency_stop` | `combat_robot_msgs/EmergencyStop` ⚠ | E-Stop button (latched) | DCN-2026-007 | ✅ ready |
| `/mc/raw_command` | (TBD) | LOAD_PATH/START/PAUSE/RESUME/STOP | DCN-2026-019 | 🟡 future (MCMessage type 미정) |
| `/gate1/start_demo` (service) | `std_srvs/Trigger` | Demo start | DCN-2026-016 | 🟡 future |

⚠ **Schema note** — `/emergency_stop` 는 spec 초안의 `std_msgs/Bool`
이 아니라 `combat_robot_msgs/EmergencyStop` (scope + reason + operator_id
필드 포함; v1.5 IDS §3.7). Aban 은 button press 시:
```
EmergencyStop msg:
  scope: SCOPE_ALL_ROBOTS (1)
  reason: "Operator E-Stop button"
  operator_id: <login id>
  stamp: now
```

**English**: `/emergency_stop` uses `combat_robot_msgs/EmergencyStop`
(not `std_msgs/Bool`) — scope + reason + operator_id required. See
v1.5 IDS §3.7.

---

## 4. UI Mockup Requirements (요건)

Aban 디자인 산출물 (별도 docx 로 전달):
  - Audit panel: 1 Hz refresh, severity color coding (OK/WARN/ERROR/STALE)
  - SLAM panel: 동일한 1 Hz, Hub 기준 aggregate
    - 추가 입력 `/diagnostics/hub_slam_loop_closure` (~0.1 Hz): 로봇 간 SLAM 정렬(loop-closure) 상태. `loop_closure/summary` status 의 KeyValue 로 `overlapping_pairs` / `confident_pairs` / `applied_pairs` / `enabled` 를, 페어별 `loop_closure/<a>_<b>` status 로 `best_score` / `correction_dx_m` / `correction_dy_m` / `correction_dtheta_deg` / `confident` / `applied` 를 노출. 보정이 비활성(`enabled=false`)인데 `confident_pairs>0` 이면 해당 status 가 WARN — "정렬 보정 가능하나 미적용" 표시 권장.
  - Threat banner: SEVERITY_CRITICAL (2) 이상 시 modal pop-up
  - Heading indicator: 5 Hz smooth (animation interpolation 권고)
  - Map: 10 Hz PoseArray → marker per pose
  - Demo timeline: state machine view, v1.5.3 future (mock 으로 dev)

---

## 5. Authentication

  - Operator login: 별도 Auth 모듈 (TBD)
  - Role-based permission: APPROVE 권한은 통제관 (sergeant+) 만
  - Two-key fire authorization: 운용병사 + 통제관 동시 승인 시에만
    `/attack_permission = APPROVE`

---

## 6. Latency Targets

| Path | Target |
|---|---|
| Threat alert publish → UI display | ≤ 2 sec |
| E-Stop button press → `/emergency_stop` publish | ≤ 200 ms |
| Demo status refresh | ≤ 5 sec |

---

## 7. Versioning

| Version | Date | Notes |
|---|---|---|
| **v2.0** | 2026-05-23 | This document. Adds 8 v1.5.2 topics + 2 v1.5.3 future. Corrects schema names (`HeartBeat` not `Heartbeat`; `FireResult` not `FireEvent`; `EmergencyStop` not `std_msgs/Bool`) from optionA draft. |
| v1.0 | (v1.5.1 baseline) | Initial schema |

### Breaking changes from v1.0
- 8 new subscribe topics
- 2 future subscribe topics (mock-only until DCN-016 lands)
- Publish: `/emergency_stop` type corrected to `combat_robot_msgs/EmergencyStop` (was `std_msgs/Bool`)

### Schema name corrections (from optionA draft)
| Draft (incorrect) | Actual (use this) |
|---|---|
| `combat_robot_msgs/Heartbeat` | `combat_robot_msgs/HeartBeat` ⚠ |
| `combat_robot_msgs/FireEvent` | `combat_robot_msgs/FireResult` |
| `std_msgs/Bool` (E-Stop) | `combat_robot_msgs/EmergencyStop` |
| `/gun_trigger/simulated_fire_event` | `/gun_trigger/simulated_fire_result` |

---

## 8. Mock server (for offline dev)

```bash
# In SkyHunter-1.0 dev env:
colcon build --packages-select san_external_mocks --symlink-install
source install/setup.bash
ros2 launch san_external_mocks mock_server.launch.xml
# Aban app → ws://localhost:9091
```

Mock publishes synthetic data on ALL 10 subscribe topics (including
the 2 future ones) so UI dev can proceed against the full v2 schema.
Production receive activates as each DCN lands.

---

## 9. References

- DCN-2026-021 — this spec
- DCN-2026-014 v2 D-041 — rosbridge port = 9090 + robot_id
- DCN-2026-013 — `/swarm/poses` Hub-only gate (single publisher invariant)
- DCN-2026-016 — gate_demo_orchestrator (backend for `/gate1/*`, future)
- DCN-2026-019 — MC retransmit + `/mc/raw_command` (future)
- IDS-CMD v1.5 §3.7 — EmergencyStop schema
- IDS-CMD v1.5 §4.6 — FireResult schema
- IDS-CMD v1.5 §5.8 — HeartBeat schema
- SDD-SWARM v1.5 §5 — communication stack
- `[[ADR-008]]` — Tier-based language policy (mock is Tier 3, C++)

---

**Document Owner**: 김태근 (PM, ㈜스카이오토넷)
*Date: 2026-05-23 v2 (DCN-2026-021)*
