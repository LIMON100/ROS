# 3대 군집 주행 시뮬레이션 — 실행 가이드 & 구조 (KO)

> English: [`swarm_sim_3robot.en.md`](./swarm_sim_3robot.en.md)

리더 1대(s1) + 팔로워 2대(s2, s3)를 **하나의 공유 Gazebo 월드**에 띄우고 경로를
주입해 **편대 주행**을 재현합니다. 전부 이 PC 한 대에서 돌아갑니다. 명령은 모두
저장소 루트(`combatrobot_1/`)에서 실행하세요.

---

## 1. 한눈에 보기

```bash
# 터미널 A — 스택 기동 (~40초, 띄운 채로 둘 것)
./scripts/run_swarm_sim3.sh

# 터미널 B — 경로 주입 + 주행 시작
./scripts/swarm_drive.sh

# 끝났으면 정리
./scripts/swarm_kill.sh
```

성공 = gz 창에서 로봇 3대가 북쪽으로 나란히 편대 주행 후 모두 도착
(launch 로그에 `FollowPath 완료 — 미션 도착` ×3).

---

## 1b. 검증된 실행 시나리오 (이 세션 기준)

단일호스트(16코어)에서 안정적으로 완주하는 권장 흐름:

```bash
# (재시작이면) 정리 후 load<6 · shm_fast=0 확인
./scripts/swarm_kill.sh

# 터미널 A — 3대 기동 (rviz 끔: 부하 절감, gz 창으로 관찰)
./scripts/run_swarm_sim3.sh -- use_rviz:=false

# 터미널 B — 기본 횡대 편대 주행 (55 m)
./scripts/swarm_drive.sh
```

**검증 결과:** s1·s2·s3 모두 ~40초에 `FollowPath 완료 — 미션 도착`, 종간격오차 0.3 m
로 안정(서징 없음). 더 긴 경로는 `./scripts/swarm_drive.sh -d 90` (롤링 코스트맵
±100 m 한계 내).

**부하 현실:** 이 호스트에서 주행 중 load ≈ 27 (16코어). 주범은 **gz sim server
(물리+라이다3) + gz sim gui(렌더) + 로봇별 파이썬 노드(executor·라이다필터)** — rviz·
센서주기 절감만으론 크게 안 떨어짐. 완주엔 지장 없으나 더 가볍게 하려면 2대 또는 HIL
분산(§10).

**알려진 한계:** *주행 중* 대형전환(`line`/`wedge` 등, §6b)은 전환 후 페이싱 교착으로
멈출 수 있음. **고정 대형(`-f` 로 출발) 주행은 안정적.**

---

## 2. 핵심 개념 — "한 프로그램"이 아니라 "독립 스택 3개"

한 프로그램이 로봇 3대를 굴리는 게 **아닙니다.** 기동 명령은 하나지만, 그 결과로
**대칭·독립 풀스택 3세트**가 뜨고, 각각 자기 ROS 네임스페이스(`/s1`, `/s2`, `/s3`)로
`PushRosNamespace` 격리됩니다.

```
run_swarm_sim3.sh
  └─ swarm_sim.launch.py  (num_robots:=3)
       ├─ gz_world_sim.launch.py        ← 공유 Gazebo 월드 1개 + clock bridge (공유)
       ├─ robot_bringup_sim.launch.py  ns=s1  robot_id=1  role=leader     ┐
       ├─ robot_bringup_sim.launch.py  ns=s2  robot_id=2  role=follower   ├ 풀스택 3개
       ├─ robot_bringup_sim.launch.py  ns=s3  robot_id=3  role=follower   ┘
       └─ rviz2 + TF 통합 relay         ← 3대를 한 rviz 에 표시 (공유)
```

즉 3대면 **nav2 스택 3개 + FSM 3개 + swarm_path_executor 3개 + command_server 3개**가
각각 별도 노드 인스턴스로 뜹니다. 서로 상태를 직접 공유하지 않고, 로봇별
네임스페이스 DDS 토픽(`/s1/...`, `/s2/...`, `/s3/...`)으로만 통신합니다. **트리거는
한 번, 결과는 격리된 스택 3개**입니다.

> **sim vs 실차:** sim 은 3대 스택이 전부 **이 PC 한 대**에서 gz 월드 하나를 공유하며
> 돕니다. 


---

## 3. 로봇 1대당 켜지는 노드 (각 `/sN` 네임스페이스 안)

`robot_bringup_sim.launch.py` 가 로봇 1대분 노드 세트를 `PushRosNamespace(ns)` 로
감싸 만듭니다. 노드는 HIL 분할 플래그 3개로 게이팅되며(기본 셋 다 `true` = 일반
단일호스트 sim), 아래는 접두사 없는 이름 — 런타임에는 `/sN/<이름>` 으로 보입니다.

| 그룹 (플래그) | 노드 | 패키지 · 실행파일 | 역할 |
|---|---|---|---|
| 항상 | `robot_state_publisher` | `robot_state_publisher` | URDF → TF 트리 |
| **몸체** (`launch_body`) | `ros_gz_sim/create` (spawn) | `ros_gz_sim · create` | gz 에 로봇 스폰 |
| | `gz_bridge` | `ros_gz_bridge · parameter_bridge` | cmd_vel / odom / lidar / IMU / GNSS 브리지 |
| **두뇌** (`launch_brain`) | `ekf_filter_node_odom` | `robot_localization · ekf_node` | local(odom) EKF |
| | `ekf_filter_node_map` | `robot_localization · ekf_node` | global(map) EKF |
| | `navsat_transform` | `robot_localization · navsat_transform_node` | GNSS → map, 공유 datum |
| | `frame_fixer` | `combat_robot_nav2 · frame_fixer.py` | GPS odom 을 `sN/map` 프레임으로 고정 |
| | **`swarm_path_executor`** | `combat_robot_nav2 · swarm_path_executor` | 편대 슬롯 오프셋, FollowPath 구동 |
| | `map_server` + `lifecycle_manager_map` | `nav2_map_server`, `nav2_lifecycle_manager` | `sN/map` 정적 빈 맵 |
| | **nav2** (아래 참조) | `navigation_lite.launch.py` | 자율주행 스택 |
| | `swarm_lidar_filter` | `combat_robot_nav2 · swarm_lidar_filter.py` | 자기 라이다에서 편대원 마스킹 |
| **명령** (`launch_command`) | **`command_server`** | `robot_server · command_server_node` | role 어댑터(leader/follower), 태블릿/앱 TCP·UDP |
| | **`combat_robot_operation_system`** (FSM) | `combat_robot_operation_system` | executor 앞단 명령 게이트 |

**nav2 스택** (`navigation_lite.launch.py` — 단일호스트에서 N스택 돌릴 때 CPU 절감을
위해 route_server/waypoint_follower/smoother_server/docking 을 뺀 경량 navigation_launch):

```
controller_server · planner_server · bt_navigator · behavior_server
velocity_smoother · collision_monitor · local_costmap · global_costmap
lifecycle_manager_navigation
```

**명령 게이트 체인** (로봇별 — 각 로봇이 자기 FSM 으로 자기 executor 를 게이트):

```
command_server → /sN/swarm/{path,control}_command
              → FSM (combat_robot_operation_system)
              → /sN/mission/{path,control}_command
              → swarm_path_executor → nav2 FollowPath → /sN/cmd_vel
```

sim 은 detection/gun/pan-tilt HW 가 없으므로 FSM 의 status 체크가 비활성
(`checks.*=false`)이고 게이트는 투명 전달입니다.

> `dynamic` 모드 팔로워는 편대 컨트롤러가 `/cmd_vel` 을 직접 몰기 때문에
> nav2/map_server/lidar_filter 를 건너뜁니다(안 그러면 collision_monitor 가 cmd_vel
> 을 두고 경합). 리더는 항상 nav2 를 돌립니다. 기본 모드는 `static` — 모든 로봇이
> 자기 nav2 로 오프셋 경로를 추종합니다.

**공유 노드** (`swarm_sim.launch.py` 가 로봇별이 아니라 1회만 기동): gz 월드 +
`clock_bridge`, `rviz2`, 그리고 로봇별 TF 통합 relay(`/sN/tf(_static)` → `/tf(_static)`
+ 항등 `map → sN/map`) — rviz 하나로 전 로봇을 보기 위함.

---

## 4. 사전 준비 (최초 1회)

| 항목 | 확인 / 설치 |
|---|---|
| OS / ROS | Linux + ROS 2 (이 호스트: **jazzy**). `gz sim --version` → Gazebo Sim 8.x |
| 워크스페이스 빌드 | `ros/install/` 존재해야 함. 코드 변경 시에만 재빌드 |
| 의존성(최초 1회) | `cd ros && rosdep install --from-paths src --ignore-src -r -y` |

빌드(코드 변경 시에만):

```bash
cd ros && colcon build --symlink-install --cmake-args -DCMAKE_BUILD_TYPE=Release && cd ..
# 또는 런처가 먼저 빌드하게:
./scripts/run_swarm_sim3.sh --build
```

> DDS 환경은 스크립트가 자동 설정합니다(둘 다 `scripts/lib/common.sh` source:
> `ROS_DOMAIN_ID=96`, `RMW_IMPLEMENTATION=rmw_fastrtps_cpp`, FastDDS 프로파일).
> **수동** `ros2` 명령은 §8 처럼 직접 맞춰야 합니다.

---

## 5. 스택 기동 — 터미널 A

```bash
./scripts/run_swarm_sim3.sh
```

내부적으로 `swarm_sim.launch.py num_robots:=3` 실행. 기동은 **계단식**
(gz 월드 → s1 @8초 → s2 @20초 → s3 @28초 → rviz)으로, nav2 라이프사이클이 순서대로
안정되게 합니다. **준비 완료** 신호(로봇마다):
`lifecycle_manager_navigation: Managed nodes are active`,
`command_server: [Swarm] role=leader namespace=s1`, rviz 에 3대 표시.

런처 뒤 `--` 로 launch 인자 전달:

```bash
./scripts/run_swarm_sim3.sh -- formation_lateral_spacing_m:=3.0   # 횡간격 3 m
./scripts/run_swarm_sim3.sh -- formation_mode:=dynamic            # 레거시 추종
./scripts/run_swarm_sim3.sh -- random_spawn:=true                 # 흩뿌린 뒤 form-up
```

### 편대 슬롯 (`robot_id` 로 자동)

`slot = formationSlotOffset(robot_id)`: rank `id-1` → 1:+1(좌) 2:-1(우) 3:+2 4:-2…
북쪽 경로에서 `+slot = 서(west)`(화면 왼쪽).

| 로봇 | role | slot | 스폰 | 북쪽 경로에서 |
|---|---|---|---|---|
| s1 | leader | 0 (중앙) | (0, 0) | 가운데 |
| s2 | follower | +1 | (−spacing, 0) | 왼쪽(서) |
| s3 | follower | −1 | (+spacing, 0) | 오른쪽(동) |

기본 횡간격 = 2.0 m.

---

## 6. 경로 주입 & 주행 — 터미널 B

스택이 "Managed nodes active" 를 띄운 뒤 **새 터미널**에서:

```bash
./scripts/swarm_drive.sh            # 기본 'go' = LOAD(3 wp, 북쪽 ~55 m) → START
```

`go` 동작: **LOAD**(`command:5`, 3-waypoint 경로를 s1,s2,s3 에 캐싱) → 2초 대기 →
**START**(`command:1`). 리더가 편대 앵커를 방송·정렬 후 출발하고, 팔로워는 횡오프셋
슬롯을 추종합니다.

```bash
./scripts/swarm_drive.sh -d 80      # 북쪽 ~80 m
./scripts/swarm_drive.sh -n 2       # s1,s2 만 (2대 sim 용)
./scripts/swarm_drive.sh load       # LOAD 만
./scripts/swarm_drive.sh start      # START
./scripts/swarm_drive.sh pause      # 일시정지
./scripts/swarm_drive.sh resume     # 재개
./scripts/swarm_drive.sh stop       # 정지
./scripts/swarm_drive.sh -w '{"waypoints":[{"lat":36.6101,"lon":127.2877},{"lat":36.6106,"lon":127.2877}]}' go
```

`SwarmPathCommand.command` enum: `1`=START `2`=STOP `3`=PAUSE `4`=RESUME
`5`=LOAD_PATH `6`=COMPLETE. 기본 경로는 sim GNSS datum(`sejong.world` 원점
`36.61002559225, 127.28772570583`)에서 정북으로 뻗습니다.

**대안 — rviz 클릭:** 리더(`/s1`)에 rviz 의 **Nav2 Goal** 을 찍으면 goal_bridge 가
편대 미션으로 변환합니다(단일호스트 sim 함정: 메모리 `swarm-rviz-goal-bridge`).

---

## 6b. 대형 변경 (주행 중 포함)

대형은 `SwarmControlCommand.formation_type` 으로 바꾸며 `/sN/swarm/control_command`
에 발행 → FSM 게이트 → `/sN/mission/control_command` → executor 로 전달됩니다.
**초기 대형은 횡대(line)** 입니다. **주행 중** 바꾸면 executor 가 정지 → 팔로워
재슬롯(NavigateToPose 로 새 횡오프셋) → 남은 경로 재개를 합니다.

| 명령 | type | 대형 | 모양 |
|---|---|---|---|
| `./scripts/swarm_drive.sh line` (또는 `횡대`) | 0 | 횡대 | 팔로워 나란히 |
| `./scripts/swarm_drive.sh column` (또는 `종대`) | 1 | 종대 | 리더 뒤 일렬 |
| `./scripts/swarm_drive.sh diamond` | 2 | 마름모 | 측면 호위 |
| `./scripts/swarm_drive.sh wedge` | 3 | 쐐기 | 뒤-대각 V |

`-f` 로 특정 대형으로 출발:

```bash
./scripts/swarm_drive.sh -f wedge go        # 쐐기로 정렬 후 주행
```

전체 데모 — 긴 경로 주행 중 횡대로 전환:

```bash
./scripts/swarm_drive.sh -d 90 -f wedge go  # 쐐기로 ~90 m 주행
# ...주행 몇 초 뒤, 같은/다른 터미널에서:
./scripts/swarm_drive.sh line               # → 횡대 (주행 중 나란히 펼침)
```

> ⚠️ 단일 미션 경로는 출발점 **±100 m** 안에 둘 것: 글로벌 코스트맵이 200×200 m
> 롤링 윈도우(§3)라서. 더 긴 경로는 `nav2_params_sim.yaml` 의 `width/height` 를 키울 것.
> ⚠️ **주행 중 대형전환은 현재 취약합니다(미해결).** 전환 명령은 정상 트리거되지만
> (리더 정지→재정렬→재개), 재개 시 리더-팔로워 페이싱이 교착(frac arc 가비지 →
> 리더 미전진·팔로워 v=0)되어 미션이 중간에 멈출 수 있습니다. 검증된 케이스에서
> wedge→line 전환이 ~중간지점에서 정지했습니다. 정지 대형(`-f` 로 출발시 고정)은
> 안정적입니다. 배경: 메모리 `swarm-column-transition-stuck-rootcause`,
> `swarm-formation-transition-redesign`.

---

## 7. 주행 확인

```bash
# (먼저 §8 의 DDS 환경 설정)
ros2 topic echo --once /s1/cmd_vel geometry_msgs/msg/Twist --field linear.x   # >0 = 주행중
ros2 topic echo --once /s1/odom nav_msgs/msg/Odometry --field pose.pose.position
```

기대 로그:

```
s1.swarm_path_executor  [Formation] 리더 앵커 방송 — 대형 정렬 후 출발
s2.swarm_path_executor  편대 오프셋 적용: slot=1  lateral=+2.00m
s3.swarm_path_executor  편대 오프셋 적용: slot=-1 lateral=-2.00m
sN.swarm_path_executor  FollowPath 추종 시작: NNN 점, 54.8 m
sN.controller_server    Reached the goal!  →  FollowPath 완료 — 미션 도착   (×3)
```

---

## 8. 수동 `ros2` — DDS 환경

스크립트 밖에서는 노드와 같은 도메인/전송을 맞춰야 합니다(불일치 → 도메인 0 →
아무것도 안 보임):

```bash
export ROS_DOMAIN_ID=96
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export FASTRTPS_DEFAULT_PROFILES_FILE=$PWD/scripts/fastdds_profile.xml
unset CYCLONEDDS_URI
source ros/install/setup.bash
```

---

## 9. 종료 / 정리

```bash
./scripts/swarm_kill.sh
```

⚠️ **반드시 이 스크립트로 종료하세요.** 재시작을 반복하면 죽지 않은 launch 부모가
자식 노드를 재spawn 하고 `/dev/shm/fastrtps_*` 세그먼트가 누적되어 **코드 버그처럼
보이는 환경 문제**(form-up freeze, 미션 미동작)를 일으킵니다. `swarm_kill.sh` 는
부모 먼저 kill → 노드 정리 → SHM 정리 → load 보고. **재기동 전 반드시 `load < ~6`
그리고 `shm_fast=0`** 확인(스크립트가 출력). CPU 과부하면 executor 가 굶어 편대
측정이 깨집니다.

---

## 10. HIL 분할 (`launch_body` / `launch_brain` / `launch_command`)

로봇 1대 스택을 여러 머신에 쪼갤 수 있습니다(hardware-in-the-loop). 셋 다 기본
`true`(= 일반 단일호스트 sim):

| 플래그 | 노드 | `true` 로 켜는 곳 |
|---|---|---|
| `launch_body` | gz spawn + bridge ("몸체") | gz 제공 호스트 |
| `launch_brain` | ekf + nav2 + executor ("두뇌") | 연산 보드 |
| `launch_command` | command_server + FSM | 해당 패키지 있는 호스트 (실보드: `false`) |

`robot_state_publisher` 는 항상 포함(gz spawn 은 `/robot_description`, nav2/ekf 는 TF
트리 필요).

---

## 11. 트러블슈팅

| 증상 | 원인 / 조치 |
|---|---|
| 로봇/토픽이 안 보임 | DDS 환경 미설정(§8) 또는 도메인 불일치 |
| `/sN/fix`(GPS) 비어 있음 | gz-transport 가 죽은 iface 를 잡음 → 런처가 `GZ_IP=127.0.0.1` 핀; 멀티호스트면 실제 IP |
| START 했는데 안 움직임 / form-up freeze | 호스트 과부하(좀비 스택) → `swarm_kill.sh` 후 load<6 확인하고 재기동 |
| `node list` 에 s3 가 늦게 뜸 | 계단식 기동(s3 ~28초). 다 뜰 때까지 대기 |
| 반복 대형변경 중 도중정지 | 알려진 이슈(메모리 `tablet-formation-change-demo`, `swarm-column-transition-stuck-rootcause`) |
| GPS 토픽 `echo` 멈춤 | lazy gz 브릿지 — `timeout -s KILL 6 ros2 topic echo --once ...` |

---

## 12. 관련 파일

- `scripts/run_swarm_sim3.sh` — 3대 런처(§5)
- `scripts/swarm_drive.sh` — 경로 주입 + 주행 제어(§6)
- `scripts/run_swarm_sim.sh` — 일반 N대 런처(`-- num_robots:=N`)
- `scripts/swarm_kill.sh` — 정리(§9)
- `ros/.../combat_robot_nav2/launch/swarm_sim.launch.py` — 멀티로봇 오케스트레이션
- `ros/.../combat_robot_nav2/launch/robot_bringup_sim.launch.py` — 로봇 1대 스택 + datum + HIL 플래그
- `ros/.../combat_robot_nav2/launch/navigation_lite.launch.py` — 경량 nav2 스택
