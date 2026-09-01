# RIPOSTE-SUPERVISOR-SDD-001
## Riposte — 감독/헬스(L5) 모듈 설계서

| 항목 | 내용 |
|---|---|
| 문서 ID | RIPOSTE-SUPERVISOR-SDD-001 |
| 버전 | 1.4 (2026-08-16 코드리뷰 P2-01/P2-02: 페이로드 신선도 판정(스테일=미기록·非active §2)·블랙박스 I/O 오류 ok() 반영·NaN/Inf finite 가드 §3 / 1.3: 2026-07-04 심층 리뷰 반영: active 폴 주기 50ms §3 / 1.2: 블랙박스 보존 §3 — 크기상한 번호회전 `Blackbox`, 오프타깃 단위시험 / 1.1: 코딩표준 Rev C — FILE* RAII, best-effort 기록) |
| 대상 모듈 | `riposte-sw/supervisor/` (프로세스 `riposte-supervisor`) |
| 상위 문서 | RIPOSTE-SAD-001 (§6 결정 A-4) |
| 계층 | L5 감독/안전 (cross-cutting) |
| 주기 | 헬스 폴 active 20Hz·유휴 10Hz / 블랙박스 active 20Hz·유휴 1Hz |

---

## 1. 목적 및 범위

`riposte-supervisor`는 세 상태 버스(ObcStatus·TrackState·SeekerHealth)를 **읽기 전용**으로 소비해 헬스를 집계하고, JSONL 블랙박스를 기록하며, systemd watchdog을 관리한다.

**핵심 결정 A-4 (감독 프로세스 분리)**: 기록·헬스집계는 제어 경로와 무관한 **디스크 I/O**를 수반하므로 OBC에서 분리한다. supervisor가 죽어도 비행 기능은 무영향이고, OBC가 죽으면 PX4 offboard 페일세이프가 기체를 인수한다(D-1). 이로써 제어 스레드의 20Hz 결정성을 I/O 지터로부터 보호한다.

**권한 경계**: supervisor는 **비행에 대한 어떤 권한도 없다** — 오직 관측·기록·watchdog. 명령 채널이나 FC 접근 없음.

---

## 2. 데이터 흐름

```
[obc]    ─ SHM_OBC_STATUS ──▶┐
[seeker] ─ SHM_TRACK ───────▶├─ riposte-supervisor ─▶ JSONL 블랙박스
[seeker] ─ SHM_SEEKER_HEALTH▶┘         │
                                       └─▶ sd_notify(WATCHDOG=1)
```

- 모든 버스는 `ShmSeqSlot` **Reader**로 부착. 시작 시 없어도 매 폴 `ensure_open()` 재시도(기동 순서 독립).
- 세 버스 중 일부만 살아 있어도 동작(부분 관측 허용) — 각 값에 `have_*` 플래그.
- **페이로드 신선도 판정 (2026-08-16, 코드리뷰 P2-01)**: `read()` 성공(=값 존재)과 **값이 최신**은 다르다. 생산자(OBC·시커)가 죽으면 shm의 마지막 값이 그대로 남아, 종전 감독자는 얼어붙은 `OFFBOARD_ACTIVE`를 계속 "현재"로 기록하고 그 상태를 20Hz 적응 트리거로 삼았다. 각 버스 값의 `mono_ns`로 **age**를 계산해 신선도 임계(`SUPERVISOR_STALE_NS`, 기본 500ms)를 넘으면 **스테일**로 간주한다: ① 블랙박스에 스트림별 age와 `stale` 플래그를 기록해 사후분석이 "정지"와 "정상"을 구분 ② 스테일 OBC는 active 취급하지 않아 얼어붙은 상태가 20Hz 기록·폴을 유발하지 못한다. **watchdog 관리는 감독자 자신의 생존 신호이므로 불변**(생산자 스테일과 무관하게 pet). 생산자 죽음의 비행 안전 처리는 감독자가 아니라 SM-7(OBC)·offboard failsafe(PX4) 소관이며, 감독자는 그 사실을 **관측·기록**할 뿐이다.

---

## 3. 블랙박스 (JSONL)

한 줄=한 레코드(JSON Lines). 사후분석·시험증빙용:
```json
{"t":<mono_s>,"obc":{"state","viol","jit_ms","eng"},
 "trk":{"valid","q","age_ms"},"seeker":{"fps","infer_ms","det_ok"}}
```

**적응형 기록률**: OFFBOARD_ACTIVE 중 20Hz(제어 세션 상세), 유휴 시 1Hz(하트비트). 저장 용량과 해상도를 균형. 폴 루프 주기도 이에 맞춰 **OFFBOARD_ACTIVE 중 50ms(~20Hz), 유휴 100ms(10Hz)** 로 적응한다(2026-07-04 심층 리뷰 반영) — 유휴 주기 그대로면 20Hz 기록률이 물리적으로 달성 불가(제어 세션 블랙박스가 실제로는 10Hz가 되는 결함).

**보존(retention) — `Blackbox` (`supervisor/src/Blackbox.h`)**: 크기 상한 도달 시
logrotate식 번호 회전 — `base → base.1 → … → base.N`, `base.(N+1)`은 폐기. 디스크 사용량이
**약 `(keep+1) × max_bytes`** 로 유한하게 유지되어, 타깃의 소용량 플래시에서 무한 append가
디스크를 채워 타 서비스를 죽이는 사태를 방지. 설정 `supervisor.blackbox_max_bytes`(기본
16MiB)·`blackbox_keep`(기본 5, 0이면 아카이브 없이 절단). 모든 파일연산은 **best-effort(A-4)**
— rename/remove 실패는 로깅만 저하시킬 뿐 감독자를 죽이지 않는다. `Blackbox`는 헤더로 분리해
회전 로직을 오프타깃 단위시험(`test_blackbox`, ctest `blackbox`)으로 검증(회전·N개 보존·
`(N+1)` 폐기·append 크기 이월·keep=0 절단, ASan/UBSan clean).

**I/O 오류 반영 + 유효 JSON (2026-08-16, 코드리뷰 P2-02)**: 종전 `write_line`은 `fwrite`/`fflush` 반환값을 무시하고 무조건 `bytes_`를 올려, **ENOSPC·부분 쓰기에서도 정상처럼 보이고** `ok()`가 계속 true였다 — 증거가 실제로는 기록되지 않는데 감독자는 성공으로 오인. 이제 `fwrite`가 요청 길이 전량을 쓰지 못하거나 `fflush`가 실패하면 스트림을 **닫고 `ok()`를 false로 전이**해, 감독자가 다음 폴에서 기록 저하를 감지·로깅한다. 또한 `%f` 포매팅은 NaN/Inf에서 `nan`/`inf`를 출력해 **유효하지 않은 JSON**을 만들 수 있어, 감독자는 블랙박스 라인의 모든 float를 **finite 검사 후 비유한값은 `0`으로 치환**해 JSON 파서가 라인을 버리지 않게 한다(증거 라인의 나머지 필드 보존 우선).

---

## 4. systemd 통합

- 유닛 타입 `notify`, `WatchdogSec=5`. 매 폴 `sd_notify(WATCHDOG=1)` → supervisor 헝 시 systemd가 재시작.
- `RIPOSTE_WITH_SYSTEMD=OFF`(개발 PC) 시 sd_notify 컴파일 배제 — 의존 없이 빌드.

---

## 5. 추적성

| 결정 | 구현 |
|---|---|
| A-4 감독 분리 | 독립 프로세스, 읽기전용 버스 소비 |
| D-1 무권한 감독 | 명령/FC 접근 없음 |
| 기동순서 독립 | `ensure_open()` 지연부착 |
| Recorder(ARCH-001 계승) | JSONL 블랙박스 |

---

## 6. ASSUMPTION / DEFERRED

| 태그 | 항목 |
|---|---|
| ASSUMPTION | 블랙박스 경로 쓰기 가능(`/var/log/riposte/`) |
| DEFERRED | 블랙박스 보존정책·용량관리(로테이션) |
| DEFERRED | 헬스 임계 초과 시 GCS 경보 전송 경로 |
| DEFERRED | 다중 세션 로그 분리·인덱싱 |
