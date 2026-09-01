# RIPOSTE — Gazebo 접근 시뮬레이션 · 테스트 시나리오 (GAZEBO-TEST-001)

| 항목 | 값 |
|---|---|
| 문서 | RIPOSTE-GAZEBO-TEST-001 |
| 버전 | 1.3 (2026-08-17: GAZEBO-STAGES-001 흡수 — 시나리오와 단계별 래더를 한 문서로 통합) / 1.2 (2026-08-13: §4 S-G4 풍선 순찰 시나리오·§1 bridge 다중대상/FOV 모드 추가 / 1.1: 2026-07-04 심층 리뷰 반영: bridge 스테일 가드·속도 차분, 판정 phase 윈도우, 회피 트리거 12m·지연 샘플 제외, 스크립트 강건화 / 1.0: 2026-07-04) |
| 대상 | Gazebo Sim 8 (Harmonic) + PX4 SITL(gz) + riposte-obc |
| 관련 | SAD-001 §12(시험), SEEKER-SDD §4·§8, OBC-SDD §7(유도)·§7.1(자세), §6 단계별 브링업 래더 |

> 개발 중 계층별 회귀 격리는 **§6 단계별 브링업 래더**
> (`test/gazebo/stages/`)를 사용한다. 이 문서(S-G1~3 시나리오)는 그 래더의
> Stage 3~5에 해당하며, 본 스크립트가 Stage 4/5의 권위 소스다.

물리 시뮬레이션된 **이동 대상(침입 드론)** 을 상대로 Riposte의 유도(PN)·안전감시·MAVSDK
Offboard 전 체인을 검증한다. 기존 SITL(SIH, 합성 대상)과 달리 대상이 Gazebo 안의 실제
모델이며, **판정 기준은 근접(range closure)** — 방어 드론이 세션 활성 중 대상과의 거리를
실제로 좁히는지다.

---

## 1. 아키텍처

```
Gazebo(riposte_closure world)
  ├─ target (이동 대상 모델, VelocityControl)   ── truth pose ─┐
  └─ x500_0 (PX4 방어 드론)  ◀── MAVLink/gz ──▶ PX4 SITL          │
                                                                  ▼
        gz_track_bridge:  /world/.../pose/info → 상대 FRD 변환 → TrackBus(shm)
                                                                  │
   riposte-obc:  GuidanceSource(PN) → SafetyMonitor.clamp(SM-4) → MAVSDK Offboard
                                                                  │
                                                          PX4 → x500 추종
```

- **gz_track_bridge** (`tools/gz_track_bridge.cpp`, `RIPOSTE_WITH_GZ`): Gazebo pose 피드를
  구독해 대상 기체의 위치·속도를 **방어기 body FRD** 로 변환, 시커가 발행할 `TrackState`와
  동일 계약으로 TrackBus에 기록한다. 즉 **riposte-seeker를 대체**해 인지 스택 없이
  유도/안전 루프를 실 대상으로 구동한다. (변환 검증: 월드 (10,4,3)m ENU → FRD [10,−4,−3]m,
  range √125=11.2m 확인.)
  - **스테일 가드 (2026-07-04 심층 리뷰 반영)**: pose 피드가 **300ms 이상 끊기면**(시뮬
    일시정지/크래시/모델 제거) 캐시된 pose를 fresh·valid로 계속 내보내지 않고 **`valid=0`으로
    발행을 지속**한다 — 그러지 않으면 OBC의 SM-7 스테일 게이트가 무력화된다. 스테일
    전환(WRN)·복구(INFO)를 로그로 남긴다.
  - **속도 산출**: **새 pose 메시지가 도착했을 때만** world-frame 상대위치를 도착 간격으로
    유한차분한 뒤 현재 자세로 FRD 회전한다(캐시 pose를 벽시계 dt로 재표집하면 0~2×로
    앨리어싱, body-frame에서 차분하면 방어기 회전이 속도에 혼입). 스테일/미추적 중에는 0으로
    리셋하고 복구 시 공백을 넘겨 차분하지 않도록 재시드한다.
- **월드** (`test/gazebo/worlds/riposte_closure.sdf`): 지면·태양·PX4 GPS 원점(구면좌표)과
  침입 드론. 대상 기체는 `VelocityControl`로 등속 이동(기본 −1.5, 0.6, 0 m/s 횡단). PX4가
  x500을 원점에 스폰한다.
- **다중대상 모드 (2026-08-13 신설)**: 대상 인자가 `*`로 끝나면(예: `balloon_*`) 접두사에
  일치하는 **모든 모델**을 추적하고, **거리 + 시야각(FOV) 센서 모델**을 적용한다. 시야 안에
  들어온 대상만 검출로 치고, 그중 **최근접(=겉보기 최대, R-8)** 을 primary로, 보이는 개수를
  `num_targets`로 발행한다. 모델명별로 id를 고정 배정해 대상 정체성이 프레임 간 유지된다
  (순찰의 "이미 방문한 대상" 기록이 track id 기준이므로 필수).
  - **FOV 모델이 없으면 순찰 탐색이 시험되지 않는다**: 모든 풍선이 항상 보이면 기체가 선회해
    대상을 찾을 이유가 사라져 T-4/T-5의 핵심 거동이 통째로 검증에서 빠진다.
  - 환경변수: `GZ_BRIDGE_MAX_RANGE_M`(기본 60 — 25cm 풍선의 광각 타일 탐지거리, REQ-001 §3),
    `GZ_BRIDGE_HFOV_DEG`(60), `GZ_BRIDGE_VFOV_DEG`(34).
  - **단일대상 모드는 FOV 게이트가 없다**(의도적). 접근 시나리오는 대상 진리값을 주입해
    유도를 시험하는 것이라, 여기에 시야 제한을 넣으면 기존 시험이 재는 대상이 조용히 바뀐다.
  - 무발행 사유를 **구분해 로그**한다(방어기 pose 없음 / 대상 pose 없음 / 피드 스테일 /
    전부 시야 밖) — 구분이 없으면 실패한 실행이 "대상이 안 보이는 월드"와 똑같이 보인다.

---

## 2. 사전 준비

```bash
# (1) Gazebo Harmonic + dev 라이브러리
sudo apt install gz-harmonic libgz-transport13-dev libgz-msgs10-dev libgz-math7-dev v4l-utils

# (2) PX4 (gz 지원) — 최초 1회 빌드
cd ~/PX4-Autopilot && make px4_sitl gz_x500   # 창이 뜨면 정상, 종료

# (3) riposte-sw: MAVSDK + GZ 옵션으로 빌드
cd riposte-sw
cmake -S . -B build-gz -DRIPOSTE_WITH_MAVSDK=ON -DRIPOSTE_WITH_GZ=ON
cmake --build build-gz -j
```

`gz_track_bridge`는 `RIPOSTE_WITH_GZ=ON`에서만 빌드된다(기본 OFF → 게이트/비행 이미지 무영향).

---

## 3. 실행

```bash
PX4_BUILD=~/PX4-Autopilot/build/px4_sitl_default \
RIPOSTE_BUILD=$PWD/build-gz \
  test/gazebo/run_gazebo_closure.sh      # GAZEBO_CLOSURE_PASS 출력이면 성공
# GUI 없이: HEADLESS=1 를 앞에 추가
```

스크립트 시퀀스: PX4(gz) 기동 → bridge가 TrackBus에 대상 발행 → OBC READY → arm/이륙
→ engage(토큰) → OFFBOARD_ACTIVE → **20초간 근접 관측** → range가 `MIN_CLOSE_M`(기본 5m)
이상 감소 & 위반 0 & 계속 제어 세션이면 통과 → disengage → READY.

**판정 phase 윈도우 (2026-07-04 심층 리뷰 반영)**: R0/RMIN은 **engage 이후의 bridge 로그
슬라이스에서만** 산출한다 — engage 전 대상 기체가 지상 대기 중인 방어기 곁을 스쳐 지나간 것이
접촉(CONTACT)으로 오판되는 것을 방지.

**스크립트 강건화 (2026-07-04 심층 리뷰 반영)**: cleanup은 이 실행이 띄운 **PID를 먼저
kill**하고 패턴 `pkill`은 직접 띄우지 않은 자식(gz sim은 PX4의 자식)·고아 프로세스용
폴백으로만 사용한다. **잔존 `riposte-seeker`도 제거** — bridge와 함께 TrackBus의 이중
writer가 되면 seqlock이 조용히 오염된다. `LC_ALL=C` 고정으로 `sort -n`의 로케일 의존
오판정(소수점 기호)을 배제.

---

## 4. 테스트 시나리오

대상 기체 거동은 월드의 `target` 모델 `<pose>`(시작 위치)와 `VelocityControl/<initial_linear>`
(등속 속도)로 정한다. 아래 3개 시나리오를 제공한다.

### S-G1 · 호버 대상 (정지 접근)
- **설정**: `<pose>12 0 3 …`, `<initial_linear>0 0 0`.
- **의도**: 정지 대상으로 PN 유도·클램프의 기본 수렴 확인(가장 단순).
- **판정**: range 12m→<7m(≥5m 감소), OFFBOARD_ACTIVE 유지, 위반 0.

### S-G2 · 등속 횡단 (기본)
- **설정**: `<pose>18 0 3 …`, `<initial_linear>-1.5 0.6 0`.
- **의도**: 이동 대상 추종 — PN이 리드각으로 선도 추적, SM-4 수평 클램프(8m/s)가
  비행 중 명령을 제한하는지 관측.
- **판정**: 20초 창에서 ①접촉 접근(최소 range ≤1.5m) 시 안전측 복귀(자동 SM
  disengage 또는 정상 disengage) 또는 ②range ≥5m 감소 + 위반 0 + 제어 세션 유지.
  실측(2026-07-04)상 PN이 약 9초 만에 접촉 거리(0.2~0.4m)까지 도달하므로
  기본 결과는 ①이다. 접촉 시 gz 물리 충돌로 EKF 수직속도가 튀며 SM-3가
  발동할 수 있는데, 이는 안전측 이탈 설계 의도대로의 거동이라 PASS로 본다.

### S-G3 · 회피 기동 (자동화: `run_gazebo_evasive.sh`)
- **설정**: S-G2로 추종을 시작하고, **사거리 12m(`EVADE_AT_M`) 도달 시점**에 대상 기체
  `VelocityControl` 런타임 토픽(`/model/target/cmd_vel`)으로 120°+ 급선회를
  주입한다(`gz topic -m gz.msgs.Twist -p 'linear: {x: 0.9, y: -2.4}'`,
  2.56m/s). 트리거는 시간이 아니라 사거리 — 접근 속도가 실행마다 달라
  고정 딜레이로는 접촉 전 주입을 보장할 수 없다(실측: 4초 고정 딜레이 시
  이미 접촉 후였음). 임계 12m는 명령→실선회 지연 창 안에서 접촉이 나지 않도록
  `CONTACT_M` 대비 여유를 둔 값(2026-07-04 심층 리뷰 반영, 종전 8m).
- **의도**: 대상 급기동 시 재수렴/안전(스테일·타임박스) 거동 관측.
- **판정**(둘 중 하나면 PASS, `GAZEBO_EVASIVE_PASS`):
  ① 재수렴 접촉(기동 후 최소 range ≤1.5m) + 안전측 종료(정상 disengage 또는
  SM 자동 disengage→READY) ② 재수렴 실패 시 SM 자동 disengage(안전측 이탈).
  기동 후 최소 range 산출 시 **명령 직후 2샘플(`EVADE_SKIP_N`, bridge 1Hz 로그
  기준 ~1초)은 제외**한다(2026-07-04 심층 리뷰 반영) — `VelocityControl` 명령이
  실제 선회로 이어지기까지 ~0.5–1.5초가 걸려 그 구간 샘플은 기동 **전** 기하를
  반영하므로, 지연 창의 접촉으로 "기동 미시험 통과"가 되는 것을 막는다.
- **실측(2026-07-04)**: 23.3m 추종 → 6.7m에서 회피 명령 → 재수렴 **0.6m 접촉**
  → 충돌 여파 SM-3 자동 disengage → READY 복귀. 판정 ① PASS.

---

### S-G4 · 풍선 순찰 (자동화: `run_gazebo_balloon.sh`, 2026-08-13 신설)

야외 풍선 시험(REQ-001 T-1~T-5)을 **사거리 예약도 학습 모델도 없이** 미리 리허설한다.

- **월드** (`worlds/riposte_balloon.sdf`): 50×50 m 구역 안에 25 cm 빨간 풍선 6개를 고도
  3.5~6 m, 원점에서 12~23 m 거리의 서로 다른 방위에 배치. `balloon_5`는 동쪽 경계에
  일부러 가깝게(23 m) 두어 **접근 시 순찰의 turn-away 마진이 걸리도록** 했다 — T-5가
  가장 검증이 필요한 거동이기 때문. 풍선은 static(로터 후류·접촉으로 밀리지 않게).
- **체인**: 풍선 진리값 → `gz_track_bridge`(다중대상+FOV) → TrackBus →
  **BalloonPatrolSource** → SafetyMonitor.clamp → MAVSDK Offboard → PX4(gz x500).
- **설정**: 스크립트가 `[obc] source=balloon`, `[fence] side_m=50`, `[patrol]` 프로파일을
  생성한다. `safety.geofence_r`은 폴리곤보다 **넓게**(80 m) 둔다 — 기체를 멈추는 것이 SM-3여선
  안 되고, **행동계층이 스스로 안쪽에 머무르는지**가 관측 대상이기 때문.
- **판정** (모두 만족 시 `GAZEBO_BALLOON_PASS`):
  1. 90초 창에서 풍선 **2개 이상 도달**(`balloon reached`) — 한 대상에 영원히 고착되면 실패.
  2. bridge 로그의 **primary id가 2종 이상** — 같은 풍선을 반복 처리해 (1)만 만족하는 것을 배제.
  3. **SM-10 미발화**: 행동계층이 스스로 경계를 지켰어야 한다. 하드 한계가 걸렸다는 것은
     행동계층이 실패했다는 뜻이므로 PASS가 아니다.
  4. 창 종료 시 여전히 OFFBOARD_ACTIVE, 이후 정상 disengage → READY.
- **검증 안 되는 것**: 탐지 성능. bridge가 진리값으로 인지를 대체하므로(접근 시나리오와 동일),
  25 cm 풍선이 실제로 검출되는지는 P1/HEF의 문제이지 이 시나리오의 결과가 아니다.

#### S-G4 최초 실행에서 드러난 결함 (2026-08-14)

첫 실비행에서 **FAIL**(`TOO_FEW_SERVICED`, 접근 2회·도달 0회)했고, 두 가지 실결함이 나왔다. 둘 다 책상 위 단위시험으로는 나올 수 없는 종류였다.

1. **GCS 링크 상실 → PX4 failsafe (하네스 결함)**. `arm_takeoff.py`의 하트비트 스레드는 데몬이라 스크립트 종료와 함께 죽는다. PX4 v1.17은 GCS 연결 상실을 failsafe로 처리하므로, 이륙 11초 뒤 `Connection to ground station lost` → Hold → RTL → 착륙했고, OBC에는 **SM-2(mask 0x02)** 로 보였다. 관측 창이 이륙보다 오래 지속되는 시나리오에서만 드러난다(기존 접근 시나리오는 창이 20초라 통과했다).
   - 조치: `test/sitl/gcs_heartbeat.py` 신설, 시나리오 전 구간 1 Hz 하트비트 유지. **실기체에는 SiK GCS 링크가 상시 존재하므로 이것이 현실적인 구성이고, 회피책이 아니다.** `arm_takeoff.py` 종료 후에 기동해 GCS 포트를 공유하지 않는다.
2. **접근 종말에 대상이 수직 시야를 벗어남 (실기 설계 결함)**. 순찰 고도(5 m)를 고정으로 유지한 채 3.5~6 m의 풍선에 접근하면, 거리가 줄수록 고도각이 커진다. bridge 로그 실측 — range 6.3 m에서 대상이 1.6 m 아래(고도각 −15°), 도달 판정 거리 3 m에서는 **−28°로 수직 FOV(±17°)를 벗어난다**. 즉 **도달하기 직전에 대상을 잃는다.**
   - 조치: APPROACH 중 순찰 고도가 아니라 **대상 고도를 추종**하도록 변경(`min_alt_m`/`max_alt_m` 밴드로 제한 — 낮은 대상을 쫓다 지면에 박지 않도록). 단위시험 2건 추가.
   - **이것은 시뮬레이션 인공물이 아니라 R-9(화면 중심 유지) 미구현이 드러난 것이었다**(당시 기준 — R-9는 2026-08-15에 구현됐고, 이 항목은 결함이 어떻게 드러났는지의 이력으로 남긴다)**.** 동체 고정 카메라는 시선을 보어사이트 근처에 유지하지 못하면 종말 구간에서 대상을 잃는다. 협각(HFOV≈15°)에서는 같은 현상이 훨씬 이른 거리에서 발생하므로, R-9 폐루프 중심 유지가 협각 도입의 선행 조건임이 실측으로 확인된 셈이다.

---

## 5. 검증 완료 / 미검증

| 항목 | 상태 |
|---|---|
| 월드 SDF 로드 + 대상 기체 `VelocityControl` 이동 | ✅ gz-sim 8.14 헤드리스 실측(18→12m/4s=1.5m/s) |
| bridge 변환(world ENU → body FRD) 정확성 | ✅ (10,4,3)→[10,−4,−3], range 11.2m |
| bridge TrackBus 기록 + 30Hz 발행 | ✅ shm WRITER, valid 게이팅 |
| `gz_track_bridge` 빌드·정적분석 | ✅ `-Werror` 0, clang-tidy 0(RIPOSTE_WITH_GZ) |
| PX4(gz) 연동 전 체인 (arm→engage→근접) | ✅ 2026-07-04 실측 PASS — S-G2에서 range 18.2m→최소 0.2m 접촉 접근, disengage 후 READY 복귀 (`GAZEBO_CLOSURE_PASS`). **2026-07-17 PX4 v1.17.0 재실측 PASS** — range 29.1m→최소 0.9m 접촉, `SAFE_EXIT_OK` |
| S-G3 회피기동 자동화 (`run_gazebo_evasive.sh`) | ✅ 2026-07-04 실측 PASS — 6.7m 회피 선회 후 재수렴 0.6m 접촉, SM-3 안전측 disengage (`GAZEBO_EVASIVE_PASS`). **2026-07-17 v1.17.0 재실측 PASS** — 5.7m 회피 선회 주입 후 재수렴 최소 8.9m, SM-2(mask 0x02) 안전측 disengage |

전 체인은 PX4 v1.15.4(2026-07-04) 및 **v1.17.0(2026-07-17)** + gz-sim 8.14 헤드리스로
실측했다. CI에는 여전히 제외(PX4 빌드 필요).

### S-G4 풍선 시나리오 검증 상태 (2026-08-13)

| 항목 | 상태 |
|---|---|
| `riposte_balloon.sdf` 로드 + 풍선 6개 pose 발행 | ✅ gz-sim 8.14 헤드리스 실측 |
| bridge 다중대상 접두사 매칭·id 고정 배정 | ✅ 실측 |
| FOV **거부**(시야 밖 대상은 미검출) | ✅ 실측 — 관측자 `balloon_1`, HFOV 60°에서 az 32°의 `balloon_5`가 `all targets out of range/FOV`로 배제 |
| FOV **수용** + FRD 변환 정확도 | ✅ 실측 — HFOV 120°로 넓히자 `balloon_5` 검출, range 9.4 m / FRD [8.0, 5.0, 0.0] (손계산 일치) |
| 다중대상 계수 + **최근접 primary 선정(R-8)** | ✅ 실측 — 관측자 `balloon_3`, HFOV 180°에서 `targets=5`, primary는 최근접 `balloon_6`(19.6 m, FRD [4.0, −19.0, −2.5], 손계산 일치) |
| `num_targets`의 TrackBus(shm) 전달 | ✅ 실측 — 리더가 `valid=1 id=5 num_targets=5` 수신 |
| `gz_track_bridge` 빌드(`RIPOSTE_WITH_GZ=ON`) | ✅ `-Werror` 경고 0 |
| `run_gazebo_balloon.sh` 문법·shellcheck | ✅ `bash -n` 통과, shellcheck 지적은 기존 closure 스크립트와 동일한 SC1091(info) 1건뿐 |
| **전 체인 비행 (판정 1~4)** | ✅ **2026-08-14 실측 PASS** — PX4 v1.17.0 + gz-sim 8.14 헤드리스, 90초 창에서 접근 11회·**풍선 7개 도달**·**서로 다른 대상 6종**·SM-10 미발화·정상 disengage (`GAZEBO_BALLOON_PASS`). 단, 아래 두 결함을 고친 뒤의 결과다 |

### PX4 v1.17 호환 수정 (2026-07-17)

v1.15.4 → v1.17.0 에서 스크립트/월드가 걸린 지점 세 곳을 고쳤다. 재발 시 참조.

1. **`gz_env.sh`의 `GZ_SIM_SYSTEM_PLUGIN_PATH` unbound** — v1.17 의 gz_env.sh 가
   이 변수를 참조하는데 실행 스크립트가 `set -u` 라 소싱이 치명 오류. 소싱 전
   빈 기본값을 export (closure/evasive/stages 3곳).
2. **`Found 0 compass` preflight 실패** — v1.16+ x500 는 자력계 센서를 포함하지만
   월드가 자체 플러그인 목록을 명시하면 서버 기본 목록이 적용되지 않는다.
   `riposte_closure.sdf` 에 `gz-sim-magnetometer-system` 을 직접 추가.
3. **`No connection to the GCS` arming 거부** — v1.17 은 arming 체크에서 GCS
   연결을 엄격히 요구한다. `arm_takeoff.py` 가 1 Hz GCS heartbeat 를 별도
   스레드로 송신하도록 수정(pymavlink 는 자동 송신 안 함).

---

## 6. 단계별 브링업 검증 래더 (구 GAZEBO-STAGES-001, 2026-08-17 병합)

개발 중 **어느 계층에서 회귀가 생겼는지 즉시 짚기 위한** 단계별 시뮬레이션이다.
시나리오(§4)가 "임무가 성립하는가"를 묻는다면, 래더는 "어느 계층이 깨졌는가"를
묻는다. 스크립트 위치는 `riposte-sw/test/gazebo/stages/`.

단계 구성

| # | 이름 | 검증 대상 | 격리 범위 | 판정 마커 |
|---|---|---|---|---|
| 0 | env | 월드 로드·PX4 스폰·EKF 센서·대상 기체 이동 | riposte/MAVSDK 불필요 | `STAGE0_ENV_PASS` |
| 1 | bridge | pose→FRD 변환 정확도 + 신선도(pause→valid=0) | 비행 없음 | `STAGE1_BRIDGE_PASS` |
| 2 | offboard | FSM·SafetyMonitor 수명주기(hover, 대상 없음) | 유도/인지 분리 | `STAGE2_OFFBOARD_PASS` |
| 3 | static | PN이 **정지** 대상에 수렴(S-G1) | 리드·기동 없음 | `STAGE3_STATIC_PASS` |
| 4 | crossing | 이동 대상 추종(S-G2), SM-4 비행중 클램프 | — | `STAGE4_CROSSING_PASS` |
| 5 | evasive | 추종 중 급선회 재수렴/안전이탈(S-G3) | — | `STAGE5_EVASIVE_PASS` |
| 6 | safety | 트랙 소스 강제 종료→SM-7 자동 이탈 | 결함 주입 | `STAGE6_SAFETY_PASS` |

**진단 논리**: Stage N이 실패하고 N−1이 통과했다면 결함은 N이 새로 도입한 계층에
있다. 예) Stage 2 실패·1 통과 → FcuLink/FSM/SafetyMonitor(인지·유도 아님).
Stage 3 실패·2 통과 → 유도 법칙 또는 bridge→guidance 결합.

---

### 2. 각 단계 상세

### Stage 0 · env (`stage0_env.sh`)
- **의도**: 시뮬레이션 토대. riposte 코드가 개입하기 전에 gz/PX4/월드가 건강한지.
- **검사**: ① PX4 startup 성공 ② `Preflight Fail: …missing` 없음(월드에 IMU/기압/
  navsat 시스템 존재) ③ pose 피드에 `x500_0`·`target` 모두 출현 ④ 대상 기체가 실제
  이동(dynamic_pose에서 x 2회 표집, |Δ|>0.5m).
- **실측(2026-07-04)**: PASS — x500_0/target 확인, 대상 기체 x 15.1→11.9m 이동.

### Stage 1 · bridge (`stage1_bridge.sh`)
- **의도**: 시커 대체물(`gz_track_bridge`)의 좌표 변환과 신선도 게이팅.
- **검사**: ① 첫 샘플이 스폰 기하와 일치(대상 기체 ~18m 전방, 위쪽 → range∈[14,22],
  FRD 전방≈range, 측방≈0, 하방<0) ② sim을 pause해 pose 피드를 멈추면 300ms 가드가
  `valid=0`을 발행(SM-7 실효성). pause 중에도 pose가 계속 발행되는 빌드면 그 사실을
  정직히 보고하고 신선도는 단위시험에 위임.
- **비행 불필요** — 결정적·고속.

### Stage 2 · offboard (`stage2_offboard.sh`)
- **의도**: 인지·유도와 분리한 제어 평면. OBC를 **hover 소스**(속도 0)로 돌려 FSM과
  안전감시만 구동.
- **검사**: arm/이륙 → engage → OFFBOARD_ACTIVE → 8초 유지 → disengage → READY.
  정상 hover에서 **위반 0**이어야 함(SafetyMonitor 오탐 방지). bridge를 띄우지 않아
  TrackBus 부재가 FCU/FSM 결함을 가리지 못하게 한다.

### Stage 3 · static (`stage3_static.sh`)
- **의도**: 전 체인 첫 투입. 대상 기체를 `set_pose`로 (12,0,3)에 **고정**(부팅 드리프트
  제거)하고 PN이 정지 대상에 수렴하는지만 본다(리드·기동 배제).
- **판정**: 접촉(min range ≤1.5m) 또는 ≥5m 근접 + 위반 0. 접촉 시 안전측 복귀 확인.

### Stage 4 · crossing (`stage4_crossing.sh`)
- **의도**: S-G2 이동 대상 추종. 검증된 `run_gazebo_closure.sh`에 위임(얇은 래퍼).
- **판정**: `GAZEBO_CLOSURE_PASS` → `STAGE4_CROSSING_PASS`.

### Stage 5 · evasive (`stage5_evasive.sh`)
- **의도**: S-G3 급선회. 검증된 `run_gazebo_evasive.sh`에 위임.
- **판정**: `GAZEBO_EVASIVE_PASS` → `STAGE5_EVASIVE_PASS`.

### Stage 6 · safety (`stage6_safety.sh`)
- **의도**: 안전측 이탈. 세션 활성 중 트랙 소스(bridge 프로세스)를 **강제 종료**해 TrackBus를
  얼리면, OBC 자신의 신선도 게이트(트랙 age > `TRACK_STALE_NS`+coast)가 SM-7을 발동해
  **운용자 명령 없이** READY로 자동 이탈해야 한다. bridge의 valid=0 가드와 구별되는
  소비자측 age 검사 경로(=쓰기측이 아예 죽는 경우)를 검증.
- **격리**: 이동 대상을 공격적으로 추종하면 고도(SM-3)가 먼저 걸려 SM-7을 가릴 수
  있으므로, 대상을 **정지 고정**(gentle 접근)하고 브리지를 일찍 종료한다 — 이탈 원인이
  오직 트랙 스테일이 되도록.
- **판정**: `OFFBOARD_ACTIVE -> DISENGAGING`을 유발한 위반 mask에 **SB_TRACK_STALE(0x40)
  비트가 설정**되어 있어야 함(단순 자동 이탈이 아니라 SM-7이 원인임을 단언) → READY.

---

### 3. 실행

```bash
# 사전: PX4(gz) 빌드 1회, riposte-sw를 -DRIPOSTE_WITH_MAVSDK=ON -DRIPOSTE_WITH_GZ=ON 빌드
PX4_BUILD=~/PX4-Autopilot/build/px4_sitl_default \
RIPOSTE_BUILD=$PWD/build-gz \
MAVSDK_LIB=<libmavsdk.so.3 dir> \
HEADLESS=1 \
  test/gazebo/stages/run_stages.sh          # 전체 0~6, 첫 실패에서 정지

# 일부만:  run_stages.sh 0 1 2   |   run_stages.sh 0-3   |   run_stages.sh 6
# 개별 단계 직접:  TESTDIR=/tmp/s3 bash test/gazebo/stages/stage3_static.sh
# GUI로 관찰:  GUI_FOLLOW=1 (HEADLESS 생략) — 카메라가 방어기를 위에서 추종
# 실패해도 계속:  KEEP_GOING=1
```

`run_stages.sh`는 각 단계를 **독립 프로세스·독립 TESTDIR**로 실행하고, 끝에
단계별 PASS/FAIL 요약과 `LADDER_ALL_PASS`/`LADDER_HAD_FAILURES`를 출력한다.

**단일 인스턴스 가정**: 각 단계는 실행 동안 호스트의 PX4/gz/riposte 프로세스를
독점한다(시작 시 스윕, 종료 시 트랩 정리). 다른 gz/PX4 세션과 동시 실행 금지.
PX4 빌드가 필요하므로 CI에서는 제외(Stage 0/1도 gz+PX4 필요).

---

### 4. 검증 상태 (2026-07-04)

| 단계 | 상태 |
|---|---|
| 0 env | ✅ 실측 PASS — x500_0/target 확인, 대상 기체 이동 |
| 1 bridge | ✅ 실측 PASS — 변환(range 16.8, frd sane) + pose 소스 종료 시 valid=0 스테일 가드 발동 |
| 2 offboard | ✅ 실측 PASS — hover engage→8s 유지(위반 0)→disengage READY |
| 3 static | ✅ 실측 PASS — 정지대상(12,0,3)에 min 0.1m 수렴 |
| 4 crossing | ✅ 기존 `run_gazebo_closure.sh` 검증 재사용 (GAZEBO-TEST-001 §5) |
| 5 evasive | ✅ 기존 `run_gazebo_evasive.sh` 검증 재사용 |
| 6 safety | ✅ 실측 PASS — 세션 활성 중 bridge 종료→SM-7(mask=0x40)→자동 DISENGAGING→READY(~85ms) |

전 단계 실측 완료(2026-07-04, 헤드리스). 4/5는 상위 시나리오 스크립트로 이미 검증됨.

> 공통 스캐폴딩은 `gz_lib.sh`. 기존 `run_gazebo_closure.sh`/`run_gazebo_evasive.sh`는
> 그대로 두고(검증 자산), 래더는 그 위에 얹은 브링업 하네스다.

---

## 7. 트러블슈팅

- **bridge에 range 안 뜸**: 모델명 확인 — PX4 기본 방어기는 `x500_0`. 다른 이름이면
  `gz_track_bridge riposte_closure <ownship> target`. `gz topic -e -t
  /world/riposte_closure/pose/info -n1`로 실제 이름 확인.
- **월드 못 찾음**: PX4 rcS는 `rootfs/gz_env.sh`를 소싱하며 `PX4_GZ_WORLDS`를 PX4
  트리로 강제 덮어쓰므로, 월드는 그 디렉터리에서만 찾는다. 스크립트가
  `riposte_closure.sdf`를 `$PX4_GZ_WORLDS/`에 심링크해 해결한다.
- **EKF 데이터 없음(Preflight Fail: Accel/Gyro/baro missing)**: 월드에 센서 시스템
  플러그인(`gz-sim-imu-system`, `gz-sim-air-pressure-system`, `gz-sim-navsat-system`)
  필요 — `riposte_closure.sdf`에 포함되어 있는지 확인.
- **PX4가 gz 안 띄움**: PX4를 `make px4_sitl gz_x500`로 최소 1회 빌드해 gz 리소스 생성.
- **대상이 안 움직임**: `gz-sim-velocity-control-system` 플러그인 로드 확인(Harmonic 포함).
```
