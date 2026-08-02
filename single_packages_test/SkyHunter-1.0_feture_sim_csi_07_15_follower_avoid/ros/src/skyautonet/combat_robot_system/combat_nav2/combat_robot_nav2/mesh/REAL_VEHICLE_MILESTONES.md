# 실차 군집주행 목표 도달 체크리스트 (sim → 실차)

> 시뮬레이션에서 구현한 편대주행 시나리오(리더/팔로워, line/column/wedge/diamond 대형전환, 주행중 전환)를
> **실차 2보드(mesh_board1=s1 리더, mesh_board2=s2 팔로워)** 로 달성하기 위한 단계별 체크리스트.
> 의존 순서대로 정렬. `[x]`=검증완료 · `[~]`=부분/조건부 · `[ ]`=미달성 · `[!]`=현재 블로커.
> (기준일 2026-07-03)

---

## Phase 0 — 단일차량 기반 (Foundations)  … 완료
- [x] 센서 브링업 양 보드 — IMU(Xsens 921600, ~100Hz) · GPS(Prolific 921600) · LiDAR(RSE1, eth0 /32)
- [x] **GNSS heading 보정** — 안테나 측면장착 `antenna_yaw_offset_deg=90` (raw 138°→48°=실제방향), 커밋 `c561cb2`
- [x] CAN 통신 + 휠 오도메트리 — can_reader `/sN/odom`, can0 250k UP
- [x] mesh DDS cross-board — `mesh_dds.xml`(eth1 whitelist), board1/2 정합(커밋 `ec66d9e`)
- [x] 센서 경로 고정 — udev SYMLINK(/dev/gps·/dev/xsens_imu), 라이다 ARP 격리

## Phase 1 — 단일차량 자율주행 (Single-vehicle autonomy)
- [ ] **실외 RTK Fixed 안정** — 양 보드 q=4/5 + THS(헤딩) 지속. *검증: gnss log `q=4/5 THS=100 dropped=0`*
- [!] **navsat map localization finalize** — `transform_good` 확정 → `/sN/odometry/gps` 발행 → `map→odom` TF. **★현재 최대 블로커**(실외 q4에서도 datum 무한리셋). 가설: `/gps/heading_imu` frame_id=`gps`가 sN TF트리에 없음. *검증: `tf2_echo sN/map sN/odom` 성립*
- [ ] nav2 navigation active — planner/controller/costmap lifecycle "active"(map TF 있어야 activate). *검증: lifecycle active, abort 없음*
- [ ] 단일차량 goal 주행 — 목표점 waypoint 도달. *검증: `/sN/plan` 생성 + 도달*
- [ ] LiDAR costmap 장애물 반영 — obstacle_layer 마킹/회피. *검증: costmap lethal + 우회경로*
- [ ] 저사양보드 부하 확보 — nav2 풀스택 EKF rate miss 0 (또는 경량 스택). *검증: `Failed to meet update rate` 0*

## Phase 2 — 군집 조율 (Swarm coordination)  … 대부분 완료
- [x] 리더/팔로워 executor cross-board — s1(id1/leader1)·s2(id2/leader1) 상호 발견
- [x] /swarm 버스 발행 — `formation_ready`·`progress` 발행자 2, `robot_poses`·`leader_ref_path` 리더 발행
- [x] 미션 파이프라인 — executor→`/sN/swarm/mission_state`→FSM→`/operation_state`(→command_server→태블릿)
- [~] 월드좌표 위치 산출 — `/swarm/robot_world_pos`·`formation_anchor`. **localization(Phase1) 의존**으로 현재 비어있음
- [ ] datum 동기 — 리더 first fix를 팔로워와 공유(동일 ENU 원점). *검증: 양보드 동일 datum*

## Phase 3 — 실차 편대주행 (Real formation driving)
- [ ] 경로 로드 — 리더에 `SwarmPathCommand cmd5 LOAD` 수신·파싱. *검증: mission_state.total_waypoints>0*
- [ ] 정지 form-up — 같은 방향·지정 간격 정렬(RTK 위치 기반). *검증: rviz 두 화살표 정렬·간격*
- [ ] 편대 START — `cmd1 START` → 리더 주행 + 팔로워 슬롯 추종. *검증: 팔로워 `/sN/cmd_vel` 슬롯추종*
- [ ] 직진 편대 유지 — 종/횡 간격 유지(sim 0.3m 수준 목표). *검증: 상대위치 오차 범위내*
- [ ] 곡선/회전 편대 — 리더 경로 곡선 구간 추종
- [ ] 안전정지 — 통신두절/E-stop 시 팔로워 즉시 정지(fail-safe)

## Phase 4 — 대형 전환 (sim 시나리오 재현)
- [ ] line 대형
- [ ] column 대형
- [ ] wedge 대형
- [ ] diamond 대형
- [ ] **주행중 대형 전환** — line↔column↔wedge↔diamond 무정지 전환(sim 달성분 실차 재현)

## Phase 5 — 안정화 / 운용 (Robustness & Ops)
- [!] **can_reader 안전 트리거** — CAN 송신이 tkinter 메인루프 종속 → X11 스톨 시 하트비트 끊김(헤드라이트 깜빡). 대안: 유선LAN / CAN TX 스레드 분리 / 물리 E-stop / 보드 게임패드 데드맨 / 토픽 estop(headless)
- [ ] 보드 부하 최적화 — 주행 안 할 땐 경량 스택(localization+executor), 주행 시 실차 로컬데이터로 rate 확보
- [ ] 호스트↔보드 안정화 — 호스트 유선LAN(wifi 지터 제거 → rviz/can_reader/ssh 안정)
- [ ] 태블릿 연동 — 두 차량 GPS 지도표시 + 명령생성 → 주행 트리거
- [ ] 3대+ 확장 — 팔로워 추가(sim 3대 → 실차 N대)

---

## 현재 크리티컬 패스 (다음 지워야 할 것 순서)
1. **실외 RTK Fixed 확보** (Phase1 첫 항목) — 실내 GPS로는 아무것도 검증 불가
2. **navsat finalize** (Phase1 블로커) — 이거 하나가 nav2주행·월드위치·form-up을 전부 막고 있음
3. → nav2 active → 단일차량 주행 → 정지 form-up → 편대 START → 대형전환

> ★ Phase 2(조율/미션 파이프라인)는 **이미 시스템으로 동작 확인됨**. 실차 편대주행의 남은 관문은
> 사실상 **localization(navsat) 하나**로 수렴함. 이걸 뚫으면 Phase3~4가 순차적으로 열림.
