# 앱-로봇 인터페이스 명세

## 1. 문서 목적

이 문서는 앱과 로봇이 주고받는 데이터 정의를 최종 제품 기준으로 정리한다.

## 1.1 현재 태블릿 테스트 서버 기준 스냅샷

`combatrobotcontroller/src/robot_server_rknn.cpp`와 `combatrobotcontroller/src/robot_server_rpi.cpp` 기준 현재 구현은 아래와 같다.

- 이 절은 현재 태블릿 앱 테스트 서버의 실제 wire layout을 설명한다.
- 아래 스냅샷과 이후 본문이 충돌하면, 현재 테스트 서버 연동에서는 이 절을 우선한다.
- 태블릿 앱은 leader robot에만 명령을 보낸다.
- follower robot 직접 제어는 하지 않으며, leader가 follower 상태를 집계해 앱으로 보낸다.
- follower 제어/상태 수집은 장기적으로 ROS 2 topic + mesh 계층에서 처리할 계획이지만, 현재 태블릿 테스트 서버 범위에는 포함하지 않는다.

현재 핵심 결정:

- robot ID는 문자열이 아니라 `uint32`다.
- `1..8`은 각각 `S1..S8`에 대응한다.
- leader robot도 `S1..S8` 안에 포함되며, 현재 dummy telemetry에서는 `S1`이 leader다.
- 태블릿 스트리밍 요청은 leader에만 보내며, `stream_target_robot_id`로 어떤 로봇 영상을 원하는지 지정한다.
- leader는 현재 앱에 내보내는 영상 소스를 `active_stream_robot_id`로 상태에 포함한다.
- 집계 상태는 `SwarmStatusPacket`으로 보내며, `robots[8]`과 `logs[16]`을 포함한다.
- `leader_status`는 leader 요약 블록이고, `robots[8]` 안에도 같은 leader(`S1`)가 포함된다.
- `StatusPacket`에는 raw echo용 `last_tablet_command_id`가 포함되며, state machine feedback이 없어도 태블릿이 마지막에 보낸 `command_id`를 그대로 확인할 수 있다.
- formation 체계는 `formation_type + formation_number` 조합으로 단순화한다.
- `formation_type`: `0=None`, `1=Recon`, `2=Protect`, `3=Assault`
- `formation_number`: `0=None`, `1..4=해당 타입의 preset 번호`
- 현재 테스트 서버는 앱 테스트용으로 dummy telemetry를 기본 로드하며, disconnected robot도 포함한다.

현재 `StateCommand` wire layout:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `command_id` | `uint8` | 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 6=Assault, 7=Return to Home, 8=E-Stop |
| `e_stop_command` | `uint8` | 긴급 정지 플래그 |
| `attack_permission` | `uint8` | 발사 승인 플래그 |
| `pan_speed` | `int8` | 팬 속도 |
| `tilt_speed` | `int8` | 틸트 속도 |
| `zoom_command` | `int8` | 줌 증감 |
| `lateral_wind_speed` | `float32` | 횡풍 추정값 |
| `stream_command` | `uint8` | 0=None, 1=Start, 2=Stop |
| `stream_target_robot_id` | `uint32` | 앱이 보고 싶은 영상 소스 로봇 ID, 0이면 leader/default |
| `drone_target_lat` | `float64` | Protect Drone 목표 위도 |
| `drone_target_lon` | `float64` | Protect Drone 목표 경도 |
| `drone_target_valid` | `uint8` | 좌표 유효 여부 |

현재 leader -> app 집계 상태 요약:

| 구조체 | 설명 |
| --- | --- |
| `StatusPacket` | leader 기본 상태 + navigation/mission/assault 상태 + swarm/stream 메타데이터 + raw tablet command echo (`last_tablet_command_id`) |
| `RobotAggregateStatus` | `S1..S8` 각 로봇의 연결 상태, 통신 품질, 배터리, GPS, mode, formation slot 상태 |
| `RobotLogEntry` | 적 발견, stream 시작, mesh disconnect 같은 최근 로그 |
| `SwarmStatusPacket` | `leader_status + robot_count + robots[8] + log_count + logs[16]` |

현재 `RobotAggregateStatus` 핵심 필드:

- `robot_id`
- `role`
- `link_status`: `0=Disconnected`, `1=Connecting`, `2=Connected`
- `comm_quality_level`: `0=None`, `1=Poor`, `2=Fair`, `3=Good`, `4=Excellent`
- `battery_pct`
- `active_mode_id`
- `mission_status`
- `estop_active`
- `formation_type`
- `formation_number`
- `slot_index`
- `error_code`
- `status_flags`
- `latitude`
- `longitude`
- `heading`
- `speed_mps`

현재 mode gate 스냅샷:

- `ESTOP`, `RETURN_TO_HOME`, `IDLE`는 항상 통과
- 그 외 기능 모드는 현재 기능이 끝나서 `IDLE`로 돌아온 뒤에만 새 mode로 반영
- hold 시 패킷을 버리지 않고 `command_id`만 현재 상태 대응 값으로 유지
- 따라서 `pan/tilt`, `zoom`, `stream_command`는 그대로 유지된다

기준 구현:

- ROS 노드: `robot_server`
- 상태 머신: `combat_robot_operation_system`
- 현재 소켓 전송 형식: C/C++ `packed struct` 기반 바이너리

주의:

- 현재 구현은 JSON API가 아니라 바이너리 패킷 기반이다.
- 앱은 구조체 정렬 없이 `1-byte packed` 기준으로 직렬화해야 한다.
- 일반적인 x86/RK3588 환경 기준으로 리틀엔디언을 전제로 한다.
- 모드와 세부 진행 상태는 분리한다.
- `Ready`, `Moving`, `Paused`, `Reached` 등은 `Mission Status`로 표현한다.
- 스트리밍은 고정 파이프라인 기반이며, 해상도/비트레이트 동적 변경은 사용하지 않는다.

## 2. 네트워크 채널 정의

| 채널 | 방향 | 프로토콜 | 포트 | 용도 |
| --- | --- | --- | --- | --- |
| Command | App -> Leader Robot | TCP | `65432` | 모드 전환, 짐벌, 스트림 시작/정지 |
| Touch | App -> Leader Robot | UDP | `65433` | 터치 조준점 전달 |
| Driving | App -> Leader Robot | UDP | `65434` | 주행 조작 |
| Status | Leader Robot -> App | TCP | `65435` | leader 및 follower 집계 상태 피드백 |
| Path | App -> Leader Robot | TCP | `65436` | Assault Mission 경로 및 제어 |
| RTSP | Robot -> App | RTSP | `8554` | 영상 스트리밍 |

swarm 정책:

- 앱은 follower robot에 직접 이동 명령을 보내지 않는다.
- leader robot이 앱 명령을 받아 swarm 내부 이동 명령으로 재분배한다.
- follower robot 제어 채널과 follower 상태 수집 채널은 leader robot과 swarm 통신 계층 사이의 내부 인터페이스로 정의한다.

RTSP 주소:

- `rtsp://<robot_ip>:8554/cam0`
- `rtsp://<robot_ip>:8554/cam1`

## 3. 앱 논리 명령 모델

앱 내부 의미 체계는 아래처럼 유지한다.

### 3.1 모드 명령

| 제품 모드 | protectType | wire 기준 |
| --- | --- | --- |
| `idle` | 없음 | `STOP` |
| `recon` | 없음 | `MOVE_MODE` |
| `protect` | `general` | `SURVEILLANCE_MODE` |
| `protect` | `drone` | `DRONE_SURVEILLANCE_MODE` |
| `assault` | 없음 | `ASSAULT_MODE` |
| `return_to_home` | 없음 | `RTH_MODE` |
| `estop` | 없음 | `EMERGENCY_STOP` |

leader 제어 원칙:

- 앱의 `mode`, `drive`, `route`, `mission action`은 leader robot에만 적용한다.
- follower robot의 모드는 leader robot의 relay 명령에 의해 결정된다.

### 3.2 Mission Status

하위 진행 상태는 모드에 포함하지 않고 별도 상태로 관리한다.

| mission_status | 의미 |
| --- | --- |
| `none` | 진행 중인 세부 상태 없음 |
| `ready` | 수행 준비 완료 |
| `moving` | 이동 또는 임무 수행 중 |
| `paused` | 일시정지 |
| `reached` | 목표 도달 또는 완료 |
| `error` | 수행 오류 |

예시:

- `mode = assault`, `mission_status = moving`
- `mode = recon`, `mission_status = moving`
- `mode = protect`, `mission_status = ready`

### 3.3 비노출 모드

| 모드 | 제품 노출 여부 | 비고 |
| --- | --- | --- |
| `assault_tracking` | 숨김 | 개발/데모/시험용 |
| `assault_manual` | 제거 | 최종 제품 미사용 |

## 4. App -> Robot 데이터 정의

## 4.1 Command 채널: `StateCommand`

전송 방식:

- TCP 고정 길이 바이너리
- 패킷 크기: 28 bytes

필드 정의:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `command_id` | `uint8` | 0=Stop, 1=Recon, 2=Protect General, 3=Protect Drone, 4/5=Debug, 6=Assault, 7=Return to Home, 8=E-Stop |
| `e_stop_command` | `uint8` | 긴급 정지 플래그 (0 이외의 값일 경우 최우선 정지) |
| `attack_permission` | `uint8` | 발사 승인 플래그 |
| `pan_speed` | `int8` | 팬 속도 |
| `tilt_speed` | `int8` | 틸트 속도 |
| `zoom_command` | `int8` | 줌 명령 |
| `lateral_wind_speed` | `float32` | 횡풍 추정값 |
| `stream_command` | `uint8` | 0=None, 1=Start, 2=Stop |
| `drone_target_lat` | `float64` | Protect Drone 목표 위도 |
| `drone_target_lon` | `float64` | Protect Drone 목표 경도 |
| `drone_target_valid` | `uint8` | 드론 목표 좌표 유효 여부 |

`stream_command` 정의:

| 값 | 의미 |
| --- | --- |
| `0` | 없음 |
| `1` | 고정 파이프라인 스트림 시작 |
| `2` | 고정 파이프라인 스트림 중지 |

`command_id` 의미:

| 값 | 앱 의미 | 비고 |
| --- | --- | --- |
| `0` | Idle | 정지/대기 |
| `1` | Recon | 최종 제품 노출 |
| `2` | Protect General | 최종 제품 노출 |
| `3` | Protect Drone | 최종 제품 노출 |
| `4` | Debug Reserved | 현재 코드상 추적 계열로 사용 가능 |
| `5` | Assault Tracking | 디버그 전용 |
| `6` | Assault Mission | 최종 제품 노출 |
| `7` | Return to Home | 초기 출발지 복귀 |
| `8` | E-Stop | 최우선 안전 명령 |

앱 논리 예시:

```json
{
  "mode": "assault",
  "gimbal": {
    "panSpeed": 0,
    "tiltSpeed": 0,
    "zoomCommand": 0
  },
  "streamCommand": "start"
}
```

Return to Home 예시:

```json
{
  "mode": "return_to_home",
  "homePositionId": "home-001"
}
```

## 4.2 Driving 채널: `DrivingCommand`

전송 방식:

- UDP 고정 길이 바이너리
- 패킷 크기: 2 bytes

필드 정의:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `move_speed` | `int8` | 전후진 입력값, `-100 ~ 100` |
| `turn_angle` | `int8` | 좌우 회전 입력값, `-100 ~ 100` |

변환 규칙:

- 로봇 서버는 `move_speed / 100 * max_linear_speed`로 변환한다.
- 로봇 서버는 `turn_angle / 100 * max_angular_speed`로 변환한다.

송신 주기:

- 20Hz ~ 30Hz

적용 대상:

- `DrivingCommand`는 leader robot에만 송신한다.
- follower robot은 direct drive를 받지 않는다.

## 4.3 Touch 채널: `TouchCoordinate`

전송 방식:

- UDP 고정 길이 바이너리
- 패킷 크기: 8 bytes

필드 정의:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `x` | `float32` | 터치 X, 정규화 좌표 `0.0 ~ 1.0` |
| `y` | `float32` | 터치 Y, 정규화 좌표 `0.0 ~ 1.0` |

용도:

- 디버그용 Tracking 또는 향후 보조 조준 기능
- 최종 제품 핵심 플로우에서는 필수 UI가 아님

## 4.4 Path 채널: `AssaultCommandHeader + JSON Payload`

전송 방식:

- TCP 헤더 + 가변 길이 JSON 문자열

헤더 필드:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `command` | `uint8` | 0=None, 1=Start, 2=Stop, 3=Pause, 4=Resume, 5=LoadPath |
| `num_waypoints` | `uint16` | waypoint 개수 |
| `data_length` | `uint32` | 뒤따르는 JSON 문자열 길이 |

`command` 정의:

| 값 | 의미 |
| --- | --- |
| `0` | 없음 |
| `1` | 미션 시작 |
| `2` | 미션 중지 |
| `3` | 미션 일시정지 |
| `4` | 미션 재개 |
| `5` | 경로 업로드 |

JSON payload 예시:

```json
{
  "missionId": "mission-001",
  "frame": "wgs84",
  "waypoints": [
    { "seq": 1, "lat": 37.5665, "lon": 126.9780 },
    { "seq": 2, "lat": 37.5667, "lon": 126.9785 }
  ]
}
```

규칙:

- `LoadPath` 시에만 JSON payload를 붙인다.
- `Start`, `Pause`, `Resume`, `Stop`은 payload 없이 보낸다.
- waypoint 수와 `num_waypoints` 값은 일치해야 한다.
- route 및 mission payload는 leader robot 기준 master path이다.

## 4.5 Recon Route 편집 데이터

UI Spec 기준으로 Recon 이동은 route setup 기반으로 수행한다.

앱 논리 모델:

```json
{
  "type": "recon.route",
  "leaderRobotId": "S1",
  "routeId": "recon-route-001",
  "waypoints": [
    { "seq": 1, "lat": 37.5665, "lon": 126.9780 },
    { "seq": 2, "lat": 37.5667, "lon": 126.9785 }
  ],
  "formation": {
    "type": "line|column|wedge|diamond|custom",
    "slotCount": 4
  }
}
```

앱 동작:

- 지도 pin 또는 위도/경도 입력으로 waypoint를 생성한다.
- `add / delete / reorder / reset / finish`를 지원한다.
- `finish` 후 leader robot에 route를 업로드한다.
- `start`는 route 확정 이후에만 가능하다.
- route 시작 시점에 home position을 기록해야 한다.

## 4.6 스트리밍 제어 정책

스트리밍은 고정 파이프라인을 사용한다.

정책:

- 앱은 해상도 변경 명령을 보내지 않는다.
- 앱은 비트레이트 모드 변경 명령을 보내지 않는다.
- 앱은 `stream start` 또는 `stream stop`만 보낸다.
- 파이프라인 생성과 세부 GStreamer 설정은 로봇 내부 고정 설정으로 관리한다.

## 5. Robot -> App 데이터 정의

## 5.1 Status 채널: `StatusPacket`

전송 방식:

- TCP 고정 길이 바이너리
- 송신 주기: 약 10Hz

상위 필드:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `rtsp_server_status` | `uint8` | RTSP 서버 상태, 0=off, 1=on |
| `active_mode_id` | `uint8` | 현재 활성 모드 ID |
| `estop_active` | `uint8` | 긴급 정지 활성 여부 |
| `permission_request_active` | `uint8` | 승인 요청 활성 여부 |
| `crosshair_x` | `float32` | 조준점 X |
| `crosshair_y` | `float32` | 조준점 Y |
| `current_zoom_level` | `float32` | 현재 줌 값 |
| `mission_status` | `uint8` | 세부 진행 상태 |
| `nav_state` | struct | 위치/방향/속도 |
| `mission_state` | struct | waypoint 진행 상태 |
| `assault_status` | struct | Assault Mission 상태 |

상태 전달 원칙:

- 앱은 follower robot으로부터 상태를 직접 수신하지 않는다.
- leader robot이 자신의 상태와 follower robot 상태를 집계해 Status 채널로 앱에 전달한다.
- 앱의 swarm 화면, Device Check, formation 상태, follower 상태 카드는 모두 leader robot이 전달한 집계 상태를 기준으로 표시한다.

`mission_status` 정의:

| 값 | 의미 |
| --- | --- |
| `0` | None |
| `1` | Ready |
| `2` | Moving |
| `3` | Paused |
| `4` | Reached |
| `5` | Error |

### 5.1.1 `NavigationStatePacket`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `latitude` | `float64` | 현재 위도 |
| `longitude` | `float64` | 현재 경도 |
| `heading` | `float32` | 현재 방위 |
| `current_speed_mps` | `float32` | 현재 속도 |

### 5.1.2 `MissionStatePacket`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `current_waypoint_index` | `uint16` | 현재 waypoint 인덱스 |
| `total_waypoints` | `uint16` | 전체 waypoint 개수 |
| `progress_ratio` | `float32` | 전체 진행률 `0.0 ~ 1.0` |
| `distance_to_next_wp_m` | `float32` | 다음 waypoint까지 거리 |
| `distance_to_goal_m` | `float32` | 최종 목표까지 거리 |

### 5.1.3 `AssaultStatusPacket`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `assault_state` | `uint8` | Assault 전용 세부 상태 또는 내부 확장 상태 |
| `error_code` | `uint8` | 오류 코드 |

## 5.2 Leader -> App 집계 follower 상태: `FollowerRobotStatus`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `robot_id` | `char[16]` | follower robot ID |
| `link_status` | `uint8` | 0=Disconnected, 1=Connected |
| `active_mode_id` | `uint8` | follower 현재 모드 |
| `mission_status` | `uint8` | follower Mission Status |
| `battery_pct` | `uint8` | 배터리 잔량 퍼센트 |
| `latitude` | `float64` | 현재 위도 |
| `longitude` | `float64` | 현재 경도 |
| `heading` | `float32` | 현재 heading |
| `speed_mps` | `float32` | 현재 속도 |
| `slot_index` | `uint8` | formation slot index |
| `error_code` | `uint8` | follower 오류 코드 |
| `status_flags` | `uint16` | follower 상태 비트 플래그 |

`status_flags` 비트 정의:

| 비트 | 의미 |
| --- | --- |
| bit0 | formation 유지 중 |
| bit1 | path 추종 중 |
| bit2 | hold 상태 |
| bit3 | obstacle detected |
| bit4 | low battery |
| bit5 | gps degraded |
| bit6 | estop active |
| bit7 | reserved |

## 5.3 Leader -> App 집계 상태 패킷: `SwarmStatusPacket`

`SwarmStatusPacket`은 leader 기본 상태와 follower 상태 목록을 하나의 상태 프레임으로 묶는다.

상위 필드:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `leader_status` | `StatusPacket` | leader robot 상태 |
| `follower_count` | `uint8` | 포함된 follower 수 |
| `followers` | `FollowerRobotStatus[]` | follower 상태 목록 |

운용 규칙:

- `StatusPacket`은 leader robot의 기준 상태로 사용한다.
- `followers[]`는 현재 leader가 관리 중인 follower만 포함한다.
- follower link가 끊긴 경우에도 마지막 수신 상태와 `link_status=0`을 함께 전달한다.
- 앱은 `follower_count`와 `followers[]`를 기준으로 지도, Device Check, formation 패널을 갱신한다.

## 5.4 앱 표시용 상태 매핑

| `active_mode_id` | 앱 표시명 |
| --- | --- |
| `0` | Idle |
| `1` | Recon |
| `2` | Protect General |
| `3` | Protect Drone |
| `4` | Assault |
| `5` | Return to Home |
| `6` | E-Stop |

현재 태블릿 테스트 서버 기준으로 `active_mode_id`는 연속값 `0..6`을 사용한다.

디버그 전용:

| `active_mode_id` | 앱 표시명 |
| --- | --- |
| 없음 | 현재 태블릿 테스트 서버 기준 별도 디버그 active mode 미사용 |

## 5.5 Follower -> Leader 내부 상태 보고: `FollowerStateReport`

이 절은 leader robot이 follower 상태를 집계하기 위한 swarm 내부 인터페이스 정의다.

전송 주체와 수신 대상:

- 송신: follower robot
- 수신: leader robot
- 전송 계층: ROS 2 DDS 또는 mesh communication layer

토픽:

- `/swarm/follower_state_report`
- 또는 `/<robot_ns>/swarm/state_report`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `header` | `std_msgs/Header` | 공통 헤더 |
| `robot_id` | `string` | follower robot ID |
| `active_mode_id` | `uint8` | 현재 모드 |
| `mission_status` | `uint8` | 현재 Mission Status |
| `battery_pct` | `uint8` | 배터리 잔량 |
| `latitude` | `float64` | 현재 위도 |
| `longitude` | `float64` | 현재 경도 |
| `heading` | `float32` | 현재 heading |
| `speed_mps` | `float32` | 현재 속도 |
| `slot_index` | `uint8` | formation slot index |
| `error_code` | `uint8` | 오류 코드 |
| `status_flags` | `uint16` | 상태 비트 플래그 |

집계 규칙:

- leader robot은 각 follower의 최신 `FollowerStateReport`를 캐시한다.
- 앱으로 나가는 상태 프레임에는 캐시된 follower 상태를 `FollowerRobotStatus`로 변환해 포함한다.
- timeout이 발생한 follower는 `link_status=0`으로 표시한다.

## 6. Leader -> Follower 이동 명령 데이터 정의

이 절은 swarm SDD의 multi-robot 구조와 UI Spec의 leader 기반 운용 방식을 반영한 제안 규격이다.

전송 주체와 수신 대상:

- 송신: leader robot
- 수신: follower robot
- 전송 계층: ROS 2 DDS 또는 mesh communication layer

토픽:

- `/swarm/formation_move_command`
- 또는 `/<robot_ns>/swarm/move_command`

메시지명:

- `SwarmMoveCommand`

### 6.1 `SwarmMoveCommand` 필드 정의

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `header` | `std_msgs/Header` | 공통 헤더 |
| `leader_robot_id` | `string` | leader robot ID |
| `target_robot_id` | `string` | follower robot ID |
| `seq` | `uint32` | 명령 시퀀스 번호 |
| `mode` | `uint8` | 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 4=Assault Mission, 5=Return to Home, 6=E-Stop |
| `movement_type` | `uint8` | 0=Hold, 1=FollowLeader, 2=MoveToSlot, 3=FollowRoute, 4=MoveToWaypoint, 5=ReturnHome |
| `formation_type` | `uint8` | 0=None, 1=Line, 2=Column, 3=Wedge, 4=Diamond, 5=Custom |
| `slot_index` | `uint8` | formation 내 follower 위치 index |
| `offset_x_m` | `float32` | leader 기준 종방향 offset |
| `offset_y_m` | `float32` | leader 기준 횡방향 offset |
| `offset_yaw_deg` | `float32` | leader 기준 자세 offset |
| `reference_lat` | `float64` | leader 기준 위도 |
| `reference_lon` | `float64` | leader 기준 경도 |
| `reference_heading_deg` | `float32` | leader 기준 heading |
| `target_lat` | `float64` | follower 목표 위도 |
| `target_lon` | `float64` | follower 목표 경도 |
| `target_speed_mps` | `float32` | follower 목표 속도 |
| `waypoint_index` | `uint16` | 현재 waypoint 인덱스 |
| `total_waypoints` | `uint16` | 전체 waypoint 수 |
| `hold_time_ms` | `uint32` | waypoint 도달 후 대기 시간 |
| `command_flags` | `uint16` | 비트 플래그 |

`command_flags` 비트 정의:

| 비트 | 의미 |
| --- | --- |
| bit0 | formation 유지 |
| bit1 | obstacle avoidance 허용 |
| bit2 | route 재계산 허용 |
| bit3 | hold position |
| bit4 | synchronized start |
| bit5 | emergency stop |

### 6.2 `SwarmMoveCommand` 운용 규칙

- 앱은 follower robot에 직접 `DrivingCommand`를 보내지 않는다.
- leader robot은 앱의 route 또는 mode 명령을 follower별 개별 명령으로 분해한다.
- `Recon`에서는 `FollowLeader` 또는 `MoveToSlot`이 기본값이다.
- `Assault Mission`에서는 `FollowRoute` 또는 `MoveToWaypoint`를 사용한다.
- `Return to Home`에서는 `ReturnHome` movement_type을 사용한다.
- `E-Stop` 시 leader는 모든 follower에 `command_flags.bit5=1` 명령을 즉시 전파한다.

### 6.3 `SwarmMoveCommand` 예시

```json
{
  "leader_robot_id": "S1",
  "target_robot_id": "S3",
  "seq": 1024,
  "mode": 1,
  "movement_type": 2,
  "formation_type": 3,
  "slot_index": 2,
  "offset_x_m": -2.5,
  "offset_y_m": 1.2,
  "offset_yaw_deg": 0.0,
  "reference_lat": 37.5665,
  "reference_lon": 126.9780,
  "reference_heading_deg": 90.0,
  "target_lat": 37.56647,
  "target_lon": 126.97811,
  "target_speed_mps": 0.8,
  "waypoint_index": 1,
  "total_waypoints": 4,
  "hold_time_ms": 0,
  "command_flags": 19
}
```

## 7. UI 반영 규칙

### 7.1 조준점 표시

- `Protect`에서 조준점이 유효하면 영상 위에 오버레이한다.
- 값이 `-1.0`이면 화면에 표시하지 않는다.

### 7.2 승인 요청 표시

- `permission_request_active = 1`이면 운용자 승인 필요 배지를 표시한다.
- 최종 제품에서 Assault Mission 화면은 승인 UI보다 미션 상태 UI를 우선한다.

### 7.3 미션 상태 표시

| `mission_status` | 앱 배지 |
| --- | --- |
| `0` | None |
| `1` | Ready |
| `2` | Moving |
| `3` | Paused |
| `4` | Reached |
| `5` | Error |

### 7.4 follower 상태 표시

- 앱은 leader robot이 전달한 `followers[]` 기준으로 follower 카드와 지도 마커를 갱신한다.
- follower별로 `robot_id`, `link_status`, `active_mode_id`, `mission_status`, `battery_pct`, `slot_index`, `error_code`를 표시한다.
- follower와 leader 간 상태가 불일치하면 follower 카드에 `Sync Lost` 또는 오류 배지를 표시한다.

## 8. 앱 내부 DTO

```json
{
  "status": {
    "rtspServerUp": true,
    "activeMode": "assault",
    "protectType": null,
    "permissionRequestActive": false,
    "missionStatus": "moving",
    "crosshair": {
      "x": -1.0,
      "y": -1.0,
      "visible": false
    },
    "zoomLevel": 1,
    "nav": {
      "lat": 37.5665,
      "lon": 126.9780,
      "heading": 0.0,
      "speedMps": 0.0
    },
    "mission": {
      "currentWaypointIndex": 0,
      "totalWaypoints": 0,
      "progressRatio": 0.0,
      "distanceToNextWpM": 0.0,
      "distanceToGoalM": 0.0,
      "state": "ready",
      "errorCode": 0
    },
    "followers": [
      {
        "robotId": "S2",
        "linkStatus": "connected",
        "activeMode": "recon",
        "missionStatus": "moving",
        "batteryPct": 82,
        "slotIndex": 1,
        "errorCode": 0,
        "nav": {
          "lat": 37.56651,
          "lon": 126.97803,
          "heading": 2.5,
          "speedMps": 0.7
        }
      }
    ]
  }
}
```

## 9. 제품 기준 인터페이스 정책

- 제품 UI는 `Assault Manual` 송신 기능을 제공하지 않는다.
- 제품 UI는 `Assault Tracking` 송신 기능을 기본 제공하지 않는다.
- Assault 화면은 반드시 Path 업로드 후 `Start` 가능 상태가 되어야 한다.
- `Return to Home`는 `Recon` 또는 `Assault` 종료 후 사용할 수 있어야 한다.
- `Return to Home` 시작 전에는 home position이 존재해야 한다.
- `E-Stop`은 별도 최우선 액션으로 유지한다.
- 앱의 이동/경로/미션 명령은 leader robot에만 전달한다.
- follower robot 이동은 leader relay 명령으로만 수행한다.
- follower robot 상태는 leader robot이 집계한 상태만 앱에 전달한다.
- 모드는 `activeMode`로, 세부 진행 상태는 `missionStatus`로 분리하여 표시한다.
- 스트리밍 설정 UI는 두지 않고 `start/stop` 제어만 제공한다.

## 10. 구현 주의사항

- 현재 코드상 일부 상태값과 제품 용어는 완전히 일치하지 않을 수 있다.
- 앱은 제품 용어를 기준으로 표시하고, 서버와는 별도 매핑 테이블로 연결하는 것이 안전하다.
- 향후 인터페이스 정식 버전에서는 바이너리 구조체 대신 버전 필드 포함 포맷으로 확장한다.
- `SwarmMoveCommand`는 현재 저장소에 구현된 실코드가 아니라 제품 설계를 위한 제안 데이터 정의다.
- `mission_status`와 `stream_command`는 제품 문서 기준 확장 정의이며, 현재 코드와 불일치할 수 있다.





