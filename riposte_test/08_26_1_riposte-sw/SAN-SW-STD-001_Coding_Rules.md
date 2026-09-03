# SAN-SW-STD-001 — C++ / Real-Time Embedded Coding Standard

**Rev A · 2026-06 · SkyAutoNet — Riposte 적응판 (2026-08-15)**

전사 표준 Rev A(AIRYS Main2 리팩터에서 도출)를 Riposte 저장소 맥락으로 옮긴
판이다. **규칙 번호(`R<section>.<n>`)와 규칙 문장은 원 표준과 동일하게 보존**
하여 추적성을 유지하고, 각 규칙을 설명하는 예시만 Riposte의 실제 구조로
교체했다. 원 표준의 AIRYS 사례는 그 규칙이 인코딩한 "교훈"으로서 근거가
있으므로 필요한 곳에는 근거로 병기한다. 규칙은 명령형=필수, "prefer/should"=
문서화된 예외 경로가 있는 강한 기본값이다. Riposte 특화 적용·안전 불변식
(I1~I6)의 상세는 저장소 루트 `AGENTS.md`가 담당하며 각 R-규칙을 교차 인용한다.

---

## 1. Project structure & build system

- **R1.1** subsystem마다 **static library 하나**, leaf→up 단방향 의존, 순환
  금지. Riposte layering: `common(헤더온리 leaf) → {seeker, obc, supervisor}
  → exe`. seeker↔obc 직접 참조 금지 — 오직 `common`만 공유한다(안전 불변식
  I2가 여기서 컴파일 타임에 강제된다).
- **R1.2** 하나의 **타깃 매크로가 모든 backend feature flag를 한 옵션 파일에서
  구동**한다. Riposte는 확립된 관례로 `RIPOSTE_WITH_{HAILO,V4L2,RGA,RKNN,
  MAVSDK,SYSTEMD,GZ}` 개별 옵션(루트 `CMakeLists.txt`)을 사용한다 — 원
  표준의 단일 `AIRYS_TARGET` 방식과 다르나, 옵션을 여러 CMake 파일에서
  독립적으로 켜지 않는다는 정신은 동일하다. 옵션 조합 관리가 어려워지면
  단일 `RIPOSTE_TARGET` 매크로로 수렴을 검토한다.
- **R1.3** **Host target = 모든 backend OFF (stub path).** host 빌드는 vendor/
  HW 의존 0으로 compile·unit-test된다. Riposte에서 옵션 전부 OFF면
  `SyntheticCamera`/`SyntheticDetector`/`SyntheticEmbedder`/SIL FcuLink가
  대체한다 — CI와 개발 PC의 기본 게이트다.
- **R1.4** real/stub variant는 **`#if`로 전환되는 단일 TU** 안에 둔다(평행
  파일 금지). Riposte: `HailoDetector.cpp`가 `RIPOSTE_WITH_HAILO` 뒤에서
  장치 호출만 담고, host는 인터페이스(`IDetector`) 뒤 Synthetic 구현으로
  간다. (인터페이스 경계가 명확한 곳은 별도 Synthetic 파일이 관례이나,
  실/스텁 로직이 한 TU에 섞이는 곳은 이 규칙을 따른다.)
- **R1.5** 모든 헤더에 `#pragma once`. **leaf 헤더**(domain 타입, stdlib만
  의존)는 어디서나 include 가능해야 한다. Riposte: `common/include/riposte/`
  (`Types.h`·`Tunables.h`·`SeqSlot.h`·`Clock.h`…)가 leaf이며 vendor SDK나
  상위 subsystem 헤더를 포함하지 않는다.
- **R1.6** 한 파일에 primary class 하나. **PascalCase** 파일·타입명이 클래스명과
  일치.

## 2. Build verification gate (MANDATORY — non-negotiable)

- **R2.1** 모든 변경은 zip/release/PR merge 전에 **두 게이트**를 통과한다:
  1. host build: `cmake -S riposte-sw -B build -DCMAKE_BUILD_TYPE=Release &&
     cmake --build build -j` (옵션 전부 OFF)
  2. **target real-path 문법 검사** (아래 R2.2 참조)
  Riposte 통합 게이트: `bash riposte-sw/ci/run_gates.sh` (format → host
  build+ctest → tidy → ASan/UBSan → TSan).
- **R2.2** **target 문법 검사는 선택이 아니다.** host stub은 모든 flag-gated
  라인을 숨긴다 — 한 code path에만 존재하는 멤버는 host에서 안 보이고 real
  flag에서만 실패한다. *원 표준에서 이 게이트는 실제 리팩터 누락을 잡았고
  생략이 여러 사이클을 태웠다.* **Riposte의 알려진 갭**: `RIPOSTE_WITH_*=ON`
  TU는 vendor 헤더(HailoRT·rknn_api·librga)가 있어야 `-fsyntax-only`조차
  가능하다 — 현재 이 환경에 헤더가 없어 이 게이트를 충족하지 못한다. 따라서
  해당 TU를 수정하면 결과 보고에 **"실 HW 브링업 검증 대기"를 명시**하고
  검증된 것처럼 표현하지 않는다. 개선 과제: vendor 스텁 헤더 기반 문법
  게이트 추가(SEEKER-SDD 브링업 체크리스트 연동).
- **R2.3** 게이트의 일부로 host `ctest`(stub flags)를 돌린다. Riposte:
  `ctest --test-dir riposte-sw/build --output-on-failure`. 빌드·시험 안 된
  코드를 handoff하지 않는다.

## 3. Shared state & concurrency

- **R3.1** 공유 상태는 flat god-struct가 아니라 **subsystem별 substate의
  aggregate**다. Riposte는 god-struct를 두지 않고 프로세스별로 shm 슬롯을
  분리한다(`SHM_TRACK`·`SHM_OBC_STATUS`·`SHM_SEEKER_HEALTH`·`SHM_GPS`) —
  각 페이로드 타입이 그 도메인을 소유한다(`Types.h`). (원 표준의 단일 프로세스
  `SharedState{camera;ai;fire;...}`에 대응하는, 다중 프로세스판 적용이다.)
- **R3.2** thread/process 간 단일 값 전달은 **typed, seq-versioned slot**을
  쓴다(bare pointer + ad-hoc flag 금지). Riposte `SeqSlot`(`SeqSlot.h`)이
  정확히 이것이다 — shm/local seqlock, 최신값 슬롯, seq 카운터로 수신측이
  갱신을 판별한다. 강타입 슬롯이 시그니처 오류를 컴파일 타임에 잡는다.
- **R3.3** **모든 atomic의 memory order를 문서화·정당화한다.** 기본 규율:
  producer는 data field write 후 "ready" stamp/seq에 **release** store,
  consumer는 stamp를 **acquire** load 후 data를 읽는다, 순수 통계 counter는
  **relaxed**. Riposte `SeqSlot`의 seqlock이 이 패턴이며, 의도적 benign race는
  `test/tsan.supp`로 필터되는 검증된 deviation이다(변경 시 TSan 결과+근거
  동반).
- **R3.4** high-rate lossless stream은 **bounded SPSC ring**(`try_push`/
  `try_pop_batch`, `total_dropped`/`approx_size` 노출)을 쓴다. Riposte의
  카메라 프레임 경로는 무손실이 아니라 **latest-wins(S-1)** 정책이다 — 밀린
  프레임은 의도적으로 버리고(`FRAME_STALE_NS`) 항상 최신만 처리해 유도 종단
  지연 예산(<100 ms)을 보호한다. 무손실 스트림을 신설한다면 이 규칙의 SPSC
  ring을 쓰고 drop 카운터를 HealthBus에 노출한다.
- **R3.5** `std::recursive_mutex`는 **public method가 정당하게 다른 locking
  public method로 재진입하는 경우에만** 쓰고 재진입 경로를 주석한다. 그 외엔
  일반 mutex. 재진입 API에 일반 mutex는 self-deadlock한다. (Riposte 현행
  코드는 재진입 락이 없다 — 신설 시 이 규칙을 적용.)
- **R3.6** side-effecting producer를 가진 flag word의 외부 reader는
  **acquire**, producer는 **release**. relaxed read가 flag 검사를 지나쳐
  stale snapshot을 관측하지 않게 한다.

## 4. Real-time task pattern

- **R4.1** 모든 worker thread는 **공통 Task base**에서 파생하고
  `Config{name, cpu_index, rt_priority}`와 `on_init/run/on_stop/
  stop_requested`를 갖는다. thread name·CPU affinity·scheduler policy는
  base에서 한 번 설정한다. (Riposte POC는 seeker 파이프라인을 단일 스레드로
  배선한다 — InferThread 분리(TR-4) 시 이 공통 base를 도입한다.)
- **R4.2** **hot-path callback은 최소 작업만: publish + atomic store.** logging과
  무거운 처리는 throttling된 outer loop로 넘기고 per-sample time budget을
  주석한다. Riposte: 카메라 콜백/버스 발행 경로가 이에 해당하며, RK NPU
  작업을 프레임 경로에 직렬로 붙이지 않는다(§동기 detect() 예산, TR-4).
- **R4.3** **HW에 무제한 blocking 금지.** 모든 장치 대기에 timeout과 만료 시
  fault. Riposte: `INFER_TIMEOUT_NS`(Hailo async), V4L2 `poll` timeout,
  MAVSDK connect timeout — 연속 실패 카운터가 `healthy()`를 강하시킨다. *원
  표준 교훈: 무제한 대기 루프가 bring-up을 hang시킨 사례.*
- **R4.4** queue가 비면 busy-spin 대신 producer batch period보다 짧은 측정된
  backoff. 임의 sleep으로 race를 숨기지 않는다.

## 5. Fault / health monitoring

- **R5.1** **단일 owner(하나의 monitor)가 fault 상태의 single source of
  truth**를 소유한다. 컴포넌트는 자신만 고유하게 검출하는 fault만 직접
  설정하고(예: 모델 로드 실패), liveness/rate fault는 monitor가 담당한다.
  Riposte: 컴포넌트가 `healthy()`를 노출하고, `riposte-supervisor`가 HealthBus를
  집계하며, 비행 안전 판정은 OBC의 SafetyMonitor(SM-1~SM-10)가 소유한다.
- **R5.2** shm/telemetry 페이로드 필드는 **안정적 위치 — 재배치 금지.**
  telemetry·블랙박스·외부 디코더가 의존한다. 신규 필드는 기존 패딩 또는
  말미에 append한다. Riposte: `TrackState` shm ABI는 크기 `static_assert`로
  고정되어 있고 `num_targets`를 기존 패딩에 배치해 크기를 유지했다(P6). *원
  표준: `FaultFlags.h` bit 재번호 금지 — telemetry 호환.*
- **R5.3** **Liveness** = `now − last_*_ns > silent_threshold`. `last == 0`은
  fresh-boot(fault 아님)로 취급해 첫 sample 전에 트립하지 않는다. producer는
  monitor가 읽는 것과 같은 clock(`steady_clock`)에 스탬프한다. Riposte:
  단조시각(`Clock.h`) + 공용 `age_ns()`(언더플로 방지, F-12 교훈). SM-1
  (텔레메트리)·SM-7(트랙 신선도)이 이 패턴이다.
- **R5.4** **Rate-based fault** = poll당 cumulative-counter delta + hysteresis:
  interval delta가 threshold 초과 시 set, N회 연속 clean poll 후에만 clear.
  Riposte: SM-5(제어 루프 지터, 연속 위반 카운트)가 이 패턴을 따른다.
- **R5.5** 외부 notification은 edge-trigger(0→1, 1→0에 한 번씩), 매 poll 아님.

## 6. Safety invariants (compile-time)

- **R6.1** **안전-임계 계층 분리는 컴파일 타임에 강제한다**(관례·리뷰 아님).
  상위 자율 계층이 하위 출력 계층에 직접 도달해선 안 되면, 도달할 **linkage
  경로/타입이 아예 없게** 하여 위반이 빌드 실패가 되게 한다. Riposte 적용:
  **I2** — `riposte-seeker`(인지 계층)는 `common`에만 의존하고 FC 링크·MAVSDK
  타입에 접근 경로가 없다. 시커가 비행제어에 도달하는 코드는 컴파일되지
  않는다. 인지 출력은 TrackBus 발행까지이며, 그 뒤 명령은 OBC의 오퍼레이터
  인증 게이트(**I1**, man-in-the-loop)를 통해서만 나간다. (*원 표준의 교훈:
  자율 계층 출력을 표시 계층까지로 제한하고, 하위 출력은 인증 경로 뒤에만
  두는 컴파일 타임 분리.*)
- **R6.2** 각 불변식을 ID와 한 줄 contract로 관련 module 상단에 명시하고
  설계문서에서 교차 참조한다. Riposte 불변식 I1~I6은 `AGENTS.md §2`에
  집결되어 있으며 코드는 해당 결정 ID(D-2·A-1·SM-7·S-6·TR-3·S-10)를 주석에
  인용한다.
- **R6.3** 의도적 safe default는 그렇게 문서화한다(gated 조건 전까지 inert한
  capability 등). Riposte: `RIPOSTE_WITH_*` OFF의 Synthetic 대체물, engage
  토큰 미설정 시 default-deny(D-2/SI-2) — "아직 X를 안 한다"가 버그로
  오인되지 않게 한다.

## 7. Code quality & review

- **R7.1** **deliverable은 생산 품질: stub·TODO-as-deliverable 금지.** 기존
  baseline stub의 충실한 이식은 허용하나 이식임을 표기한다. Riposte: HW
  브링업 대기 항목은 SDD의 DEFERRED 표에 명시하는 것만 예외다.
- **R7.2** **모든 멤버를 초기화한다.** 미초기화 `const`/`bool`/scalar는 비결정
  동작을 만든다. in-class initializer 우선. *원 표준 교훈: 미초기화
  `const bool`이 무작위 ACK/NACK를 냈다.* Riposte: `Detection d{}` 등 값
  초기화가 관례.
- **R7.3** **peripheral/HW 설정을 권위 문서로 검증**한다(기억 아님). 데이터
  시트/RCN의 ID·section을 코드 주석에 인용하고 superseded는 표시한다.
  Riposte: HEF 출력 포맷·V4L2 픽셀 포맷·RK NPU 텐서 형상은 **로드된
  모델/드라이버가 진실**이며 config 기대치와 불일치 시 fail-closed(S-7).
  *원 표준: SPI DataSize 오설정, 방향은 RCN이 지배.*
- **R7.4** **정적분석 + sanitizer를 HW 전에 실행:** cppcheck·ASan·**TSan**·
  Valgrind. TSan은 수동/리뷰가 놓치는 data race를 드러낸다 — 멀티스레드
  변경의 clean TSan을 게이트로 취급한다. Riposte: `bash test/run_sanitizers.sh`
  (ASan+UBSan / TSan 별도 빌드).
- **R7.5** **완전성을 도전하는 리뷰.** "완료"를 받아들이지 않고 누락 case를
  탐색한다 — 각 probe는 실제 결함을 사냥한다. 질문받기 전에 능동적으로
  보고한다.
- **R7.6** **낙관적 프레이밍보다 정직한 정량 분석.** hard limit(정보이론적
  불가, thermal/runtime 예산 실패, 선행기술 차단)을 그대로 진술한다. 불리하되
  정확한 결론이 유리하되 틀린 것보다 낫다.

## 8. Naming & document-driven engineering

- **R8.1** 타입/파일 PascalCase; compile-time 상수 UPPER_SNAKE; namespace
  소문자. (Riposte: 함수/변수 snake_case, private 멤버 `trailing_`.)
- **R8.2** 모든 주요 설계 결정·단계·리뷰는 **번호 문서**(`SAN-[domain]-
  [type]-[seq]`)로 기록하고 URD/SDD/TST/OPS를 교차 참조한다. 코드 주석은
  requirement ID를 인용한다. Riposte 문서 세트: `RIPOSTE-SW-ARCH-*` ·
  `RIPOSTE-{OBC,SEEKER,COMMON,COMMS,SUPERVISOR}-SDD-*` · `RIPOSTE-DUALEO-
  REQ-*` · `RIPOSTE-TRACKER-REQ-*` · `RIPOSTE-LENS-REQ-*` · `RIPOSTE-GAZEBO-*`.
  ID 체계: S-x·SM-x·R-x/T-x/TR-x·F-x·I-x.
- **R8.3** 변경은 **explicit delta + Definition of Done**(build gate·ctest·
  sanitizer 상태·DoD 체크리스트)으로 handoff한다 — 불투명한 diff 금지.
  Riposte DoD는 `AGENTS.md §10`에 있다.
- **R8.4** tooling 위생: 헬퍼/진단 script를 stdlib module 이름으로 짓지 않는다
  (`inspect.py`가 Python `inspect`를 가림). SVG/XML text의 raw `&`를
  escape한다.

## 9. Cross-product reuse & portability

- **R9.1** subsystem이 제품 간 이식되도록 **공통 BSP/HAL**을 유지한다(AIRYS/
  HawkEye/Riposte 간 ~80–100% 재사용 목표). 플랫폼 코드는 타깃 flag(R1.2)와
  HAL 경계 뒤에 둔다. Riposte 인터페이스(`IDetector`/`ICamera`/`IEmbedder`/
  `ITemplateTracker`)가 이 HAL 경계다 — SkyHunter/구 Riposte의 Hailo 통합이
  이 경계를 통해 재사용되었다.
- **R9.2** algorithm/domain 코드는 플랫폼 무관하며 leaf domain 타입(R1.5)에만
  의존한다 — HAL 위에서 vendor SDK를 직접 include하지 않는다. Riposte:
  ModelIo·AssocCost·SearchScheduler·Tracker는 vendor-free라 호스트에서
  단위시험된다.
- **R9.3** 새 SoC/accelerator 평가는 **dual-track**: 검증된 플랫폼을 Track A로
  유지하고 새 것을 날짜 있는 Go/No-Go PoC(Track B)로 격리한다. Riposte:
  RK3588 NPU 추적 보조 AI(TRACKER-REQ)는 Hailo 탐지 경로(검증 트랙)를 건드리지
  않고 별도 트랙으로 도입하며, K-x 리스크로 Go/No-Go를 관리한다.

## 10. Working agreement under Claude Code (CLI)

- **R10.1** **in-place 편집**, 규칙/요구를 명시한 메시지로 **작은 reviewable
  unit** 커밋. (Riposte: 커밋·푸시는 사용자 요청 시만.)
- **R10.2** **편집 전 관련 subsystem 헤더를 읽고**, memory-ordering·invariant
  주석(R3.3, R6.2)을 **보존**한다. load-bearing이다.
- **R10.3** 매 변경 후 **build gate(host + target) + ctest**(R2)를 돌린다.
  작업 간 트리를 빌드 안 된 상태로 두지 않는다.
- **R10.4** 새 형태를 발명하기보다 **기존 패턴을 확장**한다(sibling fault
  check, sibling task). 코드베이스 전반의 일관성이 기능이다.

---

### Adoption checklist (Riposte 현황)

- [x] host = 모든 backend OFF stub path (R1.2–R1.3) — SIL 빌드 성립
- [~] CI: host build + ctest (R2.1/R2.3) 완비; **target 문법 게이트는 갭**(R2.2)
- [x] shm 슬롯 분리 aggregate; SeqSlot atomic order 문서화 (R3.1–R3.3)
- [ ] 공통 Task base — InferThread 분리(TR-4) 시 도입 (R4.1–R4.2)
- [x] 단일 fault owner(supervisor/SM); 안정 shm ABI; SM-5 delta+hysteresis (R5)
- [x] 안전 불변식 I1~I6 열거·컴파일 타임 강제 (R6) — AGENTS.md §2
- [~] cppcheck/ASan/TSan wired (R7.4); 로컬 clang-tidy/cppcheck 미설치(CI 커버)
- [x] SAN 문서 세트 + per-change DoD 교차 참조 (R8)

*원 전사 표준은 각 repo 루트에 복사해 Claude Code 컨텍스트 파일로도 쓴다.
이 판은 Riposte 예시로 적응했으나 규칙 번호·문장은 Rev A와 동일하다 —
규칙 자체는 제품 간 이식된다.*
