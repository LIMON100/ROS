# DCN-2026-026 — 위협 대응 행동 확장 (다중 위협 추적 · Encircle 기동 · 교전 합의 투표)

> **Status**: **APPROVED (ratified)** — PM 승인 2026-06-10 (김태근)
> **Origin**: `limon/features_gazebo_sim` 커밋 `defbb64` 리뷰(2026-06-10) — PR #259 에서 **제외**된 3개 기능의 재설계 제안
> **Extends**: DCN-2026-010 D-028 (Detection→Threat) · DCN-2026-025 (track_id) · SDD-SWARM v1.5 §8 (Surveillance) · §6 (Formation)
> **Document Owner**: 김태근 (PM, ㈜스카이오토넷)
> **Created**: 2026-06-10
> **Implementation**: C-1 → C-2 → C-3 순 분리 PR (C-1 착수 2026-06-10)
>
> **비준 시 확정된 결정 사항**:
> - **C-2 운용자 확인**: 기본 **확인 필수**(App 1-tap) — 자동 개시는 `encircle_auto` opt-in (기본 off)
> - **C-3 합의 기준**: **k=2** (k-of-n, 1.5 s 윈도) 확정

---

## 1. 배경

`limon/features_gazebo_sim` 의 perception 커밋(defbb64)은 위협 검출 이후의
**대응 행동** 3종을 함께 도입했으나, 2026-06-10 머지 리뷰에서 아래 사유로
PR #259 수용분(thermal fusion / geolocation / pan_tilt_driver)에서 제외되었다:

| 기능 | 제외 사유 (리뷰 판정) |
|---|---|
| C-1 sector_allocator 위협할당 변경 | SDD §8.6.1/§8.2 위반 — 팬틸트 없는 Leader 포함 **전 로봇**을 추적으로 전환, 감시 union coverage 14%(기준 ≥80%), `test_surveillance` A7/A10 FAIL |
| C-2 formation Encircle 기동 | 트리거 무필터(아무 positional alert 에 발동) + 신고로봇 상대좌표를 Leader pose 에 합산(**오지점 포위**) + TTL 부재(영구 latch) — 자율 교전성 행동인데 권원 부재 |
| C-3 FireSolution 교전 합의 투표 | `/swarm/target_confirmations` 발행자 0(end-to-end 미완성), 평문 UInt32 투표(인증 없음), fire-authorization 체인 밖 `engage_ready` 시맨틱, IDS 미등록 |

세 기능의 **운용 의도 자체는 유효**하다(다중 위협 동시 추적, 위협 포위 기동,
다중 로봇 교차확인 후 교전 준비). 본 DCN 은 SDD/IDS/보안 체계와 정합하는
재설계 요건을 명세하고, 항목별 구현을 권원화한다.

## 2. 변경 내역 (제안)

### C-1. sector_allocator 다중 위협 추적 (재설계)

원안(defbb64)의 "다중 위협(최대 2, 15° 병합) 지원" 의도는 수용하되,
할당 정책을 SDD 준수로 교정한다:

- **추적 전환 대상은 follower 만** — Leader 는 SDD §8.2 상 팬틸트 미장착
  (MODE_FIXED 전방 ±30° 고정), Hub 는 통신 허브 역할 유지. 원안의
  "전 로봇 ±25° MODE_TRACK" 제거.
- **위협당 최근접 follower ≤ 2 대, 전체 추적 전환 ≤ 3 대**(기존
  THREAT_FOLLOWER_COUNT 보존) — 위협 2 개 동시에도 perimeter 잔류
  follower 로 **union coverage ≥ 80%** (TST A10 기준) 유지. 섹터
  재분배는 잔류 로봇 기준으로 재계산.
- **위협별 상태 분리**: 원안의 fire-rate 추정기(`fire_prev_bearing_`/`_ts_`)
  스칼라 공유 → 위협 ID 키 map 으로 교체(2 위협 교차 alert 시 각속도
  추정 오염 제거). 위협 ID 는 bearing 15° 병합 클러스터에 부여.
- **`state_mu_` 락 스코프 교정** — 원안에서 brace 오정렬로 publish 블록이
  락 밖으로 빠진 잠재 race 제거.
- **테스트**: 기존 A7(3대 한정)/A10(≥80%) 시나리오 **무수정 통과** +
  다중위협 신규 시나리오(A11: 2위협 → 추적 3 대·coverage ≥80% 확인) 추가.

### C-2. formation Encircle 기동 (재설계)

- **트리거 게이트** (모두 충족 시에만 발동):
  1. `severity ≥ CRITICAL` **그리고** `threat_type ∈ {DRONE_DETECTED,
     OTHER(person)}` — battery/comm 등 시스템 알림 배제,
  2. `confidence ≥ 임계(파라미터, 기본 0.9)`,
  3. **운용자 확인**(App "포위 승인" 1-tap) — 자율 기동이되 개시는 유인
     결심. KPP/Demo 시나리오 상 자동 개시가 필요하면 별도 모드 플래그
     (`encircle_auto`) 로 opt-in, 기본 off.
- **표적 좌표 정규화**: ThreatAlert 의 bearing/range 는 **신고 로봇 기준**
  (DCN-2026-010). Hub `threat_aggregator` 가 신고 로봇 pose + bearing/range
  로 **world 좌표를 계산해 집계 알림에 실어 재발행** → formation 은 집계
  알림(`/hub/threat_alert`)만 소비. 원안의 "Leader pose + 신고로봇 상대각"
  합산(오지점 포위) 제거.
- **수명 관리**: TTL(기본 10 s, surveillance 와 동일 관례) + 갱신 시 연장,
  히스테리시스(해제 후 5 s 재진입 금지)로 모드 플리커 방지. 비위치성
  알림이 combat 상태를 해제하지 않도록 해제 조건을 TTL 만료·운용자
  해제·위협 소실 3종으로 한정.
- **슬롯 기하**: `encircle()` 의 균등 분할(2π·i/n)은 유지하되 **n = follower
  수**(Leader 는 perimeter 슬롯 미부여 — 원안은 Leader 슬롯이 영구 공백).
  combat 진입 시 즉시 재계획(원안은 정렬오차 2 m 초과 시에만 발동 →
  정상 대형에서 영구 미발동), KPP-1 정렬오차·재계획 기준 anchor 를
  combat anchor 로 일원화.
- `SlotAssignment.slot_type` 은 **소비자 설계와 함께** IDS §5.9 에 정의
  (shooter/blocker/observer enum + 소비 노드 명세). 소비자 없는 필드
  추가는 철회.

### C-3. FireSolution 교전 합의 투표 (신규 설계)

- **투표 주체·바인딩**: 위협을 **독립 검출한 로봇의
  `detection_to_threat`** 가 투표 발행. 투표는 표적에 바인딩 —
  `track_id`(DCN-2026-025) + bearing 클러스터 ID 를 담은 **전용 msg
  `TargetConfirmation`** 신설(원안의 평문 `UInt32` 폐기).
- **인증**: `TargetConfirmation` 에 `robot_id + nonce + HMAC-SHA256`(mesh
  shared secret — fire-authorization 과 동일 키 체계) 포함. 재전송 방지
  nonce sliding-window 는 기존 fire-auth 구현 재사용.
- **합의 판정**: k-of-n (기본 k=2) + 1.5 s 윈도(원안 관례 유지), 위협별
  분리 집계. 판정 결과는 `FireSolution.engage_ready` 로 발행.
- **권한 경계(필수 명시)**: `engage_ready` 는 **advisory** — 사격 개시는
  여전히 Two-key(KEY1_TARGET_TAP → KEY2_CONFIRM) + HMAC 의
  fire-authorization 체인 **단독 권한**. FireSolution 은 운용자 UI 의
  조준 제원(aim bearing/elevation, 표적 각속도) 표시와 KEY1 사전조건
  강화(`engage_ready` 미충족 시 KEY1 비활성 — 선택 파라미터)에만 사용.
- **IDS**: `FireSolution.msg`(§5.x 신규) + `TargetConfirmation.msg`(§5.x
  신규) 등록, 권원 헤더(§ 인용) 필수.

## 3. IDS / 인터페이스 영향

| 인터페이스 | 변경 | 영향 |
|---|---|---|
| `/hub/threat_alert` (집계) | world 좌표 필드 활용(기존 has_position/bearing/range 재사용 — **msg 무변경**, 채움 주체만 hub 로 확장) | 소비자 영향 없음 |
| `SlotAssignment.msg` | `slot_type` enum 추가(C-2, 소비자 동반) | 타입 해시 변경 → 전 노드 재빌드 |
| `FireSolution.msg` (신규) | C-3 | 신규 — 영향 없음 |
| `TargetConfirmation.msg` (신규) | C-3, HMAC 필드 포함 | 신규 — 영향 없음 |
| IDS 문서 | §5.9 갱신 + §5.x 2건 신설 | 본 DCN 승인 후 문서 갱신 |

## 4. 아키텍처 규칙 준수

| 규칙 | 계획 |
|---|---|
| DCN-2026-002 / ADR-006 (no shell-out, ROS 2 IPC only) | 신규 코드 전부 rclcpp 토픽/서비스 |
| ADR-008 Tier 정책 | Tier 2 C++ (san_surveillance / san_formation / san_hub_orchestrator) |
| Fire-auth 체계 (HMAC + Two-key) | C-3 은 기존 체인 **재사용·비우회** — engage_ready 는 advisory 한정(§2 C-3) |
| Test seam (pure-logic, rclcpp-free) | 할당 정책·합의 판정·트리거 게이트는 pure 함수로 분리, standalone gtest 등록 |

## 5. 영향 모듈

| 모듈 | 변경 |
|---|---|
| `san_surveillance` | sector_allocator 다중위협 정책, 위협별 fire-rate 상태, 락 스코프 교정 (C-1) |
| `san_formation` | Encircle 트리거 게이트·TTL·재계획·anchor 일원화 (C-2) |
| `san_hub_orchestrator` | threat_aggregator world 좌표 계산·재발행 (C-2), detection_to_threat 투표 발행 (C-3) |
| `combat_robot_msgs` | SlotAssignment.slot_type, FireSolution.msg, TargetConfirmation.msg (C-2/C-3) |
| `san_operator_tools` / App | 포위 승인 1-tap, engage_ready 표시 (C-2/C-3) |

## 6. 리스크 / 완화

- **자율 기동 오발동(C-2)**: 운용자 확인 기본 — 자동 개시는 opt-in 플래그.
  트리거 3중 게이트 + TTL 로 latch/플리커 차단.
- **투표 위·변조(C-3)**: HMAC + nonce — fire-auth 와 동일 위협모델.
  평문 투표는 본 DCN 으로 명시적 폐기.
- **감시 공백(C-1)**: 추적 전환 상한 3 대 + coverage ≥80% 를 **테스트로
  고정**(A10 보존 + A11 신설). 회귀 시 CI red.
- **단계적 구현**: C-1 → C-2 → C-3 순(의존: C-2 가 C-1 의 위협 클러스터
  ID, C-3 가 C-1 의 위협별 상태를 사용). 항목별 독립 PR — 부분 승인 가능.

## 7. 검증 계획

| 항목 | 기준 |
|---|---|
| C-1 | `test_surveillance` A1~A10 무수정 통과 + A11(2위협: 추적≤3, coverage≥80%) 신설 |
| C-2 | pure-logic gtest: 트리거 게이트 진리표, 신고로봇 좌표 변환(오지점 포위 회귀 테스트), TTL/히스테리시스 타임라인; sim 시나리오(S20 계열) 1건 |
| C-3 | pure-logic gtest: k-of-n 윈도·track 바인딩·HMAC 검증/재전송 거부(기존 fire-auth 테스트 패턴 재사용); engage_ready 가 fire-auth 를 우회하지 못함을 명시 테스트 |
| 공통 | colcon test (ament_uncrustify 포함) + standalone 러너 green, full colcon build |

## 8. 결정 일정 / 승인

- **DCN 검토 / 승인**: ✅ 2026-06-10 (김태근 PM) — C-2 운용자확인 기본·C-3 k=2 확정
- **C-1 구현**: ✅ 2026-06-10 — PR #262 (`e399e2e`, A11/A12 신설, CI green)
- **C-2 구현**: ✅ 2026-06-10 — PR #263 (`225a4af`, E1~E6 신설, CI green)
- **C-3 구현**: ✅ 2026-06-10 — PR #264 (`cd1d9df`, 인증 V1~V5 + VoteTally V1~V3, CI green)
- **DCN ID 할당**: DCN-2026-026

> **구현 완결 (2026-06-10)**: C-1/C-2/C-3 전체가 분리 PR 로 머지되었다.
> 잔여 후속(§2 C-3): 운용자 UI FireSolution 표시, fire-auth KEY1 사전조건
> 선택 파라미터, IDS 문서 §5.23/§5.24 표 반영 — 별도 작업으로 추진.

## 9. Cross-refs

- 리뷰 원본: `limon/features_gazebo_sim` `defbb64` · PR #259(수용분, `417cb2b`)
- DCN-2026-010 D-028 — Detection→Threat 변환(신고로봇 기준 bearing/range)
- DCN-2026-025 — `Detection.track_id`(C-3 표적 바인딩에 사용)
- SDD-SWARM v1.5 §8.2(Leader 팬틸트 부재) · §8.6.1(위협 시 follower 3대 재지향) · §6(Formation)
- IDS-CMD v1.5 §5.9(SlotAssignment) · §5.11(PanTiltCommand)
- TST A7/A10(surveillance 수락 기준) · S20 계열(통합 시나리오)
- Fire-authorization: HMAC-SHA256 + nonce sliding-window + Two-key (DCN-2026-002 계열 권원)
