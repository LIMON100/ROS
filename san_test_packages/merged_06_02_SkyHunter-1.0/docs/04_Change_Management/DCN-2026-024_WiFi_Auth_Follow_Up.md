# DCN-2026-024 — WiFi-based operator authentication (follow-up to DCN-023 v2)

> **Status**: **IN PROGRESS** — Option C implemented (2026-05-24)
> **Origin**: v1.5.4 cross-audit Area D finding D5 (P1 security)
> **Replaces**: PIN authentication mechanism removed by DCN-2026-023 v2
> **Document Owner**: 김태근 (PM, ㈜스카이오토넷)
> **Created**: 2026-05-24
> **Implementation**: Option C — `OperationalModeController` shared-secret token gate for DEV_TEST mode only

---

## 1. 배경

DCN-2026-023 v2 (PR #201, 2026-05-23) 는 PM 정책에 따라 BLE 채널을
완전히 제거하면서 `san_mission/operational_modes.py` 의 PIN
authentication 메커니즘 (`requires_pin` 필드, `_pin_authenticated`,
`set_pin_authenticated()`, `is_pin_authenticated()`) 도 함께 삭제했다.

PIN 메커니즘은 BLE 0xFF05 GATT challenge 의 응답 경로였고, BLE 가
사라진 뒤 dead code 였기 때문에 cleanup 자체는 정확하다.

그러나 PIN gate 가 제거되면서 **`DEV_TEST` 모드 진입 시 인증 게이트가
없어졌다**:

| 시점 | DEV_TEST 진입 |
|---|---|
| BLE 시기 | 1. BLE 0xFF05 PIN challenge 통과 → PIN flag set<br>2. `request_mode(DEV_TEST)` 가 flag 검증 후 진행 |
| BLE 제거 후 (v1.5.3) | PIN flag 가 dead — 그러나 코드는 여전히 검증 시도 (사실상 거부) |
| **DCN-023 v2 (현재)** | **PIN gate 자체 제거 — 누구나 직접 진입 가능** |

## 2. 위협 모델

- **공격 표면**: 운영자 단말 (Galaxy Tab S9) 의 WiFi 채널을 통한
  `MissionStateCommand` (또는 `request_mode` rclpy call) publish.
- **악용 시나리오**:
  1. 적대적 access — 운영자 단말이 노출/도난 시 임의 모드 변경 가능.
  2. Configuration drift — DEV_TEST 의 1.0 m/s 속도 제한이 풀려야
     할 시점이 아닌데 적용되면 운영 임무 지연.
- **현재 보호 수단**:
  - WiFi 채널 자체의 WPA3 + EasyMesh 보안 (network layer)
  - `combat_robot_msgs/EmergencyStop` 의 operator_id 필드 (audit only)
  - DCN-2026-001 D-004 — fire authorization 만 HMAC + Two-key
- **부족분**:
  - Mission state / mode change 명령의 application-layer 인증 없음
  - 운영자 단말 위변조에 대한 challenge-response 없음

## 3. 제안 방향 (3 옵션)

### Option A — Per-request HMAC (DCN-001 D-004 패턴 확장)

`MissionStateCommand` / `request_mode` 에 HMAC-SHA256 서명 필드 추가.
shared secret 은 fire authorization 과 같은 `/etc/san/mesh_secret.bin`
재활용 또는 별도 key.

- 장점: 기존 HMAC 인프라 재사용, audit log 통합 가능
- 단점: 매 명령마다 서명 — 운영자 UI 의 implementation cost

### Option B — Session token + Two-key (DCN-001 패턴 일부 차용)

운영자가 단말 부팅 시 1회 challenge-response 통과 → session token
발급 → 모든 mission command 에 token 첨부. DEV_TEST 같은 elevated
mode 는 추가 KEY1+KEY2 confirm.

- 장점: 명령마다 서명 부담 없음, elevated mode 에 추가 보호
- 단점: token revocation 메커니즘 필요, 단말 교체 시 re-issue 절차

### Option C — Mode-only gate (최소 변경)

DEV_TEST 진입만 별도 인증 (다른 mode 는 인증 없이 유지). PIN 형태가
아닌 challenge phrase 또는 hardware-pin (e.g. USB token) 사용.

- 장점: 변경 범위 최소, 기존 mission 명령 untouched
- 단점: DEV_TEST 외 다른 elevated mode 가 향후 추가될 때 또 별도 작업

## 4. 영향 모듈

| 모듈 | 변경 |
|---|---|
| `san_mission` | `operational_modes.py` (mode gate) 또는 `mission_node.py` (command path) |
| `combat_robot_msgs` | 신규 메시지 또는 기존 메시지에 auth 필드 추가 (Option A) |
| `san_operator_tools` | 단말측 UI / command publish path (모든 옵션 공통) |
| `san_fire_authorization` 의 HMAC infra | 재사용 (Option A 시) |

## 5. 결정 일정

- **PM 검토**: 2026-W22 — ✅ Option C 확정
- **Option 확정 + DCN ID 할당**: 2026-W22 — DCN-2026-024
- **구현 sprint**: 2026-W22 — **완료** (single PR)

## 6. 구현 내역 (Option C)

`san_mission/san_mission/operational_modes.py`:

- `OperationalModeController.__init__(dev_test_secret=None, dev_test_secret_path="/etc/san/dev_test_secret")`
- 비밀 해결 우선순위 (constant-time 검증):
  1. ctor 인자 `dev_test_secret` (test injection)
  2. 환경변수 `SAN_DEV_TEST_SECRET`
  3. 파일 (default `/etc/san/dev_test_secret`)
  4. 없음 → DEV_TEST fail-closed
- `request_mode(mode, auth_token="")` — DEV_TEST 시 `secrets.compare_digest(auth_token, secret)` 검증
- 다른 모드 (NARROW/RECON/WIDE/ASSAULT) 는 `auth_token` 무시 (backward compatible)

`san_mission/test/test_operational_modes.py`:

- M3b: `test_dev_test_requires_auth_token` (no-secret fail-closed / right-token / wrong-token)
- M8: `test_dev_test_max_speed_1_0` (token 통과 후 속도 cap 확인)
- M11~M15: secret resolution priority + diagnostics (`dev_test_secret_loaded()` no-leak)

향후 확장 (별도 DCN):
- nonce + timestamp 기반 challenge-response (replay 방지)
- 다른 elevated mode 추가 시 같은 패턴 재사용

## 6. Cross-refs

- DCN-2026-001 D-004 — Fire authorization (HMAC + Two-key + Audit) — 본
  패턴이 reusable
- DCN-2026-008 — BLE 패키지 제거
- DCN-2026-023 v2 (PR #201) — PIN auth dead code 제거
- v1.5.4 cross-audit Area D finding D5 (security implications)
- `CHANGELOG.md` v1.5.4 — DCN-023 v2 Security 영향 명시
