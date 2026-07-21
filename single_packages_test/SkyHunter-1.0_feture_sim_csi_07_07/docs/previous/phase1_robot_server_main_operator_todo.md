# 1차 개발 TODO

## 범위

이번 1차 범위는 아래까지만 포함한다.

- 앱 <-> `robot_server` 연동
- Main 오퍼레이터 상태를 앱에 맞게 노출하는 부분
- RTSP 제어 정책 정리
- mission/leader/follower 관련 필드는 placeholder 허용

이번 1차 범위에서 제외한다.

- waypoint 실제 주행
- leader/follower relay
- follower 상태 집계
- home position 저장과 Return to Home 실제 수행
- mission pause/resume 실제 제어

## 구현 원칙

- 패킷 구조는 최종 문서 기준으로 먼저 맞춘다.
- 미구현 기능은 fake 동작 대신 placeholder 값만 보낸다.
- placeholder는 앱이 오인하지 않도록 중립값을 쓴다.
- movement 미구현 상태에서는 `mission_status = NONE`을 기본값으로 둔다.
- crosshair가 유효하지 않으면 `-1.0`을 사용한다.
- 실제 값이 없는데 임의 좌표를 넣는 방식은 금지한다.

## 파일별 TODO

### `ros/src/skyautonet/combat_robot_system/combat_robot_msgs/msg/UserCommand.msg`

- 앱 기준 command id 체계로 정리
- 제품 기준에서 유지할 명령만 남기고 debug 전용 명령은 별도 정리
- `RTH_MODE` id를 예약하거나 추가하되 1차에서는 미구현으로 취급
- `stream start/stop`을 받을 수 있도록 필드 재정의
- 해상도/비트레이트 동적 변경 필드는 앱 프로토콜에서 제거하거나 reserved 처리

### `ros/src/skyautonet/combat_robot_system/combat_robot_msgs/msg/OperationState.msg`

- 내부 FSM 값 나열 방식에서 앱 노출용 상태 메시지로 재정의
- 최소 포함 필드:
  - `active_mode_id`
  - `mission_status`
  - `permission_request_active`
  - `crosshair_x`
  - `crosshair_y`
  - `current_zoom_level`
  - `gps_lat`
  - `gps_lon`
  - `gps_heading`
  - `current_speed_mps`
- `EMERGENCY_STOP`, `ERROR`, `ASSAULT`, `RETURN_TO_HOME`가 앱에서 구분 가능해야 함
- movement 미구현 필드는 placeholder 허용

### `ros/src/skyautonet/combat_robot_system/robot_server/include/command_server.hpp`

- `StateCommand`를 문서 기준 wire layout으로 정리
- `stream_command` 필드를 추가
- `StatusPacket`에 `mission_status`를 별도 필드로 추가
- mission/nav placeholder를 위한 기본값 정의 추가
- 임시 해상도/비트레이트 제어 중심 구조 제거 또는 reserved 처리

### `ros/src/skyautonet/combat_robot_system/robot_server/src/command_server.cpp`

- `StateCommand` -> `UserCommand` 매핑을 제품 기준으로 수정
- `active_mode_id`를 마지막 앱 명령 echo가 아니라 실제 오퍼레이터 상태 기준으로 갱신
- `permission_request_active`를 임시 heuristic이 아니라 실제 상태로 반영
- `crosshair` invalid 시 `-1.0` 전달
- GPS/heading/speed는 실제 값 없으면 명시적인 placeholder 정책 적용
- Path 채널 동작은 1차에서 아래까지만 유지
  - payload 수신
  - 저장
  - waypoint 개수 보관
  - 실행은 하지 않음
- Start/Pause/Resume/Stop은 내부 실행이 없으므로 앱이 오인하지 않게 no-op 또는 미지원 처리 기준 명확화
- 로그 메시지에 `temp`, `temporary logic` 같은 임시 문구 남아 있는 부분 정리

### `ros/src/skyautonet/combat_robot_system/robot_server/src/rtsp_server.cpp`

- fixed pipeline 정책으로 정리
- 앱에서 보내는 제어는 `stream start/stop`만 받도록 수정
- 해상도/비트레이트 동적 변경 로직 제거 또는 무시
- 상태 채널에 보낼 `rtsp_server_status` 기준을 명확히 정리

### `ros/src/skyautonet/combat_robot_system/combat_robot_operation_system/include/combat_robot_operation_system.hpp`

- Main 오퍼레이터가 앱에 노출할 상태 enum/placeholder 정책 정리
- 내부 debug 상태와 앱 공개 상태를 분리
- 1차에서 mission executor는 없지만 앱 공개용 `mission_status`는 유지

### `ros/src/skyautonet/combat_robot_system/combat_robot_operation_system/src/combat_robot_operation_system.cpp`

- 앱 공개용 상태 publish 로직을 새 `OperationState.msg` 기준으로 수정
- `crosshair_x`, `crosshair_y`를 고정값 `0.5`로 보내지 않도록 수정
- 실제로 보여줄 수 없는 상태는 invalid placeholder로 전송
- `EMERGENCY_STOP`와 `ERROR`를 앱이 정확히 구분할 수 있게 publish
- attack/tracking 관련 상태는 1차에서 debug 경로로만 남기거나 앱 공개 상태에서 숨김
- mission 미구현 상태에서는 `mission_status = NONE`

### `ros/src/skyautonet/combat_robot_system/combat_robot_operation_system/test/test_fsm_transitions.cpp`

- 제품 기준 공개 상태 기준으로 테스트 재정리
- 유지할 테스트:
  - `INIT -> IDLE`
  - `IDLE -> RECON`
  - `IDLE -> PROTECT_GENERAL`
  - `IDLE -> PROTECT_DRONE`
  - `EMERGENCY_STOP`
- 재분류할 테스트:
  - `ATTACK_MODE`
  - `TRACKING_MODE`
- 앱 공개 경로와 debug 경로를 테스트에서 분리

## Placeholder 정책

### mode / status

- `active_mode_id`
  - 실제 진입한 오퍼레이터 상태를 기준으로 전송
  - 미구현 모드는 앱에 노출하지 않거나 `IDLE` 유지

- `mission_status`
  - 기본값 `NONE`
  - 1차에서는 mission executor가 없으므로 `MOVING/PAUSED/REACHED`를 만들지 않음

### crosshair / permission

- `crosshair_x = -1.0`
- `crosshair_y = -1.0`
- `permission_request_active = 0`

실제 보호/탐지 상태에서만 값이 있을 때 갱신한다.

### nav / motion

- 실제 위치 공급자가 연결되기 전까지 앱이 숨길 수 있는 placeholder 정책 필요
- 임의의 서울 좌표, 고정 heading 같은 fake 값은 제거
- 앱과 합의가 가능하면 `(0, 0)` 또는 별도 invalid 규칙 사용

### mission path

- `current_waypoint_index = 0`
- `total_waypoints = 0` 또는 load 시 받은 개수만 유지
- `progress_ratio = 0.0`
- `distance_to_next_wp_m = 0.0`
- `distance_to_goal_m = 0.0`
- `error_code = 0`

## 1차 완료 조건

- 앱이 `robot_server`와 연결되어 제품 기준 command/status packet으로 통신 가능
- Main 화면에서 mode, RTSP 상태, crosshair, zoom, estop/error 구분이 일관되게 표시됨
- mission/leader/follower 미구현 기능은 앱에서 오인되지 않는 placeholder로만 노출됨
- 이후 2차 개발에서 movement/swarm를 붙여도 packet 구조를 다시 깨지 않아도 됨
