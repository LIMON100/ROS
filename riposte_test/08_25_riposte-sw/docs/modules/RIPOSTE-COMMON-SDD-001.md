# RIPOSTE-COMMON-SDD-001
## Riposte — 공통 라이브러리(L0 플랫폼 · IPC) 모듈 설계서

| 항목 | 내용 |
|---|---|
| 문서 ID | RIPOSTE-COMMON-SDD-001 |
| 버전 | 1.7 (2026-08-18 코드리뷰 반영: C-6 §7 — Config 타입 게터가 "존재하나 미해석" 값을 `parse_failures()`에 기록, 비행 프로세스는 목록 비어 있지 않으면 기동 거부(레거시 인라인 `;` 주석이 안전 한계값을 무음으로 컴파일 기본값화하던 경로 차단) / 1.6: 2026-08-16 심층 코드리뷰 P1-01 반영: CommandBus 틱당 전량 처리 디스패치 §6 — 유효 명령을 틱당 복수 처리해 bad-token 폭주의 안전 명령 기아 차단 / 1.5: 2026-08-16 심층 코드리뷰 P0-03 반영: SeqSlot 페이로드 원자 워드화·benign-race deviation 폐지·writer 사망 fail-safe C-5 §4 / 1.4: 2026-07-04 심층 리뷰: SeqSlot 개방 검증·seq 보존 §4, CommandBus 드레인·권한 강건화 §6, Config 엄격 숫자 파싱 §7 / 1.3: Config INI 로더 G3 강건성 단위시험 §7 / 1.2: SeqSlot TSan 검증·benign race deviation §4 / 1.1: 코딩표준 Rev C) |
| 대상 모듈 | `riposte-sw/common/` (libriposte_common, 헤더온리) |
| 상위 문서 | RIPOSTE-SAD-001 |
| 관련 결정 | A-1(지터 격리) · A-2(SeqSlot IPC) · G1(원자 스냅샷) · D-2(자동제어 세션 금지) |
| 대상 프로세스 | seeker · obc · supervisor · engage-cli **전부** |

---

## 1. 목적 및 범위

`common`은 모든 프로세스가 공유하는 **단일 진실 원천(single source of truth)** 이다. IPC 페이로드 타입, 임계값 상수, 프로세스 간 통신 원시요소(SeqSlot·CommandBus), 시각·설정·로그 인프라를 제공한다. 상위 계층은 오직 `common`에만 의존하며 서로(seeker↔obc)를 직접 참조하지 않는다 — 이로써 A-1(인지/제어 프로세스 분리)이 코드 의존 그래프 수준에서 강제된다.

**설계 원칙**: 헤더온리(INTERFACE 라이브러리). 링크 의존은 `pthread`, `rt`(POSIX shm)만.

---

## 2. 구성 요소

| 파일 | 라인 | 역할 | 핵심 근거 |
|---|---|---|---|
| `Types.h` | 127 | 전 IPC 페이로드 · FSM enum · 명령 패킷 | 단일 진실 원천 |
| `Tunables.h` | 50 | 모든 임계값/상수 집결 | G3.3 매직넘버 금지 |
| `SeqSlot.h` | 179 | shm/local seqlock — 락 없는 최신값 슬롯 | A-2 |
| `CommandBus.h` | 101 | UDS 저빈도 명령 채널 | 핫패스/명령패스 분리 |
| `Config.h` | 95 | INI 설정 로더 | 상수 외부화 |
| `Clock.h` | 32 | 단조시각 + 신선도 가드 | G3 안전 기본값 |
| `Log.h` | 72 | 구조화 로그(단조시각 스탬프) + `errno_str` | 사후분석 |

---

## 3. Types.h — 데이터 계약

프로세스 경계를 넘는 모든 구조체는 **POD(trivially copyable)** 이어야 한다(SeqSlot 요구). 각 버스별 페이로드:

| 타입 | 방향 | 채널 | 프레임 |
|---|---|---|---|
| `TrackState` | seeker → obc·supervisor | shm `SHM_TRACK` | 대상 상대 위치·속도 (**BODY FRD**) |
| `TelemetrySnapshot` | FC → obc (프로세스 내) | LocalSeqSlot | 기체 위치·속도·자세 (NED) |
| `VelocitySetpointNed` | 소스 → FC | — | 속도 명령 (NED) |
| `ObcStatus` | obc → supervisor | shm `SHM_OBC_STATUS` | FSM 상태·위반마스크·지터 |
| `SeekerHealth` | seeker → supervisor | shm `SHM_SEEKER_HEALTH` | fps·추론지연·장애 |
| `ObcCommand` | 오퍼레이터 → obc | UDS `OBC_CMD_SOCKET` | 세션 활성화 승인 + 토큰 |

**설계 결정 C-1 (프레임 규약 명시)**: `TrackState`는 시커가 자세 정보를 갖지 못하므로 **BODY FRD**(x전방·y우·z하) 상대좌표로 발행한다. NED 변환은 OBC의 `GuidanceSource`가 FC 자세로 수행한다(POC: yaw-only). 프레임을 타입 주석에 못박아 좌표계 혼동을 원천 차단.

**핵심 enum**:
- `ObcState` : IDLE·CONNECTING·READY·PRESTREAM·OFFBOARD_ACTIVE·DISENGAGING·FAULT
- `SafetyBit` : SB_TELEM_STALE(SM-1) … SB_BATTERY(SM-9) 비트마스크 (append-only 계약).
  **의도적으로 unscoped enum + uint32_t** — `violation_mask`(u32)와 OR 연산 계약이고,
  비트 위치는 외부(블랙박스·텔레메트리) 고정 계약으로 재번호 금지·append-only (G8.2)
- `ObcCommandType` : ENGAGE · DISENGAGE (`OBC_COMMAND_MAGIC` + opcode 화이트리스트로 유효성 검사)

---

## 4. SeqSlot.h — 무락 최신값 IPC (A-2 핵심)

단일 writer / 다중 reader의 **seqlock** 슬롯. 두 가지 형태:

- `LocalSeqSlot<T>` : 프로세스 내 (MAVSDK 콜백 스레드 → 제어 스레드, G1)
- `ShmSeqSlot<T>` : POSIX 공유메모리, 프로세스 경계 통과 (TrackBus 등)

**프로토콜** (writer):
```
seq: 짝수(안정) → 홀수(seq|1, 기록중) → payload 원자 워드 복사 → 짝수(+1)
```
reader는 읽기 전후 `seq`가 같고 짝수인지 확인, 다르면 최대 8회 재시도. 락 없이 torn-read를 방지하고 항상 최신값을 얻는다.

**설계 결정 C-5 (원자 워드 페이로드 + writer 사망 fail-safe, 2026-08-16 / 심층 코드리뷰 P0-03)**:
- **페이로드는 `std::atomic<uint32_t>` 워드 배열**로 저장하고, write/read는 스택 스테이징 버퍼와 relaxed 워드 단위 원자 복사로 수행한다. 종전의 "plain payload + memcpy" 방식은 seqlock의 재검사(discard)로 실무상 안전했지만 **C++ 추상기계 기준으로는 정의되지 않은 데이터 레이스**였고, 이를 가리는 TSan suppression(`race:SeqSlot.h`)이 SeqSlot 내부의 *진짜* 레이스까지 잠재적으로 숨겼다. 원자 워드화로 형식적 UB가 제거되어 **suppression 자체를 폐지**한다(1.2의 benign-race deviation 폐지). 메모리 오더는 종전과 동일한 Boehm 패턴: writer `seq(홀수, relaxed) → fence(release) → 워드 store(relaxed) → seq(짝수, release)`, reader `seq(acquire) → 워드 load(relaxed) → fence(acquire) → seq 재검사`.
- **기록 중 사망한 writer의 홀수 seq는 복구하지 않는다.** 종전 1.4의 "다음 짝수로 복구"는 찢어진 페이로드를 유효 샘플로 공개하는 결함이었다(복구 시점에 페이로드가 어디까지 복사됐는지 알 수 없다). 새 정책: `open()`은 홀수 seq를 **그대로 두고**, `write()`의 기록-시작 마킹을 `seq|1`로 바꿔 홀수 잔존 상태에서도 프로토콜이 성립하게 한다. 그 결과 reader는 새 writer의 **첫 완전한 write가 끝날 때까지 `read()=false`** 를 보고, 이는 신선도 저하 → SM-7 수렴이라는 시스템의 기존 fail-safe 경로와 정확히 일치한다. 찢어진 값 공개(위험)와 일시적 무샘플(안전) 중 후자를 택한 것이다.

**설계 결정 C-2 (큐 배제)**: 트랙·텔레메트리는 고빈도·최신값우선·단일생산자. 큐는 스테일 누적·지연변동을 유발하므로 배제하고 SeqSlot 사용. `size` 필드로 프로세스 간 ABI 불일치를 개방 시 검출.

**안전장치**: `ensure_open()`으로 지연 부착 지원 — reader(obc)가 writer(seeker)보다 먼저 기동해도 매 틱 재부착 시도, seeker 부재 시 `read()`가 false 반환 → SM-7로 수렴.

`ShmSeqSlot`은 mmap 매핑을 소유하는 RAII 객체로 copy/move 전부 금지(G5.2 — 이중 munmap 방지).

**개방 강건화 (2026-07-04 심층 리뷰 반영)**:
- `open()`은 mmap 전에 `fstat`로 세그먼트 크기(`st_size >= sizeof(Shared)`)를 검증한다 — 상대 프로세스가 `shm_open`과 `ftruncate` 사이에 있거나 잔존 0바이트 세그먼트가 남은 경우 mmap 자체는 성공하지만 첫 접근에서 SIGBUS가 난다. 검증 실패 시 false 반환, reader는 `ensure_open()`으로 다음 틱 재시도.
- **writer 재부착 시 `seq` 보존**: seq를 0으로 리셋하지 않는다 — 리셋하면 살아있는 reader 아래에서 마지막 값이 무효화되고, 재시작한 writer의 seq가 reader가 기억한 값과 우연히 일치하는 seqlock ABA 창이 생긴다. 이전 writer가 기록 중 죽어 seq가 홀수로 남은 경우의 처리는 C-5 참조(복구하지 않고 첫 완전 write까지 무샘플 유지).
- `LocalSeqSlot`도 `ShmSeqSlot`과 동일한 원자 워드 페이로드·동일 프로토콜을 사용한다(C-5).

**동시성 검증 (2026-08-16 갱신)**: `test_seqslot`이 세 층으로 담보한다 — ① 스레드 producer/consumer 스트레스(5만+ read, torn 0), ② `fork()` 기반 **프로세스 경계** 스트레스(자식 writer를 SIGKILL로 임의 시점 사살 포함, torn 0), ③ 기록 중 사망 시나리오(홀수 seq + 오염 페이로드 → 새 writer 부착 후 첫 완전 write 전 `read()=false`, write 후 새 값 관측). 페이로드 원자화(C-5)로 **TSan suppression 없이** TSan 게이트를 통과한다 — 1.2에서 도입했던 benign-race deviation과 `test/tsan.supp`는 폐지되었고, SeqSlot에서 보고되는 어떤 race도 이제 진짜 결함이다.

---

## 5. Clock.h — 시각 기준과 신선도 가드

- `mono_now_ns()` : `CLOCK_MONOTONIC` 기준. **모든 제어·신선도 판정의 시각 기준**.
- `age_ns(now, stamp)` : `stamp > now`면 0 반환.

**설계 결정 C-3 (신선도 언더플로 가드)**: 제어 루프의 `now_ns`는 스케줄된 **예약 시각(deadline)** 이고, 텔레메트리/트랙 타임스탬프는 **실제 도착 시각**이라 예약보다 미세하게 늦을 수 있다. 부호 없는 `now - stamp`가 언더플로하면 거짓 스테일(SM-1/SM-7 오발동)을 유발한다. `age_ns()`가 이를 0으로 클램프한다. *(SIL 통합시험에서 실제 발견·수정된 결함)*

UTC(벽시계)는 FC의 `SYSTEM_TIME`(GPS)을 chrony로 보정하되, **로그 스탬프 용도로만** 사용한다. 제어는 전적으로 단조시각.

---

## 6. CommandBus.h — 명령 채널 (핫패스 분리)

UDS datagram 소켓. `CommandServer`(obc가 바인드, 비블로킹 poll) / `send_command()`(engage-cli).

**설계 결정 C-4 (명령/트랙 채널 물리 분리)**: 트랙은 **상태**(SeqSlot), 명령은 **이벤트**(UDS). 소켓 권한을 `0660`(그룹 제한)으로 두어 임의 프로세스의 제어 세션 명령 주입을 차단. `OBC_COMMAND_MAGIC`·고정 크기·**opcode 화이트리스트**(ENGAGE/DISENGAGE 외 폐기, G15.4 방어적 파싱) 검사로 malformed 패킷 폐기. `CommandServer`는 소켓 fd를 소유하는 RAII 객체로 copy/move 금지(G5.2).

**강건화 (2026-07-04 심층 리뷰 반영)**:
- `poll()`은 `MSG_TRUNC`로 실제 데이터그램 길이를 얻어 **초과 크기 패킷을 절단 수락이 아닌 폐기**하고, 틱당 큐를 **최대 64개(`MAX_DRAIN_PER_POLL`)까지 드레인**한다 — 가비지 폭주가 유효 DISENGAGE를 데이터그램당 1틱씩 지연시키지 못하고, 드레인 상한으로 제어 루프 정체도 차단. 첫 유효 명령을 반환.
- `bind()`를 `umask(0117)`로 감싸 소켓 아이노드가 **생성 순간부터** 0660을 초과하지 못한다(사후 `chmod`만으로는 관대한 프로세스 umask 아래 권한 창이 남는다).
- `send_command()` 클라이언트 소켓은 `SOCK_NONBLOCK` — OBC 수신 큐 포화 시 오퍼레이터 콘솔이 블록되지 않고 실패(false)를 반환한다.
- **틱당 전량 처리 (2026-08-16, 심층 코드리뷰 P1-01)**: `poll()`은 첫 유효 명령 하나만 반환하므로, 구조적으로 유효한 bad-token ENGAGE 폭주(>20 Hz)가 큐 선두를 계속 차지하면 그 뒤의 유효 DISENGAGE가 틱당 1개씩만 소비되어 기아에 빠질 수 있었다. 제어 틱은 이제 **틱당 최대 `MAX_DRAIN_PER_POLL`개의 유효 명령을 전부 꺼내 순서대로 디스패치**한다(`poll()` 반복 호출, 상한 공유). FSM의 명령 처리는 래치(req_engage_/req_disengage_) 기반이라 복수 디스패치가 안전하며, 같은 틱에 ENGAGE와 DISENGAGE가 공존하면 틱 핸들러의 기존 우선순위(disengage 우선)가 그대로 적용된다 — bad-token 명령은 인증에서 개별 거부될 뿐 뒤의 안전 명령을 지연시키지 못한다. 송신측 큐 포화(`EAGAIN`)는 수신측에서 해소 불가 — 빠른 드레인으로 창을 최소화하고, 콘솔이 실패를 보고한다(위 SOCK_NONBLOCK).
- `engage-cli`는 토큰이 필드 길이(`token[32]`, 유효 31자)를 넘으면 **무단 절단 대신 오류 출력 후 종료** — OBC가 절대 매칭할 수 없는 절단 토큰을 조용히 보내 오퍼레이터의 실수를 숨기는 것을 방지.

---

## 7. Tunables.h / Config.h — 상수 관리 (G3.3)

모든 임계값·주기·클램프·IPC 이름을 `Tunables.h` 한 곳에 `constexpr`로 집결(매직넘버 금지). `[CFG]` 주석 표시 항목은 `Config`(INI)로 런타임 오버라이드 가능. 같은 상수를 두 곳에 적지 않는다.

**단위시험 (`test/test_config.cpp`, ctest `config`)**: INI 로더의 **G3 강건성**을 고정 — 파일 부재 시 `load()==false`이고 게터는 기본값 반환, 미존재 키·**불량 숫자값("abc")·빈 값은 0이 아닌 호출자 기본값으로 폴백**(판단불가=안전 기본). 파싱 표면(섹션, `key=value`, `#`/`;` 인라인 주석, 공백 트림, 중복키 last-wins, `=` 없는 줄 무시, bool 변형 `true/1/yes/on`)도 검증. 40 checks, ASan/UBSan·TSan clean.

**엄격 숫자 파싱 (2026-07-04 심층 리뷰 반영)**: `get_int`/`get_double`은 **값 전체가 소비되고 `ERANGE`가 아닐 때만** 유효로 본다 — `"0x10"`(→0), `"1e3"`(int→1), `"8,5"`(→8) 같은 부분 파싱 가능 값이 조용히 오파싱되어 안전 한계값(vmax 등)에 흘러드는 것을 차단하고 기본값으로 폴백. 또한 섹션 헤더 이전에 나온 키는 접두사 없는 **bare key**로 저장되어 조회 가능하다(빈 섹션명으로 `".key"`가 되어 도달 불가가 되는 문제 제거).

**설계 결정 C-6 (미해석 값의 표면화 + 기동 거부, 2026-08-18 코드리뷰 반영)**: 기본값 폴백은 그 한계값 하나에는 fail-closed지만, **파일에 존재하는 값이 조용히 컴파일 기본값으로 대체되는 것** 자체는 운용자가 볼 수 없는 fail-open이다 — 컴파일 기본값은 범위 안이라 하류 validate()가 잡지 못한다(예: 레거시 인라인 `;` 주석 `alt_max = 15.0 ; note`는 현행 문법에서 미해석 → 무음으로 60 m). 타입 게터(`get_double`/`get_int`/`get_bool`)는 이제 "존재하나 미해석"을 `parse_failures()`에 키+원문으로 기록하고(중복 제거, lazy), **시커·OBC는 마지막 config 읽기 후 목록이 비어 있지 않으면 각 항목을 ERROR로 출력하고 기동을 거부**한다 — 잘못 입력된 안전값은 표면화할 운용자 오류이지 짐작할 숫자가 아니다(get_duration_ns와 동일 철학). 부재 키는 기록하지 않는다(기본값 사용은 정상 경로). `fence.polygon`처럼 `get_str`로 읽는 값은 대상이 아니다.

---

## 7.1 Log.h — 구조화 로그 (G10.1)

`RLOG_DEBUG/INFO/WARN/ERROR(tag, fmt, …)` 매크로 파사드. 단조시각 스탬프로 TrackBus/블랙박스와 상관 분석 가능. stderr 쓰기는 **best-effort**(반환값 의도적 무시 — 제어 루프가 로깅에 의존하지 않음). 에러 로그 경로용 **`errno_str(err, buf, len)`** 제공: `std::strerror`는 thread-unsafe(concurrency-mt-unsafe)이므로 GNU `strerror_r` 기반 헬퍼를 공용화(코딩표준 Rev C 적용 시 추가).

---

## 8. 추적성 (Traceability)

| 요구/결정 | 구현 |
|---|---|
| A-1 프로세스 분리 | common만 공유, seeker↔obc 소스 무의존 |
| A-2 SeqSlot IPC | `SeqSlot.h` |
| G1 원자 스냅샷 | `LocalSeqSlot` (텔레메트리) |
| G3 안전 기본값 | `age_ns` 가드, `read()` false → disengage |
| D-2 자동제어 세션 금지 | `ObcCommand` 토큰 필드 + magic |
| G3.3 매직넘버 금지 | `Tunables.h` |

---

## 9. ASSUMPTION / DEFERRED

| 태그 | 항목 |
|---|---|
| ASSUMPTION | `std::atomic<uint32_t>` 무락(SeqSlot 전제) |
| ASSUMPTION | 전 프로세스 동일 바이너리 ABI(구조체 레이아웃 일치) |
| DEFERRED | `Types.h` 계층별 분할(ipc/fsm) — 규모 증가 시 |
| DEFERRED | Config INI → toml++ 전환(설정 표면 확대 시) |
