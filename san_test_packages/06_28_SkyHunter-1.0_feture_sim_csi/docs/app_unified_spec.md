# 앱 연동 통합 문서

## 1. 문서 목적

이 문서는 전투로봇 앱 연동에 필요한 운용 개념, 모드 정의, 네트워크 인터페이스, 상태전이, leader-follower 제어 구조를 하나의 문서로 통합한 사양서다.

## 1.1 현재 구현 스냅샷

오늘 기준 Android 앱과 ROS2 시스템 사이의 wire contract는 `ros/src/skyautonet/combat_robot_system/robot_server/include/command_server_protocol.hpp`를 따른다.

- `combatrobotcontroller/src/robot_server_*.cpp`는 standalone 테스트/레거시 서버 구현이며, 제품 앱 연동의 단일 기준으로 사용하지 않는다.
- `combat_robot_msgs/msg/UserCommand.msg`는 ROS2 내부 메시지다. Android 앱 패킷 정의는 TCP `65432`의 packed `StateCommand`다.
- App -> leader command는 TCP `65432`의 `StateCommand`이며 현재 크기는 `72 bytes`다.
- Robot -> app status는 TCP `65435`의 `SwarmStatusPacket`이며 현재 크기는 `1832 bytes`다.
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
- office/test/demo telemetry는 disconnected robot과 low-battery / estop / target-detected 로그를 포함할 수 있다.
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
| `READY` | 경로 설정 완료, 이동 시작 대기 |
| `MOVING` | 경로를 따라 이동 또는 임무 수행 중. 이동 중 타겟 대응/공격이 포함될 수 있음 |
| `PAUSED` | 일시정지 |
| `REACHED` | 목표 도달 또는 임무 완료 |
| `SURVEILLING` | Recon 목표 지점 도달 후 감시 모드 수행 중 |
| `ERROR` | 수행 오류 |

적용 예:

- `activeMode = Recon`, `missionStatus = Moving`
- `activeMode = Assault`, `missionStatus = Ready`
- `activeMode = Recon`, `missionStatus = Surveilling`
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

이 절에서 `Recon`, `Finish`, `Start`, `Pause`, `Resume`, `Stop`, `Return to Home`, `E-Stop`은 모두 클라이언트 앱 UI에서 운용자가 수행하는 조작을 뜻한다.

1. 운용자는 leader robot을 선택한다.
2. 클라이언트 앱 메인 화면에서 `Recon`을 선택한다.
3. 클라이언트 앱 지도 위 목표 지점을 선택하거나 위도/경도를 직접 입력한다.
4. 클라이언트 앱에서 waypoint를 추가한다.
5. 필요 시 클라이언트 앱에서 waypoint를 삭제하거나 순서를 재정렬한다.
6. 클라이언트 앱에서 `Finish`로 route를 확정한다.
7. 앱은 Path 채널로 `LOAD_PATH` 헤더와 route JSON payload를 전송한다.
8. leader robot은 payload를 검증하고 route를 저장한 뒤 `Mission Status = Ready`로 전이한다.
9. 운용자는 클라이언트 앱에서 `Start`를 눌러 이동을 시작한다.
10. 앱은 Path 채널로 `START` 신호만 전송한다. 이때 route payload는 다시 보내지 않는다.
11. leader robot은 직전에 저장한 route를 사용해 `Mission Status = Moving`으로 전이한다.
12. 앱은 현재 waypoint, 진행률, 현재 위치를 지도에 표시한다.
13. follower robot은 formation slot에 따라 leader를 추종한다.
14. 운용자는 필요 시 짐벌, 줌, 스트리밍만 추가 조작한다.
15. Recon은 route goal 도달 후 `SURVEILLING`을 거쳐 `REACHED`로 전이한다.
16. `REACHED`는 다음 `LOAD_PATH`, `Return to Home`, `Idle/Stop`, `E-Stop` 등 새 명령이 오기 전까지 유지한다.

#### 6.4.3 운용 중 확인 항목

- 현재 waypoint / 전체 waypoint
- 진행 방향
- 지도상의 경로 이탈 여부
- follower formation 유지 여부
- follower 개별 mode, Mission Status, 배터리, 오류 여부
- 영상 품질 및 스트리밍 상태
- 장애물 또는 우회 필요 여부

#### 6.4.4 Recon 중 예외 처리

- 일시 정지 필요 시 클라이언트 앱에서 `Pause`
- 재시작 시 클라이언트 앱에서 `Resume`
- 경로 변경 필요 시 클라이언트 앱에서 `Stop` 후 route 수정
- mission 종료 후 필요 시 클라이언트 앱에서 `Return to Home`
- 즉시 안전 정지 필요 시 클라이언트 앱에서 `E-Stop`

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

기준 파일: `ros/src/skyautonet/combat_robot_system/robot_server/include/command_server_protocol.hpp`

- TCP 고정 길이 packed binary
- `#pragma pack(push, 1)` 기준 C/C++ raw struct layout
- 패킷 크기: `72 bytes`
- 현재 구현은 별도 byte-order 변환 없이 packed struct raw bytes를 송수신한다.
- Android 앱 패킷 정의는 이 `StateCommand`이며, `combat_robot_msgs/msg/UserCommand.msg`가 아니다.

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `command_id` | `uint8` | 앱 명령 ID. 제품 운용은 `0/1/2/3/6/7/8` 사용 |
| `e_stop_command` | `uint8` | 긴급 정지 플래그 (0 이외의 값일 경우 즉시 정지) |
| `attack_permission` | `uint8` | 승인 응답 (`NONE / APPROVE / DENY`), backend는 effective state를 별도 유지 |
| `approval_request_id` | `uint32` | 현재 응답 중인 approval request id. active request가 없으면 `0` |
| `pan_speed` | `int8` | 팬 속도 |
| `tilt_speed` | `int8` | 틸트 속도 |
| `zoom_command` | `int8` | 줌 명령 |
| `lateral_wind_speed` | `float32` | 확장용 보정값 |
| `stream_command` | `uint8` | 0=None, 1=Start, 2=Stop |
| `stream_target_robot_id` | `uint32` | 스트리밍 source robot id. `1..8`은 `S1..S8` |
| `formation_type` | `uint8` | 0=None, 1=Recon, 2=Protect, 3=Assault |
| `formation_number` | `uint8` | formation preset number. 각 타입 내부 `1..4` |
| `grouping_index` | `uint8` | app-provided grouping index |
| `selected_robot_count` | `uint8` | `selected_robot_ids`에 포함된 유효 robot 수. 최대 `8` |
| `selected_robot_ids` | `uint32[8]` | 선택된 robot id 목록. 미사용 slot은 `0` 권장 |
| `drone_target_lat` | `float64` | Protect Drone 목표 위도 |
| `drone_target_lon` | `float64` | Protect Drone 목표 경도 |
| `drone_target_valid` | `uint8` | 좌표 유효 여부 |

`command_id` 정책:

| 값 | 의미 | 제품 UI 정책 |
| --- | --- | --- |
| `0` | Idle | 사용 |
| `1` | Recon | 사용 |
| `2` | Protect General | 사용 |
| `3` | Protect Drone | 사용 |
| `4` | Debug Attack | 숨김, 개발/테스트 전용 |
| `5` | Debug Tracking | 숨김, 개발/테스트 전용 |
| `6` | Assault | 사용 |
| `7` | Return to Home | 사용 |
| `8` | E-Stop | 사용 |

주의:

- `command_id`는 Android -> robot wire packet의 필드다.
- `combat_robot_msgs/msg/UserCommand.msg`의 동일 숫자 상수는 ROS2 내부 publish/subscribe 구현에서 재사용될 수 있지만, Android packet schema의 원본은 아니다.
- 제품 앱은 일반 운용에서 `4/5` debug command를 표시하거나 전송하지 않는다.
- Status의 `active_mode_id`는 수신 상태 필드이며, 송신 `command_id`와 혼동하지 않는다.

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

- packed header 크기: `7 bytes`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `command` | `uint8` | 0=None, 1=Start, 2=Stop, 3=Pause, 4=Resume, 5=LoadPath |
| `num_waypoints` | `uint16` | waypoint 개수 |
| `data_length` | `uint32` | JSON 길이 |

명령별 의미:

| command | 의미 |
| --- | --- |
| `LOAD_PATH` | route payload를 검증하고 내부 저장 route를 갱신한다. 성공 시 `mission_status = READY` |
| `START` | 저장된 route로 mission을 시작한다. 일반적으로 payload 없이 보낸다 |
| `PAUSE` | 현재 mission 진행을 일시정지한다 |
| `RESUME` | 일시정지된 mission을 이어서 진행한다 |
| `STOP` | 현재 mission 진행만 중지하고 `mission_status = NONE`으로 전이한다 |

payload 사용 규칙:

- `LOAD_PATH`일 때만 path payload를 해석하고 저장한다.
- `START/PAUSE/RESUME/STOP`에 포함된 payload는 무시한다.
- 실운용 기준으로 `Finish` 버튼은 `LOAD_PATH`, `Start` 버튼은 `START`를 전송한다.
- `START`는 보통 `num_waypoints = 0`, `data_length = 0`으로 보내며, 직전에 성공한 `LOAD_PATH`를 사용한다.

허용 path payload format:

1. waypoint object 배열
2. GeoJSON `LineString` 좌표 배열

waypoint object 배열 예:

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

GeoJSON `LineString` 예:

```json
{
  "type": "LineString",
  "coordinates": [
    [126.9780, 37.5665],
    [126.9785, 37.5667]
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

- `num_waypoints`는 payload에서 실제 파싱되는 waypoint 수와 반드시 일치해야 한다.
- waypoint object format에서는 `lat`/`latitude`, `lon`/`lng`/`longitude`를 허용한다.
- GeoJSON `coordinates`는 반드시 `[longitude, latitude]` 순서를 사용한다.
- `missionId` 또는 `routeId`가 있으면 active path 식별자로 사용할 수 있다.
- route 시작 시점에 home position을 기록해야 한다.
- `Return to Home`는 이 home position을 기준으로 수행한다.
- `START`가 `READY`, `REACHED`, `SURVEILLING` 상태에서 들어오면 route를 처음부터 다시 시작한다.
- `START` 또는 `RESUME`가 `PAUSED` 또는 `MOVING` 상태에서 들어오면 현재 진행도를 이어서 사용한다.
- `LOAD_PATH`가 성공하면 이전 mission 진행도, 복귀 거리, home 유효 상태를 초기화하고 새 route 기준 `READY`로 재준비한다.
- `REACHED`는 별도 종료 명령이 오기 전까지 유지된다.

path/mission 오류 코드:

| error_code | 의미 |
| --- | --- |
| `0` | `NONE` |
| `1` | `INVALID_PATH_PAYLOAD` |
| `2` | `WAYPOINT_COUNT_MISMATCH` |
| `3` | `PATH_NOT_LOADED` |
| `4` | `INVALID_PATH_COMMAND` |
| `5` | `RETURN_HOME_UNAVAILABLE` |
| `6` | `INVALID_STREAM_TARGET` |

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

기준 파일: `ros/src/skyautonet/combat_robot_system/robot_server/include/command_server_protocol.hpp`

- TCP `65435`의 `SwarmStatusPacket.leader_status` 내부 블록
- packed 크기: `161 bytes`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `rtsp_server_status` | `uint8` | RTSP 상태 |
| `active_mode_id` | `uint8` | 현재 모드 |
| `last_tablet_command_id` | `uint8` | 마지막으로 수신한 command id |
| `estop_active` | `uint8` | 긴급 정지 활성 여부 |
| `permission_request_active` | `uint8` | 승인 요청 여부 |
| `effective_attack_permission` | `uint8` | backend가 현재 유효하다고 판단한 attack permission (`NONE / APPROVE / DENY`) |
| `path_loaded` | `uint8` | 유효한 path payload가 현재 서버에 적재되어 있는지 여부 |
| `home_position_valid` | `uint8` | Return to Home에 사용할 home position이 저장되어 있는지 여부 |
| `return_home_available` | `uint8` | 현재 시점에 Return to Home을 시작할 수 있는지 여부 |
| `crosshair_x` | `float32` | 조준점 X |
| `crosshair_y` | `float32` | 조준점 Y |
| `current_zoom_level` | `float32` | 현재 줌 값 |
| `mission_status` | `uint8` | 세부 수행 상태 |
| `swarm_role` | `uint8` | leader / follower / standalone 역할 |
| `formation_type` | `uint8` | formation type |
| `formation_number` | `uint8` | formation preset number |
| `grouping_index` | `uint8` | grouping index |
| `slot_index` | `uint8` | formation slot index |
| `robot_id` | `uint32` | 현재 서버 인스턴스 robot id |
| `leader_robot_id` | `uint32` | 현재 leader robot id |
| `active_stream_robot_id` | `uint32` | 현재 태블릿에 송출 중인 source robot id |
| `home_lat` | `float64` | 저장된 home position 위도, invalid일 때 `0.0` |
| `home_lon` | `float64` | 저장된 home position 경도, invalid일 때 `0.0` |
| `active_path_id` | `char[64]` | 현재 적재된 path/route identifier. `missionId` 또는 `routeId`를 사용하고 없으면 빈 문자열 |
| `nav_state` | struct | 위치/방향/속도 |
| `mission` | struct | Mission Status, 오류 코드, waypoint 진행 상태를 함께 담는 runtime 상태 |

상태 전달 원칙:

- 앱은 follower robot 상태를 직접 받지 않는다.
- leader robot은 자신의 상태와 follower 상태를 하나의 상태 프레임으로 집계해 앱에 전달한다.
- 앱의 지도, Device Check, formation 패널, follower 카드는 모두 leader robot이 전달한 집계 상태를 기준으로 한다.
- 앱은 attack permission UI를 `permission_request_active`만으로 해석하지 말고, `effective_attack_permission`을 backend accepted state로 사용해야 한다.
- 앱은 `path_loaded`를 기준으로 `Start/Resume` 가능 여부를 판단할 수 있다.
- 앱은 `home_position_valid`와 `return_home_available`를 기준으로 `Return to Home` 버튼 활성 여부를 판단할 수 있다.
- `active_path_id`는 현재 leader robot이 들고 있는 route/path 식별자 확인용이다.

### 11.2 RobotAggregateStatus

`SwarmStatusPacket.robots[8]`의 각 entry다. leader도 이 배열 안에 포함된다.

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `robot_id` | `uint32` | robot id. `1..8`은 `S1..S8` |
| `role` | `uint8` | leader / follower / standalone 역할 |
| `link_status` | `uint8` | 연결 상태 |
| `comm_quality_level` | `uint8` | 통신 품질 단계 |
| `battery_pct` | `uint8` | 배터리 잔량 |
| `active_mode_id` | `uint8` | robot 표시용 현재 모드 |
| `mission_status` | `uint8` | Mission Status |
| `estop_active` | `uint8` | 해당 robot E-Stop 활성 여부 |
| `formation_type` | `uint8` | formation type |
| `formation_number` | `uint8` | formation preset number |
| `grouping_index` | `uint8` | grouping index |
| `slot_index` | `uint8` | formation slot index |
| `movement_type` | `uint8` | leader-following / hold / return-home 등 movement assignment |
| `error_code` | `uint8` | 오류 코드 |
| `status_flags` | `uint16` | 상태 비트 플래그 |
| `latitude` | `float64` | 현재 위도 |
| `longitude` | `float64` | 현재 경도 |
| `heading` | `float32` | 현재 heading |
| `speed_mps` | `float32` | 현재 속도 |
| `zoom_level` | `float32` | 해당 robot camera zoom level |

### 11.3 RobotLogEntry

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `robot_id` | `uint32` | log source robot id |
| `timestamp_sec` | `uint32` | timestamp seconds |
| `severity` | `uint8` | log severity |
| `event_code` | `uint8` | event code |
| `message` | `char[64]` | 표시용 메시지 |

`TARGET_DETECTED` log는 monitoring/log 표시용이며, approval UI trigger가 아니다.

### 11.4 ApprovalRequestStatus

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `active` | `uint8` | active approval request 여부 |
| `target_type` | `uint8` | 승인 대상 타입 |
| `reserved` | `uint16` | reserved |
| `request_id` | `uint32` | approval session id |
| `confidence` | `float32` | 탐지 confidence |
| `summary` | `char[64]` | approval UI 요약 텍스트 |

### 11.5 SwarmStatusPacket

- TCP `65435` 고정 길이 packed binary
- packed 크기: `1832 bytes`

| 필드 | 타입 | 설명 |
| --- | --- | --- |
| `leader_status` | `StatusPacket` | leader robot 상태 요약 |
| `selected_robot_count` | `uint8` | 현재 선택된 robot 수 |
| `selected_robot_ids` | `uint32[8]` | 현재 선택된 robot id 목록 |
| `robot_count` | `uint8` | `robots[8]`에 포함된 유효 robot 수 |
| `robots` | `RobotAggregateStatus[8]` | leader와 follower를 포함한 고정 크기 robot 상태 배열 |
| `log_count` | `uint8` | `logs[16]`에 포함된 유효 log 수 |
| `logs` | `RobotLogEntry[16]` | 최근 monitoring log 배열 |
| `approval_request` | `ApprovalRequestStatus` | object-level approval request state |

운용 규칙:

- `StatusPacket`은 leader robot의 기준 상태로 사용한다.
- `robots[8]`에는 leader robot도 포함된다. 기본 dummy/demo telemetry에서는 `S1`이 leader다.
- follower link가 끊긴 경우에도 마지막 상태와 `link_status=0`을 함께 전달한다.
- 앱은 `robot_count`, `log_count`, `selected_robot_count` 범위만 유효 데이터로 읽는다.
- 고정 배열의 남는 slot은 표시하지 않는다.

### 11.6 activeMode 매핑

현재 ROS2 `OperationState` 기준 app-facing `active_mode_id`는 아래 값을 사용한다.

| 값 | 앱 표시명 |
| --- | --- |
| `0` | Idle |
| `1` | Recon |
| `2` | Protect General |
| `3` | Protect Drone |
| `6` | Assault |
| `7` | Return to Home |
| `8` | E-Stop |

주의:

- `active_mode_id`는 Robot -> App 상태 필드다. App -> Robot의 `command_id`와 같은 목적의 필드가 아니다.
- E-Stop은 보통 `estop_active=1` overlay로 표시한다.
- backend가 이전 operation mode context를 유지하는 동안에도 앱은 `estop_active=1`이면 E-Stop 상태를 우선 표시한다.
- aggregate/test data에서 `active_mode_id=8`이 들어오면 앱은 E-Stop으로 표시한다.

### 11.7 missionStatus 매핑

| 값 | 표시명 |
| --- | --- |
| `0` | None |
| `1` | Ready |
| `2` | Moving |
| `3` | Paused |
| `4` | Reached |
| `5` | Surveilling |
| `6` | Error |

### 11.8 내부 DTO 예

```json
{
  "status": {
    "activeMode": "return_to_home",
    "missionStatus": "moving",
    "rtspServerUp": true,
    "permissionRequestActive": false,
    "effectiveAttackPermission": "none",
    "pathLoaded": true,
    "activePathId": "mission-001",
    "homePositionValid": true,
    "returnHomeAvailable": true,
    "crosshair": {
      "x": -1.0,
      "y": -1.0,
      "visible": false
    },
    "homePosition": {
      "lat": 37.5659,
      "lon": 126.9778
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

- `NONE -> READY` on successful `LOAD_PATH`
- `READY -> MOVING` on `START`
- `MOVING -> PAUSED`
- `PAUSED -> MOVING` on `RESUME`
- `PAUSED -> MOVING` on `START` when continuing the paused route
- `MOVING -> REACHED`
- `MOVING -> SURVEILLING` for `Recon` when the route goal is reached
- `SURVEILLING -> REACHED`
- `REACHED -> READY` on next successful `LOAD_PATH`
- `REACHED -> MOVING` on route restart `START` or `Return to Home`
- `REACHED` is sticky until a new command changes it
- `* -> ERROR`

### 12.3 버튼과 전이 매핑

이 표의 버튼명은 모두 클라이언트 앱 UI에서 운용자가 누르거나 선택하는 조작을 의미한다.

| 버튼 | 결과 |
| --- | --- |
| Recon | 클라이언트 앱에서 `Recon` 선택, 결과적으로 `activeMode = Recon` |
| Protect General | 클라이언트 앱에서 `Protect General` 선택, 결과적으로 `activeMode = Protect General` |
| Protect Drone | 클라이언트 앱에서 `Protect Drone` 선택, 결과적으로 `activeMode = Protect Drone` |
| Finish / Load Path | 클라이언트 앱에서 `Finish` 또는 `Load Path` 수행, Path `LOAD_PATH` 전송, route 검증/저장 성공 시 `missionStatus = Ready` |
| Start | 클라이언트 앱에서 `Start` 수행, Path `START` 전송, 저장된 route로 `missionStatus = Moving` |
| Pause | 클라이언트 앱에서 `Pause` 수행, Path `PAUSE` 전송, `missionStatus = Paused` |
| Resume | 클라이언트 앱에서 `Resume` 수행, Path `RESUME` 전송, `missionStatus = Moving` |
| Path Stop | 클라이언트 앱에서 path 제어용 `STOP` 수행, Path `STOP` 전송, `missionStatus = NONE` |
| Return to Home | 클라이언트 앱에서 `Return to Home` 선택, `activeMode = Return to Home` |
| Stream Start | 클라이언트 앱에서 `Stream Start` 수행, 모드 유지, stream 시작 |
| Stream Stop | 클라이언트 앱에서 `Stream Stop` 수행, 모드 유지, stream 중지 |
| Stop | 클라이언트 앱에서 일반 `Stop` 수행, `activeMode = Idle` |
| E-Stop | 클라이언트 앱에서 `E-Stop` 수행, `activeMode = E-Stop` |

추가 규칙:

- Recon은 route goal 도달 후 `missionStatus = Surveilling`으로 전이한다.
- Recon은 감시 단계 종료 후 `missionStatus = Reached`로 전이한다.
- Assault와 Return to Home은 goal 도달 후 `missionStatus = Reached`로 전이한다.
- `Start`는 path를 다시 싣지 않고, 가장 최근에 성공한 `LOAD_PATH`를 사용한다.
- UI의 일반 `Stop`과 Path 채널의 `STOP`은 구분한다. 일반 `Stop`은 `activeMode = Idle`, Path `STOP`은 mission 진행만 중단한다.

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

## 부록: 추가 세부 사항

### Tablet-Backend Protocol V1

이 프로토콜은 leader-centric control model을 따름.

- 태블릿은 leader robot에 명령을 보냄.
- follower robot은 직접 제어되지 않음.
- follower behavior는 leader state에서 파생됨.

#### Leader-Centric Commanding

- 태블릿이 leader robot mode를 변경함.
- follower robot은 high-level mode 명령을 직접 받지 않음.
- follower behavior는 movement assignment에서 파생됨.

#### Robot Selection

- 태블릿은 `robot_id[]` 배열로 선택된 로봇을 보냄.
- 선택된 로봇만 명령을 실행함.
- 선택되지 않은 로봇은 `HOLD` 상태 유지.
- leader robot도 selectable set에 포함됨.

#### Follower Mode Derivation

Follower display mode는 movement assignment에서 파생됨, 별도 per-follower mode 명령이 아님.

- `ESTOP`이 최고 우선순위.
- `RETURN_HOME`이 `FOLLOW_LEADER`보다 우선.
- `FOLLOW_LEADER`는 follower가 leader와 같은 mode를 표시함.
- `HOLD`는 follower가 `IDLE`을 표시함.

우선순위 순서:

1. `ESTOP`
2. `RETURN_HOME`
3. `FOLLOW_LEADER`
4. `HOLD`

#### Mode IDs

Android command id와 backend status mode id는 서로 다른 필드임.

App -> Robot `StateCommand.command_id`:

- `0`: `IDLE`
- `1`: `RECON`
- `2`: `PROTECT_GENERAL`
- `3`: `PROTECT_DRONE`
- `4`: `DEBUG_ATTACK`, 개발/테스트 전용
- `5`: `DEBUG_TRACKING`, 개발/테스트 전용
- `6`: `ASSAULT`
- `7`: `RETURN_TO_HOME`
- `8`: `ESTOP`

Robot -> App `active_mode_id`:

- `0`: `IDLE`
- `1`: `RECON`
- `2`: `PROTECT_GENERAL`
- `3`: `PROTECT_DRONE`
- `6`: `ASSAULT`
- `7`: `RETURN_TO_HOME`
- `8`: `ESTOP`

제품 UI는 debug command `4/5`를 숨기고 일반 운용에서 전송하지 않음.

#### E-Stop Policy

`E-Stop`은 UI에서 mode replacement로 취급되지 않음.

- 백엔드는 기존 operation mode context를 유지함.
- `estop_active`는 현재 mode 위에 overlay state로 전달됨.
- 앱은 `E-Stop`을 현재 mode 대신에 표시해야 함.

예:

- `RECON + estop_active=1`
- `ASSAULT + estop_active=1`

#### Formation and Grouping

Formation과 grouping은 app-driven settings임.

- 태블릿이 formation/grouping settings를 백엔드에 보냄.
- 백엔드가 검증하고 적용함.

##### Formation Model

Formation은 다음과 같이 정의됨:

- `mode`
- `preset(1..4)`

유효한 formation presets:

- `RECON` preset `1..4`
- `PROTECT` preset `1..4`
- `ASSAULT` preset `1..4`

##### Grouping Model

Grouping은 app-provided grouping index임.

현재 결정:

- grouping은 앱에서 보냄.
- 백엔드가 저장하고 적용함.

나중에 finalize할 세부 사항:

- 각 grouping index의 정확한 semantic table.

#### Camera and Zoom Policy

Zoom level은 현재 선택된 robot camera에 연결됨, leader only가 아님.

- 태블릿이 leader를 통해 스트리밍을 요청함.
- 백엔드가 `active_stream_robot_id`를 보고함.
- `current_zoom_level`은 현재 스트림 로봇의 zoom level을 반드시 참조해야 함.
- Follower robots도 자신의 zoom level을 per-robot status에 포함해야 함.

스트림 target 변경 시:

- 앱이 선택된 로봇의 현재 zoom state를 받아야 함.
- zoom이 leader-global camera zoom으로 해석되지 않아야 함.

#### Attack Approval Flow

Attack approval은 log messages와 분리됨.

##### Important Separation

`TARGET_DETECTED` log entries는 monitoring/log display only임.

Approval UI trigger로 사용되지 않음.

##### Approval Request Trigger

Operator approval이 필요한 objects는 dedicated approval-request state로 전송됨, `RobotLogEntry`를 통해 아님.

##### Approval Response Enum

`attack_permission`은 3-state enum을 사용함:

- `0`: `NONE`
- `1`: `APPROVE`
- `2`: `DENY`

의미:

- `NONE`: 아직 응답 없음 / hold
- `APPROVE`: operator가 승인함
- `DENY`: operator가 거부함

##### Approval Flow

1. Approval-required detection이 발생함.
2. 백엔드가 dedicated approval-request state를 앱에 보냄.
3. 앱이 approval UI를 표시함.
4. Operator가 승인하면 앱이 `APPROVE`를 보냄.
5. Operator가 거부하면 앱이 `DENY`를 보냄.
6. Operator가 hold하면 앱이 아무것도 보내지 않거나 `NONE`을 유지함.

##### Current Backend Attack Permission Lifecycle

현재 구현 기준에서 `attack_permission`은 raw tablet value를 그대로 유지하는 sticky mode flag가 아님.

- backend는 `attack_permission`을 active approval request에 대한 응답으로 해석함.
- `IDLE`로 전환되면 effective `attack_permission`은 즉시 `NONE`으로 초기화됨.
- `IDLE` 이후 다른 mode로 다시 진입하더라도, 새 approval request가 발생하기 전까지 effective `attack_permission`은 `NONE`으로 유지됨.
- `APPROVE` 또는 `DENY`는 approval request가 active인 동안에만 유효한 응답으로 반영됨.
- active approval request 없이 들어온 `APPROVE` 또는 `DENY`는 stale input으로 간주하고 무시함.
- `drone_target_valid != 0`으로 새 approval request를 열 때 effective `attack_permission`은 다시 `NONE`으로 초기화됨.
- `E-Stop`도 safety rule로 취급하며 effective `attack_permission`을 `NONE`으로 초기화하고 approval request metadata를 함께 clear함.

##### Object-Level Approval Session Policy

object-level approval을 위해 app은 `attack_permission`만 보내면 안 되고, 현재 응답 중인 `approval_request_id`를 함께 보내야 함.

- `approval_request_id = 0`은 invalid default value이며, `no active request`를 의미함.
- active request가 없을 때 app outbound 기본값은 `attack_permission = NONE`, `approval_request_id = 0`.
- active request가 있을 때 app은 항상 현재 `approval_request_id = current_request_id`를 함께 보내야 함.
- active request는 있지만 아직 operator 응답이 없을 때도 app은 `attack_permission = NONE`, `approval_request_id = current_request_id`를 보냄.
- operator가 승인하면 app은 `attack_permission = APPROVE`, `approval_request_id = current_request_id`를 보냄.
- operator가 거부하면 app은 `attack_permission = DENY`, `approval_request_id = current_request_id`를 보냄.
- backend는 `approval_request.active = 1`이고 incoming `approval_request_id == current_request_id`일 때만 해당 응답을 현재 approval session에 대한 유효 응답으로 처리함.
- active request 없이 들어온 `APPROVE` 또는 `DENY`, 또는 현재 request와 맞지 않는 `approval_request_id`는 stale response로 간주하고 무시함.
- `approval_request.request_id`가 바뀌면 app은 이전 로컬 approval 상태를 즉시 폐기하고 새 approval session을 처음부터 시작해야 함.
- request가 종료되거나 cancel / expire / `IDLE` / `E-Stop`이 발생하면 app은 즉시 `attack_permission = NONE`, `approval_request_id = 0`으로 복귀해야 함.

##### Approval Request Scope

Approval request state는 system level에서 통합됨, single robot card에 tied되지 않음.

이는 의미함:

- 앱이 unified approval-request state를 받음.
- Robot-specific context는 필요 시 포함될 수 있음.
- Approval UI는 monitoring log messages에서 trigger되지 않음.

#### Status Delivery Model

백엔드는 다음을 제공해야 함:

- leader status block
- per-robot aggregate status
- recent monitoring logs
- dedicated approval-request state

##### Per-Robot Status Expectations

각 robot status entry는 최소 다음을 포함해야 함:

- `robot_id`
- `movement_type`
- derived `active_mode_id`
- `estop_active`
- `battery`
- `link_status`
- `comm_quality_level`
- `gps position`
- `formation mode`
- `formation preset`
- `grouping index`
- `zoom_level`

#### Current App -> Backend Command Direction

태블릿 command model은 현재 `StateCommand`에 다음을 포함함:

- `command_id`
- `selected_robot_ids[]`
- `formation_mode`
- `formation_preset`
- `grouping_index`
- `stream_target_robot_id`
- `attack_permission`
- `approval_request_id` for object-level approval responses

#### Current Backend -> App Status Direction

백엔드 status model은 현재 `SwarmStatusPacket`에 다음을 포함함:

- `active_mode_id`
- `estop_active`
- `active_stream_robot_id`
- `current_zoom_level`
- swarm-wide per-robot status
- recent logs
- approval-request state

#### Explicit Non-Goals

다음은 tablet-triggered high-level control model의 일부가 아님:

- 앱에서 direct per-follower high-level mode command
- `TARGET_DETECTED` log entries를 approval triggers로 사용
- zoom을 leader-only camera state로 해석
- UI에서 current mode를 `ESTOP`으로 replace

#### Implementation Notes

현재 백엔드 direction:

- selected robots가 명령을 실행함.
- unselected robots가 `HOLD` 상태 유지.
- followers in `FOLLOW_LEADER`가 leader mode를 mirror함.
- followers in `RETURN_HOME`가 `RETURN_HOME`을 표시함.
- `attack_permission`이 `NONE / APPROVE / DENY`임.
- backend는 tablet이 보낸 raw `attack_permission`을 그대로 재사용하지 않고 effective permission state를 유지함.
- object-level approval에서는 app response에 `approval_request_id`가 함께 포함되어야 하며, `approval_request_id = 0`은 no active request를 의미함.

현재 packet contract에 반영된 항목:

- dedicated approval-request status structure
- app/server command and status contract의 grouping index
- follower robots에 대한 per-robot zoom reporting
- selected robot id list

남은 work:

- 오래된 test code나 documentation에 남아 있는 legacy assumptions 제거
- Android/Kotlin packet struct를 C++ source of truth와 자동 대조하는 검증 추가
- `robot_server` protocol serialization/deserialization regression test 보강

### 1차 개발 TODO

#### 범위

이번 1차 범위는 아래까지만 포함함.

- 앱 <-> `robot_server` 연동
- Main 오퍼레이터 상태를 앱에 맞게 노출하는 부분
- RTSP 제어 정책 정리
- mission/leader/follower 관련 필드는 placeholder 허용

이번 1차 범위에서 제외함.

- waypoint 실제 주행
- leader/follower relay
- follower 상태 집계
- home position 저장과 Return to Home 실제 수행
- mission pause/resume 실제 제어

#### 구현 원칙

- 패킷 구조는 최종 문서 기준으로 먼저 맞춤.
- 미구현 기능은 fake 동작 대신 placeholder 값만 보냄.
- placeholder는 앱이 오인하지 않도록 중립값을 씀.
- 실제 값이 없는데 임의 좌표를 넣는 방식은 금지함.

#### Placeholder 정책

##### mode / status

- `active_mode_id`
  - 실제 진입한 오퍼레이터 상태를 기준으로 전송
  - 미구현 모드는 앱에 노출하지 않거나 `IDLE` 유지

- `mission_status`
  - 시작 기본값은 `NONE`
  - 현재 테스트 서버는 `LOAD_PATH/START/PAUSE/RESUME/STOP/Return to Home`에 따라 `READY/MOVING/PAUSED/SURVEILLING/REACHED/ERROR`를 생성함

##### crosshair / permission

- `crosshair_x = -1.0`
- `crosshair_y = -1.0`
- `permission_request_active = 0`

실제 보호/탐지 상태에서만 값이 있을 때 갱신함.

##### nav / motion

- 실제 위치 공급자가 연결되기 전까지 앱이 숨길 수 있는 placeholder 정책 필요
- 테스트 서버는 `LOAD_PATH` 후 저장된 route를 따라 현재 위치와 heading을 시뮬레이션함
- route가 없으면 마지막 known pose를 유지하고 mission 관련 거리/진행률은 0 또는 상태 기준값을 사용함

##### mission path

- 시작 시점에는 in-progress mission/path를 미리 seed 하지 않음
- 첫 `LOAD_PATH` 전에는 `current_waypoint_index = 0`, `total_waypoints = 0`, `progress_ratio = 0.0`
- `LOAD_PATH` 성공 시 `total_waypoints`와 저장 route를 갱신하고 `mission_status = READY`
- `START` 후에는 route 진행도에 따라 waypoint, progress, 거리, 현재 위치가 함께 갱신됨
- `error_code`는 path payload 검증 실패, path 미적재, 잘못된 path 명령, Return Home 불가 등의 사유를 반영함

#### 1차 완료 조건

- 앱이 `robot_server`와 연결되어 제품 기준 command/status packet으로 통신 가능
- Main 화면에서 mode, RTSP 상태, crosshair, zoom, estop/error 구분이 일관되게 표시됨
- mission/leader/follower 미구현 기능은 앱에서 오인되지 않는 placeholder로만 노출됨
- 이후 2차 개발에서 movement/swarm를 붙여도 packet 구조를 다시 깨지 않아도 됨

### robot_server 선차단 구현과 개발 계획 비교

#### 목적

이 문서는 현재 `robot_server`에 추가한 mode 선차단 로직을 기준으로, 아래 두 수준의 목표와 비교했을 때 아직 부족한 부분을 정리함.

비교 기준 문서:

- 단기 기준: `docs/app_integration/phase1_robot_server_main_operator_todo.md`
- 최종 기준: `docs/app_integration/app_unified_spec.md`
- 최종 상태전이 기준: `docs/app_integration/app_fsm.md`
- 최종 운용 기준: `docs/app_integration/operation_scenario.md`
- 인터페이스 기준: `docs/app_integration/app_interface_spec.md`

비교 대상 구현:

- `ros/src/skyautonet/combat_robot_system/robot_server/include/command_server_protocol.hpp`
- `ros/src/skyautonet/combat_robot_system/robot_server/include/command_server_internal_utils.hpp`
- `ros/src/skyautonet/combat_robot_system/robot_server/src/command_server_dispatch.cpp`
- `ros/src/skyautonet/combat_robot_system/robot_server/src/command_server_status.cpp`

#### 현재 반영된 선차단 구조

현재 `robot_server`는 `operation_state_`를 보고 mode 전환 허용 여부를 판단함.

- 항상 허용: `ESTOP`, `RETURN_TO_HOME`, `IDLE`
- 그 외 기능 모드:
  - 현재 `operation_state == IDLE`이면 허용
  - `IDLE`이 아니면 요청한 mode command를 현재 상태에 대응하는 값으로 덮어쓴 뒤 publish
- `pan/tilt`, `zoom`, `stream`, `formation`, `selected_robot_ids`, `approval_request_id` 같은 mode 외 필드는 같은 `StateCommand` 안에서 유지됨

관련 코드:

- `ros/src/skyautonet/combat_robot_system/robot_server/include/command_server_internal_utils.hpp`
- `ros/src/skyautonet/combat_robot_system/robot_server/src/command_server_dispatch.cpp`

이 로직은 "현재 기능이 끝나서 `IDLE`로 돌아오기 전에는 다른 일반 기능으로 넘어가지 않게 한다"는 단기 목적에는 맞음.

#### 1차 개발 계획 대비 현재 상태

1차 계획은 제품 기준 command id 체계 정리와 debug 명령 분리를 요구함.

현재 코드 기준으로 `command_id == 6`은 `ASSAULT`로 매핑됨.

정리된 부분:

- Android wire packet은 `StateCommand` 72 bytes로 확장됨.
- `command_id == 6`은 `ASSAULT`, `7`은 `RETURN_TO_HOME`, `8`은 `ESTOP`로 취급됨.
- `approval_request_id`, `stream_target_robot_id`, `formation_type`, `formation_number`, `grouping_index`, `selected_robot_ids[8]`가 command contract에 포함됨.
- status는 `SwarmStatusPacket` 1832 bytes로 `robots[8]`, `logs[16]`, `approval_request`를 포함함.

남은 부분:

- 제품 Android UI는 debug command `4/5`를 숨기고 일반 운용에서 보내지 않아야 함.
- Android/Kotlin packet 정의가 C++ `command_server_protocol.hpp`와 어긋나지 않도록 자동 검증이 필요함.
- 오래된 테스트 서버/문서에 남은 28-byte command, follower-only status, 0..6 mode id 가정을 제거해야 함.

#### 최종 목표 대비 부족한 부분

최종 목표 문서들을 보면, 현재 mode gate는 전체 제품 구조 중 아주 작은 일부만 충족함.

최종 문서는 `Recon`, `Protect`, `Assault`, `Return to Home`, `E-Stop`만이 아니라 `Mission Status`와 `Path` lifecycle까지 포함함.

현재 `robot_server`는 Path 채널에서 `LOAD_PATH/START/PAUSE/RESUME/STOP`을 받으면 내부 카운터와 `mission_status_`만 바꾸고 있음.

즉 현재는:

- mode gate는 있음
- 하지만 최종 목표의 `Path Load -> Assault + Ready -> Start -> Moving -> Pause/Resume -> Reached` 흐름을 실제 제어하지는 않음
- `mission_status`가 실제 FSM/mission executor 상태가 아니라 `robot_server` 내부 placeholder로도 바뀔 수 있음

이 점은 최종 목표와 가장 큰 차이임.

`Return to Home` 전제도 구현 구조에 없음.

최종 문서는 `Return to Home`를 `Recon` 또는 `Assault` 종료 후, 저장된 home position이 있을 때만 사용하는 복귀 모드로 정의함.

하지만 현재 gate는 `RETURN_TO_HOME`를 거의 무조건 예외 통과시킴.

즉 현재는 "안전상 예외 허용" 수준이고, 최종 목표의 아래 조건은 반영하지 않음.

- home position 존재 여부
- mission 완료 또는 중단 후 복귀라는 맥락
- `Recon/Assault -> RTH -> IDLE` 흐름 검증

제품 모드와 디버그 모드 분리는 wire protocol 수준에서는 구분되어 있음.

최종 문서는 `Assault Tracking`은 디버그 전용이고 `Assault Manual`은 제거한다고 정의함.

`combat_robot_msgs/msg/UserCommand.msg`는 ROS2 내부 메시지이며 Android 앱 패킷 정의의 기준이 아님.

제품 앱 기준으로는 `StateCommand.command_id`의 `4/5`를 debug 전용으로 유지하고 UI에서 노출하지 않는 것이 계약임. 남은 작업은 Android UI, test fixture, legacy documentation에서 이 정책을 일관되게 검증하는 것임.

최종 목표의 leader-follower 구조도 반영하지 않음.

최종 문서는 앱이 leader robot만 직접 제어하고, follower 상태는 leader가 집계해서 앱으로 전달해야 한다고 정의함.

현재 `robot_server`의 선차단 로직은 단일 로봇 기준 `operation_state_`만 보고 판단함. swarm/leader-follower 맥락은 없음.
