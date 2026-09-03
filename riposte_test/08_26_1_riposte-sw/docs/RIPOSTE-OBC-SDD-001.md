# RIPOSTE-OBC-SDD-001
## Riposte POC — Offboard Controller (MAVSDK C++) Software Design Document

| 항목 | 내용 |
|---|---|
| 문서 ID | RIPOSTE-OBC-SDD-001 |
| 버전 | 1.1 (2026-08-18 코드리뷰 반영: §6 **SM-1 필드별 감시에 자세(attitude) 스트림 추가** — FRD→NED 회전 기저가 위치 스트림 뒤에서 얼어붙는 경로 차단, engage 게이트에도 자세 신선도 포함 / §8 **자세 모드 명령 의미 확정** — TARGET은 소비자 부재로 거부(engage 미래치), HOLD/RETURN_HOME은 disengage(PX4 Hold)로 사상(무동작-로그 제거), `IAttitudeSource::on_engage()` 신설(세션 경계에서 캐시 트랙 초기화) / §8.1 patrol·mission **수치 파라미터 기동 검증** + patrol 고도 대역의 safety.alt_min/alt_max 미러링 / Config 미해석 값 기동 거부(COMMON-SDD C-6) / 1.0: 2026-08-17 코드리뷰 CR-01/CR-03/CR-04 반영: §6 disarm 게이트를 필드별 스탬프로 완결(rel_alt=global-position, armed)·TrackBus 프로세스 경계 검증 신설·**SM-10 폴리곤 설정 시 fresh GPS 없으면 engage 거부**(강등 폐지) / 0.9: 2026-08-16: §9 시험 계획의 실기 단계를 RIPOSTE-BRINGUP-001로 위임 — 통과 기준·기록표 일원화) / 0.8 (Draft — 2026-08-16 PX4 SITL 검증: P1-02 판정 규칙 개정(ack-이후-스탬프 조기판정 폐기 → 확인 타임아웃 시점 판정), SM-1~9 주입 9/9·유도추종 통과 / 0.7: 2026-08-16 심층 코드리뷰 P1-02/03 반영: §4 PRESTREAM 오버라이드 판정·DISENGAGING 확인-후-READY(재시도·타임박스·FAULT)·SB_CMD_LINK setpoint 실패 감시·shutdown_hold 확장 / 0.6: 2026-08-16 심층 코드리뷰 P0-04 반영: §6 SM-1 필드별(스트림별) 신선도·SB_FIELD_STALE/SB_POS_INVALID 신설·SM-2 모드 스트림 스테일 처리·SM-9/engage/disarm 게이트 신선도 조건·FcuLink 경계 finite 검증 / 0.5: 2026-08-13: §8.2 Closure 신설(R-2 예상 접근점·큐 속도 필드) / 0.4: 2026-08-12 P3: §3.1 소스 목록 갱신·§6 SM-10 폴리곤 지오펜스·§8.1 BalloonPatrolSource 신설 / 0.3: 2026-08-12 코드리뷰 반영: §4 AUTO_LANDING 상태 명문화·§6 SM-3 고도하한 이륙 유예·§8 D-2 명령별 토큰 게이트/`on_engage()` / 0.2: 2026-07-04 심층 리뷰 §4·§5·§6 / 0.1 초안) |
| 대상 | RK3588 미션 컴퓨터 상 Offboard 유도 노드 (POC) |
| 상위 문서 | AIRYS-URD-001, AIRYS-SDD-001 (개념 참조) |
| FC | Pixhawk 6X / PX4 (버전 DEFERRED — 브링업 시 확정) |
| 링크 | RK3588 ↔ FC: Ethernet, MAVLink UDP / GCS ↔ RK3588: SiK 433MHz + mavlink-router |

---

## 1. 목적 및 범위

본 문서는 Riposte POC 단계에서 RK3588 상에서 동작하는 **Offboard 유도 노드**(이하 OBC: Offboard Controller)의 소프트웨어 설계를 정의한다.

**범위 내 (POC Phase 1):**
- PX4 Offboard 모드 진입/유지/이탈의 안전한 수명주기 관리
- Velocity setpoint (NED) 20Hz 고정 주기 스트리밍
- 시커 유도 법칙을 위한 setpoint 소스 추상화 (POC에서는 테스트 패턴)
- 스트림 워치독, 오퍼레이터 오버라이드 감지, 속도 클램프 등 소프트웨어 안전 계층

**범위 외 (DEFERRED):**
- 시커(추대상 기체) 실데이터 연동 및 PN 유도 법칙 구현 — Phase 2
- ROS 2 / uXRCE-DDS 전환 — Phase 3, 주기/지연 병목 확인 후
- STM32H7 서브보드 안전 인터록 연동 — POC 이후 (SI-2 하드웨어 승격 시점)

---

## 2. 시스템 컨텍스트

```
[GCS/QGC] ←SiK 433MHz→ [RK3588: mavlink-router] ←UDP:14540→ [OBC 프로세스]
                                    ↕ Ethernet UDP
                              [Pixhawk 6X / PX4]
```

- OBC는 mavlink-router의 로컬 UDP 엔드포인트에 접속한다 (FC 직결 아님). 이로써 GCS·OBC·FC 트래픽이 단일 라우터에서 관리된다.
- ASSUMPTION: mavlink-router가 systemd 서비스로 OBC보다 먼저 기동됨.
- GCS 링크(SiK)는 저대역(~57.6kbps)이므로 OBC의 상태 보고는 저주기(1Hz) STATUSTEXT/커스텀 최소화.

---

## 3. 아키텍처

### 3.1 컴포넌트 구성

```
main
 └─ OffboardController          ← 최상위 오케스트레이터 + 상태기계
     ├─ FcuLink                 ← MAVSDK 수명주기 (System discovery, plugin 보유)
     │    ├─ Offboard plugin
     │    ├─ Telemetry plugin
     │    └─ Action plugin      ← Hold/RTL/Land 이탈 경로
     ├─ SetpointStreamer        ← 20Hz 고정주기 스레드 (단일 제어 스레드)
     ├─ ISetpointSource         ← setpoint 소스 인터페이스 (전략 패턴)
     │    ├─ TestPatternSource  ← 브링업: 호버/정속/원형 패턴
     │    ├─ GuidanceSource     ← 시커 트랙 → 순수추적+리드 (L4 유도)
     │    ├─ MissionSource      ← 외부 대상 큐 임무 (이륙→전이→시커 접근→복귀)
     │    └─ BalloonPatrolSource ← 시험비행: 지오펜스 내 풍선 순찰 (T-1~T-5)
     ├─ Geofence                ← 폴리곤 경계 모델 (SM-10 + 행동계층 공용)
     └─ SafetyMonitor           ← 워치독·오버라이드·한계 감시 (독립 관심사)
```

### 3.2 설계 원칙 (AIRYS 원칙 계승)

- **G1 단일 제어 스레드**: setpoint 생성·전송·안전판정은 SetpointStreamer 스레드 하나에서만 수행. MAVSDK 텔레메트리 콜백은 원자적 스냅샷 기록만 담당 (AIRYS SeqSlot 개념의 사용자공간 대응 — `std::atomic` + 버전 카운터).
- **G2 명시적 상태기계**: 암묵적 부울 플래그 조합 금지. 모든 모드 전이는 FSM 이벤트로만.
- **G3 안전 기본값**: 모든 판단 불가 상황(스테일 데이터, 링크 유실)은 이탈(disengage) 방향으로 수렴.
- **G4 클램프 최종 방어선**: 어떤 소스가 setpoint를 내놓든 전송 직전 단계에서 무조건 한계 적용 (`Tunables` 참조).
- **G5 오퍼레이터 우선**: RC/GCS에 의한 모드 변경 감지 시 OBC는 즉시 스트리밍을 중단하고 재진입을 시도하지 않는다 (SI-2 소프트웨어 유사 — 인간 개입은 항상 승리).

---

## 4. 상태기계

```
IDLE → CONNECTING → READY → PRESTREAM → OFFBOARD_ACTIVE → DISENGAGING → READY
                      ↑                       │                │
                      │                       └─ 미션 착륙 요청 ─→ AUTO_LANDING ─┐
                      │                                                        │
                      └────────── FAULT ←──────────────────────────────────────┘
```

| 상태 | 진입 조건 | 동작 | 이탈 |
|---|---|---|---|
| IDLE | 프로세스 시작 | 설정 로드 | 즉시 CONNECTING |
| CONNECTING | — | System discovery, heartbeat 대기 | 발견+텔레메트리 유효 → READY / 타임아웃 → FAULT |
| READY | FC 연결·armed·위치 유효 | 대기, 상태 보고 | engage 명령(텔레메트리 신선·armed·위치 유효 시에만 수용) → PRESTREAM |
| PRESTREAM | engage 수신 | setpoint 선스트리밍 ≥1.0s @20Hz (Offboard 미진입), 매 틱 SafetyMonitor 평가(SM-1/SM-6 발화 가능). start ack 후에는 **모드 확인을 기다린다**(heartbeat 지연 허용 — 조기 오버라이드 판정 금지) | 모드 확인 → OFFBOARD_ACTIVE / disengage·안전위반: start ack 후 → DISENGAGING, ack 전 → READY 직행 / **확인 타임아웃**: 그 시점 신선한 모드가 비-Offboard면 오버라이드로 DISENGAGING(no-command·latch), 모드 정보 없으면 일반 DISENGAGING |
| OFFBOARD_ACTIVE | `Offboard::start()` 성공 + 모드 확인 | 소스로부터 setpoint 획득→클램프→전송, 매 주기 SafetyMonitor 판정. **setpoint 전송 실패 연속 `SETPOINT_FAIL_MAX_CONSEC`회 → SB_CMD_LINK로 DISENGAGING** (명령 링크가 죽은 채 ACTIVE 유지 금지) | disengage/안전 트리거 → DISENGAGING |
| DISENGAGING | — | 매 틱 `Offboard::stop()`·`Action::hold()`를 **성공할 때까지 재시도**(성공한 명령은 스킵; SM-2 기인 시 둘 다 미전송 — 조종사 소유 존중) | **신선한 모드 샘플이 비-Offboard 확인 → READY** (확인 없이 READY 금지 — FC가 Offboard에 남아 있는데 READY 보고 방지) / `DISENGAGE_CONFIRM_NS` 내 미확인 → FAULT (스트리밍은 이미 중단, D-1 위임) / SM-2 기인은 모드가 이미 비-Offboard(또는 판정 불가·no-command)이므로 즉시 READY |
| AUTO_LANDING | 소스가 착륙 요청(`requests_land()`) → `Action::land()` 수락 | PX4 자동 착륙 감시. 텔레메트리가 **신선할 때만** disarm 판정(스테일 시 보류) | 실제 disarm 확인(`armed==0`) → READY / 오퍼레이터 disengage → READY |
| FAULT | 복구 불가 오류 | 스트리밍 완전 중단, FC 자체 페일세이프에 위임(`COM_OF_LOSS_T`), 로그 | 수동 재시작만 허용 (POC 정책) |

**핵심 결정 D-1**: FAULT에서 OBC는 FC를 제어하려 들지 않는다. 스트림 중단 자체가 PX4 offboard failsafe(`COM_OBL_RC_ACT`)를 발동시키는 것이 가장 검증된 경로다. OBC가 고장 상태에서 Action 명령을 남발하는 것보다 안전하다.

**이탈 경로 강건화 (2026-08-16, 심층 코드리뷰 P1-02/P1-03)**:
- **PRESTREAM 오버라이드 창 폐쇄(P1-02, 2026-08-16 SITL 검증 후 개정)**: `start_offboard()` ack 후 조종사가 모드를 잡고 있으면 종전에는 확인 타임아웃 → 일반 DISENGAGING(stop/hold 전송!)으로 흘러 조종사와 싸우고 재진입 latch도 걸리지 않았다. 판정은 **확인 타임아웃 시점에만**, 그때 **신선한 모드 샘플이 비-Offboard**면 SM-2 동등 처리(`reentry_blocked_` latch + no-command DISENGAGING)하고, 모드 정보 자체가 없으면 링크 문제로 보아 일반(commanded) DISENGAGING한다. **ack 이후 스탬프인지로 조기 판정하지 않는다** — PX4의 ~1 Hz heartbeat는 ack 직후에도 **ack 이전 모드**를 실은 샘플을 배달하며, 이를 오버라이드로 본 최초 구현은 **정상 engage를 전부 중단시켰다**(PX4 SITL SM-3 시나리오에서 검출·수정).
- **DISENGAGING 확인-후-READY(P1-03)**: 종전에는 stop/hold를 한 번 쏘고 결과 무시 후 즉시 READY였다 — stop이 실패하면 FC는 Offboard에 남았는데 READY를 보고한다. 이제 stop/hold를 틱마다 재시도(성공분 스킵)하고, **신선한 모드 샘플이 비-Offboard를 확인해야 READY**. `DISENGAGE_CONFIRM_NS`(기본 5 s) 내 미확인 시 FAULT — 스트리밍은 이미 멎었으므로 PX4 offboard-loss 페일세이프가 기체를 소유한다(D-1). 페일세이프가 모드를 바꾸면 그 확인으로도 READY에 도달한다.
- **setpoint 전송 실패 감시(P1-03)**: ACTIVE 중 전송 실패가 연속 `SETPOINT_FAIL_MAX_CONSEC`(기본 5)회면 `SB_CMD_LINK`로 DISENGAGING — 스트림이 실제로 나가는지 모른 채 ACTIVE를 유지하면 PX4 offboard-loss와 OBC 상태가 어긋난다. 성공 시 카운터 리셋.
- **shutdown 시 stop/hold(P1-03)**: `shutdown_hold()`는 OFFBOARD_ACTIVE·PRESTREAM(ack 후)에 더해 **확인 미완의 DISENGAGING**(비-오버라이드)에서도 stop/hold를 시도한다 — 종료 시점이 이탈 도중이어도 commanded Hold 시도가 생략되지 않는다.

---

## 5. 스레딩 및 타이밍 모델

| 스레드 | 주기 | 역할 |
|---|---|---|
| 제어 스레드 (SetpointStreamer) | 50ms (20Hz), `CLOCK_MONOTONIC` 절대시각 기반 | FSM 틱, setpoint 파이프라인, 안전 판정 |
| MAVSDK 내부 스레드 | 비동기 | 텔레메트리 콜백 → `TelemetrySnapshot` 원자 기록 |
| 메인 스레드 | — | 시그널 처리(SIGINT/SIGTERM→협조 종료; 세션 활성 중이었다면 제어 스레드 join 후 best-effort stop+hold `shutdown_hold()`), 상태 로그 1Hz (`state_`는 `std::atomic` — 제어 스레드와의 레이스 제거) |

- **지터 예산**: 주기의 ±20% (10ms) 초과가 연속 N회(기본 3) 발생 시 SafetyMonitor가 경고, 지속 시 disengage. RK3588 리눅스는 비실시간이므로 `SCHED_FIFO` + CPU affinity 적용 (ASSUMPTION: root 또는 CAP_SYS_NICE 가용).
- **TelemetrySnapshot**: 위치/속도/자세/모드/armed/타임스탬프를 담는 POD. 콜백이 쓰고 제어 스레드가 읽는 seqlock 스타일 이중버퍼. 스테일 판정 기준 `TELEM_STALE_MS = 500ms`.

---

## 6. 안전 설계 (SafetyMonitor)

매 제어 주기에 아래를 평가, 하나라도 위반 시 DISENGAGING 트리거. 평가는 OFFBOARD_ACTIVE 전용이 아니다 — PRESTREAM에서도 매 틱 수행되어 SM-1/SM-6이 선스트리밍 중에도 발화 가능하다(start 전 위반은 READY 복귀로 중단):

| ID | 감시 항목 | 기준 (Tunables) | 근거 |
|---|---|---|---|
| SM-1 | 텔레메트리 신선도 (**필드별**) | 위치/속도 스트림 age > 500ms → SB_TELEM_STALE; 스트림별(모드·armed·global position·EKF health·배터리·**자세**) age > telem_flag_stale_s(기본 3 s) → SB_FIELD_STALE; ACTIVE 중 `position_ok==0` → SB_POS_INVALID | 스테일/무효 상태로 유도 금지 (G3). 단일 스탬프는 위치 스트림만 살아 있으면 다른 안전 입력의 정지를 가렸다 (P0-04). 자세는 고속 스트림이지만 FRD→NED 회전 기저라 정지 시 위치가 살아 있어도 조향이 얼어붙은 회전으로 나간다(2026-08-18) |
| SM-2 | 외부 모드 변경 | flight_mode ≠ OFFBOARD (진입 후), **또는 ACTIVE 중 모드 스트림 스테일** | 오퍼레이터 오버라이드 존중 (G5). 소유권을 확인할 수 없으면 오버라이드로 간주 — no-command 경로(D-1) |
| SM-3 | 지오펜스(소프트) | 홈 기준 수평 반경 / 고도 상한 / 고도 하한(**이륙 후 무장**, 아래 참조) | POC 시험장 한계 |
| SM-4 | 속도 클램프 | ‖v_cmd‖ ≤ V_MAX (수평/수직 분리) | G4, 전송 직전 무조건 적용 |
| SM-5 | 주기 지터 | 3연속 >±20% | 제어 품질 붕괴 감지 |
| SM-6 | disarm 감지 | armed == false | 즉시 스트리밍 중단 |
| SM-7 | 트랙 신선도/품질 | OFFBOARD 중 **탐지-앵커 트랙** 스테일 > 임계 | 시커 소실 시 관성 질주 방지. 신선도 시계는 마지막 탐지-앵커(`visual_coast==0`) 샘플 기준 — T2 시각 coast 발행이 신선해도 coast window를 연장하지 못하게 함(TRACKER-REQ TR-D-b) |
| SM-8 | engage 타임박스 | engage 후 경과 > engage_timebox_s | 제어 세션 시간 상한 (무한 추적 방지) |
| SM-10 | 폴리곤 경계 | 설정된 폴리곤(로컬 NED) 밖 → SB_FENCE_POLY | 측량된 시험장 경계(T-1). SM-3 반경과 **독립·병행** |
| SM-9 | 배터리 게이트 | engage: 잔량 < bat_engage_min_frac 또는 판독 불가·**스테일** 시 거부 / 비행 중: 판독 가능 & **신선** & 잔량 < bat_land_frac | PX4 저전압 페일세이프(COM_LOW_BAT_ACT) 이전의 OBC측 게이트. MAVSDK v3 는 잔량을 0~100%로 보고하므로 FcuLink 경계에서 0~1 비율로 정규화. 스테일 배터리는 "판독 불가"와 동일 취급 |

- SM-2 이행 시 재진입 금지 플래그(latch) 설정 — 같은 틱에 오퍼레이터 disengage가 겹쳐도 latch되며, READY는 ENGAGE를 거부하고 READY에서 명시적 DISENGAGE 수신 시에만 해제된다(오퍼레이터 확인). SM-2 기인 DISENGAGING은 stop/hold를 보내지 않는다 — 조종사 소유 존중(G5), 스트리밍 중단만 수행. **모드 스트림 스테일로 인한 SM-2도 동일 경로다**: 소유권을 확인할 수 없는 상태에서 hold를 보내면 실제 오버라이드 중인 조종사와 싸우게 되므로, 스트리밍만 중단하고 PX4 offboard-loss 페일세이프(D-1)에 위임한다. latch 해제는 링크 회복 뒤 오퍼레이터 DISENGAGE로 — 보수적이지만 오퍼레이터가 복구 가능하다.
- **SM-1 필드별(스트림별) 신선도 (2026-08-16 신설, 심층 코드리뷰 P0-04)**: 종전에는 `mono_ns` 하나가 위치/속도 스트림에서만 스탬프되어, 위치 스트림만 살아 있으면 모드·armed·고도·배터리 스트림이 정지해도 스냅샷 전체가 "신선"으로 보였다 — 오래된 `in_offboard=1`이 조종사 오버라이드를, 오래된 `rel_alt`가 SM-3 위반을, 래치된 `gps_ok`가 fix 상실을 가렸다. `TelemetrySnapshot`은 이제 **MAVSDK 구독 스트림별 도착 스탬프**(위치/자세/global position/모드/armed/landed/EKF health/배터리)를 갖는다.
  - **이중 임계**: 위치/속도(40 Hz 설정)는 종전 500 ms(SM-1). 저속 스트림(대부분 1 Hz heartbeat 파생)은 `safety.telem_flag_stale_s`(기본 3 s, Tunables `TELEM_FLAG_STALE_NS` [CFG]) — 1 Hz 스트림이 정상일 때 오발동하지 않으면서 정지는 3초 내 검출한다. 검증: `telem_flag_stale_s >= telem_stale`.
  - **ACTIVE 중 감시 대상**: 모드 스트림 스테일 → SM-2(위 참조) / global position·armed·EKF health·**자세** 스트림 스테일 → SB_FIELD_STALE / `position_ok==0` → SB_POS_INVALID (EKF가 위치를 무효 선언했는데 계속 유도하는 것을 금지). 배터리 스테일은 위반이 아니라 "판독 불가"로 강등된다(SM-9 행 참조 — PX4 페일세이프가 하한). **자세 스트림 (2026-08-18 신설)**: 유도(GuidanceSource·AttitudeTrackingSource)는 매 틱 트랙 벡터를 자세 DCM으로 FRD→NED 회전시키므로, 위치가 살아 있는 채 자세만 정지하면 **얼어붙은 회전으로 명령이 계속 나간다** — 자기 기체가 yaw할수록 명령 방향이 틀어지는데 다른 증상이 없다. 고속 스트림에 3 s 급 완만한 상한은 오발동 없이 완전 정지만 잡는다.
  - **`gps_ok` 래치 제거**: `gps_ok`는 마지막 수신값일 뿐이며, 소비자는 반드시 global position 스트림 스탬프의 신선도와 함께 판정한다. engage 시 폴리곤 투영(SM-10)은 fix가 신선할 때만 수행 — 스테일 fix로는 경계를 만들지 않는다(기존 "없는 경계를 지어내지 않음" 원칙의 신선도 확장).
  - **engage 게이트 확장**: READY의 engage 게이트는 위치 신선도에 더해 모드·armed·EKF health·**자세** 스트림 신선도와 배터리 신선도를 요구한다 — engage는 모든 안전 입력이 검증 가능하게 현재여야 하는 유일한 순간이고, 회전 기저가 이미 스테일인 채 세션을 시작하면 SM-1 필드 감시는 첫 오명령들이 나간 뒤에야 잡는다(2026-08-18). **폴리곤(SM-10)이 설정된 경우 `gps_ok`와 global-position 스트림 신선도도 함께 요구한다**(CR-04) — 투영에 쓸 fix가 없으면 경계를 세울 수 없고, 경계 없이 시작하지 않는다.
  - **disarm 게이트**: AUTO_LANDING의 disarm은 `landed` 플래그가 **신선할 때만** FC landed 판정을 신뢰한다 — 비행 중 얼어붙은 `landed=1`이 공중 disarm을 유발하는 것을 차단.
  - **FcuLink 경계 finite 검증**: MAVLink에서 오는 모든 float(위치/속도/자세/고도/위경도)는 콜백에서 finite 검사를 통과해야 스냅샷에 반영·스탬프된다. 비유한 샘플은 통째로 폐기되어 해당 스트림이 스테일로 늙는다 — NaN 위치가 지오펜스 비교(모두 false)를 우회하는 경로를 원천 차단(배터리는 기왕 동일 정책).
- **SM-3 고도 하한의 이륙 유예 (2026-08-12 코드리뷰 반영)**: 고도 **하한**은 기체가 한 번 `alt_min` 이상에 도달한 뒤에 무장(arm)된다. 지상에서 시작하는 미션 이륙(MissionSource TAKEOFF)은 OFFBOARD_ACTIVE 첫 틱에 `rel_alt≈0`이라, 유예가 없으면 **모든 이륙이 SM-3 위반으로 즉시 중단**된다. 무장 이후에는 계속 유지되므로 "세션 활성 중 지면으로 하강"은 그대로 검출된다. 무장 상태는 `capture_home()`(engage마다 호출)에서 해제되어 제어 세션 단위로 재적용된다. 고도 **상한**은 유예 없이 첫 틱부터 하드 바운드다.
- **SM-10 폴리곤 경계 (2026-08-12 신설, T-1)**: 시험장 경계를 GPS 꼭짓점(≥3개) 또는 engage 지점 중심 정사각형(`fence.side_m`, 시험 기본 50 m)으로 정의한다.
  - **투영 시점 = engage 1회**: GPS↔로컬 NED 대응은 **한 텔레메트리 샘플이 두 좌표계를 동시에 담고 있는 순간**(READY의 engage 게이트 통과 직후, 신선도·위치유효 확인 완료 시점)에 등거리 원통 투영으로 고정한다. 매 틱 재투영하면 GPS 잡음만큼 경계가 흔들린다. GPS fix가 없거나 스테일이면 경계를 **만들지 않는다**(없는 경계를 지어내지 않음). **폴리곤이 설정된 경우 이때 engage를 거부하고 READY에 머문다**(2026-08-17 개정, 코드리뷰 CR-04) — 종전에는 SM-10을 비활성한 채 SM-3 반경만으로 세션을 시작했으나, 이는 **오퍼레이터가 명시적으로 설정한 하드 경계를 GPS 스트림 결함 하나로 자동 제거**하는 것이어서 default-deny 원칙과 충돌한다. 폴리곤 설정 자체가 "이 경계 안에서만 비행한다"는 운용 결정이므로, 그 경계를 세울 수 없으면 비행하지 않는 것이 그 결정에 맞는 해석이다. 폴리곤을 설정하지 않은 프로파일은 영향이 없다(SM-3 반경이 그대로 한계).
  - **감시자와 행동계층이 같은 폴리곤을 공유**: `SafetyMonitor::set_fence()`와 `ISetpointSource::set_fence()`에 동일 객체를 전달해 "경계가 어디인가"에 대해 두 계층이 불일치할 수 없게 한다.
  - **역할 분담**: SM-10은 **하드 한계**(넘으면 disengage)이고, 감속·선회는 행동계층(BalloonPatrolSource)이 담당한다. **SM-10이 발화했다는 것은 행동계층이 실패했다는 뜻**이다.
  - 오목(concave) 폴리곤도 지원(even-odd 광선 교차). 경계까지 거리는 내부 양수/외부 음수 부호를 갖는다. **내측 방향(`inward`)은 정점 산술평균(centroid)이 아니라 가장 가까운 에지 기준의 부호거리 gradient로 산출**한다(2026-08-16, 코드리뷰 P2-03) — 오목 폴리곤에서 centroid가 폴리곤 밖 노치에 놓이면 경계 근처 기체를 오히려 밖으로 유도할 수 있기 때문. 나선/L자 등 임의 형상에서 항상 내부를 가리킨다.
- DEFERRED: SM-3의 하드 지오펜스는 PX4 파라미터(GF_*)로 이중화 — FC 측 설정 체크리스트에 포함. **SM-10 사용 시에도 PX4 GF_*를 최종 방벽으로 병행 설정**한다.

---

## 7. 디렉터리 구조 및 빌드

```
riposte-obc/
├─ CMakeLists.txt              # C++17, MAVSDK find_package, -Wall -Wextra -Werror
├─ config/
│  └─ obc.toml                 # 접속 URL, Tunables 오버라이드
├─ src/
│  ├─ main.cpp
│  ├─ Tunables.h               # 모든 매직넘버 집결 (AIRYS 컨벤션)
│  ├─ Types.h                  # TelemetrySnapshot, VelocitySetpointNed, FSM enum
│  ├─ Log.h                    # 구조화 로그 (spdlog ASSUMPTION)
│  ├─ FcuLink.h / .cpp
│  ├─ OffboardController.h / .cpp
│  ├─ SetpointStreamer.h / .cpp
│  ├─ SafetyMonitor.h / .cpp
│  └─ sources/
│     ├─ ISetpointSource.h
│     └─ TestPatternSource.h / .cpp
└─ test/
   └─ (Phase 1 후반: FSM 단위시험, SITL 스크립트)
```

빌드 게이트 (AIRYS 준용): `-Werror`, clang-tidy 기본 프로파일, 정적 크기 상수화. 예외는 MAVSDK 경계에서만 흡수하고 내부는 `Result<T>` 스타일 반환 (DEFERRED: 기존 SafetyContracts.h 재사용 여부 — 로컬 소스 접근 후 결정).

---

## 8. 핵심 인터페이스 스케치

```cpp
// Types.h
enum class ObcState { Idle, Connecting, Ready, Prestream,
                      OffboardActive, Disengaging, Fault };

struct VelocitySetpointNed {
    float vn_mps, ve_mps, vd_mps;
    float yaw_rad;               // POC: 진행방향 고정 or 패턴 정의
};

struct TelemetrySnapshot {
    uint64_t mono_ns;
    // position, velocity, attitude, flight_mode, armed ...
};

// sources/ISetpointSource.h
class ISetpointSource {
public:
    virtual ~ISetpointSource() = default;
    // 반환 false = 소스가 setpoint를 낼 수 없음 → disengage 사유
    virtual bool compute(const TelemetrySnapshot& t,
                         VelocitySetpointNed& out) = 0;
    virtual const char* name() const = 0;
};

// SafetyMonitor.h
struct SafetyVerdict { bool ok; uint32_t violation_mask; };
class SafetyMonitor {
public:
    SafetyVerdict evaluate(const TelemetrySnapshot& t,
                           ObcState s, uint64_t now_ns);
    void clamp(VelocitySetpointNed& sp) const;   // SM-4, 항상 마지막 호출
};

// OffboardController.h — 상태 전이는 여기서만
class OffboardController {
public:
    bool init(const Config& cfg);
    void requestEngage();        // 외부(콘솔/GCS 명령) → PRESTREAM
    void requestDisengage();
    void tick(uint64_t now_ns);  // SetpointStreamer가 20Hz로 호출
private:
    std::atomic<ObcState> state_{ObcState::Idle}; // 메인 1Hz 로그 리더와 공유
    // FcuLink, SafetyMonitor, ISetpointSource* ...
};
```

**핵심 결정 D-2 (SI-2 소프트웨어 대응)**: `requestEngage()`는 POC에서 오퍼레이터 콘솔 명령으로만 호출 가능하며, 코드 어디에서도 자동 호출 경로를 만들지 않는다. 시커 연동 후에도 "추적 성립 → 자동 engage"는 금지되고, 오퍼레이터 승인 토큰을 요구하는 구조(AIRYS `OperatorAuthorization` 패턴)로 승격한다.

**D-2의 명령별 적용 (2026-08-12 코드리뷰 반영)**: 토큰 게이트는 제어 세션을 **시작하거나 방향을 바꾸는** 명령에만 적용한다.

| 명령 | 토큰 | 근거 |
|---|---|---|
| ENGAGE | 필요 | 제어 세션 개시 — D-2의 본체 |
| TARGET | **필요** | 외부 대상 큐를 주입하고 engage를 요청한다 = 제어 세션 개시 경로. 토큰을 요구하므로 "자동 engage"가 아니라 **오퍼레이터 인증 engage**이며 D-2를 만족한다. (종전 구현은 ENGAGE 외 전 타입을 거부해 TARGET이 유효 토큰으로도 항상 거부됨 — 미션 경로 자체가 동작 불능이었다.) |
| DISENGAGE / OPERATOR_HOLD / RETURN_HOME | 불요 | 안전측으로만 이동시키는 명령. 제어 세션을 **멈추는** 데 토큰을 요구하는 것은 안전장치가 아니라 고장모드다 |

**자세(attitude) 모드에서의 명령 의미 (2026-08-18 코드리뷰 반영)**: HOLD/RETURN_HOME/TARGET은 속도 소스의 행동이고 자세 모드에는 그 소스가 없다. 종전 구현은 널 소스에만 전달하고 "requested"를 로깅해 **세션 중 오퍼레이터 명령이 조용히 무시**됐고, TARGET은 큐를 버린 채 engage만 래치했다. 확정 의미:
- **TARGET → 거부**(engage 미래치): 소비자 없는 큐로 세션을 시작하면 큐가 도착 즉시 증발한 세션이 된다. 오퍼레이터에게 즉시 WARN.
- **OPERATOR_HOLD → disengage 요청으로 사상**: DISENGAGING은 스트림 중단 + PX4 Hold 명령이므로 "지금 하는 것을 멈추고 그 자리에 있어라"의 실행 가능한 안전 의미 그 자체다.
- **RETURN_HOME → disengage 요청으로 사상 + WARN**: FcuLink에 RTL 액션이 없어 귀환은 수행 불가 — 기체는 **복귀가 아니라 정지 유지**함을 로그로 명시한다. 세션을 계속 끄는(종전) 것보다 엄격히 안전측이다.

회귀 시험은 술어가 아니라 **실제 FSM**을 구동한다(`test_safety_fsm`, SIL FcuLink): 자세 모드에서 유효 토큰 TARGET은 READY를 벗어나지 못하고(engage 미래치), 같은 상태의 ENGAGE는 벗어난다(시험이 공허하지 않음을 증명), PRESTREAM 중 HOLD·RETURN_HOME은 각각 READY로 되돌린다. 종전 무동작 구현을 재주입하면 이 3건이 실패한다.

**소스 상태의 제어 세션 단위 초기화**: `ISetpointSource::on_engage()`가 engage(PRESTREAM 진입) 시 1회 호출되어 소스의 제어 세션 단위 상태(홈 지점·미션 phase·유도 필터 이력)를 초기화한다. 대상 큐(`target_`)는 초기화 대상이 아니다 — engage를 요청한 명령이 실어 오기 때문. 이 훅이 없으면 두 번째 제어 세션이 **이전 제어 세션의 홈 지점으로 복귀**하고 이전 phase에서 이어진다. 직후 `set_fence()`로 이번 제어 세션의 경계(SM-10과 동일 객체)가 전달된다. **`IAttitudeSource::on_engage()`(2026-08-18 신설)** 도 같은 세션 경계에서 호출된다 — 자세 추종 소스는 캐시된 마지막 유효 트랙과 탐지 시계를 세션 간 보존하고 있었고, 초기화 없이는 두 번째 세션이 coast 예산만큼 **이전 세션의 트랙으로 조향**할 수 있다.

---

## 8.2 Closure — GCS 큐 기반 예상 접근점 (R-2)

GCS가 대상 기체의 위치·**속도**를 주면, 도착 시점에 대상이 있을 자리로 전이한다. 큐 지점(대상이 *있었던* 자리)으로 날아가면 전이 시간 내내 대상을 뒤쫓게 되어 도착 시 시커 획득 거리에 들지 못한다.

- **큐 노화(aging)**: 큐는 저빈도로 도착하므로, 해석 시점에 대상은 이미 이동해 있다. 모든 계산은 큐 시각이 아니라 **현재로 외삽한 대상 위치**에서 시작한다.
- **해법**: `|R + V·t| = s·t`를 t에 대해 푼 **닫힌 형태 2차방정식**의 최소 양근.
  `(|V|² − s²)t² + 2(R·V)t + |R|² = 0`. 반복 근사가 아니라 정확해이며, 근의 순서가 `a`의 부호에 의존하므로 두 근을 모두 구해 양수 중 최솟값을 택한다.
  - **대상이 더 빨라도 접근 가능할 수 있다** — 횡단 기하에서는 해가 존재한다. `closing = s − |V|` 식의 순진한 구현이 틀리는 지점이며, 단위시험으로 고정했다.
  - 해가 없으면(더 빠른 대상이 정면으로 이탈) `ok=false`와 함께 **대상의 현재 외삽 위치**를 돌려준다 — 순수추종 폴백으로, 최선의 답이며 방향은 여전히 옳다.
- **`RENDEZVOUS_MAX_TGO_S`(120초) 상한**: 몇 분 앞을 예측하는 해는 등속 가정을 신뢰 범위 밖까지 외삽한 것이다. 초과 시 `ok=false`로 보고해 환상이 아니라 대상을 향하게 한다.
- **도달 판정도 조준점 기준**: 이동 대상의 큐 지점은 대상이 이미 떠난 자리이므로, 그곳에서 전이를 끝내면 빈 하늘에서 멈춘다.
- **운용 함의**: 접근점은 큐 지점보다 훨씬 멀 수 있다. 실측 예 — 300 m 북쪽 대상이 동쪽 8 m/s로 횡단, 추종 10 m/s → t_go 50.3 s, **lead 403 m**. 즉 **R-2 임무 프로파일은 SM-3 반경(기본 150 m)과 SM-8 타임박스(기본 60 s)를 시나리오에 맞게 재설정해야 한다** — 기본값 그대로는 전이 도중 강제 disengage된다.

**명령 확장**: `ObcCommand`/`MissionTarget`에 `target_vel_ned_mps` 추가. 수신측이 **정확한 크기 일치**를 요구하므로 구버전 송신자는 필드가 어긋난 채 해석되지 않고 **통째로 거부**된다 — `engage_cli`와 OBC는 함께 재배포한다.

**단위시험 (`test/test_rendezvous.cpp`, ctest `rendezvous`)**: 39 checks — 정지 큐=큐 지점, 정면 접근 시 전이 단축, 횡단 대상의 리드 생성, 느리게 이탈하는 대상 포획, 빠르게 정면 이탈 시 해 없음+폴백, **빠른 횡단 대상은 접근 가능**, 큐 노화 반영, 정지 큐는 노화해도 불변, 과도한 t_go 거부, 이미 대상 위, 속도 0 폴백, 3차원 접근. 여러 시험이 대수식이 아니라 **"추종자와 대상이 같은 시각에 같은 지점에 도착하는가"** 를 직접 검사해, 2차방정식의 부호 오류가 통과하지 못하게 했다.

---

## 8.1 BalloonPatrolSource — 풍선 시험 행동 (T-1~T-5)

지오펜스 안에서 풍선을 찾아 접근하고, 경계가 가까워지면 감속·선회해 **다른** 풍선을 찾는 시험비행 전용 소스.

```
SEARCH      순찰고도 유지 + 제자리 yaw 선회, 확정 트랙 대기
   │ 미방문 트랙 획득
APPROACH    유도(GuidanceSource) 기반 접근, 경계가 가까울수록 감속·내측 편향
   │ 도달(range≤reach) / 경계 근접(edge≤turn_margin) / 트랙 소실
TURN_AWAY   내측으로 이동하며 시험장 중앙을 지향, dwell 경과+경계 이탈 시 SEARCH
```

- **위상 전환을 명령 생성보다 먼저 수행**한다. 반대 순서면 전환이 일어난 틱마다 **이전 위상의 명령이 한 번 더 나간다** — 특히 "경계가 너무 가깝다"고 판단한 직후에 "계속 접근" 명령이 1틱 나가는 것은 T-5의 취지에 정면으로 어긋난다(개발 중 단위시험이 검출).
- **감속 (T-5)**: 경계까지 거리가 `soft_margin_m`(10 m) 이내면 `k = edge/margin`으로 추종 방향과 내측 방향을 혼합한다 — 감속과 선회가 동시에 일어나 하드 한계에 닿기 전에 곡선으로 빠져나온다. `turn_margin_m`(3 m)에서는 대상을 포기한다.
- **재선정 방지**: 방문 완료 대상은 **track id**로 기억한다(위치가 아니라). 시커 Tracker의 primary가 sticky라, 방금 처리한 id를 무시하는 것이 곧 "선회하여 다른 풍선을 찾는" 동작이 된다 — 프로세스 간 조율이 필요 없다. 쿨다운(기본 30초) 경과 시 항목은 만료·정리된다.
- **SEARCH는 제자리 선회**다. 탐색 중 수평 이동이 없으므로 탐색 자체가 경계를 넘을 수 없다(경계 근처면 내측으로만 복귀).
- **경계 없이는 유도하지 않는다**: `fence.valid()`가 아니면 `compute()`가 false를 반환해 SM-7 경로로 수렴한다. 경계 없는 무한 탐색 비행을 하지 않는다.
- 트랙 소실은 **정상**이다 — 접근과 달리 순찰에서는 대상을 잃는 것이 일상이므로 disengage가 아니라 SEARCH로 복귀한다.
- 안전 계층은 그대로다: engage는 여전히 오퍼레이터 토큰(D-2)이 필요하고, 경계를 실제로 넘으면 SM-10이 제어 세션을 끝낸다.
- **파라미터 기동 검증 + 고도 대역 미러링 (2026-08-18 코드리뷰 반영)**: patrol.* 수치는 `validate(BalloonPatrolSource::Params)`로, mission.* 수치는 `validate(MissionSource::Params)`로 기동 시 범위 검증한다(SafetyMonitor::Limits와 동일 패턴 — 종전에는 지속시간만 거부되고 속도/레이트/마진의 부호 오타는 무진단이었다: `approach_speed_mps=-3`은 TURN_AWAY를 경계 **바깥으로** 몰고, `land_rate_mps=-0.8`은 LANDING을 상승시켜 disarm이 영영 안 걸린다 — SafetyMonitor::clamp는 크기만 제한한다). 또한 patrol의 접근 고도 대역(min/max_alt_m)은 MissionSource의 상한 미러와 같은 방식으로 **safety.alt_min/alt_max에서 미러링**한다 — 종전에는 컴파일 기본값 [3, 15] m로 굳어 있어, 설정된 상한이 더 낮으면 접근 클램프가 상한 위로 상승을 명령해 SM-3 위반으로 세션이 끝났다. patrol.alt_m 자체도 이 대역 안이어야 기동한다.

**단위시험 (`test/test_patrol.cpp`, ctest `patrol`)**: 81 checks — Geofence(정사각 포함/거리 부호/오목 폴리곤/GPS 투영 스케일·오프셋/꼭짓점 부족 거부/내측 방향), SM-10(폴리곤 밖 발화·미설정 시 무발화·engage 전 미검사·SM-3와 독립), 패트롤(경계 없으면 유도 거부, SEARCH→APPROACH, 도달→방문기록→TURN_AWAY→SEARCH, 동일 대상 재선정 금지·다른 대상은 허용, 소프트 마진 감속, turn margin 포기·내측 선회, 순찰고도 유지, 트랙 소실 시 SEARCH 복귀, on_engage 방문목록 초기화).

---

## 9. 시험 계획 (Phase 1)

1. **SITL (Gazebo/jMAVSim)**: FSM 전 상태 전이, 선스트리밍 타이밍, Offboard 진입 성공률
2. **페일세이프 주입 시험**: 스트림 강제 중단 → `COM_OF_LOSS_T` 동작 확인, SM-1~SM-6 각각 인위 트리거
3. **오버라이드 시험**: SITL에서 RC/GCS 모드 전환 → SM-2 감지·재진입 금지 확인
4. **HIL/실기 지상**: 프로펠러 제거 상태에서 진입/이탈 시퀀스
5. **실비행**: 호버 → 정속 패턴 → 원형 패턴 순

각 단계 통과 기준은 **RIPOSTE-BRINGUP-001**이 관리한다 — 4단계(HIL/지상)는 §6(B5)·§7(B6), 5단계(실비행)는 §10(B8).

---

## 10. ASSUMPTION / DEFERRED 목록

| 태그 | 항목 |
|---|---|
| ASSUMPTION | PX4 v1.14+ (Offboard velocity NED 인터페이스 안정 버전) |
| ASSUMPTION | MAVSDK v2/v3 C++ (버전은 브링업 시 고정) |
| ASSUMPTION | mavlink-router systemd 선기동, OBC는 로컬 UDP만 접속 |
| ASSUMPTION | RK3588에서 SCHED_FIFO 사용 가능 |
| DEFERRED | GuidanceSource(PN) 설계 — 시커 인터페이스 확정 후 |
| DEFERRED | ROS 2/DDS 전환 판단 — 20Hz MAVSDK 실측 지연 데이터 확보 후 |
| DEFERRED | SafetyContracts.h 등 기존 AIRYS 헤더 재사용 — 로컬 소스 접근 후 |
| DEFERRED | 시험 절차서(RIPOSTE-OBC-TP) 분리 여부 |
