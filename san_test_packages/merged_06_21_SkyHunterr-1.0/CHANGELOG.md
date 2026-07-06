# Changelog

본 파일은 [SkyHunter](https://github.com/adasone/SkyHunter-1.0) 의 주요
변경 사항을 정리한다. 형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/)
관례를 따르며, 버전 체계는 [SemVer](https://semver.org/lang/ko/) 를 따른다.

릴리즈는 GitHub Releases 페이지에서 확인:
https://github.com/adasone/SkyHunter-1.0/releases

---

## [Unreleased]

### Added

- **feat(sim): Go2 SITL 콘보이 데모 — 로봇개 리더 1 + UGV 4대 단일종대**.
  `san_operator_tools` 에 `convoy_coordinator`(리더 측)·`convoy_ugv`(UGV 측)
  노드 추가 + `san_sim_gazebo/launch/convoy_demo.launch.py`. 외부 Go2 SITL
  (`unitree_go2_ros2`) 리더가 계획경로를 pure-pursuit 로 주행(급회전 억제로
  CHAMP gait 전복 방지)하며, 데이터 통신 정책(UGV→리더 위치보고 @2Hz /
  리더→UGV 선행로봇 타겟·장애물맵 @2Hz)으로 UGV 4대(deputy/hub/follower×2)가
  breadcrumb 단일종대 추종·gap 유지·장애물 측면안전(≥2.25 m) 확보. UGV 는
  `san_description` 실모델(lidar none + 카메라 strip)로 5로봇 RTF 확보.
  **검증(Ubuntu 24.04 / ROS 2 Jazzy / Gazebo Harmonic 8.14):** 리더 waypoint
  8/8 완주(dx≈15 m), UGV gap≈3.5 m 종대 유지, 충돌 0·전복 0·장애물 회피 충족.

- **feat(sim): 콘보이 RViz 경로 시각화**. `san_operator_tools/convoy_viz` 노드 +
  `san_sim_gazebo/rviz/convoy.rviz`. 계획경로(`/convoy/plan`)·리더 실주행 궤적
  (`/convoy/leader_track`)·로봇 마커(리더+UGV4, 역할별 색)·콘보이 체인 라인·장애물
  실린더를 MarkerArray 로 발행(메시 비의존 → 공백경로 이슈 회피). `convoy_demo.launch.py`
  에 `rviz:=true`(기본) 통합.

- **feat(sim): Go2 SITL(`unitree_go2_ros2` + CHAMP) vendoring** → `ros/src/third_party/`.
  콘보이 데모를 self-contained 화(외부 clone 불필요). `convoy_demo.launch.py` 가
  `sitl:=true`(기본)로 Go2 SITL 리더까지 **한 명령 기동**(SITL → warmup → UGV spawn →
  콘보이 → RViz). champ\* = BSD, unitree_go2_\* = upstream 미선언 + Go2 메시 © Unitree
  (내부 R&D 시뮬용; 출처/수정 내역 `third_party/.../SKYHUNTER_PROVENANCE.md`). 대용량
  로봇 메시(>500 KB, 총 ~30 MB)는 `^ros/src/third_party/` 를 pre-commit 전체 훅에서
  제외하여 직접 커밋(upstream 보존).

- **feat(sim): 콘보이 데모 — Gazebo waypoint 큰 점 마커 + 중간경로 장애물 + 파라미터
  최적화**. `convoy_demo.launch.py` 가 계획 경로(S-curve) 각 waypoint 에 녹색 구
  마커(9개)와 경로 중간(8,0) 장애물을 Gazebo 에 spawn. **다양한 경로×속도 sweep**
  (4 path × 5 speed, `skyhunter_sim/convoy_sweep.sh`)으로 최적 도출: straight 경로는
  급회피로 CHAMP gait **전복**, 0.3 m/s 는 저RTF 에서 **미보행** → **S-curve @ 0.6 m/s
  최적**(clean 검증 8/8·무전복·무충돌). 결과/방법 `skyhunter_sim/SWEEP_RESULTS.md`.

### Fixed

- **fix(sim): 장애물이 waypoint 위/근처일 때 리더 영구 정체 버그**. waypoint 전진 판정이
  *원래* waypoint(장애물 내부) 기준이라 회피 중 도달 불가 → `wp_idx` 미전진. 회피로
  shift 된 실제 조준점 기준으로 전진하도록 `convoy_coordinator._shifted_target()` 도입.
- **fix(sim): UGV 가 Gazebo 에서 안 보이던 문제**. `convoy_demo.launch.py` 에
  `GZ_SIM_RESOURCE_PATH`(san_description share) 추가 — 없으면 `package://` UGV 메시
  미해석으로 물리/odom 은 정상이나 시각화만 누락.

### Test

- **test(sim): `skyhunter_sim/` 콘보이 데모 테스트 하니스**. `build_test_ws.sh`(레포
  소스만으로 vendored SITL 빌드 검증), `run_convoy_test.sh`(전체 데모 clean 실행+판정),
  `convoy_sweep.sh`(경로×속도 sweep), `vendor_go2.sh`(재-vendoring), README + SWEEP_RESULTS.

### Infra / Toolchain

- **chore(platform): DCN-2026-027 — 개발 플랫폼 이관 (Ubuntu 24.04 / ROS 2
  Jazzy / Boardcon EM3588)**. 메인 보드 교체(Custom RK3588 → Boardcon EM3588,
  **RK3588 SoC 동일**)에 따른 공식 툴체인 상향. CI 3종(`ubuntu-22.04`→`24.04`,
  `osrf/ros:humble-desktop`·`ros:humble-ros-base`→`jazzy` 계열, `/opt/ros/humble`
  →`jazzy`) · CI 템플릿 2종 · Docker sbc1/sbc2(`ARG ROS_DISTRO=jazzy` +
  ARG 우회 하드코딩 CMD source 경로) · systemd `san-squadron.service` · 문서
  일괄 정합화. **소스/`package.xml` 무변경**(distro-agnostic), NPU/RKNN·
  Gazebo Harmonic·RMW(FastDDS, ADR-007) 무영향. D-007 표준 툴체인 baseline
  supersede. **Jazzy `colcon build` CI 그린이 머지 게이트**(DCN §4).
  O-1 resolved: EM3588 = **RK3588(비-J)** 확정 → BOM 상단 주석 + 라이브 문서
  갱신. ⚠ RK3588J(−40~85℃) 대비 옥외 운용 온도 derating 재검토 필요.
  **DCN-2026-027 ratified** (PM 승인 2026-06-15, PR #266 머지 `c48bba2`).

- **docs(ADR-009): RK3588 열 등급 derating 대응** (Proposed). DCN-2026-027 O-1
  후속 — 일반등급 RK3588(0~80℃) 옥외 운용 리스크 분석 + 대안 A(산업용 SKU)/
  B(능동 enclosure)/C(envelope derate)/D(Hybrid, 잠정권고). **EM3588 thermal
  챔버 시험이 결정 게이트** — 데이터 없이 등급 미확정.

### Perception / Surveillance

- **feat(combat_robot_msgs/san_fire_authorization/san_surveillance/san_hub_orchestrator):
  DCN-2026-026 C-3 — 교전 합의 투표 + FireSolution** (한도: **advisory** —
  사격 개시 권한은 Two-key + HMAC fire-authorization 체인 단독, 암호 경계
  테스트 V5 로 고정).
  - **`TargetConfirmation.msg`(IDS §5.23 신규)**: HMAC-SHA256 + nonce 인증
    투표 — limon 원안의 평문 UInt32 폐기. `track_id`(DCN-025) 표적 바인딩.
  - **`TargetConfirmationAuth`**(san_fire_authorization 신규): 기존
    HmacAuthenticator 위 도메인 분리 어댑터(`target_confirm.v1` 태그) —
    동일 mesh secret, 분리된 nonce window(투표가 fire-auth window 를 오염
    못함), HMAC→drift→nonce 검증 순서 동일.
  - **`FireSolution.msg`(IDS §5.24 신규)** + surveillance 발행: 위협
    클러스터별(C-1) shooter 선정(최근접 TRACK follower)·lead 조준
    (rate × lead_time)·**위협별 rate 추정**(원안의 스칼라 상태 교차오염
    제거)·k-of-n(k=2, 1.5 s) `engage_ready`.
  - **detection_to_threat 투표 발행**: person/drone + conf ≥ 0.9 + 위치
    보유 시 트랙당 0.5 s 스로틀로 서명 발행. mesh secret 부재(sim) 시
    투표만 자동 비활성 — 기존 경로 무영향.
  - **`VoteTally`**(pure-logic): 로봇-distinct k-of-n, 15° bearing 바인딩,
    윈도 만료, latest-wins. 테스트 V1~V3 + 인증 V1~V5(변조/재전송/drift/
    도메인 분리) 신설.

- **feat(san_formation/san_hub_orchestrator): DCN-2026-026 C-2 — Encircle 포위 기동**.
  - **트리거 3중 게이트**(pure-logic `encircle_combat`): severity ≥ CRITICAL +
    type ∈ {DRONE_DETECTED, OTHER} (시스템 알림은 절대 불가) + confidence ≥ 0.9
    (detail JSON 파싱) + 위치 보유. **운용자 확인 기본**(`/operator/encircle_confirm`
    Bool 1-tap) — 자동 개시는 `encircle_auto` opt-in (비준 결정).
  - **표적 좌표 정규화**: threat_aggregator 가 드랍하던 geo 필드
    (has_position/bearing/range) passthrough 수정 + formation 이 **신고 로봇
    pose** 기준으로 world 좌표 계산(`threatWorldXY` — limon 원안의 leader 합산
    오지점 포위 제거).
  - **수명 관리**: TTL 10 s(갱신 연장) + Cooldown 5 s 히스테리시스. 해제는
    TTL 만료·운용자 해제만 — 비해당 알림은 combat 을 절대 해제 못함.
  - **슬롯 기하**: `encircleSlots(n_followers, r)` 균등 링(leader 슬롯 없음 —
    leader 는 현 위치 유지), combat 진입/이탈 시 즉시 재계획, KPP-1 오차·
    SlotAssignment·FollowerTarget·상태 모두 **combat anchor 로 일원화**,
    FormationStatus.phase = PHASE_ENGAGE 보고.
  - 테스트 E1~E6(게이트 진리표·파서·신고자 좌표·확인 플로·TTL 타임라인·링 기하)
    신설 — 30/30 PASS, 양 패키지 colcon lint 포함 all green.

- **feat(san_surveillance): DCN-2026-026 C-1 — 다중 위협 추적** (sector_allocator).
  위협 bearing 을 15° 병합으로 ≤2 클러스터화(`clusterThreatBearings`, pure-logic).
  추적 전환은 **follower 한정**(Leader §8.2 팬틸트 부재·Hub 제외 유지), 단일
  위협은 기존 3대(§8.6.1, A7/A10 무수정 통과), 2위협은 위협당 ≤2 / 총 ≤3 으로
  근접도 분배. 잔류 follower 는 전방 240° 밴드를 **재분배**해 union coverage
  ≥80% 유지(A11 신설로 고정). 노드는 위협 클러스터 ≤2 를 독립 TTL 로 관리,
  Track 섹터 tilt 는 자기 클러스터의 elevation 사용. 테스트 A11/A12 추가
  (31/31 PASS, colcon lint 포함 all green).

- **feat: limon perception 작업 선별 수용** (`limon/features_gazebo_sim` 정리 머지 1차;
  sector_allocator 위협할당 변경·formation encircle·FireSolution voting 은 SDD/DCN
  검토 필요로 **제외**, 별도 DCN 후 후속 PR).
  - **human_detector — RGB↔열상 bbox 융합:** `thermal_topic` 파라미터(기본 "" = off)로
    mono16(CV_16UC1) 열상 구독, 검출 bbox 패치의 avg/max 온도를
    `Detection.thermal_avg/max_temp_c` + `has_thermal_signature` 로 발행
    (raw→°C 스케일/오프셋·stale 윈도 파라미터화). 기본 off — 기존 sim/HW 경로 무변경.
  - **san_hub_orchestrator — detection_to_threat 단안 geolocation:** bbox 중심 +
    핀홀 모델 + TF yaw + 짐벌 joint 각으로 `ThreatAlert` 의
    `has_position/bearing_deg/elevation_deg/range_m` 채움. 클래스별 실높이 prior
    (person 1.7 m / vehicle 1.8 m / drone 0.4 m; weapon·unknown 은 prior 없음 →
    range "unknown" 유지 — 원본의 전-클래스 1.7 m 가정 수정), `base_frame` 자동
    네임스페이스 유도(robot_N/base_footprint — 원본은 follower 에서 TF lookup 전면
    실패), pure-logic `computeGeo()` gtest G1..G7 추가.
  - **san_surveillance — pan_tilt_driver(신규 실행파일):** `/swarm/cmd/pantilt`
    (IDS §5.11) → 짐벌 joint rad 명령 브리지, 순수 pan_tilt_controller 를 50 Hz 로
    스테핑. Track/Engage 명령 stale(기본 3 s) 시 Fixed-hold 복귀 가드 추가(원본은
    stale 표적을 영구 추종). uncrustify/Allman 재정형.
  - **san_bringup/squadron.launch.xml:** 위 3개 노드/파라미터 배선
    (`source_robot_id` string 강제 — int 코어션 launch 버그 수정). 모델 경로 정책은
    main(DCN-2026-025, `/opt/san/models/`) 유지.

- **feat(human_detector): 시뮬레이션 ONNX 디텍터 백엔드 + Hailo-8 async InferModel +
  ByteTrack 추적** (PR #257, DCN-2026-025). sim/데스크톱에서 RK3588·Hailo HW 없이
  실제 추론이 가능한 ONNX Runtime 백엔드 추가(CUDA EP → CPU 폴백); Hailo-8 경로를
  신형 InferModel async API(`AsyncModelInfer`)로 재작성; ByteTrack(`tracking_lib`, MIT)로
  탐지 박스에 트랙 ID 부여.
  - `combat_robot_msgs/Detection.msg` +`uint32 track_id` (IDS §5.21 — 메시지 해시
    변경, 전 노드 재빌드 필요).
  - `san_bringup/squadron.launch.xml` — sim→onnx / 실HW→hailo8|rk3588 백엔드·모델
    선택(`hw_backend`, `/opt/san/models/` 관례 일원화); 실HW rk3588 모델 경로가 빈
    문자열로 해석되던 버그 수정.
  - 게이트-스텁(ADR-006 / DCN-2026-002): `HAVE_ONNX` 가드 — ONNX Runtime 없는
    host/CI 빌드·링크 무손상(CI full colcon build green).
  - `models/y5s_person_drone.hef` + `models/README.md`(프로비저닝 관례);
    `tracking_lib/THIRD_PARTY_NOTICES.md`(ByteTrack/lapjv MIT 표기).

### Sim / Demo

- **feat: Leader+Follower 데모 — 복잡 경로 주행 + 궤적 추종 + sim 주행계 개선**
  (#254 데모 후속). RViz 에서 완전 동작(2대 + 점선 경로); Gazebo GUI 멀티로봇 렌더는
  WSL/WSLg 환경 한계로 이동 중 1대만 표시될 수 있어 RViz 를 메인 뷰로 권장.
  - **san_description (sim-only):**
    - `gazebo_control.xacro` — DiffDrive 휠 적분 odom 대신 `OdometryPublisher`
      ground-truth odom 사용. 6륜 스키드-스티어가 급커브에서 미끄러져 휠 odom 이
      ~20 m 까지 drift 하던 문제 제거(실제 위치와 odom 일치). DiffDrive odom/tf 는
      미사용 토픽(`/…/odom_wheel`)으로 우회.
    - `gazebo.xacro` — 휠 측면 마찰 `mu2` 1.0→0.1 (+`fdir1`). 스키드-스티어가
      급회전/제자리회전 가능(이전엔 코너서 못 돌고 직진→벽 충돌). 사실상 버그픽스.
    - `san_robot.urdf.xacro` + `spawn_robot.launch.py` — `<robot name>` 을
      `robot_name` arg 로 파라미터화해 SDF 모델명을 로봇별 고유화(다중로봇 spawn 개선).
  - **san_follower_tier/follower_pursuit_node.cpp:**
    - `path_follow` 모드(breadcrumb) — 팔로워가 리더의 **실제 주행 궤적**을 따라
      `offset_back` 만큼 뒤에서 추종(코너를 큰 반경으로 가로지르지 않고 같은 경로로 회전).
    - `min_leader_dist_m` — 리더 충돌(rear-end/밀기) 방지 가드.
    - `own_odom_offset_*` — world-frame odom 정합(데모는 0; OdometryPublisher 가 절대좌표).
  - **san_operator_tools/leader_path_nav.py:** 점선 마커를 출발점→전 waypoint 의
    **전체 고정 경로**로 발행(RViz 경로 = 월드 경로 일치) + 코너 타이트 회전(turn_factor).
  - **san_sim_gazebo:** `leader_follower_world.sdf` — arena ±22 m, 바닥 경로 데칼(루프),
    센서 없는 경량 월드; `config/leader_follower_gui.config`(신규) — 완전 기본 GUI +
    루프 부감 카메라; 데모 launch 에 복잡 루프 waypoint·충돌방지·breadcrump·gui-config 배선.

- **feat(san_sim_gazebo): Leader+Follower Gazebo 데모 추가** — `leader_follower_demo.launch.py`
  (리더 1 + 팔로워 1). 리더는 신규 `san_operator_tools/leader_path_nav` 노드가
  `/odom` P-제어로 waypoint 경로를 주행하고, 목표까지의 경로를 `/leader/waypoint_path`
  MarkerArray **점선**(LINE_LIST dash/gap)으로 RViz 에 시각화. 팔로워는 기존
  `follower_pursuit_node` 를 재사용해 리더 정후방(offset_side=0) 일정거리(`follow_distance`,
  기본 4 m)를 유지. `follower_pursuit_node` 에 `own_odom_offset_{x,y,yaw}` 파라미터를
  추가(기본 0 = 하위호환) — Gazebo 의 로봇별 DiffDrive odom 원점 차이를 리더 world
  프레임으로 정합(squadron/실HW 동작 무영향). RViz 설정 `rviz/leader_follower.rviz`.
- **RTF 병목 회피용 경량 월드** `worlds/leader_follower_world.sdf` — `gz-sim-sensors-system`
  (카메라·라이다 렌더) 과 Fuel 모델 include 를 제거. GPU passthrough 없는 호스트(WSL)
  에서 센서 software 렌더가 RTF 를 ~0.0015 로 떨어뜨리는 문제를 회피(데모엔 odom·diff-drive
  만 필요). 2026-06-02 GUI end-to-end 검증: 리더 목표(28,0) 완주·정지, 팔로워 ~4 m 간격
  유지, RTF ~1.0. 풀센서가 필요하면 `world:=empty_world.sdf` 로 복원.

### Docs / Sim

- **docs(san_description): `gz_frame_id` 경고 출처 주석화** (PR #250) — Gazebo Harmonic
  (SDFormat 1.11) sim 스폰 시 7개 센서(front/rear/rgb/ir camera, gps, imu,
  lidar)에서 나오는 `XML Element[gz_frame_id] ... not defined in SDF` 경고가
  benign 임을 `camera.xacro` / `chassis_cameras.xacro` / `sensors.xacro` 에
  명시. `<gz_frame_id>` 는 SDFormat 스펙 외 gz-sim 커스텀 태그라 libsdformat
  이 경고하지만 sensor frame_id 는 정상 적용됨(2026-05-29 end-to-end 검증).
  태그 제거 시 frame_id 가 scoped name(`san_combat_robot/<link>/<sensor>`)
  으로 떨어져 robot_state_publisher TF 트리와 불일치 → RViz/Nav2 깨짐이라
  제거 불가. gz verbosity 로는 `-v 0`(전체 묵음, 에러까지 사라짐)만 억제 가능
  하므로 미적용. 스펙-클린 `<frame_id>` 대체는 SDFormat 1.12 / Gazebo Ionic
  부터(sdformat#1454, gz-sensors#306) — Ionic 전환 전까지 경고는 예상된 동작.

(v1.5.5 다음 sprint 의 변경분이 여기에 누적.)

---

## [v1.5.5] — 2026-05-24

> **Sprint 결과**: v1.5.4 6-DCN cross-audit 결과 (37 findings, 21
> fixes + 6 historical documented) + Gate-1 CI workflow 도입 +
> DCN-2026-024 WiFi auth gate (DEV_TEST mode) 묶음 minor release.
>
> **GitHub release**:
> https://github.com/adasone/SkyHunter-1.0/releases/tag/v1.5.5

### v1.5.4 6-DCN cross-audit follow-up fixes (P1 + P2 + P3 모두 완료)

- **fix(comm): DCN-019 audit B1+B10** (PR #207) — sliding-window dedup
  correctness (replay vulnerability) + sender pending mutex (MTE race)
- **fix(safety): DCN-016 audit A1+A5** (PR #208) — Gate-1 E-Stop
  non-blocking RTH dispatch + SCOPE_LEADER_ONLY uses authoritative
  `is_leader_role_` flag (was robot_id_==1 proxy)
- **fix(L5): DCN-022 audit C1-C5** (PR #209) — `Outcome::SKIP` enum +
  topic-presence false-positives → SKIP + L5_30 reset_home_pose=false +
  elapsed_ms real measurement + JUnit `<skipped/>` emission
- **chore(docs): audit D1+D5** (PR #210) — `docs/CI_GUIDE.md` BLE row
  cleanup + `DCN-2026-024_WiFi_Auth_Follow_Up.md` proposed DCN
- **fix(thread-safety): P2-1 A2+A3+A7+B2** (PR #215) — DemoSequencer
  isEnabled lock + watchdog CAS reset + FireSim gimbal mutex +
  McProtocol state_mu_
- **fix(comm): P2-2 B5+B7** (PR #218) — receiver crc 중복 제거 +
  sender INVALID_CHECKSUM 즉시 timeout (retransmit 1.5s 낭비 제거)
- **fix(L5): P2-3 B15** (PR #219) — McStressScenario `--seed N`
  deterministic replay 옵션
- **chore(audit): P3 cleanup** (PR #220) — 11 minor refactor
  (FireSim sentinel/주석, MC seq wrap doc, late-ack debug log,
  reorder queue drain, JUnit attr length cap) + 6 historical 문서화

### CI workflow stabilization
- **PR #212 / #213 / #216 / #217** — Gate-1 Regression workflow 도입
  + `xmllint` → `libxml2-utils` + `safe.directory` + dorny
  `fail-on-empty=false` + workflow `permissions: { contents:read,
  checks:write, pull-requests:write }`. Final GREEN PASS verified
  via `gh workflow run` (run 26352795009).

### Added

- **DCN-2026-024 — WiFi auth gate for DEV_TEST (Option C, this PR)** —
  DCN-2026-023 v2 가 BLE PIN auth 를 제거하면서 남긴 DEV_TEST
  진입 인증 공백을 매움. `OperationalModeController` 에
  shared-secret token 검증 추가:
  - ctor 인자 → `SAN_DEV_TEST_SECRET` env → `/etc/san/dev_test_secret`
    파일 → 없으면 fail-closed
  - `request_mode(mode, auth_token="")` — DEV_TEST 시 only,
    `secrets.compare_digest`
  - 다른 모드 backward-compatible — `auth_token` 무시
  - 8 새 pytest cases (gate + resolution priority + fail-closed +
    no-leak)

### Pending

- (없음 — v1.5.4 cycle + audit + DCN-024 모두 완료)

---

## [v1.5.4] — 2026-05-24

> **Sprint 결과**: 154-1 spec v1.5.4 Sprint 6 phase 모두 완료.
> 6개 DCN (016 / 018 / 019 / 020 / 022 / 023 v2) main 적용, Phase 5
> 검증 PASS (38/38 packages build / **381/381 gtest PASS**), tag 발행.
>
> **GitHub release**:
> https://github.com/adasone/SkyHunter-1.0/releases/tag/v1.5.4

### Added

- **DCN-2026-019 — MC ACK/retransmit protocol** (`san_operation_control` +
  `combat_robot_msgs`)
  - `MCMessage.msg` + `MCAck.msg` — wire-level seq + crc32 envelope.
    higher-level MissionStateCommand 등과 분리된 reliability 계층.
  - `mc_protocol_node` (receiver) — `/mc/command` 구독, boost::crc_32_type
    검증, 16-deep sliding window dedup + OUT_OF_ORDER 필터, 합격된 명령
    을 `/mc/command_validated` 로 republish.
  - `mc_sender_node` (sender) — `/mc/raw_command` 구독, seq + crc32
    stamp 후 `/mc/command` publish, 500 ms ack timeout + 3 retry,
    초과 시 `/mc/timeout` 발행.
  - 8 gtest cases (pure-logic via evaluateForTest / stampForTest seam).

- **DCN-2026-020 — MC stress test scenario** (`san_l5_regression`)
  - `McStressScenario` — 1 kHz 부하 + drop/dup/reorder 합성 noise
    injection, RTT p50/95/99 측정, CSV 출력.
  - 합격: p99 < 50 ms (Gate-1 KPP).
  - `regression_main --scenario mc_stress` dispatch.
  - 3 gtest cases — summarize() 결정 로직 (empty / within target /
    over target).

- **DCN-2026-022 — L5_26~L5_33 Gate-1 acceptance suite** (`san_l5_regression`)
  - 8 시나리오: Deputy boot / RTK lock / Costmap rate / Nav2 waypoint
    / RTH accuracy (/rth action) / E-Stop response / Mission BT loop
    / Gate-1 demo E2E (/gate1/start_demo).
  - 각 시나리오는 live dependency 없으면 graceful FAIL (crash 없음).
  - `renderJunitXml()` — pure-logic JUnit XML emitter (jest-junit
    compatible).
  - `regression_main --scenario gate1_suite` 가 8 모두 실행 + JUnit
    출력. `--scenario L5_NN` 로 단일 실행.
  - `.github/workflows/gate1-regression.yml` — PR + workflow_dispatch
    트리거, dorny/test-reporter@v1 로 결과 publish.
  - 6 gtest cases — Gate1Junit emitter pure-logic 검증.

- **DCN-2026-016 — Gate-1 demo ROS integration** (`san_operation_control`)
  - `/gate1/start_demo` (`std_srvs/Trigger`) — operator-facing service.
    deployment_mode ∈ {DEMO, LAB_TEST} 일 때만 accept.
  - `/gate1/demo_status` (`combat_robot_msgs/OperationState`) — 5 s
    publisher. operator_banner 가 현재 DemoPhase 를 carry.
  - `/emergency_stop` (`combat_robot_msgs/EmergencyStop`) subscriber —
    scope (ALL / SINGLE_ROBOT / LEADER_ONLY) 필터 후 DemoSequencer
    disable + `/rth` ActionGoal 발사.
  - 5 s watchdog tick → 60 s 동안 phase 변화 없으면 `/rth` fallback.
  - DemoSequencer phase callback wiring → RTB 진입 시 자동 `/rth`.
  - `gate1_demo.launch.xml` (operation_control_node + san_rth 합성).
  - 6 gtest cases (T1-T6) — service accept/reject + status pub +
    estop scope filter + RTB auto-trigger.

- **DCN-2026-018 — Fire simulator (co-located)** (`san_fire_authorization`)
  - `fire_simulator_node` — `/swarm/fire/authorization_response` 구독,
    `/gimbal/pan_tilt_state` (`sensor_msgs/JointState`) 구독,
    `/swarm/fire/result` (`combat_robot_msgs/FireResult`) 발행.
  - 정렬 평가: HIT iff `|pan_err| < tol AND |tilt_err| < tol` (기본 2°).
  - `FireResult` 의 actual IDS §4.6 schema 사용
    (`result` enum / `impact_point_{x,y}_m` / `rounds_fired` /
    `authorization_chain` / `timestamp_{fire,report}_ms`).
  - 5 gtest cases — granted+aligned → SUCCESS / denied → NO_AUTHORIZATION
    / misaligned → MISS / gimbal cache / FireResult field 완성.
  - 가공된 가짜 무기 발사 없음 — `FireResult` schema 호환만 보장.

### Changed

- **DCN-2026-023 v2 — BLE 완전 제거** (PIN auth + archive 포함)
  - `san_mission/operational_modes.py` — PIN auth dead code 전부 제거:
    `requires_pin` 필드, `_pin_authenticated`, `set_pin_authenticated()`,
    `is_pin_authenticated()`, `request_mode()` 의 PIN gate 블록 모두 삭제.
  - DEV_TEST 진입 시 PIN gate 없어짐 — **보안 영향**: WiFi-기반
    인증은 별도 DCN 으로 추가 예정.
  - `san_mission/__init__.py` + `mission_node.py` — stale BLE 주석 정리.
  - `test_operational_modes{,_patch}.py` — PIN auth 가정 test 제거,
    thread-safe coverage (PO1) 유지.
  - `docs/PDR/{PKG,MSG,ARCH}-001` — `san_ble_control` 행 / `BleCommand`
    + `BlePhaseStatus` 메시지 / `T1_BLE` 노드 / "BLE Controller" 표기
    모두 삭제 + footnote 추가.

### Removed

- `archive/v15_python_prototype/` — 6 BLE 파일 git rm:
  - `control/ble_control_process.py`
  - `control/_aban_reference/ble_sim.{c,h}`
  - `tests/blesim_tcp.py`
  - `tests/test_ble_control.py`
  - `tests/test_ble_gatt_api.py`

### Fixed

- `san_fire_authorization/src/fire_authorization_node.cpp` — `OperationState.msg` 에
  없는 `hub_term` / `leader_term` 필드 참조 build error 수정 (msg schema 에
  실제 존재하는 `hub_robot_id` / `leader_robot_id` 와 혼동). audit slot 0
  으로 처리, msg 가 term 필드를 추가하면 복원 예정.

### Pending

- (없음) — v1.5.4 sprint Phase 4 INTEGRATION 6/6 DCN 완료. Phase 5
  (full validation) + Phase 6 (v1.5.4 tag + GitHub release) 후속.

---

## [Pre-v1.5.4 baseline] — main HEAD `5b45569` 이전 history

상세 변경 history 는 git log 참조:
```bash
git log --oneline 5b45569
```

주요 작업 묶음:

### v1.5 PDR-prep + R-series deep-dives (2026-05-12 ~ 2026-05-24)
- R-3 san_sim_gazebo_helpers (#105) — drone target + GPS disturbance
- R-4 san_formation (#107) — frame transform + heading prediction
- R-5 san_surveillance (#108) — world-frame sectors
- R-6 san_follower_tier (#109) — dt-aware 5-Tier FSM
- R-7 san_fire_authorization (#110/#192/#193) — safety-critical auth + audit
- R-8 san_reroute_planner (#111) — PNG decode + lethal guards
- R-9 comm-modules (#112) — san_comm_link + san_comm deprecates LinkSelector
- R-10 san_role_management (#113) — non-blocking grace + split-brain hardening
- R-13 san_mission (#115) — BT semantic + thread safety
- R-14 san_perception (#116) — Python deep-dive (stub safety, timestamp)
- R-15 san_hub_comm + san_lte_redundancy (#117) — comm deep-dive
- R-16 san_cameras (#119) — parameter override, atomics, timestamp validation

### Phase 0 (operator/sensor/driver safety) — 2026-05-22 ~ 2026-05-24
- PR-A san_fire_authorization (#118) — KEY1↔KEY2 binding, audit fail-closed
- PR-B sensor stub safety (#120) — IMU/cameras/RTK/LRF
- PR-C driver/actuation safety (#121) — Unitree stub, cmd_vel watchdog, pan-tilt clamp
- PR-D operator command auth interim (#122)

### Phase 1 sensor stub surfacing (#123)
- `~/stub_status` latched topic 으로 downstream 이 stub 여부 인지 가능.

### Phase 5 / Phase 6 misc residual fixes
- Phase 5 hub health hysteresis + bounded clock skew (#127)
- Phase 6 threat_aggregator double-publish + tier_node KPP-2 (#128)
- D-018 + D-019 human_detector postprocess hardening (#200)
- R-1 cppcheck cleanup (#196)
- v1.5 PDR sw verification posture (#197) — mission_node B1 wiring +
  coverage/sanitizers CI + S20-7b

### Infrastructure
- DCN-2026-014 v2 (#179) — FastDDS + EasyMesh unification
- DCN-2026-013 (#180) — swarm_monitor_node + Hub-only /swarm/poses gate
- DCN-2026-012 v2 (#178) — multi-robot sim hardening
- DCN-2026-021 (#182) — Aban Android rosbridge schema + mock
- DCN-2026-022 (#183) — san_test Gate-1 regression (L5_26~33)
- ADR-008 (#177) — Tier-based language policy
- DCN-2026-017 (#177) — san_rth RTH action server (Tier 1 C++)

---

## 참조

- **DCN log**: `docs/04_Change_Management/DCN_log.md` (예정)
- **PDR 산출물**: `docs/PDR/SAN-PDR-*.md`
- **ADR**: `docs/05_Supplementary/ADR-*.md`
- **GitHub Issues**: https://github.com/adasone/SkyHunter-1.0/issues
- **Release page**: https://github.com/adasone/SkyHunter-1.0/releases
