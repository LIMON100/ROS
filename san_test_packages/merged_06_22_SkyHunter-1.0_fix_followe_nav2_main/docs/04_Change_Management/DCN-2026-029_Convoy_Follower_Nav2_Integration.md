# DCN-2026-029 — 콘보이 추종자 nav2 통합 (계층형 유도: 리더 참조경로 + 로봇별 자율)

> **Status**: **APPROVED (ratified)** — PM 승인 2026-06-23 (김태근). 설계 제안
> `san_operator_tools/CONVOY_NAV2_INTEGRATION_DESIGN.md` (Proposed) 를 정식 DCN 으로 승격.
> **P1(리더측 참조경로 발행) 구현 완료**(브랜치 `convoy/comm-5hz-nav2-design`); P2–P4 후속.
> **Origin**: 콘보이 POC(머지 #277–#281)의 갭 — 현 추종자는 단순 breadcrumb pure-pursuit
> (로컬 costmap/플래너/recovery 없음). 추종자를 nav2 로 구동하되 리더 경로를 global plan 으로 사용.
> **Scope**: POC = Unitree Go2(리더) ×1 + UGV ×4 단일종대. 2계층(전역 참조 + 로컬 자율).
> **Related**: DCN-2026-027(플랫폼 Jazzy), DCN-2026-028(RMW drift), ADR-006(IPC Unification),
> ADR-007(RMW FastDDS), TST S20-1…9, L5 Gate-1(L5_26…33). `san_nav2`/`san_costmap`/`san_localization` 재사용.
> **Document Owner**: 김태근 (PM, ㈜스카이오토넷)
> **Created**: 2026-06-23
> **Implementation**: 단계형(P1→P4). P1 = `convoy_coordinator` + `convoy_avoidance`(순수로직) 델타.

---

## 1. 배경

콘보이 POC 아키텍처는 **2계층**이다:

- **전역(참조):** Go2 가 4D lidar 로 대형을 선도하고 **참조 주행경로**를 생성해 전 로봇에
  **@5 Hz(0.2 s)** 제공(`comm_period_s=0.2`, DCN delta로 2→5 Hz 상향 완료). 각 로봇은 자기
  위치를 Go2 에 **@5 Hz** 보고.
- **로컬(자율):** Go2 경로는 **명령이 아닌 참조**다. 각 UGV 는 **자체 nav2** 로 참조를 추종
  하면서 돌발상황(급장애물·이탈·복구)을 자율 처리.

현 `convoy_ugv` 는 선행 로봇의 breadcrumb 을 되밟는 **pure-pursuit + 반응형 avoid_override**
안전망일 뿐, nav2 구동이 아니며 로컬 costmap/플래너/recovery 가 없다. **갭**: 각 추종자를
리더 참조경로를 global plan 으로 삼아 nav2 로 구동한다.

## 2. 아키텍처 (2계층)

```
 Go2 (리더)                                     각 UGV (추종자)
 ┌─────────────────────────────┐                ┌──────────────────────────────┐
 │ convoy_costmap (4D lidar)   │                │ local costmap (자기 lidar)   │
 │ convoy_coordinator          │  /convoy/      │ nav2: planner+controller     │
 │  ├ 참조경로/슬롯  ──────────┼─ ref_path/r{n}─▶│  (DWB|MPPI) + recovery       │
 │  └ 대형 throttle            │   @5 Hz        │  ├ 참조 추종                 │
 │                             │◀─ report/r{n} ─┤  └ 돌발 장애물 회피          │
 └─────────────────────────────┘   @5 Hz        └──────────────────────────────┘
   전역: 경로 + 슬롯 + 대형                        로컬: 추종 + 장애물 + recovery
```

- **전역(Go2):** 경로 유도, 로봇별 슬롯(~3 m along-path), 대형 회피(`convoy_costmap`) + 대형
  throttle(낙오자 대기).
- **로컬(UGV별 nav2):** 자기 차선 내 참조 추종, 자기 센서로 로컬 costmap, 추종 + 돌발 장애물
  회피, nav2 recovery(clear-costmap/backup/spin) 후 참조 재획득.

## 3. 재사용 원칙 (포크 금지)

- 로봇별 nav2 = **SkyHunter `san_nav2`/`san_costmap`/`san_localization`** 단일소스.
  CombatRobot_1/test_nav2 에서 제2 nav2 를 세우지 않는다(필요 자산은 `san_nav2` 로 이관).
- 리더 참조 + lidar = 머지된 **`convoy_coordinator`/`convoy_costmap`** 재사용·확장(본 DCN delta).
  제2 Go2 스택 금지.

## 4. 인터페이스 (리더 참조경로 → 추종자 nav2)

리더는 이미 `/convoy/target/r{n}`(선행 pose+vel) @5 Hz 를 발행한다. nav2 용으로 **로봇별 참조
경로**를 추가한다:

| 토픽 | 타입 | 방향/주기 | 의미 |
|---|---|---|---|
| `/convoy/ref_path/r{n}` | `nav_msgs/Path` | 리더→로봇 @5 Hz | 로봇 참조 차선(슬롯점→head) |
| `/convoy/report/r{n}` | `nav_msgs/Odometry` | 로봇→리더 @5 Hz | 위치 보고(기존) |
| `/convoy/target/r{n}` | `nav_msgs/Odometry` | 리더→로봇 @5 Hz | 선행 pose+vel(**fallback 유지**) |

- **참조 차선 정의:** 리더가 실제 주행한 breadcrumb 궤적(이미 장애물을 회피한 안전 경로).
  단일종대라 **차선은 전 로봇 공통**, 슬롯(체인상 간격)이 각 로봇의 시작점만 정한다.
  로봇 r{n} 의 슬롯거리 = `SLOT[n] * slot_gap` (리더 head 뒤 호장; `SLOT={3:1,4:2,5:3,2:4}`,
  `slot_gap=3.0`). 참조경로 = 궤적의 슬롯점부터 head 까지 → nav2 가 앞쪽 차선을 추종.
- **추종자측(P3, 신규 노드):** 얇은 `convoy_nav2_follower` 가 참조 Path 를 nav2 에 투입.
  - **Option A(권장):** nav2 `FollowPath`(controller_server) 로 참조 Path 추종 — 컨트롤러
    (DWB/MPPI)가 추종, 로컬 costmap 이 돌발 장애물 주입 → 이탈 후 재수렴. 리더를 global
    planner 로 유지(최단). POC 채택.
  - **Option B:** `NavigateThroughPoses` — 큰 우회 시 nav2 전역 플래너 사용(무거움). 로컬
    컨트롤러로 해결 못 하는 전역 재경로 필요시에만 승격.

## 5. domain_bridge 정합 (Task B)

본 계층화 = 네트워크 부하 논거다.

- **무거운 데이터는 온보드 잔류:** 각 UGV 의 nav2 로컬 costmap + 원시 lidar/cam 은 보드 밖으로
  나가지 않는다.
- **도메인 경계를 넘는 것은 5 Hz 조정 토픽뿐:** `/convoy/ref_path/r{n}`(리더→로봇) +
  `/convoy/report/r{n}`(로봇→리더). 이것이 **domain_bridge 가 전달하는 정확한 토픽 집합**.
- 베이스라인(전 도메인 공유: 센서 보드↔보드 브로드캐스트) 대비 큰 감소 예상 — 측정 목표.

## 6. 돌발상황(contingency) 처리

- 로컬 costmap(자기 lidar)이 참조 경로가 지나는 급장애물 검출.
- nav2 컨트롤러가 차선 내 이탈; 막히면 **recovery**(clear costmap→backup→spin) 발동 후 슬롯에서
  참조 재획득.
- 대형 허용오차: 리더 gap throttle(floor 0.5)이 추종자 해결 동안 Go2 를 감속 → 콘보이 유지.

## 7. 단계 계획 + 시험 매핑

| 단계 | 내용 | 상태 |
|---|---|---|
| **P1** | 리더가 breadcrumb/슬롯에서 `/convoy/ref_path/r{n}` @5 Hz 발행(추종자 무변경; Path 내용·주기 검증) | **구현 완료**(PR #282) |
| **P2** | convoy sim 5대에 `san_nav2` + UGV lidar 로컬 costmap bringup; 정적 goal 항법 검증 | **bringup 실증**(Jazzy; lifecycle ACTIVE) — goal 항법은 gz 필요. §8-1 |
| **P3** | `convoy_nav2_follower` 가 참조 Path → nav2 `FollowPath`; convoy_ugv 직접 cmd_vel 은 flag fallback 으로 격하; 대형 ~3 m 유지 검증 | **골격 구현**(브랜치 `convoy/p3-nav2-follower`; §8-2) — sim 검증 보류 |
| **P4** | 급장애물 주입 → 로컬 회피 + recovery + 참조 재획득, 대형 보존 검증 | **wiring 실증**(Jazzy; bt_navigator goal accepted) — 실 회피/recovery 는 gz 필요. §8-3 |

- **시험 매핑:** P3/P4 → **TST S20-1…9** + **L5 Gate-1(L5_26…33)**(ad-hoc 아님). 부하시
  센서 pass/fail(lidar/cam/IMU rate + **RTF** + dropped frames)은 **실 RK3588/Linux** 에서
  (WSL RTF ~10% 비대표).

## 8. P1 구현 내역 (본 DCN)

| 분류 | 파일 | 변경 |
|---|---|---|
| 순수 로직(seam) | `san_operator_tools/convoy_avoidance.py` | `slot_index(trail, slot_arc)` + `ref_path_from_trail(trail, slot_arc)` 추가 — ROS 무관, **호장(arc-length) 기준** 결정론적. |
| 리더 노드 | `san_operator_tools/convoy_coordinator.py` | 리더 breadcrumb `self.trail`(deque, `trail_step_m=0.12`) 누적; `slot_gap_m=3.0` param; 추종자별 `/convoy/ref_path/r{n}` publisher; `pub_ref_paths()` 를 `broker()`(@5 Hz)에서 호출; diag 에 `trail` 길이. |
| 단위테스트 | `san_operator_tools/test/test_convoy_avoidance.py` | `slot_index`/`ref_path_from_trail` 7 건(슬롯 호장, 클램프, 중첩 차선, ㄱ자 호장≠직선거리, 짧은 궤적 안전). standalone pytest 러너 자동 수집. |

- **P1 범위 한정:** 리더측 발행만. `convoy_ugv`(추종자) **무변경** — 기존 breadcrumb pure-pursuit
  로 계속 주행하며 신규 토픽은 P3 까지 미구독. 회귀 위험 없음(추가 발행만).
- `/convoy/target/r{n}` 은 backward-compat/fallback 으로 유지(제거 아님).

## 8-1. P2 골격 구현 (브랜치 `convoy/p2-nav2-bringup`)

per-UGV Nav2 bringup 골격을 `swarm_nav.launch.py` 패턴 재사용으로 작성. **bringup 은
Ubuntu 24.04 / ROS 2 Jazzy(convoy_ws)에서 실증**(아래 검증 결과); 실제 goal 항법/대형 추종은
gz 물리 필요(WSLg 중도종료 → 실 RK3588/Linux).

> **Jazzy 포팅 버그 발견·수정(Linux 실증):** base `nav2_params.yaml` 계승분의 플러그인
> lookup 명이 슬래시 형식(`nav2_smac_planner/SmacPlanner2D`, `nav2_behaviors/Spin`,
> `nav2_bt_navigator/NavigateToPoseNavigator` 등) → Jazzy pluginlib 미등록으로 planner/
> behavior/bt FATAL. 전부 `::` 형식으로 수정 → 전 플러그인 정상 로드. (DCN-2026-027 §4-2
> "Nav2 API churn" 리스크 실체화 — base `nav2_params.yaml` 도 동일 잠재버그, 별도 수정 권장.)

| 분류 | 파일 | 변경 |
|---|---|---|
| Nav2 params | `san_nav2/config/convoy_nav2_params.yaml` | san_nav2 단일소스 변형 — RPP `FollowPath`(O-1 계승)·SmacPlanner2D·rolling costmap **계승**, **AMCL 제거**(sim 지상진실), 속도 0.6 m/s·UGV 풋프린트, odom/scan 상대토픽. |
| tf shim | `san_operator_tools/convoy_odom_tf.py` (+ setup.py entry point) | 지상진실 `/robot_<id>/odom` → tf `<ns>/odom`→`<ns>/base_footprint` 중계(**O-2** sim 해소). 실HW 는 `san_localization` dual-EKF. |
| bringup launch | `san_sim_gazebo/launch/convoy_nav2.launch.py` | convoy_demo 위 **오버레이**(로봇 spawn 안 함). 로봇별: static `map`→`<ns>/odom` + `convoy_odom_tf` + Nav2(controller/planner/behavior/bt/smoother/waypoint + lifecycle), `RewrittenYaml(root_key=ns)`, staggered. |

### P2 검증 결과 (Ubuntu 24.04 / ROS 2 Jazzy, convoy_ws)
- ✅ **colcon build** 클린(san_operator_tools/san_nav2/san_sim_gazebo) + entry point
  (`convoy_odom_tf`) 등록 + 노드 import(rclpy/tf2_ros) 해소 + `convoy_nav2_params.yaml` 설치.
- ✅ **per-UGV nav2 bringup**: `convoy_nav2.launch.py robots:=3` → 네임스페이스 params 생성,
  controller(RPP)/planner(SmacPlanner2D)/behavior/bt 전 플러그인 로드(슬래시→:: 수정 후).
- ✅ **tf 설계 검증**: odom 미공급 시 정확히 `<ns>/base_footprint`→`<ns>/odom` 미존재로 costmap
  대기(= sim odom 만이 결손). 합성 odom 공급 시 `convoy_odom_tf` 가 tf 완성 → **lifecycle
  "Managed nodes are active" 도달**(전 스택 ACTIVE).

### 남은 검증 (gz 물리 필요, 실 RK3588/Linux)
1. `convoy_demo` 실 sim 의 `/robot_<id>/odom` 으로 tf 완성 → lifecycle ACTIVE(합성 odom 으로 선검증).
2. **정적 goal 항법**: `ros2 action send_goal /robot_<id>/navigate_to_pose ...` → 도달(실 주행).
3. **로컬 costmap**: convoy UGV 기본 `lidar_mode=none` → costmap 빈값. UGV lidar 활성
   (convoy_demo `lidar_mode` 변경)해 로컬 costmap 점유 — **P2 완결 항목**.
4. RTF/부하는 실 RK3588/Linux (WSL ~10% 비대표).

## 8-2. P3 골격 구현 (브랜치 `convoy/p3-nav2-follower`, 검증 보류)

리더 참조경로 → nav2 `FollowPath` 추종(설계 §4 Option A: Go2=global planner, nav2=추종+로컬
회피). **sim 실행 검증은 Linux/CI 보류** — DRAFT PR.

| 분류 | 파일 | 변경 |
|---|---|---|
| 추종자 노드 | `san_operator_tools/convoy_nav2_follower.py` (+entry point) | `/convoy/ref_path/r{id}` 구독 → 네임스페이스 `follow_path`(controller_server) 액션으로 추종. 경로 frame 을 `map` 으로 re-stamp(sim 지상진실 map≡전역 odom). |
| 재전송 판정 | `convoy_avoidance.should_resend_path(prev_head, new_head, move_tol)` | 순수 — 참조경로(@5Hz) 매 goal 전송의 controller preempt 를 억제(head 이동 ≥ tol 시만 갱신). standalone pytest 3건. |
| 런처 연동 | `san_sim_gazebo/launch/convoy_nav2.launch.py` | `follow_ref_path:=true` 시 로봇별 `convoy_nav2_follower` 기동. |

- **cmd_vel 택일:** `convoy_nav2_follower`(nav2 cmd_vel) ↔ `convoy_ugv`(직접 cmd_vel) 동시
  구동 시 `/robot_<id>/cmd_vel` 경합 → P3 사용 시 convoy_demo 의 convoy_ugv 비활성 필요
  (convoy_demo 비활성 플래그는 후속 작업). convoy_ugv 는 pure-pursuit fallback 으로 보존(§9 O-4).

### P3 sim 검증 체크리스트 (Linux/CI, 머지 전)
1. P2 bringup ACTIVE 후 `follow_ref_path:=true` → `follow_path` goal accepted(로봇별).
2. 리더 주행 시 각 UGV 가 참조경로(breadcrumb 슬롯)를 ~3 m 간격으로 추종(대형 유지).
3. 재전송 주기/tol 튜닝(`resend_period_s`/`resend_move_tol_m`)으로 preempt 빈도·추종 지연 균형.
4. convoy_ugv 대비 추종 정밀도/RTF 비교(실 RK3588/Linux).

## 8-3. P4 돌발상황 대응 (브랜치 `convoy/p4-contingency`)

돌발 장애물(§6) → 로컬 회피 + nav2 recovery + 참조 재획득. **핵심 설계점:** raw `FollowPath`
(controller_server)는 **recovery 를 호출하지 않는다**(recovery 는 bt_navigator BT 소관). 따라서
P4 는 추종자를 **`NavigateThroughPoses`(bt_navigator)** 모드로 돌려 기본 BT 의 recovery
(clear costmap→backup→spin) + 참조 재획득을 얻는다.

| 분류 | 파일 | 변경 |
|---|---|---|
| 추종자 모드 | `convoy_nav2_follower` | `mode` param: `follow_path`(P3) \| `navigate_poses`(P4). P4 는 `navigate_through_poses` 액션으로 via-pose 통과 + goal ABORTED/CANCELED 시 `prev_head` 리셋 → 다음 tick 강제 재전송(**참조 재획득**). |
| via-pose 솎기 | `convoy_avoidance.downsample_path(points, spacing)` | 순수 — 조밀 breadcrumb 를 호장 spacing 으로 솎아 NavigateThroughPoses 경유점화(전역 플래너 과부하 방지). 첫/끝 보존. standalone pytest 4건. |
| UGV lidar | `convoy_demo.launch.py` | `ugv_lidar:=true`(+`ugv_lidar_mode`, 기본 low) → UGV lidar 활성 + `/robot_<n>/scan/points` 브리지 → 추종자 nav2 **로컬 costmap 입력**(돌발 장애물 인지). 기본 none(5로봇 RTF 보존). |
| 런처 모드 | `convoy_nav2.launch.py` | `follower_mode:=navigate_poses` 로 추종자 P4 모드 전환. |

- **돌발 장애물 흐름:** UGV 로컬 costmap(자기 lidar)이 참조경로상 급장애물 인지 → bt_navigator
  BT 가 컨트롤러로 우회 시도, 막히면 recovery 발동 → 후 참조(via-pose) 재획득. 리더 gap
  throttle 이 콘보이를 늦춰 대형 유지(§6).

### P4 검증 결과 (Ubuntu 24.04 / ROS 2 Jazzy, convoy_ws)
- ✅ **wiring 실증**: 추종자 `mode=navigate_poses` 기동 → nav2 ACTIVE → 합성 ref_path(13점)
  공급 시 추종자가 **`downsample_path` 로 4 via-pose 로 솎아** `NavigateThroughPoses` goal 전송
  → **bt_navigator accepted**: `Begin navigating ... through 4 poses to (3.00, 0.00)`. FATAL 0.
- ⏸ **실 회피/recovery/재획득**(돌발 장애물 물리·로봇 이동·대형 보존)은 gz 필요 → 실 RK3588/Linux.

### P4 sim 검증 체크리스트 (gz 필요)
1. `convoy_demo ugv_lidar:=true` + `convoy_nav2 follow_ref_path:=true follower_mode:=navigate_poses`.
2. 참조경로상 급장애물 → UGV 로컬 costmap 점유 → 컨트롤러 우회/실패 시 recovery 발동.
3. recovery 후 참조 재획득(`prev_head` 리셋 → 재전송) + 대형 ~3 m 유지.
4. RTF(UGV lidar 5대 부하) — 실 RK3588/Linux(WSL 비대표).

## 9. 미결정 사항 (Open Items)

| # | 항목 | 내용 |
|---|---|---|
| O-1 | **컨트롤러 DWB vs MPPI** | MPPI 가 동적 추종에 더 매끄러우나 RK3588 부하 큼. P2 에서 실측 결정. |
| O-2 | **localization/frames** | convoy 는 tf2-free `/odom_gt` 기반인데 nav2 는 tf 트리(`map`→`odom`→`base_link`) 필요. 로봇별 `san_localization` 로 정합(P2). |
| O-3 | **참조 오프셋** | 기존 along-path 슬롯 로직(검증된 ~3 m) 을 Path 생성기로 유지; nav2 는 추종+회피만. |
| O-4 | **fallback flag** | sim/no-nav2 런용으로 convoy_ugv pure-pursuit 를 flag 뒤에 유지(P3). |

## 10. 검증 요건 (머지 전)

1. **P1(본 DCN):** 순수 로직 standalone pytest 그린(리더/CI 환경). `/convoy/ref_path/r{n}` 의
   Path 내용(슬롯점→head, 중첩 차선) + 발행율 5 Hz 를 convoy sim 에서 확인(**Linux/CI**;
   WSLg 에서 gz 중도 종료 → 실 검증은 Linux/CI).
2. **P2–P4:** 각 단계 acceptance(§7) + 부하시 센서 pass/fail 은 실 RK3588/Linux.
3. **standalone 러너는 distro/ROS 미링크** — Path 발행율·tf 정합은 full colcon/sim 에서만 입증.

## 11. 롤백

P1 은 **추가 발행 + 순수 함수**뿐이라 `git revert` 단일 커밋으로 완전 원복(추종자 무변경이라
런타임 영향 없음). 신규 토픽 미구독 시 무동작.
