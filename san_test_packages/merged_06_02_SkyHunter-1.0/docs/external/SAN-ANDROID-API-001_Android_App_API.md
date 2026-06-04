# SAN-ANDROID-API-001 — SkyHunter Android App API

> **문서번호**: SAN-ANDROID-API-001 · **Rev**: A · **발행일**: 2026-05-26
> **Owner**: 김태근 (PM, ㈜스카이오토넷) · **Vendor**: Aban Co. (Android 단말)
> **권원/정합**: 본 문서는 `docs/external/Aban_Android_rosbridge_schema_v2.md`
> (DCN-2026-021) 와 **실제 로봇 코드**(`san_bringup/squadron.launch.xml` 의
> rosbridge 설정, `combat_robot_msgs/*.msg`, `san_external_mocks` mock 서버)
> 에서 검증한 통합 구현 레퍼런스입니다. `san_external_mocks` 의 schema
> validator(T1~T8) 가 본 계약과 코드의 정합을 CI 에서 강제합니다.
> (구 `SAN-WIFI-OPSPEC-001` 은 **superseded** — 포트/토픽이 본 문서와 다름.)

> **English**: This is the implementation contract the Android tablet
> client must build against. It is kept in sync with the robot code by the
> `san_external_mocks` schema validator. Korean is the source of truth;
> English notes follow where useful.

---

## 1. 통신 구조 / Transport

```
Android Tablet (Kotlin + OkHttp WebSocket + roslib4j/roslibjs)
   └─ Wi-Fi 6 / WPA3 (EasyMesh 192.168.50.0/24)
        └─ WiFi Router
             └─ LAN ─ Robot: rosbridge_websocket  (per-robot port)
```

- **Protocol**: rosbridge protocol **v2.0** over **WebSocket (WS, not WSS)**
- **Encoding**: **JSON** (바이너리 이미지/PNG 는 base64)
- **Client lib (권장)**: Kotlin + OkHttp WebSocket + `roslib4j`, 또는 WebView 면 `roslibjs`
- App 은 **한 번에 1개 로봇**(현재 Leader/Hub)에 연결

### 1.1 IP / Port 할당 (port = `9090 + robot_id`)

> 코드 권원: `squadron.launch.xml` → `rosbridge_port = 9090 + robot_id`.
> 실제 운용에선 로봇마다 별도 host 라 offset 은 무해, localhost dev/sim 충돌 회피용.

| Robot | robot_id | IP | WS endpoint |
|---|---|---|---|
| Leader (Go2) | 1 | 192.168.50.10 | `ws://192.168.50.10:9091` |
| Hub UGV SBC #1 | 2 | 192.168.50.20 | `ws://192.168.50.20:9092` ← **평상시 기본** |
| Hub UGV SBC #2 | 2 | 192.168.50.21 | `ws://192.168.50.21:9092` (SBC#1 failover) |
| Deputy UGV | 3 | 192.168.50.30 | `ws://192.168.50.30:9093` |
| Follower 1-5 | 4-8 | 192.168.50.40-44 | `ws://192.168.50.40-44:9094-9098` |
| **Dev / sim** | — | localhost | `ws://localhost:9091` |

### 1.2 자동 재연결 (App 측 책임)

| 항목 | 값 |
|---|---|
| `reconnect_on_close` | `true` |
| backoff | 1s → 2s → 4s → 8s → 16s → 30s → 30s … (factor 2.0, max 30s) |
| 재시도 | 무한 (App 종료까지) |

재연결 성공 시: 모든 subscription 재구성(다시 `subscribe`). rosbridge 는 새
client 세션을 발급하므로 App 이 상태를 보존해야 함.

### 1.3 접근 제어 (rosbridge glob)

rosbridge 는 아래 glob 으로 노출 토픽/서비스를 제한합니다(코드 권원:
`squadron.launch.xml`). 목록 외 토픽/서비스는 외부에서 접근 불가:

```
topics_glob:   [/operator/*, /swarm/*, /robot_*, /diagnostics/*,
                /rtk_gnss_node/*, /hub_internal/*, /gate1/*, /gun_trigger/*,
                /emergency_stop, /attack_permission, /mc/*]
services_glob: [/gate1/start_demo]
```

---

## 2. 구독 토픽 (Robot → App)

| # | Topic | Type | Rate | QoS | 상태 |
|---|---|---|---|---|---|
| 1 | `/diagnostics/robot_status_audit` | `diagnostic_msgs/DiagnosticArray` | 1 Hz | Reliable | ✅ |
| 2 | `/diagnostics/hub_slam_audit` | `diagnostic_msgs/DiagnosticArray` | 1 Hz | Reliable | ✅ |
| 3 | `/swarm/threat_alert_raw` | `combat_robot_msgs/ThreatAlert` | event | Reliable | ✅ |
| 4 | `/swarm/threat_alert_consensus` | `combat_robot_msgs/ThreatAlert` | event | Reliable | ✅ |
| 5 | `/rtk_gnss_node/heading` | `sensor_msgs/Imu` | 5 Hz | Best Effort | ✅ |
| 6 | `/hub_internal/sbc1/heartbeat` | `combat_robot_msgs/HeartBeat` ⚠ | 1 Hz | Best Effort | ✅ |
| 7 | `/hub_internal/sbc2/heartbeat` | `combat_robot_msgs/HeartBeat` ⚠ | 1 Hz | Best Effort | ✅ |
| 8 | `/swarm/poses` | `geometry_msgs/PoseArray` | 10 Hz | Reliable | ✅ |
| 9 | `/gate1/demo_status` | `std_msgs/String` | 0.2 Hz | Reliable | 🟡 future (DCN-016) |
| 10 | `/gun_trigger/simulated_fire_result` | `combat_robot_msgs/FireResult` ⚠ | event | Reliable | ✅ |

⚠ **Schema 이름 주의**: `HeartBeat`(중간 대문자, ≠ Heartbeat), `FireResult`
(≠ FireEvent). C++ `combat_robot_msgs::msg::HeartBeat`, Python
`from combat_robot_msgs.msg import HeartBeat`.

> SBC #1 vs #2 구분은 **토픽 이름**으로만(메시지 내부 `robot_id` 는 둘 다 2).

### rosbridge subscribe 예시 (JSON)
```json
{ "op": "subscribe",
  "topic": "/swarm/threat_alert_consensus",
  "type": "combat_robot_msgs/msg/ThreatAlert" }
```
이후 서버가 보내는 메시지:
```json
{ "op": "publish", "topic": "/swarm/threat_alert_consensus",
  "msg": { "severity": 2, "threat_type": 5, "message_ko": "...", ... } }
```

---

## 3. 발행 토픽 (App → Robot)

| Topic | Type | UI 컨트롤 | 상태 |
|---|---|---|---|
| `/emergency_stop` | `combat_robot_msgs/EmergencyStop` ⚠ | E-Stop 버튼 (latched) | ✅ ready |
| `/attack_permission` | `std_msgs/String` ("APPROVE"/"DENY") | Two-key 승인 | 🟡 future (백엔드 미구현) |
| `/mc/raw_command` | (TBD, DCN-019) | LOAD_PATH/START/PAUSE/RESUME/STOP | 🟡 future |

⚠ `/emergency_stop` 는 `std_msgs/Bool` 이 아니라 `combat_robot_msgs/EmergencyStop`.

### rosbridge advertise + publish 예시 (E-Stop)
```json
{ "op": "advertise", "topic": "/emergency_stop",
  "type": "combat_robot_msgs/msg/EmergencyStop" }
```
```json
{ "op": "publish", "topic": "/emergency_stop",
  "msg": {
    "header": { "frame_id": "" },
    "scope": 1,                       // SCOPE_ALL_ROBOTS
    "target_robot_id": 0,
    "reason": "Operator E-Stop button",
    "operator_id": "OP-12",
    "timestamp_ms": 1779750000000
  } }
```
> E-Stop 은 latched UI 권장(버튼 누름 유지). publish 후 로봇은
> `/rth` fallback + 60s phase watchdog 로 안전 정지(Gate-1 경로).

---

## 4. 서비스 (App → Robot)

| Service | Type | 상태 |
|---|---|---|
| `/gate1/start_demo` | `std_srvs/Trigger` | 🟡 future (DCN-016) |

```json
{ "op": "call_service", "service": "/gate1/start_demo",
  "type": "std_srvs/srv/Trigger", "args": {} }
```
> `services_glob` 가 `/gate1/start_demo` 만 허용. 그 외 모든 ROS 서비스는 차단.

---

## 5. 메시지 스키마 (실제 .msg 권원)

### 5.1 EmergencyStop (App → Robot)
```
uint8 SCOPE_SINGLE_ROBOT=0  SCOPE_ALL_ROBOTS=1  SCOPE_LEADER_ONLY=2
std_msgs/Header header
uint32 command_id, sequence
uint8  scope                # SCOPE_*
uint32 target_robot_id      # scope==SINGLE_ROBOT 일 때만
string reason, operator_id
uint64 timestamp_ms
```

### 5.2 ThreatAlert (Robot → App) — 위협 배너
```
# severity: 0 INFO / 1 WARNING / 2 CRITICAL / 3 FATAL
# threat_type: 1 SBC_FAILED · 2 BATTERY_CRITICAL · 3 RTK_LOST ·
#   4 LTE_OUTAGE · 5 DRONE_DETECTED · 6 OBSTACLE_BLOCKED ·
#   7 FIRE_AUTH_DENIED · 8 COMM_LINK_DEGRADED · 9 FOLLOWER_LOST · 99 OTHER
std_msgs/Header header
uint8  severity, threat_type
string source_robot_id, peer_id
string message_ko           # 운용자 표시용 한국어
string detail               # JSON-encoded (machine)
uint64 timestamp_ms
uint32 instance_count       # 묶인 이벤트 수
bool   has_position         # true 일 때만 아래 사용
float32 bearing_deg, elevation_deg, range_m
```
> UI: `severity >= 2` (CRITICAL) 시 modal pop-up 권장.

### 5.3 HeartBeat (Robot → App) — SBC 상태 패널
```
# role: 0 LEADER / 1 HUB / 2 FOLLOWER
# health_status: 0 HEALTHY / 1 DEGRADED / 2 CRITICAL
# operation_mode: 0 RECON / 1 COMBAT / 2 RTB / 3 DEV_TEST
# current_tier: 0..5 (T0/T1/T1.5/T2/T3/T4)
std_msgs/Header header
uint32 robot_id, sequence
uint8  role, health_status
float32 battery_percent
uint8  operation_mode, current_tier
uint64 timestamp_ms
```

### 5.4 FireResult (Robot → App) — 사격 결과 (audit)
```
# result: 0 SUCCESS / 1 MISS / 2 MALFUNCTION / 3 ABORTED /
#         4 OUT_OF_RANGE / 5 NO_AUTHORIZATION
std_msgs/Header header
uint32 robot_id, command_id, sequence
uint8  result
uint32 rounds_fired, target_id
float32 distance_to_target_m, impact_point_x_m, impact_point_y_m, confidence
string authorization_chain   # D-004 HMAC chain (audit join key)
string notes
uint64 timestamp_fire_ms, timestamp_report_ms
```

### 5.5 표준 메시지
- `/swarm/poses` → `geometry_msgs/PoseArray` (`header.frame_id` = map; `poses[]` = 활성 로봇 위치 마커. 개별 pose↔robot_id 매핑은 발행 측 규약을 별도 확인 — 현재 단일 publisher invariant(DCN-2026-013))
- `/rtk_gnss_node/heading` → `sensor_msgs/Imu` (orientation 의 yaw 만 유효; angular_velocity/linear_acceleration covariance = -1 → 무시)
- `/diagnostics/*` → `diagnostic_msgs/DiagnosticArray` (`status[].level` 0 OK/1 WARN/2 ERROR/3 STALE, `status[].values[]` key/value)
- `/gate1/demo_status` → `std_msgs/String`

---

## 6. 보안 (중요 — 정확한 현행 기준)

| 항목 | 현행 (v1.5.x) |
|---|---|
| Wi-Fi | **WPA3-Personal/Enterprise** (격리 closed-mesh 전제) |
| WebSocket | **인증 없음** — closed-mesh 가정. (v1.6 V16-02 에서 SROS2 TLS+token 예정) |
| E-Stop | `EmergencyStop.operator_id` 는 **자체신고(서명 아님)**. HMAC 서명 **아님**. |
| 발사 인증 | **HMAC-SHA256 + Two-key + nonce + audit** (`san_fire_authorization`) — 로봇 내부 경로이며 **본 App API 범위 밖**. App 의 `/attack_permission` 은 별도 백엔드(미구현)와 연결 예정. |

> **주의**: E-Stop 을 포함한 App→Robot 토픽은 현재 서명되지 않습니다. 보안은
> 네트워크(WPA3 + 격리 mesh) 가정에 의존합니다. 라우터/링크가 노출되는
> 위협 환경에서는 SROS2 도입 전까지 운영 SOP 로 보완해야 합니다.

---

## 7. 지연 목표 (KPP)

| 경로 | 목표 |
|---|---|
| Threat alert publish → UI 표시 | ≤ 2 s |
| E-Stop 버튼 → `/emergency_stop` publish | ≤ 200 ms |
| Demo status refresh | ≤ 5 s |

---

## 8. 오프라인 개발용 mock 서버

```bash
colcon build --packages-select san_external_mocks --symlink-install
source install/setup.bash
ros2 launch san_external_mocks mock_server.launch.xml   # ws://localhost:9091
```
mock 은 §2 의 10개 구독 토픽(future 2개 포함)에 합성 데이터를 발행 → App
UI 를 전체 v2 스키마로 미리 개발 가능. 실데이터는 각 DCN landing 시 활성화.

---

## 9. 참조

- `docs/external/Aban_Android_rosbridge_schema_v2.md` (DCN-2026-021) — 기계검증 권원
- `san_external_mocks/test/test_schema_validator.cpp` — spec↔mock↔launch↔glob 정합 CI
- IDS-CMD v1.5 §3.7 (EmergencyStop), §4.6 (FireResult), §5.8 (HeartBeat), §5.18 (ThreatAlert)
- `squadron.launch.xml` — rosbridge port/glob (코드 권원)

---

**Document Owner**: 김태근 (PM, ㈜스카이오토넷) · Rev A · 2026-05-26
