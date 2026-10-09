# Codebase Analysis: combatrobot_1

> Generated: 2026-03-04
> Updated: 2026-05-11

---

## 1. 프로젝트 개요

**ROS2 기반 전투 로봇 운용 시스템** — AI 타겟 획득, 시각 서보잉(Visual Servoing), 원격 조종을 지원하는 실시간 임베디드 시스템. 시뮬레이션이 아닌 실제 하드웨어(총기, 모터, 카메라)를 제어한다.

| 항목 | 내용 |
|---|---|
| **플랫폼** | Rockchip RK3588 (Orange Pi 5 / Rock 5B) |
| **AI 칩** | Hailo8 M.2 (YOLOv5s / YOLO11s) |
| **주요 언어** | C++ (ROS2 패키지), Bash (설정 스크립트) |
| **미들웨어** | ROS2 Humble, `colcon` 빌드 |
| **비전** | OpenCV 4.x, GStreamer (RTSP 스트리밍) |
| **탐지 대상** | 사람(Human), 드론(Drone) |

---

## 2. 아키텍처 구조

```
combat_robot.launch.xml          ← 전체 시스템 단일 진입점
        │
        ├── combat_robot_operation_system  ← 두뇌 (FSM + 미션/데모 시퀀서)
        │       ├── subscribes: human_detector, pan_tilt, gun_trigger, mission/swarm
        │       └── publishes:  operation_state, pan_tilt/drive/gun commands
        │
        ├── human_detector           ← Hailo8 AI 추론 (YOLO + SORT 트래커)
        ├── pan_tilt_controller      ← 시리얼 액추에이터 + PID 시각 서보잉
        ├── robot_server             ← TCP/UDP 브리지 (태블릿/원격 앱)
        ├── swarm_coordinator        ← Leader/Follower 명령 relay + heartbeat
        ├── teleop_controller        ← Modbus RTU 섀시 구동
        ├── gun_trigger              ← Linux sysfs PWM 발사 제어
        ├── camera_interface         ← GStreamer RTSP 스트림
        └── image_best_effort_viewer ← 디버깅용 영상 뷰어
```

`combat_robot.launch.xml` 외에 `combat_robot_test.launch.xml`이 별도로 존재하여, 실제 하드웨어 없이 `combat_robot_test_dummy_node`로 IMU/Pan-Tilt/Detector/Gun 토픽을 시뮬레이션 publish 하는 오피스 테스트 환경을 제공한다.

### 디렉토리 구조

```
combatrobot_1/
├── docs/                             # 문서 (매뉴얼, 코딩 표준, 앱 통합 스펙)
├── ros/src/skyautonet/
│   ├── combat_robot_launch/          # 메인/테스트 런치 파일 + device_profile.yaml
│   └── combat_robot_system/
│       ├── combat_robot_msgs/        # 커스텀 ROS 메시지
│       ├── combat_robot_operation_system/  # 핵심 FSM 노드 (4-file 분리)
│       ├── human_detector/           # Hailo8 AI 탐지 노드
│       ├── pan_tilt_controller/      # Pan/Tilt 시리얼 드라이버
│       ├── robot_server/             # 네트워크 인터페이스 (분리 구조)
│       ├── swarm_coordinator/        # Leader/Follower swarm relay 노드
│       ├── teleop_controller/        # 원격 조종 로직
│       ├── gun_trigger/              # 무기 트리거 제어
│       ├── camera_interface/         # 카메라 드라이버
│       ├── image_best_effort_viewer/ # 디버깅 영상 뷰어
│       ├── imu_publisher/            # IMU 센서 인터페이스
│       └── laser_distance/           # 레이저 거리 측정
├── combatrobotcontroller/            # RTSP/GStreamer 카메라 스트리밍 서브 프로젝트
├── scripts/                          # 시스템 설정 스크립트
└── test/                             # 테스트 스크립트
```

---

## 3. 핵심 비즈니스 로직

### 3.1 유한 상태 머신 (FSM)

`combat_robot_operation_system` 패키지의 `CombatRobotOperationSystem`이 고수준 상태를 관리한다. 2026-04 리팩터링으로 거대 `.cpp` 한 개가 4-file 구조로 분리되어, 헤더는 그대로지만 구현은 책임별 파일에 분산되어 있다.

| 파일 | 책임 |
|---|---|
| `src/combat_robot_operation_system.cpp` | 노드 lifecycle, 파라미터, 토픽 binding |
| `src/combat_robot_operation_system_callbacks.cpp` | 외부 토픽/시간 콜백 진입점 |
| `src/combat_robot_operation_system_state.cpp` | `*_statefunc()` 본문, `updateState()`/`transitState()` |
| `src/combat_robot_operation_system_control.cpp` | Pan/Tilt 제어, 발사, scan/tracking, demo 시퀀서 |
| `src/combat_robot_test_dummy_node.cpp` | 오피스 테스트용 더미 토픽 publisher (별도 실행 노드) |
| `test/test_fsm_transitions.cpp` | GTest 기반 FSM 전환 + 데모 시퀀스 검증 |

| 상태 (`e_operation_state`) | 설명 |
|---|---|
| `INIT_STATE` | 부팅 직후, Pan/Tilt 영점/초기화 대기 |
| `IDLE` | 대기 상태, 사용자 명령 대기 |
| `MOVE_STATE` | 차량 주행 모드 (path/swarm 명령 기반) |
| `SURVEILLANCE_STATE` | 자동 스캔 패턴 (줌 레벨 기반 동적 속도 조절) |
| `DRONE_SURVEILLANCE_STATE` | 드론 탐지 전용 스캔 패턴 |
| `TRACKING_STATE` | AI가 타겟 고정, PID로 Pan/Tilt 센터링 |
| `ATTACKING_STATE` | 안전 인터록 확인 후 발사 시퀀스 실행 |
| `ASSAULT_STATE` | 돌격 모드 (이동 + 자동 교전) |
| `RTH_STATE` | Return-to-Home (예약) |
| `EMERGENCY_STOP_STATE` | 하드웨어/앱 e-stop 인터록 |
| `ERROR_STATE` | 센서 타임아웃 워치독 감지 시 전환 |

런-타임 모드(`e_run_mode_`)는 위 상태와 별개로 운용 모드(`IDLE/MOVING/SURVEILLANCE/DRONE_SURVEILLANCE/ATTACKING/MANUAL_ATTACK/ASSAULT/EMERGENCY_STOP/DEMO`)를 분리하여 표현한다. 앱이 보낸 `ACTIVE_MODE_*` ID와 매핑되며, `OperationState.msg`로 publish 된다.

**상태 전환 흐름:**
- `updateState()` + `transitState()` 가 탐지 결과·사용자 명령·미션 상태를 기반으로 다음 상태를 결정
- `checkSensorState()` 가 각 센서의 업데이트 주기를 감시하여 노드 크래시 탐지
- `last_non_estop_active_mode_id_` 로 e-stop 해제 시 직전 모드로 복귀

### 3.1.1 데모 시퀀스 (`DEMO` 운용 모드)

`RECON / PROTECT_GENERAL / ASSAULT` 등 앱 명령으로 들어오는 데모 모드는 `e_demo_phase_` 로 단계화된다.

| Phase | 설명 |
|---|---|
| `DEMO_PHASE_IDLE` | 데모 트리거 대기 |
| `DEMO_PHASE_FORWARD` | `demo_forward_distance_m_` 만큼 전진 |
| `DEMO_PHASE_SCAN` | `demo_scan_default_pan/tilt_deg_` 기준 스캔 시작 |
| `DEMO_PHASE_ENGAGE` | 타겟 락 → `demo_fire_duration_sec_` 동안 발사 |
| `DEMO_PHASE_REVERSE` | `demo_reverse_distance_m_` 만큼 후진 |
| `DEMO_PHASE_COMPLETE` | 데모 종료, IDLE 복귀 |

데모 관련 파라미터(`demo_target_count_`, `demo_drive_speed_mps_`, `demo_fire_duration_sec_` 등)는 ROS 파라미터화되어 있어 launch에서 조정 가능하다. `resetDemoSequence()` 가 강제 중단/완료 후의 상태를 정리한다.

### 3.2 핵심 알고리즘

| 알고리즘 | 위치 | 목적 |
|---|---|---|
| **PID 제어** | `pan_tilt_controller.cpp` | 타겟 중심화를 위한 Pan/Tilt 시각 서보잉 |
| **SORT 트래커 + 칼만 필터** | `human_detector.cpp` | 프레임 간 다중 객체 일관된 ID 유지 |
| **헝가리안 알고리즘** | `human_detector.cpp` | 새 탐지와 기존 트랙 데이터 연관 |
| **탄도 보정 + Feedforward** | `combat_robot_operation_system_control.cpp` | 속도 기반 예측 리드, LPF 속도 평활화(`m_velocity_lpf_alpha_`), 중력/풍향 보정 |
| **동적 속도 스케일링** | `combat_robot_operation_system_state.cpp` | 줌 레벨에 따른 스캔 속도 자동 조절 |
| **데모 시퀀서 (FSM 내부)** | `combat_robot_operation_system_control.cpp` | 전진→스캔→교전→후진 데모 phase 머신 |
| **Leader↔Follower 명령 relay** | `swarm_coordinator.cpp` | 리더가 미션/포메이션/path 명령을 follower 슬롯별 토픽으로 분배, heartbeat 타임아웃 추적 |

### 3.3 커스텀 ROS 메시지 (도메인 데이터 모델)

| 메시지 | 내용 |
|---|---|
| `OperationState.msg` | 시스템 상태 + 미션 진행/총 waypoint + estop_active + GPS |
| `TargetPoint.msg` | 탐지 타겟 (x, y, height, track_id, class_id) |
| `DetectedObject(s).msg` | 탐지된 객체 리스트와 bounding box |
| `UserCommand.msg` | 태블릿 명령 (조준, 구동, 발사, 드론 탐색) |
| `MissionControlCommand.msg` | 모드 전환/미션 제어 (LOAD_PATH/START/PAUSE/RESUME/STOP) |
| `StreamControlCommand.msg` | 카메라/스트림 채널 제어 |
| `DriveCommand.msg` | 차량 구동 명령 (linear/angular velocity) |
| `TouchTargetPoint.msg` | Touch-to-Aim 화면 좌표 |
| `PanTiltControlCommand.msg` / `PanTiltState.msg` | 저수준 액추에이터 명령/상태 |
| `CenterObject.msg` | 크로스헤어 내 객체 상태 (거리, 줌 레벨) |
| `IMUState.msg` | IMU 자세 정보 |
| `SwarmControlCommand.msg` | 포메이션/그룹/선택된 robot id 등 swarm 제어 |
| `SwarmPathCommand.msg` | path/mission JSON payload + path_id |
| `SwarmRobotCommand.msg` | 리더→follower 단위 전달 명령 (mode/path/formation/sync) |
| `SwarmLeaderHeartbeat.msg` | 리더 sequence/operation_mode/estop/포메이션 상태 |
| `SwarmFollowerStatus.msg` | follower 링크 상태 + 마지막 모드/포메이션 echo |

### 3.4 네트워크 인터페이스 (`robot_server`)

| 포트 | 프로토콜 | 역할 |
|---|---|---|
| TCP 65432 | TCP | 명령 (모드 전환, PTZ 제어) |
| UDP 65433 | UDP | Touch-to-Aim 좌표 |
| UDP 65434 | UDP | 섀시 주행 명령 |
| TCP 65435 | TCP | 시스템 상태 피드백 |
| TCP 65436 | TCP | 경로/미션 제어 (`LOAD_PATH/START/PAUSE/RESUME/STOP`) |

#### 현재 내부 구현 구조

`robot_server`의 `command_server` 구현은 2026-04 리팩터링 기준으로 기능별 파일 분리가 시작된 상태다.

| 파일 | 책임 |
|---|---|
| `include/command_server_protocol.hpp` | packed wire protocol 구조체, 포트 상수, `SocketGuard` |
| `include/command_server_internal_utils.hpp` | robot ID / formation / mode gate 공통 helper |
| `src/command_server.cpp` | `CommandServerNode` 상태 보관, 초기화, swarm aggregate 동기화, ROS callback |
| `src/command_server_transport.cpp` | command/touch/driving/path 소켓 thread 구현 |
| `src/command_server_status.cpp` | `SwarmStatusPacket` 조립 + status TCP 송신 thread |
| `src/command_server_dispatch.cpp` | 큐에서 꺼낸 명령을 `state/drive/touch` handler로 분기 후 ROS 토픽 publish |
| `src/command_server_path_payload_parser.cpp` | path JSON payload 파싱, waypoint 수 검증, `missionId/routeId` 추출 |

#### 현재 데이터 흐름

1. `command_server_transport.cpp`가 외부 TCP/UDP 입력을 수신한다.
2. 입력은 `GenericCommand` 큐에 적재된다.
3. `command_server_dispatch.cpp`의 `publish_command()`가 큐를 비우며 `handleStateCommand()`, `handleDrivingCommand()`, `handleTouchCommand()`로 분기한다.
4. 각 handler는 ROS 토픽(`/mission_control_command`, `/stream_control_command`, `/swarm/control_command`, `/drive_command`, `/touch_command`)으로 변환 publish 한다.
5. 상태 피드백은 `command_server_status.cpp`가 내부 atomic 상태와 aggregate cache를 이용해 `SwarmStatusPacket`으로 조립한 뒤 앱으로 송신한다.

#### 리팩터링 상태 메모

- `command_server.cpp`는 더 이상 각 TCP/UDP thread 본문이나 path payload parser를 직접 포함하지 않는다.
- 큰 함수였던 `publish_command()`는 dispatch 전용 파일로 이동했고, 내부도 `state/drive/touch` handler로 분리됐다.
- `command_server.cpp`는 640 lines 수준까지 축소됐으며, node lifecycle/상태 cache 관리에 집중하는 coordinator로 자리 잡았다.
- 2026-05 기준 `mission_control_command` 처리는 swarm 메시지로 통합되어, `command_server_internal_utils.hpp` 의 mode-gate helper와 `swarm_coordinator` 가 함께 mission 명령을 분배한다.

### 3.5 Swarm Coordinator

`swarm_coordinator` 패키지는 다중 로봇 운용을 위한 leader/follower relay 노드다 (2026-04 신설).

| 항목 | 내용 |
|---|---|
| 노드 클래스 | `swarm_coordinator::SwarmCoordinatorNode` (`rclcpp::Node`) |
| 실행 파라미터 | `role` (`leader`/`follower`), `robot_id`, `leader_robot_id`, `relay_to_self`, heartbeat 주기/타임아웃 |
| Leader 동작 | `mission_control_command` / `swarm/path_command` / `swarm/control_command` 구독 → `selected_robot_ids` 별 `SwarmRobotCommand` 발행 + 200 ms 주기 heartbeat publish |
| Follower 동작 | 리더의 `SwarmRobotCommand` / `SwarmLeaderHeartbeat` 구독 → 자기 슬롯 명령만 follower-local 토픽으로 relay, `SwarmFollowerStatus` (링크 상태/heartbeat age) 주기 publish |
| QoS | command: `Reliable + TransientLocal`, heartbeat: `BestEffort` |
| 최대 robot 수 | `MAX_ROBOTS = 8` (배열 인덱스 1..8) |

`combat_robot.launch.xml` 에서 `use_swarm_coordinator` 인자로 on/off, `swarm_role`/`robot_id`/`leader_robot_id` 로 역할을 지정한다.

---

## 4. 코드 품질 평가

### 4.1 강점

- **명확한 관심사 분리** — 탐지 / 작동 / 오케스트레이션 / 통신이 별도 노드로 완전히 분리됨
- **`operation_system` 분할 완료** — 1400+ 라인 모놀리스가 `callbacks` / `state` / `control` 3 파일로 분리, 헤더 한 곳에서만 선언 관리
- **`robot_server` 내부 분리 진행** — protocol / transport / status / dispatch가 별도 파일로 나뉘기 시작해 `command_server.cpp` 집중도가 낮아짐
- **Swarm 운용 인프라 도입** — 다중 로봇 leader/follower relay와 heartbeat 모니터링이 별도 노드로 캡슐화됨
- **ROS2 Managed Lifecycle 노드** — 하드웨어 드라이버(`pan_tilt_controller`, `rtsp_server`)에 configure → activate → deactivate → cleanup 패턴 적용
- **하드웨어 워치독** — `checkSensorState()`가 데드 노드를 감지하여 `ERROR_STATE`로 자동 전환
- **무기 안전 인터록** — `gun_trigger_permission` 플래그 확인 후에만 PWM 발사 허용
- **`std::lock_guard` 일관 적용** — 고빈도 이미지 콜백에서 스레드 안전성 확보
- **테스트/오피스 데모 환경 구축** — `combat_robot_test.launch.xml` + `combat_robot_test_dummy_node` 로 하드웨어 없이도 FSM/명령 흐름 검증 가능

### 4.2 문제점 / 기술 부채

| 영역 | 문제 | 심각도 |
|---|---|---|
| **테스트** | `test_fsm_transitions.cpp` 가 T1~T8 + 데모 시퀀스를 커버하지만, `swarm_coordinator` / `robot_server` 자동 테스트는 아직 없음 | 🟡 중간 |
| **하드코딩된 상수** | ~~FOV/스캔 속도/Pan-Tilt 한계/매직 divider 등이 헤더 default 또는 인라인 매직 상수~~ → `config/params.yaml` + `params.demo.yaml` / `params.office_test.yaml` overlay 구조로 통합 (2026-05-11) | ✅ 해결 |
| **GStreamer 파이프라인** | `rtsp_server.cpp`에서 문자열 연결로 파이프라인 구성 — 취약하고 유지보수 어려움 | 🟡 중간 |
| **에러 핸들링 불일치** | 코딩 표준은 예외 사용 권장, 실제 구현은 `RCLCPP_ERROR_THROTTLE` + 조기 반환 혼용 | 🟡 중간 |
| **RTSP 코드 중복** | `cam0` / `cam1` 콜백 설정이 거의 동일한 코드 반복 | 🟡 중간 |
| **미완성 TODO** | FSM 내 `// Todo: add vehicle state check`, `// Todo: add Manual attack mode` 미구현 | 🟡 중간 |
| **데모 파라미터 산재** | ~~`demo_*` 파라미터가 헤더 기본값 + launch 인자로 이원화~~ → base yaml + overlay로 단일화 (2026-05-11) | ✅ 해결 |
| **Swarm 보안** | `SwarmRobotCommand`/heartbeat 가 평문 토픽이라 위·변조 무방비, 인증/sequence rollback 검증 없음 | 🟡 중간 |
| **Python 타입 힌트 부재** | 텔레오프 및 테스트 스크립트에 PEP 484 어노테이션 없음 | 🟢 낮음 |
| **RK3588 하드코딩** | `mpph264enc` GStreamer 요소가 비-Rockchip 플랫폼에서 동작 불가 | 🟢 낮음 |

---

## 5. 개선 권고사항

1. **Swarm 명령 인증/무결성 보강** (우선순위: 높음)
   - `SwarmRobotCommand` / `SwarmLeaderHeartbeat` 에 sequence 단조 증가 검증, leader_robot_id 화이트리스트, MAC/HMAC 또는 ROS2 SROS2 적용 검토
   - heartbeat 손실 시 follower의 fail-safe 동작(현재는 링크 끊김만 표시) 을 명문화

2. **테스트 커버리지 확장** (우선순위: 높음)
   - `test_fsm_transitions.cpp` 패턴을 `swarm_coordinator` (leader→follower relay, heartbeat 타임아웃) 로 복제
   - `robot_server` 의 `command_server_path_payload_parser` / `command_server_dispatch` 에 대한 단위 테스트 추가 (JSON 파싱 경계 케이스, mode-gate 거부 시나리오)
   - `colcon test` 가 CI 에서 항상 돌도록 `combat_robot_launch` 메타 패키지에 테스트 의존성 등록

3. **~~튜닝 상수 + 데모 파라미터 단일 YAML 화~~** (✅ 2026-05-11 완료)
   - FOV/aspect/FPS/Pan-Tilt 한계/scan timeout shape/Pan-Tilt speed divider/tilt aspect factor 모두 `params.yaml` 로 이전
   - 사용처 없는 `#define FPS`(타이머 주기로 흡수)·`#define TILT_INTERVAL`·`static constexpr FRAMES_LOST_TOLERANCE`·`frames_lost_counter_` 는 dead code 로 제거
   - `demo.*` 와 `dummy_leader_state.*` 는 base + deployment-mode overlay (`params.demo.yaml`, `params.office_test.yaml`) 구조로 통합. `combat_robot_device.launch.py` 가 mode 에 따라 overlay 경로를 결정. `combat_robot_operation_system.launch.xml` 의 `dummy_leader_*`/`check_*` 개별 `<arg>` 9 개 제거

4. **GStreamer 파이프라인 빌더 추상화 + RTSP 채널 통합** (우선순위: 중간)
   - `rtsp_server.cpp` 의 문자열 연결을 타입 안전한 빌더로 교체
   - `cam0`/`cam1` 콜백을 채널 ID 파라미터를 받는 단일 함수로 통합 (RK3588 외 플랫폼 대응을 위해 encoder element 도 파라미터화)

5. **미구현 TODO 완성** (우선순위: 중간)
   - 감시/공격 전 차량 상태 (`vehicle state check`) 확인 로직 추가
   - 수동 공격 모드 (`Manual attack mode`) 의 안전 인터록 정의 후 구현
   - `RTH_STATE` 의 실제 경로 복귀 동작 정의

6. **에러 핸들링 정책 통일** (우선순위: 중간)
   - 코딩 표준이 권장하는 예외 사용 vs. 현 구현의 `RCLCPP_ERROR_THROTTLE` + early-return 혼용 중 한 가지로 통일
   - 노드 경계는 예외 → ROS 로깅으로 변환하는 단일 wrapper 적용 검토

7. **`command_server.cpp` coordinator 슬림화 마무리** (우선순위: 낮음)
   - 남아 있는 lifecycle/상태 helper 를 `command_server_internal_utils.hpp` 로 이관해 coordinator 책임만 남기기

8. **Python 스크립트 타입 힌트 추가** (우선순위: 낮음)
   - 텔레오프/테스트 스크립트에 PEP 484 어노테이션 적용, `mypy --strict` 게이트 검토

---

## 6. 핵심 파일 참조

| 파일 | 역할 |
|---|---|
| `combat_robot_operation_system.hpp` | FSM/데모/미션 상태 + Pub/Sub 인터페이스 단일 선언부 |
| `combat_robot_operation_system.cpp` | Node lifecycle, 파라미터, 토픽 binding |
| `combat_robot_operation_system_state.cpp` | `*_statefunc()` + 상태 전환 결정 로직 |
| `combat_robot_operation_system_control.cpp` | Pan/Tilt/Drive/Fire 제어, scan, demo 시퀀서 |
| `combat_robot_operation_system_callbacks.cpp` | 토픽/타이머 콜백 진입점 |
| `swarm_coordinator/src/swarm_coordinator.cpp` | Leader/Follower relay 노드 |
| `human_detector.cpp` | Hailo8 추론 + SORT 트래킹 |
| `pan_tilt_controller.cpp` | PID 조준 + 시리얼 액추에이터 |
| `rtsp_server.cpp` | GStreamer 영상 스트리밍 |
| `gun_trigger.cpp` | PWM 발사 메커니즘 |
| `combat_robot.launch.xml` / `combat_robot_test.launch.xml` | 운용/오피스 테스트 진입점 |
| `combat_robot_msgs/msg/*.msg` | 도메인 데이터 컨트랙트 |

---

## 7. 문서/기여 가이드 최신화 메모

### 2026-03-05
- 저장소 루트에 `AGENTS.md`(Repository Guidelines) 문서가 추가됨.
- 신규 기여자는 `AGENTS.md`의 빌드/테스트/커밋 규약과 `docs/coding_standards/*`를 함께 참조하는 것을 권장.

### 2026-04-22 ~ 2026-05-07
- `robot_server` 의 `command_server` 가 transport/dispatch/status/path-parser 등으로 파일 분리됨 (`docs/robot_server_structure.md` 참조).
- `combat_robot_operation_system` 이 callbacks/state/control 3 파일로 분할되었고, 헤더 한 곳에서만 인터페이스가 선언됨.
- 신규 패키지 `swarm_coordinator` 가 추가되어 leader/follower 명령 relay와 heartbeat 모니터링을 담당. 관련 메시지 `SwarmRobotCommand`, `SwarmLeaderHeartbeat`, `SwarmFollowerStatus` 추가.
- `combat_robot_test.launch.xml` + `combat_robot_test_dummy_node` 도입으로 하드웨어 없는 오피스 테스트 환경 구축.
- 데모/돌격/RECON 시퀀스가 FSM 에 통합되어 `e_demo_phase_` 머신으로 단계화됨.
- 앱 통합 스펙(`docs/app_unified_spec.md`) 및 시스템 그래프(`docs/ros2_system_graph.mmd`) 정비. 명령 패킷 구조와 e-stop 처리 흐름이 명확화됨.

### 2026-05-11
- `combat_robot_operation_system` 파라미터 통합: 헤더 `#define`/`constexpr`/멤버 default 로 분산되어 있던 FOV·Pan-Tilt 한계·scan timeout shape·speed divider·tilt aspect factor 와, launch xml 중복 정의되던 `dummy_leader_*`/`check_*` 를 `config/params.yaml` (base) + `params.demo.yaml`·`params.office_test.yaml` (overlay) 구조로 단일화.
- `combat_robot_device.launch.py` 가 `deployment_mode` 에 따라 overlay 경로를 선택해 `params_overlay_file` 인자로 전달. production 은 overlay 가 base 와 동일한 파일을 가리키도록 fallback.
- `initParameters()` 메서드 신설로 constructor 본문에서 declare_parameter 호출 블록 분리.
- 헤더의 `#define FPS`, `#define TILT_INTERVAL`, `FRAMES_LOST_TOLERANCE`, `frames_lost_counter_` 는 사용처가 없어 dead code 로 제거. FPS 는 타이머 주기(`optics.fps`) 로만 사용.
