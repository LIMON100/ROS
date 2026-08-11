# 앱 연동 통합 문서

## 1. 문서 목적

이 문서는 전투로봇 앱 연동에 필요한 운용 개념, 모드 정의, 네트워크 인터페이스, 상태전이, leader-follower 제어 구조를 하나의 문서로 통합한 사양서다.

## 1.1 현재 구현 스냅샷

오늘 기준 태블릿 테스트 서버 구현은 `combatrobotcontroller/src/robot_server_rknn.cpp`와 `combatrobotcontroller/src/robot_server_rpi.cpp`를 따른다.

- 태블릿 앱은 leader robot에만 명령을 보낸다.
- follower robot 직접 제어는 하지 않는다.
- leader robot은 `S1..S8` 전체 상태를 집계해 앱에 보낸다.
- leader robot은 별도 9번째 항목이 아니라 `S1..S8` 안에 포함되며, 기본 dummy telemetry에서는 `S1`이 leader다.
- robot ID는 문자열이 아니라 `uint32`이며, `1..8`은 `S1..S8`에 대응한다.
- 앱 스트리밍 요청은 `stream_target_robot_id`로 leader에 전달되고, leader는 현재 송출 중인 소스를 `active_stream_robot_id`로 상태에 넣는다.
- 집계 상태 패킷은 `SwarmStatusPacket`이며, `robots[8]`과 `logs[16]`을 포함한다.
- `leader_status`는 leader 요약 블록이고, `robots[8]` 안에도 같은 leader(`S1`)가 포함된다.
- `RobotAggregateStatus`에는 연결 상태, 와이파이 품질 단계, 배터리, GPS, mode, mission, formation slot, error, speed가 포함된다.
- `StatusPacket`에는 raw echo용 `last_tablet_command_id`가 포함되어, state machine feedback이 없어도 태블릿이 마지막에 보낸 `command_id`를 그대로 확인할 수 있다.
- formation 체계는 `formation_type + formation_number`로 단순화하며, `formation_type`은 `None/Recon/Protect/Assault`, `formation_number`는 각 타입 내부 `1..4 preset`을 뜻한다.
- 태블릿 테스트용 dummy telemetry가 기본 로드되며, disconnected robot과 low-battery / estop / target-detected 로그도 포함한다.
- `robot_server` 선차단 정책은 `ESTOP`, `RETURN_TO_HOME`, `IDLE`만 항상 통과시키고, 다른 기능 모드는 현재 기능 종료 후 `IDLE` 복귀 이후에만 반영한다.
- 이 hold는 패킷 drop이 아니라 `command_id` 유지 방식이므로 `pan/tilt`, `zoom`, `stream` 명령은 함께 유지된다.

적용 원칙:

- 앱의 이동 명령은 `leader robot`에만 전달한다.
- follower robot은 leader robot이 relay한 이동 명령만 수행한다.
- follower robot의 상태는 leader robot이 집계해 앱으로 전달한다.
- `Move`는 앱에서 `Recon`으로, `Surveillance`는 `Protect`로 표기한다.
- 공격 기능은 제품 UI에서 `Assault`로 통일하고 최종 제품에서는 `Assault Mission`만 제공한다.
- `Assault Tracking`은 디버그 전용이며 `Assault Manual`은 제거한다.
- `Ready`, `Moving`, `Paused`, `Reached`, `Error`는 모드가 아니라 별도 `Mission Status`다.
- 스트리밍은 고정 파이프라인 기반이며 앱은 `Stream Start`와 `Stream Stop`만 제어한다.
- `Return to Home`는 `Recon` 또는 `Assault` 종료 후 초기 출발지로 복귀하는 모드다.

## 2. 제품 모드 체계

| 앱 표시명 | 설명 | 내부 구현 기준 |
| --- | --- | --- |
| Idle | 대기 상태 | `STOP` |
| Recon | 이동 및 정찰 | `MOVE_MODE` |
| Protect General | 일반 감시 | `SURVEILLANCE_MODE` |
| Protect Drone | 드론 감시 | `DRONE_SURVEILLANCE_MODE` |
| Assault | 경로 기반 임무 수행 | `ASSAULT_MODE` |
| Return to Home | 초기 출발지 복귀 | `RTH_MODE` |
| E-Stop | 긴급 정지 | `EMERGENCY_STOP` |

디버그 전용:

| 기능 | 정책 |
| --- | --- |
| Assault Tracking | 숨김 또는 데모 빌드 전용 |
| Assault Manual | 제거 |

## 3. Mission Status 체계

| mission_status | 설명 |
| --- | --- |
| `NONE` | 세부 수행 상태 없음 |
| `READY` | 수행 준비 완료 |
| `MOVING` | 이동 또는 임무 수행 중 |
| `PAUSED` | 일시정지 |
| `REACHED` | 목표 도달 또는 완료 |
| `ERROR` | 수행 오류 |

적용 예:

- `activeMode = Recon`, `missionStatus = Moving`
- `activeMode = Assault`, `missionStatus = Ready`
- `activeMode = Return to Home`, `missionStatus = Reached`

## 4. 시스템 운용 구조

### 4.1 leader-follower 구조

- 앱은 단일 leader robot만 직접 제어한다.
- leader robot은 route, formation, mission 정보를 기반으로 follower robot별 이동 명령을 생성한다.
- leader robot은 follower robot의 상태도 수집해 앱으로 전달한다.
- follower robot은 direct drive 입력을 받지 않는다.
- swarm 화면은 leader 기준 route, link overlay, formation 상태를 보여준다.

### 4.2 Device Check 운용

1. 앱은 S1~S8 장치를 지도 위 마커로 표시한다.
2. leader robot은 별(`★`) 또는 leader 배지로 표시한다.
3. 운용자는 leader robot을 선택한다.
4. 선택된 leader robot이 Main Screen의 제어 기준이 된다.
5. 이후 이동/route/mission/복귀 명령은 leader robot에만 전달된다.
6. follower robot 상태는 leader robot이 집계한 정보를 기준으로 표시한다.

### 4.3 스트리밍 운용

- 영상은 고정 파이프라인으로 송출한다.
- 앱은 품질 변경, 비트레이트 변경, 해상도 변경을 수행하지 않는다.
- 앱은 `Stream Start`, `Stream Stop`만 제어한다.
- RTSP 주소는 고정 파이프라인에 연결된 endpoint를 사용한다.

## 5. 화면 및 운용 흐름

### 5.1 앱 시작

1. 앱은 로봇 IP 또는 연결 프로파일을 선택한다.
2. 상태 TCP 채널에 연결한다.
3. leader robot 상태를 확인한다.
4. 필요 시 `Stream Start`를 수행한다.
5. RTSP 연결과 지도 상태를 확인한다.

초기 화면 정보:

- `activeMode`
- `missionStatus`
- RTSP 상태
- leader/follower 연결 상태
- GPS 및 배터리 상태
- leader가 집계한 follower별 모드, Mission Status, 배터리, 오류 상태

### 5.2 홈 화면

기본 노출 기능:

- Recon
- Protect
- Assault
- Return to Home
- E-Stop

추가 정책:

- `Return to Home`는 임무 후속 복귀 기능으로 제공한다.
- `Assault Tracking`은 기본 메뉴에 노출하지 않는다.

## 6. 운용 시나리오

### 6.1 운용 개요

기본 운용 흐름은 아래와 같다.

1. 장비 및 네트워크 점검
2. Device Check에서 leader robot 선택
3. 영상 및 상태 연결 확인
4. 임무 유형 선택
5. `Recon`, `Protect`, `Assault` 중 하나 수행
6. mission 종료 후 필요 시 `Return to Home` 수행
7. 필요 시 `Pause`, `Resume`, `Stop`, `E-Stop` 수행
8. 임무 종료 후 상태 확인 및 후속 임무 전환

### 6.2 사전 점검 시나리오

#### 6.2.1 전원 및 장치 확인

운용자는 앱 실행 전 아래를 확인한다.

- leader robot 전원 상태
- follower robot 전원 상태
- 카메라 및 RTSP 스트리밍 가능 여부
- 통신 링크 상태
- GPS 위치 수신 여부
- 배터리 상태

#### 6.2.2 앱 접속

1. 앱을 실행한다.
2. 로봇 IP 또는 연결 프로파일을 선택한다.
3. 상태 채널 연결 여부를 확인한다.
4. 스트리밍이 필요하면 `Stream Start`를 수행한다.
5. 영상이 정상 수신되는지 확인한다.

성공 조건:

- 상태 수신 정상
- leader robot 표시 정상
- 지도 위치 표시 정상
- RTSP 영상 수신 정상 또는 복구 가능 상태

### 6.3 Device Check 시나리오

#### 6.3.1 다수 로봇 확인

1. 운용자는 Device Check 화면에서 S1~S8 로봇을 확인한다.
2. 앱은 각 로봇의 위치, 배터리, 연결 상태, 현재 모드를 표시한다.
3. leader robot은 별(`★`) 또는 leader 배지로 구분한다.
4. 앱은 formation 구성과 link overlay를 표시한다.
5. follower robot 상태는 leader robot이 집계한 상태를 기준으로 표시한다.

#### 6.3.2 leader robot 선택

1. 운용자는 제어 기준이 되는 leader robot을 선택한다.
2. 선택된 leader robot이 Main Screen의 기준 장치가 된다.
3. 이후 이동/모드/임무 명령은 선택된 leader robot에만 전달된다.

운용 규칙:

- follower robot은 직접 선택해 주행시키지 않는다.
- follower robot 화면은 상태 확인 중심으로 사용한다.

### 6.4 Recon 운용 시나리오

#### 6.4.1 목적

- 지정 경로를 따라 이동하며 주변을 정찰한다.

#### 6.4.2 기본 절차

1. 운용자는 leader robot을 선택한다.
2. 메인 화면에서 `Recon`을 선택한다.
3. 지도 위 목표 지점을 선택하거나 위도/경도를 직접 입력한다.
4. waypoint를 추가한다.
5. 필요 시 waypoint를 삭제하거나 순서를 재정렬한다.
6. `Finish`로 route를 확정한다.
7. 앱은 route를 leader robot에 업로드한다.
8. `Mission Status = Ready`를 확인한다.
9. 운용자는 `Start`를 눌러 이동을 시작한다.
10. 앱은 `Mission Status = Moving`과 진행 경로를 표시한다.
11. follower robot은 formation slot에 따라 leader를 추종한다.
12. 운용자는 필요 시 짐벌, 줌, 스트리밍만 추가 조작한다.

#### 6.4.3 운용 중 확인 항목

- 현재 waypoint / 전체 waypoint
- 진행 방향
- 지도상의 경로 이탈 여부
- follower formation 유지 여부
- follower 개별 mode, Mission Status, 배터리, 오류 여부
- 영상 품질 및 스트리밍 상태
- 장애물 또는 우회 필요 여부

#### 6.4.4 Recon 중 예외 처리

- 일시 정지 필요 시 `Pause`
- 재시작 시 `Resume`
- 경로 변경 필요 시 `Stop` 후 route 수정
- mission 종료 후 필요 시 `Return to Home`
- 즉시 안전 정지 필요 시 `E-Stop`

### 6.5 Protect 운용 시나리오

#### 6.5.1 목적

- 지정 구역 또는 방향에 대해 경계 및 감시를 수행한다.

#### 6.5.2 기본 절차

1. 운용자는 leader robot을 선택한다.
2. `Protect`를 선택한다.
3. 감시 유형을 `General` 또는 `Drone`으로 선택한다.
4. 앱은 해당 모드 명령을 leader robot에 전송한다.
5. leader robot은 감시 기준 위치 또는 sector를 설정한다.
6. follower robot은 할당된 위치 또는 sector를 유지한다.
7. 앱은 탐지 이벤트, 조준점, 승인 요청 상태를 표시한다.
8. 앱은 leader robot이 집계한 follower Protect 상태를 함께 표시한다.

#### 6.5.3 운용 중 확인 항목

- 탐지 이벤트 발생 여부
- 조준점 표시 여부
- 승인 요청 상태
- leader/follower 위치 유지 여부
- follower 개별 모드, Mission Status, 배터리, 오류 여부
- 영상 가시성

#### 6.5.4 Protect 중 예외 처리

- 탐지 오경보 시 상태만 기록하고 감시 지속
- 감시 위치 재조정 필요 시 leader 기준 재배치
- 심각 상황 시 `E-Stop`

### 6.6 Assault 운용 시나리오

#### 6.6.1 목적

- leader robot을 중심으로 waypoint 기반 임무를 수행한다.

#### 6.6.2 기본 절차

1. 운용자는 leader robot을 선택한다.
2. `Assault`를 선택한다.
3. 임무 경로를 생성하거나 기존 경로를 불러온다.
4. waypoint를 검토하고 필요 시 수정한다.
5. 앱은 mission path를 leader robot에 업로드한다.
6. `Mission Status = Ready`를 확인한다.
7. 운용자는 `Start`를 수행한다.
8. leader robot은 master mission route를 따라 이동한다.
9. follower robot은 leader가 전달한 route/slot 명령에 따라 분산 이동한다.
10. 앱은 진행률, 현재 waypoint, 잔여 거리, mission status를 표시한다.
11. 필요 시 `Pause`, `Resume`, `Stop`을 수행한다.
12. mission 완료 시 `Mission Status = Reached`를 확인한다.
13. 앱은 leader robot이 집계한 follower 이동 상태와 오류 상태를 함께 확인한다.

#### 6.6.3 운용 중 확인 항목

- mission status
- waypoint 진행률
- 목표까지 잔여 거리
- follower 재배치 상태
- follower 개별 mode, Mission Status, 배터리, 오류 여부
- link 손실 여부
- event log

#### 6.6.4 Assault 중 예외 처리

- 위치 오차가 크면 `Pause` 후 상태 재점검
- follower 일부 이탈 시 leader 중심 재동기화
- mission 오류 시 `Mission Status = Error` 확인 후 `Stop`
- mission 완료 후 필요 시 `Return to Home`
- 즉시 정지 필요 시 `E-Stop`

### 6.7 Return to Home 운용 시나리오

#### 6.7.1 목적

- Recon 또는 Assault 종료 후 초기 출발지로 안전하게 복귀한다.

#### 6.7.2 사용 조건

- 출발 시점의 home position이 저장되어 있어야 한다.
- 일반적으로 `Recon` 또는 `Assault` mission 수행 이력이 있어야 한다.

#### 6.7.3 기본 절차

1. 운용자는 mission 완료 상태를 확인한다.
2. 앱에서 `Return to Home`를 선택한다.
3. 앱은 home position 존재 여부를 확인한다.
4. 앱은 복귀 명령을 leader robot에 전송한다.
5. leader robot은 저장된 출발지 좌표로 이동을 시작한다.
6. follower robot은 복귀 formation을 유지하며 leader를 추종한다.
7. 앱은 `activeMode = Return to Home`과 `Mission Status`를 표시한다.
8. 복귀 완료 시 `Mission Status = Reached`를 확인한다.
9. 운용자는 `Idle`로 복귀한다.
10. 앱은 leader robot이 집계한 follower 복귀 상태를 함께 표시한다.

#### 6.7.4 운용 중 확인 항목

- home position 유효 여부
- 복귀 경로 이탈 여부
- leader/follower formation 유지 여부
- follower 개별 mode, Mission Status, 배터리, 오류 여부
- 스트리밍 및 상태 채널 정상 여부

#### 6.7.5 예외 처리

- home position이 없으면 복귀 시작 금지
- 복귀 중 경로 이상 시 `Pause` 또는 `Stop`
- 긴급 상황 시 `E-Stop`

### 6.8 스트리밍 운용 시나리오

#### 6.8.1 기본 원칙

- 영상은 고정 파이프라인으로 송출된다.
- 앱은 품질 변경을 하지 않는다.
- 앱은 `Stream Start`, `Stream Stop`만 수행한다.

#### 6.8.2 기본 절차

1. 운용자가 메인 화면 또는 통합 화면에서 `Stream Start`를 선택한다.
2. 로봇은 고정 파이프라인을 시작한다.
3. 앱은 RTSP 연결을 시도한다.
4. 영상 수신에 성공하면 화면에 live view를 표시한다.
5. 필요 시 `Stream Stop`으로 송출을 중지한다.

#### 6.8.3 예외 처리

- 영상이 끊기면 `No Signal` 표시
- 상태 채널이 살아 있으면 임무 표시는 유지
- 필요 시 `Stream Stop -> Stream Start` 순으로 재시도

### 6.9 긴급 정지 시나리오

#### 6.9.1 발생 조건

- 충돌 위험
- 통신 이상
- 경로 이탈 심화
- 비정상 동작
- 운용자 수동 정지 판단

#### 6.9.2 절차

1. 운용자는 어느 화면에서든 `E-Stop`을 누른다.
2. 앱은 leader robot에 즉시 긴급 정지 명령을 보낸다.
3. leader robot은 follower robot에 정지 명령을 relay한다.
4. 앱은 모든 일반 제어를 잠근다.
5. 운용자는 현장 상태를 확인한다.
6. 복귀가 가능하면 `Stop/Reset` 후 `Idle`로 전환한다.

### 6.10 임무 종료 시나리오

#### 6.10.1 정상 종료

1. mission이 완료되어 `Mission Status = Reached`가 된다.
2. 앱은 완료 배지를 표시한다.
3. 운용자는 결과를 확인한다.
4. 필요 시 `Return to Home`를 수행한다.
5. 또는 새 경로를 업로드하거나 `Idle`로 복귀한다.

#### 6.10.2 중도 종료

1. 운용자는 `Stop`을 누른다.
2. leader robot은 이동을 중단한다.
3. follower robot도 정지 또는 hold 상태로 전환한다.
4. 앱은 `Idle` 또는 재시작 준비 화면으로 복귀한다.

### 6.11 다중 로봇 운용 규칙

#### 6.11.1 제어 규칙

- 앱은 leader robot만 직접 제어한다.
- leader robot은 follower robot의 이동 명령을 생성하고 분배한다.
- follower robot은 direct manual drive 대상이 아니다.

#### 6.11.2 표시 규칙

- 지도는 leader 기준으로 route와 formation을 표시한다.
- follower robot은 상태, 위치, slot 유지 여부를 중심으로 표시한다.
- follower robot 상태는 leader robot이 집계한 값으로 표시한다.
- event log에는 leader와 follower 이벤트를 모두 기록하되 source robot ID를 포함한다.

#### 6.11.3 안전 규칙

- `E-Stop`은 swarm 전체에 우선한다.
- leader 연결이 끊기면 follower는 fail-safe 정지 정책을 적용한다.

### 6.12 운용 요약

1. Device Check에서 leader robot을 선택한다.
2. 앱 명령은 leader robot에만 전달한다.
3. Recon과 Assault는 waypoint 기반으로 운용한다.
4. Recon 또는 Assault 종료 후 필요 시 `Return to Home`로 출발지에 복귀한다.
5. 세부 진행 상태는 항상 `Mission Status`로 표시한다.
6. 스트리밍은 고정 파이프라인 `start/stop`만 제어한다.
7. 긴급 상황에서는 leader를 통해 follower까지 즉시 정지시킨다.
8. follower 상태는 leader robot이 집계해 앱에 전달한다.

## 7. 네트워크 채널 정의

| 채널 | 방향 | 프로토콜 | 포트 | 용도 |
| --- | --- | --- | --- | --- |
| Command | App -> Leader Robot | TCP | `65432` | 모드 전환, 짐벌, stream start/stop |
| Touch | App -> Leader Robot | UDP | `65433` | 터치 좌표 |
| Driving | App -> Leader Robot | UDP | `65434` | direct drive 보정 |
| Status | Leader Robot -> App | TCP | `65435` | leader 및 follower 집계 상태 피드백 |
| Path | App -> Leader Robot | TCP | `65436` | route 및 mission path |
| RTSP | Robot -> App | RTSP | `8554` | 고정 파이프라인 영상 |

RTSP 주소 예:

- `rtsp://<robot_ip>:8554/cam0`
- `rtsp://<robot_ip>:8554/cam1`

## 8. App -> Leader Robot 데이터 정의

### 8.1 StateCommand

현재 wire 개념은 아래와 같이 사용한다.

- TCP 고정 길이 바이너리
- 패킷 크기: 28 bytes

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `command_id` | `uint8` | 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 4/5=Debug, 6=Assault, 7=Return to Home, 8=E-Stop |
| `e_stop_command` | `uint8` | 긴급 정지 플래그 (0 이외의 값일 경우 즉시 정지) |
| `attack_permission` | `uint8` | 승인 플래그 |
| `pan_speed` | `int8` | 팬 속도 |
| `tilt_speed` | `int8` | 틸트 속도 |
| `zoom_command` | `int8` | 줌 명령 |
| `lateral_wind_speed` | `float32` | 확장용 보정값 |
| `stream_command` | `uint8` | 0=None, 1=Start, 2=Stop |
| `drone_target_lat` | `float64` | Protect Drone 목표 위도 |
| `drone_target_lon` | `float64` | Protect Drone 목표 경도 |
| `drone_target_valid` | `uint8` | 좌표 유효 여부 |

### 8.2 DrivingCommand

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `move_speed` | `int8` | `-100 ~ 100` |
| `turn_angle` | `int8` | `-100 ~ 100` |

정책:

- direct drive는 leader robot에만 보낸다.
- follower robot은 direct drive를 받지 않는다.

### 8.3 TouchCoordinate

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `x` | `float32` | 정규화 X |
| `y` | `float32` | 정규화 Y |

### 8.4 Path 제어

`AssaultCommandHeader + JSON Payload` 형태를 사용한다.

헤더:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `command` | `uint8` | 0=None, 1=Start, 2=Stop, 3=Pause, 4=Resume, 5=LoadPath |
| `num_waypoints` | `uint16` | waypoint 개수 |
| `data_length` | `uint32` | JSON 길이 |

path payload 예:

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

### 8.5 Recon route 예

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

추가 규칙:

- route 시작 시점에 home position을 기록해야 한다.
- `Return to Home`는 이 home position을 기준으로 수행한다.

## 9. Leader -> Follower 데이터 정의

메시지명:

- `SwarmMoveCommand`

토픽:

- `/swarm/formation_move_command`
- 또는 `/<robot_ns>/swarm/move_command`

필드 정의:

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `header` | `std_msgs/Header` | 공통 헤더 |
| `leader_robot_id` | `string` | leader ID |
| `target_robot_id` | `string` | follower ID |
| `seq` | `uint32` | 시퀀스 번호 |
| `mode` | `uint8` | 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 4=Assault Mission, 5=Return to Home, 6=E-Stop |
| `movement_type` | `uint8` | 0=Hold, 1=FollowLeader, 2=MoveToSlot, 3=FollowRoute, 4=MoveToWaypoint, 5=ReturnHome |
| `formation_type` | `uint8` | 0=None, 1=Line, 2=Column, 3=Wedge, 4=Diamond, 5=Custom |
| `slot_index` | `uint8` | formation slot index |
| `offset_x_m` | `float32` | leader 기준 종방향 offset |
| `offset_y_m` | `float32` | leader 기준 횡방향 offset |
| `offset_yaw_deg` | `float32` | leader 기준 자세 offset |
| `reference_lat` | `float64` | leader 기준 위도 |
| `reference_lon` | `float64` | leader 기준 경도 |
| `reference_heading_deg` | `float32` | leader 기준 heading |
| `target_lat` | `float64` | 목표 위도 |
| `target_lon` | `float64` | 목표 경도 |
| `target_speed_mps` | `float32` | 목표 속도 |
| `waypoint_index` | `uint16` | 현재 waypoint index |
| `total_waypoints` | `uint16` | 전체 waypoint 수 |
| `hold_time_ms` | `uint32` | 대기 시간 |
| `command_flags` | `uint16` | 비트 플래그 |

`command_flags` 비트:

| 비트 | 의미 |
| --- | --- |
| bit0 | formation 유지 |
| bit1 | obstacle avoidance 허용 |
| bit2 | route 재계산 허용 |
| bit3 | hold position |
| bit4 | synchronized start |
| bit5 | emergency stop |

운용 규칙:

- Recon에서는 `FollowLeader` 또는 `MoveToSlot`을 사용한다.
- Assault에서는 `FollowRoute` 또는 `MoveToWaypoint`를 사용한다.
- Return to Home에서는 `ReturnHome` movement_type을 사용한다.

## 10. Follower -> Leader 상태 정의

### 10.1 FollowerStateReport

leader robot은 follower robot의 상태를 수집한 뒤 앱으로 집계 전달한다.

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

- follower robot은 자신의 상태를 leader robot에 주기적으로 보고한다.
- leader robot은 최신 follower 상태를 캐시하고 앱 전송용 집계 상태로 변환한다.
- timeout이 발생한 follower는 disconnected 상태로 표시한다.

## 11. Robot -> App 상태 정의

### 11.1 StatusPacket

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `rtsp_server_status` | `uint8` | RTSP 상태 |
| `active_mode_id` | `uint8` | 현재 모드 |
| `estop_active` | `uint8` | 긴급 정지 활성 여부 |
| `permission_request_active` | `uint8` | 승인 요청 여부 |
| `crosshair_x` | `float32` | 조준점 X |
| `crosshair_y` | `float32` | 조준점 Y |
| `current_zoom_level` | `float32` | 현재 줌 값 |
| `mission_status` | `uint8` | 세부 수행 상태 |
| `nav_state` | struct | 위치/방향/속도 |
| `mission_state` | struct | waypoint 진행 상태 |
| `assault_status` | struct | Assault 전용 내부 상태 |

상태 전달 원칙:

- 앱은 follower robot 상태를 직접 받지 않는다.
- leader robot은 자신의 상태와 follower 상태를 하나의 상태 프레임으로 집계해 앱에 전달한다.
- 앱의 지도, Device Check, formation 패널, follower 카드 표시는 leader robot이 전달한 집계 상태를 기준으로 한다.

### 11.2 FollowerRobotStatus

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `robot_id` | `char[16]` | follower robot ID |
| `link_status` | `uint8` | 0=Disconnected, 1=Connected |
| `active_mode_id` | `uint8` | follower 현재 모드 |
| `mission_status` | `uint8` | follower Mission Status |
| `battery_pct` | `uint8` | 배터리 잔량 |
| `latitude` | `float64` | 현재 위도 |
| `longitude` | `float64` | 현재 경도 |
| `heading` | `float32` | 현재 heading |
| `speed_mps` | `float32` | 현재 속도 |
| `slot_index` | `uint8` | formation slot index |
| `error_code` | `uint8` | follower 오류 코드 |
| `status_flags` | `uint16` | 상태 비트 플래그 |

### 11.3 SwarmStatusPacket

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `leader_status` | `StatusPacket` | leader robot 상태 |
| `follower_count` | `uint8` | 포함된 follower 수 |
| `followers` | `FollowerRobotStatus[]` | follower 상태 목록 |

운용 규칙:

- `StatusPacket`은 leader robot의 기준 상태로 사용한다.
- `followers[]`는 현재 leader가 관리 중인 follower 상태를 포함한다.
- follower link가 끊겨도 마지막 상태와 `link_status=0`을 함께 전달한다.

### 11.4 activeMode 매핑

현재 태블릿 테스트 서버 기준으로 `active_mode_id`는 연속값 `0..6`을 사용한다.

| 값 | 앱 표시명 |
| --- | --- |
| `0` | Idle |
| `1` | Recon |
| `2` | Protect General |
| `3` | Protect Drone |
| `4` | Assault |
| `5` | Return to Home |
| `6` | E-Stop |

### 11.5 missionStatus 매핑

| 값 | 표시명 |
| --- | --- |
| `0` | None |
| `1` | Ready |
| `2` | Moving |
| `3` | Paused |
| `4` | Reached |
| `5` | Error |

### 11.6 내부 DTO 예

```json
{
  "status": {
    "activeMode": "return_to_home",
    "missionStatus": "moving",
    "rtspServerUp": true,
    "permissionRequestActive": false,
    "crosshair": {
      "x": -1.0,
      "y": -1.0,
      "visible": false
    },
    "nav": {
      "lat": 37.5665,
      "lon": 126.9780,
      "heading": 0.0,
      "speedMps": 0.7
    },
    "mission": {
      "currentWaypointIndex": 1,
      "totalWaypoints": 3,
      "progressRatio": 0.5,
      "distanceToNextWpM": 4.2,
      "distanceToGoalM": 17.0
    },
    "followers": [
      {
        "robotId": "S2",
        "linkStatus": "connected",
        "activeMode": "return_to_home",
        "missionStatus": "moving",
        "batteryPct": 81,
        "slotIndex": 1,
        "errorCode": 0,
        "nav": {
          "lat": 37.56647,
          "lon": 126.97802,
          "heading": 1.8,
          "speedMps": 0.6
        }
      }
    ]
  }
}
```

## 12. FSM 요약

### 12.1 모드 전이

- `INIT -> IDLE`
- `IDLE -> RECON`
- `IDLE -> PROTECT_GENERAL`
- `IDLE -> PROTECT_DRONE`
- `IDLE -> ASSAULT`
- `IDLE -> RETURN_TO_HOME`
- `모든 운용 모드 -> EMERGENCY_STOP`
- `RETURN_TO_HOME -> IDLE` when home reached
- `운용 모드 -> ERROR` when fault

### 12.2 Mission Status 전이

- `NONE -> READY`
- `READY -> MOVING`
- `MOVING -> PAUSED`
- `PAUSED -> MOVING`
- `MOVING -> REACHED`
- `* -> ERROR`

### 12.3 버튼과 전이 매핑

| 버튼 | 결과 |
| --- | --- |
| Recon | `activeMode = Recon` |
| Protect General | `activeMode = Protect General` |
| Protect Drone | `activeMode = Protect Drone` |
| Load Path | `activeMode = Assault`, `missionStatus = Ready` |
| Start | `missionStatus = Moving` |
| Pause | `missionStatus = Paused` |
| Resume | `missionStatus = Moving` |
| Return to Home | `activeMode = Return to Home` |
| Stream Start | 모드 유지, stream 시작 |
| Stream Stop | 모드 유지, stream 중지 |
| Stop | `activeMode = Idle` |
| E-Stop | `activeMode = E-Stop` |

## 13. 예외 및 안전 규칙

### 13.1 network 단절

- 상태 채널이 끊기면 앱은 `Disconnected`를 표시한다.
- `E-Stop`을 제외한 조작을 잠근다.

### 13.2 home position 없음

- `Return to Home`를 비활성화한다.
- `Home Position Not Available` 메시지를 표시한다.

### 13.3 mission 오류

- `missionStatus = Error`를 표시한다.
- `Stop` 또는 재시작 준비 상태로 유도한다.

### 13.4 swarm 안전 규칙

- leader 연결이 끊기면 follower는 fail-safe 정지 정책을 따른다.
- `E-Stop`은 swarm 전체에 우선한다.
- follower 상태가 timeout되면 앱은 leader 집계 상태 기준으로 `Disconnected`를 표시한다.

## 14. 운용 요약

1. Device Check에서 leader robot을 선택한다.
2. 앱 명령은 leader robot에만 전달한다.
3. Recon과 Assault는 waypoint 기반으로 수행한다.
4. Recon 또는 Assault 완료 후 필요 시 Return to Home을 수행한다.
5. 세부 상태는 항상 `Mission Status`로 표시한다.
6. 스트리밍은 고정 파이프라인 `start/stop`만 제어한다.
7. 긴급 상황에서는 leader를 통해 follower까지 즉시 정지시킨다.
8. follower 상태는 leader robot이 집계해 앱에 전달한다.





