# 앱 연동 시나리오

## 1. 문서 목적

이 문서는 전투로봇과 모바일 앱 간 연동 시나리오를 최종 제품 기준으로 정의한다.

- `Move`는 앱에서 `Recon`으로 표기한다.
- `Surveillance`는 앱에서 `Protect`로 표기한다.
- 공격 기능은 제품 UI에서 `Assault`로 통일한다.
- `Assault Manual`은 제거한다.
- `Assault Tracking`은 개발/데모/시험용 숨김 모드로만 유지한다.
- 최종 제품에서 사용자가 보는 `Assault`는 `Assault Mission`만 의미한다.
- `Ready`, `Moving`, `Paused`, `Reached` 같은 값은 모드 하위 상태가 아니라 별도 `Mission Status`로 처리한다.
- 영상 스트리밍은 동적 파이프라인 변경 없이 고정 파이프라인을 사용하고, 앱은 `stream start/stop`만 제어한다.

## 2. 최종 제품 모드 구성

| 앱 표시명 | 제품 의미 | 내부 기능 기준 |
| --- | --- | --- |
| Idle | 대기 상태 | `STOP` |
| Recon | 기동 정찰 | 기존 `MOVE_MODE` |
| Protect | 경계/감시 | 기존 `SURVEILLANCE_MODE`, `DRONE_SURVEILLANCE_MODE` |
| Assault | 경로 기반 임무 수행 | 기존 `ASSAULT_MODE` + Path 제어 |
| Return to Home | 초기 출발지 복귀 | 신규 `RTH_MODE` |
| E-Stop | 긴급 정지 | `EMERGENCY_STOP` |

보조 원칙:

- `Protect`는 일반 감시와 드론 감시 2가지 하위 유형을 가진다.
- `Assault Tracking`은 제품 사용자에게 기본 노출하지 않는다.
- `Assault` 화면에는 경로 업로드, 시작, 일시정지, 재개, 중지만 제공한다.

## 3. 시스템 연결 시나리오

### 3.1 앱 시작

1. 앱은 로봇 IP를 확인한다.
2. 앱은 RTSP 스트림 주소를 준비한다.
3. 앱은 상태 수신 TCP 채널에 연결한다.
4. 앱은 명령 송신 TCP 채널에 연결한다.
5. 앱은 홈 화면에서 현재 모드, RTSP 상태, 연결 상태를 표시한다.

Swarm 운용 기준:

- 앱은 다수 로봇을 표시할 수 있지만 이동 명령은 `leader robot`에만 전달한다.
- leader robot은 UI에서 별(`★`) 또는 leader 배지로 명확히 표시한다.
- follower robot은 앱에서 직접 주행 명령을 받지 않고, leader robot이 재분배한 이동 명령을 수행한다.
- follower robot의 상태 정보는 leader robot이 수집해 앱으로 전달한다.

초기 화면 표시 정보:

- 현재 모드: `Idle`
- 현재 Mission Status: `None`
- 영상 상태: `Ready` 또는 `Disconnected`
- 네트워크 상태: `Connected` 또는 `Retrying`
- 미션 상태: `Not Loaded`

### 3.2 홈 화면

홈 화면에는 아래 5개를 기본 노출한다.

- `Recon`
- `Protect`
- `Assault`
- `Return to Home`
- `E-Stop`

추가 원칙:

- `Assault Tracking`은 개발자 옵션에서만 노출한다.
- `Assault Manual` 관련 버튼은 노출하지 않는다.
- swarm 화면에서는 leader robot을 기준으로 링크 상태, formation 구성, route 적용 상태를 표시한다.
- `Return to Home`는 임무 종료 후 복귀 전용 기능으로 노출한다.

### 3.3 Device Check 및 Leader 선택

UI Spec 기준으로 앱은 `Device Check` 화면에서 다수 로봇 상태를 지도에 표시한다.

동작:

1. 앱은 S1~S8 장치를 지도 위 마커로 표시한다.
2. leader robot은 별(`★`) 또는 전용 배지로 구분한다.
3. 운용자는 Device Check에서 leader robot을 선택한다.
4. 선택된 leader robot을 기준으로 Main Screen으로 진입한다.
5. formation 구성, 링크 상태, route overlay는 leader robot 기준으로 검증한다.

운용 원칙:

- 이동 명령, route 명령, mission 시작 명령은 leader robot에만 송신한다.
- follower robot은 앱에서 개별 제어하지 않는다.
- follower robot의 상태는 leader robot이 전달한 집계 상태 기준으로 지도와 통합 화면에서 모니터링한다.

## 4. 기능별 상세 시나리오

### 4.1 Recon 시나리오

목적:

- 운용자가 로봇을 직접 이동시키며 주변을 정찰한다.

동작:

1. 사용자가 홈에서 `Recon`을 선택한다.
2. 앱은 현재 선택 장치가 leader robot인지 확인한다.
3. 앱은 모드 전환 명령을 leader robot에 송신한다.
4. 운용자는 지도에서 목적지를 지정하거나 위도/경도를 직접 입력한다.
5. 운용자는 waypoint를 추가, 삭제, 재정렬한다.
6. 운용자는 `Finish`로 route를 확정한다.
7. 앱은 확정된 route를 leader robot에 업로드한다.
8. 사용자가 `Start of driving`을 누르면 leader robot이 경로 주행을 시작한다.
9. follower robot은 leader robot이 전달한 formation 기반 이동 명령을 수행한다.
10. 필요 시 운용자는 controller 또는 가상 조이스틱으로 leader robot에 직접 주행 보정 명령을 줄 수 있다.
11. 사용자는 필요 시 짐벌 속도와 줌을 조정한다.
12. 앱은 상태 채널로부터 현재 모드와 확대 배율을 수신해 UI에 반영한다.
13. 앱은 leader robot이 전달한 follower 상태 목록을 함께 수신해 formation 유지 여부와 각 follower 상태를 표시한다.

Recon 이동 방식:

- 기본 이동 방식은 `waypoint 기반 route driving`이다.
- 경로는 지도 pin 또는 위도/경도 입력으로 생성한다.
- waypoint 편집은 `add / delete / reorder / reset / finish` 순서를 지원한다.
- 최종 `Start`는 leader robot에 대해서만 수행한다.
- follower robot은 leader 기준 상대 위치 또는 formation slot을 따라간다.
- route driving 진행 상태는 `Mission Status`로 표시한다.

앱 UI 구성:

- 실시간 영상
- 지도 기반 route 설정 패널
- 위도/경도 입력
- waypoint 추가/삭제/재정렬
- route 확정 버튼
- 주행 시작 버튼
- 이동 조이스틱
- 짐벌 패드
- 줌 버튼
- 스트림 시작/정지 버튼
- 즉시 정지 버튼
- swarm 상태 및 formation 개요

### 4.2 Protect 시나리오

목적:

- 특정 방향 또는 지역을 감시하고, 탐지 결과와 조준 상태를 운용자에게 제공한다.

하위 유형:

- `Protect General`: 일반 감시
- `Protect Drone`: 드론 감시

동작:

1. 사용자가 홈에서 `Protect`를 선택한다.
2. 앱은 `general` 또는 `drone` 유형을 선택하게 한다.
3. 앱은 선택값에 따라 감시 모드 전환 명령을 송신한다.
4. 로봇은 자동 스캔 또는 감시 동작을 시작한다.
5. 타겟이 탐지되면 시스템은 조준점과 상태를 갱신한다.
6. 앱은 상태 채널에서 `crosshair`, `permissionRequestActive`를 수신한다.
7. 앱은 화면에 탐지 상태, 조준점, 승인 요청 상태를 표시한다.

swarm 운용 시:

- Protect 모드 전환도 leader robot에만 송신한다.
- leader robot은 follower robot에 formation 유지 또는 sector 분담 명령을 전달한다.
- 앱은 leader robot이 집계한 follower Protect 상태를 통합 지도에서 표시하되, 제어는 leader 중심으로 유지한다.

앱 UI 구성:

- 실시간 영상
- 탐지 상태 배지
- 조준점 오버레이
- 감시 유형 전환
- 줌 버튼
- 스트림 시작/정지 버튼
- `Idle` 복귀 버튼
- `E-Stop`

### 4.3 Assault 시나리오

목적:

- 사전에 입력한 waypoint 경로를 기반으로 임무를 수행한다.

핵심 원칙:

- 최종 제품에서 `Assault = Assault Mission`
- 사용자는 수동 조준 또는 Tracking 선택을 하지 않는다.

동작:

1. 사용자가 홈에서 `Assault`를 선택한다.
2. 앱은 저장된 경로 또는 새 경로를 준비한다.
3. 앱은 현재 제어 대상이 leader robot인지 확인한다.
4. 앱은 Path 채널로 waypoint 목록을 leader robot에 업로드한다.
5. 로봇은 경로를 저장하고 `READY` 상태를 준비한다.
6. 사용자가 `Start`를 누르면 앱은 미션 시작 명령을 leader robot에 보낸다.
7. leader robot은 자신의 경로를 수행하고 follower robot에 slot 또는 waypoint 기반 이동 명령을 전달한다.
8. 앱은 상태 채널에서 waypoint 인덱스, 진행률, 잔여 거리, `Mission Status`를 수신한다.
9. 사용자는 `Pause`, `Resume`, `Stop` 중 하나를 선택할 수 있다.
10. 임무 완료 시 앱은 `Mission Status = Reached`를 표시한다.
11. 사용자는 `Idle` 복귀 또는 다음 경로 업로드를 수행한다.
12. 앱은 leader robot이 집계한 follower 상태를 함께 표시하여 각 follower의 slot 유지, link 상태, 오류 여부를 확인한다.

앱 UI 구성:

- 경로 업로드 버튼
- waypoint 개수
- 현재 waypoint / 전체 waypoint
- 진행률 바
- 다음 waypoint까지 거리
- 목표점까지 거리
- Mission Status 배지: `Ready`, `Moving`, `Paused`, `Reached`, `Error`
- `Start`, `Pause`, `Resume`, `Stop`
- 스트림 시작/정지 버튼
- `E-Stop`

### 4.4 E-Stop 시나리오

목적:

- 모든 동작보다 우선하는 안전 정지

동작:

1. 사용자가 어느 화면에서든 `E-Stop`을 누른다.
2. 앱은 즉시 긴급 정지 명령을 보낸다.
3. 로봇은 `EMERGENCY_STOP_STATE`로 진입한다.
4. 앱은 모든 일반 조작 UI를 비활성화한다.
5. 사용자가 상황 확인 후 `Stop` 또는 `Idle 복귀`를 수행한다.

운용 원칙:

- `E-Stop`은 네트워크 지연을 고려해 최상단 고정 버튼으로 둔다.
- `E-Stop` 상태에서는 Recon, Protect, Assault 전환 버튼을 잠근다.
- swarm 운용 시 `E-Stop`은 최소한 leader robot에 즉시 적용되어야 하며, leader는 follower에 정지 명령을 전파한다.

### 4.5 Return to Home 시나리오

목적:

- Assault 또는 Recon mission 종료 후, 초기 출발지로 복귀한다.

사용 조건:

- `Recon` mission이 완료된 경우
- `Assault` mission이 완료된 경우
- 운용자가 복귀 필요를 판단한 경우

동작:

1. 운용자는 leader robot 기준으로 `Return to Home`를 선택한다.
2. 앱은 저장된 초기 출발지(home position)를 확인한다.
3. 앱은 복귀 명령을 leader robot에 전송한다.
4. leader robot은 출발 시 기록된 초기 위치를 목표로 복귀 경로를 수행한다.
5. follower robot은 leader 기준 복귀 formation을 유지하며 함께 이동한다.
6. 앱은 `activeMode = Return to Home`과 `Mission Status`를 함께 표시한다.
7. 출발지 복귀 완료 시 `Mission Status = Reached`를 표시한다.
8. 운용자는 `Idle` 복귀 또는 후속 임무를 선택한다.
9. 앱은 leader robot이 집계한 follower 복귀 상태를 함께 표시한다.

운용 원칙:

- `Return to Home`는 일반 기동 모드가 아니라 임무 후속 복귀 모드다.
- 출발지 정보는 mission 시작 전에 반드시 기록되어야 한다.
- home position이 없으면 `Return to Home`를 시작할 수 없다.

## 5. 예외 및 복구 시나리오

### 5.1 네트워크 단절

1. 상태 TCP가 끊기면 앱은 `Disconnected` 상태를 표시한다.
2. 앱은 자동 재연결을 시도한다.
3. 재연결 전까지 주행/미션 시작 버튼은 잠근다.
4. 재연결 후 최신 상태 패킷을 기준으로 화면을 복구한다.

### 5.2 RTSP 미연결

1. 앱이 RTSP 연결 실패를 감지한다.
2. 앱은 영상 영역에 `Video unavailable`을 표시한다.
3. 상태 채널이 살아있다면 모드 상태는 계속 표시한다.
4. 영상이 복구되면 스트림을 재연결한다.
5. 앱은 필요 시 `stream start`를 재전송할 수 있다.

### 5.3 미션 오류

1. `Mission Status = Error`가 되면 앱은 오류 배지를 표시한다.
2. `Pause`, `Resume` 대신 `Stop`과 `Idle 복귀`를 우선 제공한다.
3. 필요 시 새 경로를 재업로드한다.
4. 복귀가 필요하면 `Return to Home` 실행 여부를 검토한다.

## 6. 제품 UI 노출 기준

### 6.1 기본 노출

- Idle
- Recon
- Protect
- Assault
- Return to Home
- E-Stop

### 6.2 숨김 또는 제거

- `Assault Manual`: 제거
- `Assault Tracking`: 개발자 옵션 또는 데모 빌드에서만 노출

## 7. Leader 기반 이동 제어 정책

UI Spec와 swarm SDD를 반영한 제품 정책은 아래와 같다.

1. 앱의 controller 입력은 leader robot에만 전달한다.
2. route 편집과 driving 시작도 leader robot에 대해서만 수행한다.
3. follower robot은 leader robot이 재전달한 이동 명령만 수행한다.
4. formation 구성은 Device Check 또는 통합 화면에서 확인한다.
5. route overlay와 link overlay는 leader robot 기준으로 검증한다.
6. follower robot의 위치, 배터리, 모드, Mission Status, 오류 상태는 leader robot을 통해 앱에 전달한다.

적용 예:

- `Recon`: leader가 route를 따라 이동하고, follower는 formation slot을 유지한다.
- `Protect`: leader가 sector 또는 기준 위치를 설정하고, follower는 할당된 위치를 유지한다.
- `Assault`: leader가 mission route를 수행하며, follower는 동일 mission context 내에서 분산 이동한다.
- `Return to Home`: leader가 초기 출발지로 복귀하고, follower는 복귀 formation을 유지한다.

## 8. 앱 내 용어

| 화면 용어 | 설명 |
| --- | --- |
| Recon | 이동 및 정찰 |
| Protect | 경계 및 감시 |
| Assault | 경로 기반 임무 수행 |
| Return to Home | 초기 출발지 복귀 |
| Mission Ready | 경로 업로드 완료, 시작 대기 |
| Mission Running | 미션 진행 중 |
| Mission Paused | 미션 일시정지 |
| Mission Complete | 목표 경로 완료 |
| Mission Error | 미션 오류 |
| Stream Start | 고정 파이프라인 스트리밍 시작 |
| Stream Stop | 고정 파이프라인 스트리밍 중지 |

## 9. 구현 매핑 요약

| 제품 용어 | 현재 구현 매핑 |
| --- | --- |
| Recon | `MOVE_MODE` |
| Protect General | `SURVEILLANCE_MODE` |
| Protect Drone | `DRONE_SURVEILLANCE_MODE` |
| Assault | `ASSAULT_MODE` |
| Return to Home | `RTH_MODE` |
| Assault Tracking | `TRACKING_MODE` |
| E-Stop | `EMERGENCY_STOP` |
