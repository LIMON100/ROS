# Riposte SW (재설계 Rev.2)

RK3588 + Hailo-8 미션 컴퓨터의 전체 소프트웨어. 설계 근거는
`../docs/RIPOSTE-SAD-001.md`, L3 제어 상세는 `../docs/modules/RIPOSTE-OBC-SDD-002.md`
(모듈별 설계서는 `../docs/modules/` 참조).

## 구성

```
Telemetry(GCS) ──SiK 433MHz──USB──▶ RK3588 [seeker + obc + supervisor + mavlink-router]
                                       │ Ethernet(MAVLink UDP)
                                       ▼
                                  Pixhawk 6X / PX4  ◀── GPS(GNSS+Compass) 직결
```

- **GPS는 Pixhawk에 직결한다** (RK3588 아님). 근거: 비행 안전 독립성 — RK3588이
  죽어도 EKF2/RTL/지오펜스가 정상 동작. 상세는 SAD-001 §4.

## 프로세스

| 실행 파일 | 계층 | 역할 |
|---|---|---|
| `riposte-seeker` | L2 인지 | 카메라 → Hailo 탐지 → 추적 → 상대추정 → TrackBus |
| `riposte-obc` | L3+L4+L5 | 20Hz FSM · PN 유도 · 안전감시 · MAVSDK, 세션 활성화 승인 |
| `riposte-supervisor` | L5 | 헬스 집계 · JSONL 블랙박스 · systemd watchdog |
| `riposte-engage` | 도구 | 오퍼레이터 콘솔(engage/disengage, 토큰 필요) |
| `mavlink-router` | L1 | 외부 — GCS(USB)↔FC(Eth)↔OBC(UDP) 단일 라우팅 |

## 빌드

개발 PC(SIL, 하드웨어 없이 전체 파이프라인 검증):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build          # Safety · SeqSlot · Seeker · Guidance · Authz · Config · Recorder · Attitude · Blackbox 단위시험
```

타깃(RK3588, 하드웨어 연동):

```bash
cmake -S . -B build \
  -DRIPOSTE_WITH_HAILO=ON -DRIPOSTE_WITH_MAVSDK=ON \
  -DRIPOSTE_WITH_V4L2=ON -DRIPOSTE_WITH_SYSTEMD=ON
```

HW 의존부(HailoRT · MAVSDK · V4L2)는 CMake 옵션 뒤로 격리되어, 옵션 OFF 시
`SyntheticCamera`/`SyntheticDetector`/SIL FcuLink 스텁으로 대체된다.

## 코드 품질 게이트 (코딩표준 Rev C, SAD-001 §10.1)

CPP_Production_Coding_Standard Rev C를 적용한다. 강제 설정은 repo 루트의
`.clang-format` / `.clang-tidy`. 모든 변경은 커밋 전 아래를 통과해야 한다:

```bash
# 1) 포맷 (G16.1) — 적용 / 검증
clang-format -i $(find . -name '*.cpp' -o -name '*.h' | grep -v build)
clang-format --dry-run --Werror $(find . -name '*.cpp' -o -name '*.h' | grep -v build)

# 2) 정적분석 (G16.2) — compile_commands.json 필요
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build <changed files>       # 경고 0 (bugprone/cert/concurrency/analyzer는 에러)

# 3) 빌드 + 테스트 (G2) — -Wall -Wextra -Werror 경고 0, ctest 전부 통과
cmake --build build -j && ctest --test-dir build

# 4) 새니타이저 (G11.3) — 포인터·수명·data race 변경의 머지 조건
test/run_sanitizers.sh          # ASan+UBSan / TSan 각 빌드 + ctest, clean 확인
```

새니타이저는 `-DRIPOSTE_SANITIZE=address,undefined` 또는 `=thread`로 개별 빌드할 수 있다
(상호 배타 → 별도 빌드 디렉터리). TSan은 `test/tsan.supp`로 SeqSlot seqlock의 의도적
benign race 하나만 필터하고 그 외 race는 실패시킨다. `TSAN_OPTIONS`는 공백으로 분리되므로
suppressions 경로에 공백이 없어야 하며(빌드 디렉터리로 복사됨), WSL2 등에서는 `setarch -R`가
필요할 수 있다(스크립트가 처리).

규칙을 어겨야 하는 지점은 `// NOLINT(check): <사유>` 로 남기고, 광범위 예외는
SAD-001 §10.1 deviation 표에 기록한다 (G16.6).

### 게이트 자동화 (G16.5/G16.7)

위 4단계를 한 명령으로 묶고, 커밋·CI에서 자동 강제한다:

```bash
ci/run_gates.sh                 # format+빌드+ctest+tidy+새니타이저(+MAVSDK) 통합, ALL_GATES_PASS

# pre-commit 훅 설치 (스테이징 파일만 빠르게 format+tidy)
git config core.hooksPath riposte-sw/ci/hooks
```

- **CI**: `.github/workflows/ci.yml` — push/PR마다 lint-build-test·sanitizers·mavsdk-build 실행.
- SITL 시험(`test/sitl/*`)은 PX4 빌드가 필요해 CI에서 제외한다(수동/야간 실행).

## PX4 SITL 통합시험 (SAD-001 §12-2)

MAVSDK ON 빌드를 PX4 SITL(SIH — Gazebo 불필요)과 연동해 engage 전 시퀀스를 검증한다:

```bash
# 준비물: PX4-Autopilot 빌드(make px4_sitl_default), libmavsdk-dev, pymavlink
cmake -S . -B build-mavsdk -DRIPOSTE_WITH_MAVSDK=ON
cmake --build build-mavsdk -j

PX4_BUILD=~/PX4-Autopilot/build/px4_sitl_default \
RIPOSTE_BUILD=$PWD/build-mavsdk \
  test/sitl/run_sitl_test.sh     # SITL_TEST_PASS 출력이면 성공
```

시퀀스: PX4(SIH) 기동 → OBC READY → arm/이륙 → engage(토큰) → PRESTREAM →
offboard 모드 확인 → OFFBOARD_ACTIVE → disengage → READY.

추가 시험 (같은 환경변수):

```bash
test/sitl/run_sm_injection.sh      # SM-1~8 위반 주입 → 자동 disengage 검증
test/sitl/run_guidance_tracking.sh  # 시커→PN 유도 추종 + SM-4 클램프 비행 중 실측
```

- **SM 주입**: 각 SafetyMonitor 감시에 실결함(텔레메트리 freeze, 지오펜스 이탈, 시커 kill,
  GCS 모드 전환, 지터 버스트, 강제 disarm, 타임박스 경과)을 주입하고 위반 비트·자동
  disengage를 확인. SM-4(클램프)는 단위시험 + 추종시험 실측으로 커버.
- **guidance 추종**: `SyntheticCamera→Detector→Tracker→Estimator→TrackBus→PN→clamp→PX4`
  전 체인을 돌려 실제 변위·속도를 관측. guidance 명령(6 m/s)이 클램프(5 m/s)에 의해
  비행 중 제한됨을 텔레메트리로 확인.

## Gazebo 접근 시뮬레이션 (GAZEBO-TEST-001)

물리 시뮬레이션된 **이동 대상**을 상대로 유도·안전 루프를 검증한다(SIH 합성대상과 달리
근접 판정). `gz_track_bridge`가 Gazebo 대상 truth를 body-FRD `TrackState`로 변환해 시커를
대체한다.

```bash
cmake -S . -B build-gz -DRIPOSTE_WITH_MAVSDK=ON -DRIPOSTE_WITH_GZ=ON && cmake --build build-gz -j
PX4_BUILD=~/PX4-Autopilot/build/px4_sitl_default RIPOSTE_BUILD=$PWD/build-gz \
  test/gazebo/run_gazebo_closure.sh     # GAZEBO_CLOSURE_PASS 이면 성공
```

상세·시나리오(S-G1 호버 / S-G2 횡단 / S-G3 회피)는 `../docs/RIPOSTE-GAZEBO-TEST-001.md`.
`RIPOSTE_WITH_GZ`는 기본 OFF라 게이트·비행 이미지에 무영향.

## SIL 실행 예 (하드웨어 없이)

SIL 전용 프로필은 `config/sil.ini`다 (`synthetic=true`는 이 파일에만 둔다).
`config/riposte.ini`는 systemd가 배포하는 프로덕션 프로필로, `synthetic=false`에서
실 백엔드(Hailo·카메라)가 없으면 시커가 기동을 거부한다(fail-closed).

```bash
export RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock   # /run 권한 없을 때
./build/riposte-seeker config/sil.ini &
./build/riposte-obc    config/sil.ini &
./build/test/riposte-engage engage sil-dev-token  # 오퍼레이터 승인 게이트 (토큰 필수)
```

## 안전 원칙 (요약)

- **자동 제어 세션 금지 (D-2/SI-2)**: `requestEngage()`의 자동 호출 경로가 코드에
  존재하지 않는다. 트랙 성립은 필요조건일 뿐 트리거가 아니다.
- **FAULT 시 무개입 (D-1)**: 고장 시 OBC는 FC를 조작하지 않고 스트림만 중단 →
  PX4 offboard-loss 페일세이프에 위임.
- **클램프 최종 방어선 (G4)**: 어떤 소스의 setpoint든 전송 직전 무조건 속도 클램프.
- **판단 불가 = 이탈 (G3)**: 스테일/링크유실은 항상 disengage 방향.

## 디렉터리

```
common/     공용 헤더 (Types·Tunables·SeqSlot·CommandBus·Clock·Config·Log)
seeker/     L2 인지 (카메라·IDetector·Hailo/Synthetic·Tracker·TargetEstimator)
obc/        L3 제어 (FSM·SetpointStreamer·SafetyMonitor·FcuLink·sources/)
supervisor/ L5 헬스·블랙박스
config/     riposte.ini(프로덕션) · sil.ini(SIL) · balloon.ini(시험비행) · mavlink-router.conf
deploy/     udev 규칙 · systemd 유닛
test/       단위시험
tools/      engage_cli
```
