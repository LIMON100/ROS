# AGENTS.md — Riposte 생산 C++ / 실시간 임베디드 개발 규칙

> **목적**: 이 저장소에서 작업하는 모든 AI 에이전트(Claude Code, Codex 등)와
> 개발자가 시제품이 아니라 **최종 생산품에 통합 가능한 코드**를 일관되게
> 작성하도록 하는 최상위 개발 규칙이다. 범용 사내 템플릿이 아니라 **Riposte
> 저장소의 실제 아키텍처·게이트·안전 불변식에 맞춘 전용판**이다.
>
> **대상 시스템**: RK3588 + Hailo-8 무인기 미션 컴퓨터. 비행체를 제어하는
> **안전-임계 소프트웨어**로, 오퍼레이터 인증(man-in-the-loop) 뒤에서만
> 하위 명령이 나가는 계층 분리 구조를 갖는다.
>
> **기준 환경**: WSL2 Ubuntu 24.04 (개발/SIL) + RK3588 Debian (타깃).
> C++17, CMake. 저장소는 WSL ext4(`~/Project/...`)에 두며 `/mnt/c` 아래에서
> 빌드하지 않는다.
>
> **회사 표준**: 저장소 루트의 `SAN-SW-STD-001_Coding_Rules.md` Rev A가
> 권위 표준이다(`R<section>.<n>`으로 인용). 기존 코드 주석의 `G<n>.<m>`
> 인용은 선행 표준(CPP_Production_Coding_Standard Rev C, 원문 미보유)의
> 유산이다 — **보존하되, 신규 인용은 R-규칙을 사용**한다. 두 체계가 같은
> 관행을 가리키는 경우가 많다(예: G16.1≈format 게이트, G11.2≈호스트
> 단위시험).

---

## 0. 규칙의 강제 수준과 우선순위

- **MUST / 반드시**: 예외 없이 지킨다.
- **SHOULD / 원칙적으로**: 문서화된 기술적 이유가 있을 때만 예외.
- **MAY / 가능**: 상황에 따라 선택.

충돌 시 우선순위:

1. 사용자가 현재 작업에서 명시한 요구사항과 **안전 관련 제한**
2. 승인된 설계문서 — `docs/`의 REQ/SDD/ARCH (§8 문서 체계)
3. 이 문서와 `CLAUDE.md`
4. `SAN-SW-STD-001_Coding_Rules.md`의 필수 규칙과 안전 불변식
5. 저장소의 기존 아키텍처·빌드 체계·코딩 스타일
6. 일반적 모범 사례

기존 관례가 확립된 곳에 새 표준을 도입하지 않는다. 단, **안전 불변식(§2)과
검증 게이트(§6)를 약화하는 예외는 문서화된 근거와 사용자 승인 없이 허용하지
않는다.**

---

## 1. 최우선 개발 원칙

0. **소프트웨어 관점 서술 (MUST)** — 문서·코드·주석·커밋·응답을 전부 SW
   엔지니어링 용어(탐지·추적·상태머신·안전 감시·계층 격리)로 기술한다.
   군사 도메인 색채의 표현을 쓰지 않고, 서술은 물론 **식별자·파일명·시험
   토큰·설정 키까지** 중립 용어를 쓴다(대응표는 `CLAUDE.md` "서술 규약").
   유일한 예외는 오프보드 활성화 동사 `engage` — PX4 표준 어휘이자 외부 계약.
1. **최종 생산코드 우선 (R7.1)** — 즉시 통합 가능한 완전한 구현을 작성한다.
   골격, 빈 함수, placeholder로 작업을 끝내지 않는다. 유일한 예외는 HW
   브링업 대기 항목이며, 그 경우 SDD의 DEFERRED 표와 결과 보고에 "실 HW
   검증 대기"를 명시한다.
2. **검증 없는 완료 선언 금지 (R7.6, R10.3)** — build·ctest·sanitizer를
   실제로 실행하고 실제 결과를 보고한다. 실행하지 못한 검증은 성공으로
   표현하지 않는다. 불리한 수치도 그대로 보고한다.
3. **최소 변경으로 완전하게 해결** — 요구를 충족하는 가장 작은 일관된 변경.
   무관한 리팩터링·전체 재포맷·대규모 개명을 섞지 않는다.
4. **문서-선행 (R8.2)** — 설계 변경은 코드보다 먼저 `docs/`의 해당 REQ/SDD를
   개정한다(§8). 코드와 문서가 어긋난 채로 handoff하지 않는다.
5. **순수 로직 선행** — HW 의존 기능은 "호스트에서 시험 가능한 수학"과
   "장치 호출"로 쪼개고 전자를 먼저 구현·시험한다. 검증된 선례:
   ModelIo↔HailoDetector, HailoNmsParse, AssocCost↔RknnEmbedder,
   SearchScheduler(순수 정책 객체).
6. **기존 패턴 확장 우선 (R10.4)** — 새 형태를 발명하기 전에 유사 sibling
   패턴(기존 탐지기 어댑터, 기존 SM 항목, 기존 시험 하네스)을 찾아 확장한다.
7. **완전성 도전 리뷰 (R7.5)** — "완료"를 그대로 받아들이지 않는다. 누락
   가능성이 높은 실패 경로, 타깃 전용 `#ifdef` 경로, 종료 순서, 경계값을
   적극적으로 탐색하고, 발견한 결함은 질문받기 전에 보고한다.

---

## 2. 안전 불변식 (MUST — 이 시스템의 존재 이유, R6)

각 불변식은 ID로 인용하며(R6.2), 깨는 변경은 즉시 중단하고 보고한다.

- **I1 (오퍼레이터 인증 게이트, D-2)**: 탐지·AI 판단만으로 상위 임무 명령이
  자동 실행되지 않는다(man-in-the-loop). ENGAGE/TARGET 계열 명령은 오퍼레이터
  토큰 인증(`OperatorAuthorization`)을 요구하며, 안전측 명령(DISENGAGE/HOLD/
  RETURN_HOME)만 무인증이다. 미설정 시 default-deny(SI-2).
- **I2 (인지-비행제어 격리, A-1, R6.1)**: `riposte-seeker`는 FC에 절대
  접근하지 않는다. 시커·가속기 장애는 국소 장애로 처리되고 **TrackBus
  신선도 저하(SM-7)를 통해서만** 비행 경로에 전파된다. seeker와 obc는
  `common`만 공유한다 — 컴파일 타임 의존 그래프로 강제되는 경계다.
- **I3 (안전감시 계층 불변)**: SM-1(텔레메트리 신선도)~SM-10(폴리곤 펜스)의
  감시 항목과 트리거 조건은 설계문서 개정 없이 변경하지 않는다. PX4 자체
  failsafe(지오펜스·배터리)는 항상 최종 방벽으로 병행한다.
- **I4 (비신뢰 장치 데이터)**: NPU 출력 버퍼, 임베딩, V4L2 프레임, MAVLink,
  설정 파일 — 프로세스 밖에서 온 모든 바이트는 사용 전 검증한다(길이·범위·
  NaN·프레이밍·float→int 변환 전 유한성). 검증 실패는 **fail-closed**(빈
  결과·강등)이며 그럴듯한 쓰레기를 만들지 않는다. NaN-safe 비교
  (`!(x >= thr)`)를 사용한다.
- **I5 (조용한 기하 오류 방지, S-6)**: bbox 종횡비가 단안 거리로 직결된다.
  종횡비를 왜곡하는 전처리(stretch)는 탐지 경로에 금지(letterbox 유지)하고,
  이 성질은 회귀 시험으로 고정돼 있다 — 시험을 약화하지 않는다. (ReID
  crop만 예외적으로 stretch — 기하를 측정하지 않으므로.)
- **I6 (AI 계층 무결 강등, TR-3)**: T1/T2 추적 보조 AI가 전부 죽어도 T0
  경로가 현행과 바이트 단위로 동일하게 동작한다. 임베더/템플릿의 어떤
  실패도 예외·크래시가 아니라 "무효 임베딩 → 운동 단독"으로 흡수된다.

---

## 3. 아키텍처 규칙

### 3.1 프로세스와 계층 (R1.1)

```
riposte-seeker     L2 인지: 카메라 → Hailo 탐지 → 추적 → 상대추정 → TrackBus
riposte-obc        L3~L5: 20Hz FSM · PN 유도 · 안전감시(SM-x) · MAVSDK
riposte-supervisor L5: 헬스 집계 · JSONL 블랙박스 · systemd watchdog
riposte-engage     도구: 오퍼레이터 콘솔 (토큰 필요)
mavlink-router     L1 외부: GCS↔FC↔OBC 단일 라우팅 허브
```

- 상위 계층은 오직 `common`(헤더온리 INTERFACE, pthread/rt만 링크)에
  의존한다. seeker↔obc 직접 참조 금지(I2). 순환 의존 금지.
- 프로세스 간 통신(R3.2): 핫패스는 `SeqSlot`(shm seqlock, 최신값 슬롯),
  저빈도 명령은 `CommandBus`(UDS). **경계를 넘는 페이로드는 POD**이며,
  shm ABI 변경 시 크기 `static_assert`를 유지하고 리더/라이터를 동시
  배포한다.

### 3.2 하드웨어 격리 (R1.3/R1.4/R9.2 — 검증된 패턴, 반드시 따를 것)

- 모든 vendor SDK(HailoRT·V4L2·librga·rknn_api·MAVSDK·systemd)는
  `RIPOSTE_WITH_*` CMake 옵션 뒤 **단일 TU에 격리**한다. 인터페이스
  (`IDetector`/`ICamera`/`IEmbedder`/`ITemplateTracker`/FcuLink)가 경계이며,
  알고리즘 계층은 vendor 헤더를 직접 include하지 않는다.
- **옵션 전부 OFF = host/SIL 빌드가 항상 성립**해야 한다(Synthetic 대체물).
  이것이 CI의 기본 게이트다(R1.3).
- 새 vendor 의존 추가 시 이 패턴을 복제한다: 옵션 신설 → 인터페이스 →
  Synthetic 스텁 → 장치 TU → `find_path`/`find_library` REQUIRED.
- 참고: Riposte는 R1.2의 "단일 타깃 매크로" 대신 `RIPOSTE_WITH_*` 개별
  옵션을 사용한다(확립된 관례, §0 우선순위 5). 단 옵션 조합이 늘어나
  관리가 어려워지면 R1.2 방식(단일 `RIPOSTE_TARGET`)으로 수렴을 검토한다.
- 상수는 `common/include/riposte/Tunables.h`에 집결한다(매직넘버 금지).
  런타임 재정의 가능한 것은 `[CFG]` 표기 + `config/*.ini` 연결. 프레임
  카운트 상수는 캡처 레이트(`SEEKER_FRAME_HZ`)에 캘리브레이션되어 있다 —
  레이트를 바꾸면 반드시 함께 재조정한다.

### 3.3 언어·스타일 (저장소 확립 관례 + R1.5/R1.6/R7.2/R8.1)

- **C++17** (`-Wall -Wextra -Werror`). C-style cast 금지, `enum class`,
  RAII, 소유 raw pointer 금지, Rule of Zero 우선.
- 파일 `PascalCase.{h,cpp}`, 클래스 `PascalCase`, 함수/변수 `snake_case`,
  상수 `UPPER_SNAKE_CASE`, private 멤버 `trailing_`. 헤더는 `#pragma once`.
  한 파일에 primary class 하나.
- **모든 멤버는 선언 시 초기화한다(R7.2)** — 미초기화 `const`/`bool`/scalar가
  실결함을 만든 이력이 표준에 기록돼 있다.
- 인터페이스 클래스는 복사/이동 삭제(rule of five deleted) — 기존 패턴 유지.
- 예외: HailoRT 등 예외를 던지는 SDK는 **장치 TU의 init()/detect() 경계에서
  전부 catch**해 false로 변환한다. 예외가 파이프라인 루프를 뚫으면 국소
  장애가 supervisor 재시작으로 격상된다.
- 주석은 "왜"를 쓴다. memory-order 근거(R3.3), 안전 불변식(R6.2), UAF
  교훈(S-10), 표준 인용 같은 load-bearing 주석은 삭제·약화하지 않는다
  (R10.2).

---

## 4. 실시간성과 동시성 (R3/R4)

- **경로 분류**: OBC 20 Hz 제어 루프(SM-5 지터 감시)와 유도 종단 지연
  예산(<100 ms)이 critical path다. 시커 파이프라인(~60 Hz)은
  time-sensitive, supervisor/녹화는 background.
- **latest-wins (S-1)**: 밀린 프레임은 버린다(`FRAME_STALE_NS`). 큐잉으로
  지연을 누적시키지 않는다. 새 스테이지를 추가할 때도 이 정책을 따른다.
- **동기 detect() 예산**: 60 fps에서 프레임당 추론 1회(전처리+추론 ≈12 ms
  < 16.7 ms)가 성립 한계다. 프레임당 작업을 추가하려면 별도 스레드 +
  latest-wins로 격리한다(TR-4). 임의 sleep으로 race를 숨기지 않는다.
- **모든 HW 대기에 타임아웃(R4.3)**: `INFER_TIMEOUT_NS` 등 유한 타임아웃 +
  연속 실패 카운터 → `healthy()` 강하 → HealthBus/SM 전파가 표준 패턴이다.
- **버퍼 수명**: 타임아웃으로 포기한 뒤에도 장치가 DMA 중일 수 있다 —
  스테이징 버퍼는 detached 작업보다 오래 살아야 한다(S-10, 실 UAF 교훈).
- **시각**: 단조시각(`CLOCK_MONOTONIC`, `Clock.h`)만 사용. 신선도 판정은
  `now - stamp > threshold` + fresh-boot(0) 예외(R5.3), 공용 `age_ns()`
  사용(언더플로 방지 — F-12 교훈).
- **atomic 규율(R3.3/R3.6)**: 모든 atomic의 memory order에 근거 주석.
  기본 패턴은 data write → release stamp / acquire stamp → data read.
  SeqSlot seqlock의 의도적 benign race는 `test/tsan.supp`로 필터되는 검증된
  deviation이다 — 변경 시 TSan 결과와 correctness 근거를 함께 제시한다.
- detached thread 금지, CV는 predicate와 함께, 종료는 producer 정지 →
  consumer join → 자원 해제 순서.

---

## 5. 인지·AI 파이프라인 규칙 (S-x 결정 준수)

- Capture → Search(ROI) → Infer → Track → Estimate → Publish 스테이지
  분리를 유지한다. 프레임에는 입력 즉시 단조시각을 스탬프한다.
- **S-6**: 모델 입력은 종횡비 보존 letterbox. 패딩 검출 기각. NMS는 모델
  공간에서. (I5)
- **S-7**: 제품 표준 HEF는 NMS-on-device. 출력 포맷은 init()에서 자동
  감지하고, 모델 I/O 형상은 config 기대치가 아니라 **로드된 모델이 진실**
  이다 — 불일치는 fail-closed. 모델 세대 교체 시 호스트 코드 불변이
  계약이다.
- **S-8**: 디바이스 임계는 낮게, 운용 임계는 호스트에서 프레임마다.
- **S-11 / R-10**: 캡처 레이트는 고정, 추론 슬롯 배분만 적응. **비행 중
  센서 모드 전환 금지**(V4L2 재협상 공백이 coast 한도를 넘을 수 있음).
- **T0/T1/T2 (TRACKER-REQ)**: 탐지(Hailo)가 항상 권위. 운동 게이트가
  연관의 권위(외관은 게이트 안 재순위/기각만, TR-6). 템플릿 출력은 대체·
  교차검증 전용이며 트랙 필터로 되먹이지 않는다.
- 모델 파일(.hef/.rknn)은 커밋하지 않는다.

---

## 6. 검증 게이트 (MUST — 커밋 전, R2/R7.4)

```bash
# 통합 게이트: format → host build(-Werror) + ctest → tidy → ASan/UBSan → TSan
bash riposte-sw/ci/run_gates.sh

# 개별
cmake -S riposte-sw -B riposte-sw/build -DCMAKE_BUILD_TYPE=Release
cmake --build riposte-sw/build -j
ctest --test-dir riposte-sw/build --output-on-failure
bash riposte-sw/test/run_sanitizers.sh
```

- host 빌드는 `RIPOSTE_WITH_*` 전부 OFF가 기본(R1.3/R2.3). **멀티스레드
  변경은 TSan clean이 필수 게이트**다(R7.4; ASan과 별도 빌드 디렉터리).
- pre-commit 훅(`core.hooksPath=riposte-sw/ci/hooks`)이 스테이지 파일의
  format/tidy를 검사한다. `--no-verify`는 예외 상황에만.
- **타깃 real-path 게이트의 알려진 갭(R2.2 대비)**: `RIPOSTE_WITH_*=ON`
  TU는 vendor 헤더(HailoRT·rknn_api·librga) 없이는 호스트에서 문법 검사도
  불가하다. R2.2가 요구하는 문법 게이트를 현재 충족하지 못하므로, 해당
  TU를 수정하면 결과 보고에 **"실 HW 브링업 검증 대기"를 반드시 명시**하고
  검증된 것처럼 표현하지 않는다. 개선 과제: vendor 스텁 헤더 기반
  `-fsyntax-only` 게이트 추가(SEEKER-SDD 브링업 체크리스트와 연동).
- clang-format/clang-tidy/cppcheck가 로컬에 없으면 게이트가 스킵을
  보고한다 — 스킵 사실을 결과 보고에 포함한다(설치는 사용자 승인 필요).

### 6.1 테스트 규칙 (R2.3)

- 시험은 `riposte-sw/test/`의 **의존성-제로 CHECK 하네스**(gtest 아님)를
  따른다: 테스트 함수는 `int` 반환, `CHECK` 매크로, main에서 체인, 통과
  개수 출력.
- 신규 로직은 단위시험과 **같은 커밋**으로 작성한다. 특히: 경계값, 잘린/
  손상 입력, NaN, 프레이밍 파괴, 타임아웃, 폴백 경로, 그리고 **그 로직의
  존재 이유가 되는 시나리오**(예: 교차 ID 유지, 늘림 회귀 방지)를 종단으로
  고정한다.
- 실패하는 시험을 skip·삭제·완화로 통과시키지 않는다. 시험은 결정적이어야
  하며 실제 sleep에 의존하지 않는다.
- 검증 사다리: SIL(Synthetic*, 정책·플럼빙) → Gazebo(S-G*, 시나리오) →
  실비행. PX4 SITL 스크립트(`test/sitl/`)는 PX4 빌드 환경 전용이다.

---

## 7. Fault / Health / 로깅 (R5)

- 장애 전파의 표준 경로: 컴포넌트 `healthy()` → HealthBus(~2 Hz) →
  supervisor 집계 + JSONL 블랙박스. 비행 안전 관련 판정은 SM-x가
  소유한다(R5.1 — 단일 소유자).
- shm 페이로드 필드는 텔레메트리·블랙박스 디코더가 의존한다 — 기존 필드
  재배치 금지, 신규는 패딩/말미에 append(R5.2의 정신).
- 로그는 `RLOG_*`(구조화, 단조시각 스탬프). 실시간 경로에서 고빈도 로그
  금지(R4.2). 오류를 조용히 무시하지 않되, 비행 입력이 아닌 실패(status
  bus 등)는 경고 후 계속이 관례다.
- 반복 결함의 교훈은 F-x ID로 기록돼 있다(DUALEO-REQ §8.1) — 유사 패턴을
  만들 때 먼저 확인한다.

---

## 8. 문서 체계와 추적성 (R8.2/R8.3)

- **문서 지도**: `docs/RIPOSTE-SW-ARCH-*`(상위) · `RIPOSTE-OBC-SDD-*` ·
  `RIPOSTE-DUALEO-REQ-001`(이중 EO, R-x/T-x) · `RIPOSTE-TRACKER-REQ-001`
  (이기종 AI 추적, TR-x/K-x) · `RIPOSTE-LENS-REQ-001`(렌즈, L-*/V-x) ·
  `RIPOSTE-GAZEBO-*` · `docs/modules/RIPOSTE-{SEEKER,COMMON,COMMS,
  SUPERVISOR}-SDD-001`.
- **ID 체계**: `S-x`(SDD 설계 결정) · `SM-x`(안전 감시) · `R-x/T-x/TR-x/
  L-x/V-x/K-x`(요구/시험/리스크) · `F-x`(결함) · `I<n>`(§2 안전 불변식) ·
  `R<s>.<n>`(SAN 표준) · `G<n>.<m>`(선행 표준, 유산). 코드 주석·커밋·
  보고에 인용한다.
- 설계 변경 절차: ① 해당 문서의 본문 개정 ② 헤더 표 '버전' 행에 최신-앞
  changelog 추가 ③ ASSUMPTION/DEFERRED 표 갱신 ④ 구현 ⑤ 진행 결과를
  문서의 진행 절에 수치와 함께 기록.
- 브링업 체크리스트(예: SEEKER-SDD §4.4.5)는 실 HW 검증 항목의 단일
  출처다 — HW 의존 코드를 추가하면 여기에 확인 항목을 추가한다.
- 도구 위생(R8.4): 헬퍼 스크립트에 표준 라이브러리를 가리는 이름
  (`inspect.py` 등) 금지.

---

## 9. Git 규칙 (R10.1)

- **커밋·푸시는 사용자가 요청할 때만.** 변경은 요구/규칙 ID 단위의 작은
  reviewable unit으로 구성한다.
- 커밋에 관련 시험·문서를 **같이 포함**한다. 경로 지정 add로 파일을
  누락하지 말고 `git status`로 전수 확인한다.
- 원격: `https://github.com/adasone/riposte-sw` (프라이빗). 인증은 gh CLI
  https — `~/.ssh/skyautonet_ed25519`는 사내 서버용이지 GitHub용이 아니다.
- 바이너리·빌드 산출물·모델 파일(.hef/.rknn)·시크릿 커밋 금지.
- `git reset --hard`, `git clean -fd`, 강제 push 금지. 사용자 변경사항을
  덮어쓰지 않는다. 커밋 메시지는 영어 명령형 제목 + 근거 본문.
- 브랜치: `feat/*`에서 작업, `main`으로 병합.

---

## 10. 표준 실행 절차

**단계 1 — Preflight (R10.2)**: `git status --short` → 해당 REQ/SDD와 관련
코드 읽기(공개 헤더·Tunables·안전 불변식 주석 우선) → 유사 sibling 패턴
탐색 → 변경이 해결하는 요구/결함/규칙 ID를 한 문장으로 정의.

**단계 2 — 설계**: 필요 시 문서 먼저 개정(§8). 데이터 소유권·스레드 경계·
오류 경로·SM 전파를 확인. 가장 작은 완전한 해결책 선택.

**단계 3 — 구현**: 기존 패턴·명명 유지, 생산급 오류 처리, 시험 동시 작성.

**단계 4 — 검증 (R10.3)**: §6 게이트 전부. 각 명령의 실제 결과를 기록.

**단계 5 — 자체 리뷰**: 최종 diff에서 수명/소유권, race/종료 순서/atomic
페어링, 정수 변환/경계, 타임아웃/큐 상한, shm ABI 호환, 실시간 경로 할당,
fail-safe, 시험 누락, 문서 불일치를 재검토.

**단계 6 — 결과 보고 (R8.3)** (한국어, 간결·정확):
① 구현 내용과 해결한 요구/규칙 ID ② 변경 파일과 explicit delta
③ 실행한 게이트와 실제 결과 ④ 주요 설계 결정과 안전 판단
⑤ **검증하지 못한 항목**(브링업 대기 등)과 남은 위험 ⑥ DoD 충족 여부.

### Definition of Done (변경별 체크리스트)

- [ ] 요구/결함/규칙 ID가 명확하다
- [ ] 해당 문서(REQ/SDD)가 코드와 일치하게 개정되었다
- [ ] placeholder/TODO/stub을 deliverable로 남기지 않았다 (HW 브링업 대기
      항목은 DEFERRED 표에 명시) (R7.1)
- [ ] host 빌드(-Werror, 옵션 전부 OFF) + ctest 전체 통과 (R1.3/R2.3)
- [ ] ASan/UBSan clean; 멀티스레드 변경은 TSan clean (R7.4)
- [ ] 안전 불변식 I1~I6을 깨지 않았음을 확인했다 (R6)
- [ ] `RIPOSTE_WITH_*` TU 변경 시 "브링업 검증 대기"를 보고에 명시했다 (R2.2 갭)
- [ ] `git diff`에 무관한 변경·전체 재포맷·사용자 변경 덮어쓰기가 없다
- [ ] 미검증 항목과 한계를 숨기지 않고 보고했다 (R7.6)
