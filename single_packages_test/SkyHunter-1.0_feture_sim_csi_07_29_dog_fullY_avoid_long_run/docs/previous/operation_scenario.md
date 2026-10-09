# 운용 시나리오

## 1. 문서 목적

이 문서는 전투로봇 시스템의 실제 운용 절차를 제품 관점에서 정의한다.

기준 정책:

- 앱의 이동 명령은 `leader robot`에만 전달한다.
- follower robot은 leader robot의 relay 명령에 따라 동작한다.
- follower robot의 상태는 leader robot이 수집해 앱으로 전달한다.
- `Assault`는 최종 제품에서 `Mission`만 제공한다.
- `Ready`, `Moving`, `Paused`, `Reached`, `Error`는 모드가 아니라 `Mission Status`다.
- 스트리밍은 고정 파이프라인 기반이며 앱은 `start/stop`만 제어한다.
- `Return to Home`는 `Recon` 또는 `Assault` 종료 후 초기 출발지로 복귀하는 모드다.

## 2. 운용 개요

기본 운용 흐름은 아래와 같다.

1. 장비 및 네트워크 점검
2. Device Check에서 leader robot 선택
3. 영상 및 상태 연결 확인
4. 임무 유형 선택
5. `Recon`, `Protect`, `Assault` 중 하나 수행
6. mission 종료 후 필요 시 `Return to Home` 수행
7. 필요 시 `Pause`, `Resume`, `Stop`, `E-Stop` 수행
8. 임무 종료 후 상태 확인 및 후속 임무 전환

## 3. 사전 점검 시나리오

### 3.1 전원 및 장치 확인

운용자는 앱 실행 전 아래를 확인한다.

- leader robot 전원 상태
- follower robot 전원 상태
- 카메라 및 RTSP 스트리밍 가능 여부
- 통신 링크 상태
- GPS 위치 수신 여부
- 배터리 상태

### 3.2 앱 접속

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

## 4. Device Check 시나리오

### 4.1 다수 로봇 확인

1. 운용자는 Device Check 화면에서 S1~S8 로봇을 확인한다.
2. 앱은 각 로봇의 위치, 배터리, 연결 상태, 현재 모드를 표시한다.
3. leader robot은 별(`★`) 또는 leader 배지로 구분한다.
4. 앱은 formation 구성과 link overlay를 표시한다.
5. follower robot 상태는 leader robot이 집계한 상태를 기준으로 표시한다.

### 4.2 leader robot 선택

1. 운용자는 제어 기준이 되는 leader robot을 선택한다.
2. 선택된 leader robot이 Main Screen의 기준 장치가 된다.
3. 이후 이동/모드/임무 명령은 선택된 leader robot에만 전달된다.

운용 규칙:

- follower robot은 직접 선택해 주행시키지 않는다.
- follower robot 화면은 상태 확인 중심으로 사용한다.

## 5. Recon 운용 시나리오

### 5.1 목적

- 지정 경로를 따라 이동하며 주변을 정찰한다.

### 5.2 기본 절차

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

### 5.3 운용 중 확인 항목

- 현재 waypoint / 전체 waypoint
- 진행 방향
- 지도상의 경로 이탈 여부
- follower formation 유지 여부
- follower 개별 mode, Mission Status, 배터리, 오류 여부
- 영상 품질 및 스트리밍 상태
- 장애물 또는 우회 필요 여부

### 5.4 Recon 중 예외 처리

- 일시 정지 필요 시 `Pause`
- 재시작 시 `Resume`
- 경로 변경 필요 시 `Stop` 후 route 수정
- mission 종료 후 필요 시 `Return to Home`
- 즉시 안전 정지 필요 시 `E-Stop`

## 6. Protect 운용 시나리오

### 6.1 목적

- 지정 구역 또는 방향에 대해 경계 및 감시를 수행한다.

### 6.2 기본 절차

1. 운용자는 leader robot을 선택한다.
2. `Protect`를 선택한다.
3. 감시 유형을 `General` 또는 `Drone`으로 선택한다.
4. 앱은 해당 모드 명령을 leader robot에 전송한다.
5. leader robot은 감시 기준 위치 또는 sector를 설정한다.
6. follower robot은 할당된 위치 또는 sector를 유지한다.
7. 앱은 탐지 이벤트, 조준점, 승인 요청 상태를 표시한다.
8. 앱은 leader robot이 집계한 follower Protect 상태를 함께 표시한다.

### 6.3 운용 중 확인 항목

- 탐지 이벤트 발생 여부
- 조준점 표시 여부
- 승인 요청 상태
- leader/follower 위치 유지 여부
- follower 개별 모드, Mission Status, 배터리, 오류 여부
- 영상 가시성

### 6.4 Protect 중 예외 처리

- 탐지 오경보 시 상태만 기록하고 감시 지속
- 감시 위치 재조정 필요 시 leader 기준 재배치
- 심각 상황 시 `E-Stop`

## 7. Assault 운용 시나리오

### 7.1 목적

- leader robot을 중심으로 waypoint 기반 임무를 수행한다.

### 7.2 기본 절차

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

### 7.3 운용 중 확인 항목

- mission status
- waypoint 진행률
- 목표까지 잔여 거리
- follower 재배치 상태
- follower 개별 mode, Mission Status, 배터리, 오류 여부
- link 손실 여부
- event log

### 7.4 Assault 중 예외 처리

- 위치 오차가 크면 `Pause` 후 상태 재점검
- follower 일부 이탈 시 leader 중심 재동기화
- mission 오류 시 `Mission Status = Error` 확인 후 `Stop`
- mission 완료 후 필요 시 `Return to Home`
- 즉시 정지 필요 시 `E-Stop`

## 8. Return to Home 운용 시나리오

### 8.1 목적

- Recon 또는 Assault 종료 후 초기 출발지로 안전하게 복귀한다.

### 8.2 사용 조건

- 출발 시점의 home position이 저장되어 있어야 한다.
- 일반적으로 `Recon` 또는 `Assault` mission 수행 이력이 있어야 한다.

### 8.3 기본 절차

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

### 8.4 운용 중 확인 항목

- home position 유효 여부
- 복귀 경로 이탈 여부
- leader/follower formation 유지 여부
- follower 개별 mode, Mission Status, 배터리, 오류 여부
- 스트리밍 및 상태 채널 정상 여부

### 8.5 예외 처리

- home position이 없으면 복귀 시작 금지
- 복귀 중 경로 이상 시 `Pause` 또는 `Stop`
- 긴급 상황 시 `E-Stop`

## 9. 스트리밍 운용 시나리오

### 9.1 기본 원칙

- 영상은 고정 파이프라인으로 송출된다.
- 앱은 품질 변경을 하지 않는다.
- 앱은 `Stream Start`, `Stream Stop`만 수행한다.

### 9.2 기본 절차

1. 운용자가 메인 화면 또는 통합 화면에서 `Stream Start`를 선택한다.
2. 로봇은 고정 파이프라인을 시작한다.
3. 앱은 RTSP 연결을 시도한다.
4. 영상 수신에 성공하면 화면에 live view를 표시한다.
5. 필요 시 `Stream Stop`으로 송출을 중지한다.

### 9.3 예외 처리

- 영상이 끊기면 `No Signal` 표시
- 상태 채널이 살아 있으면 임무 표시는 유지
- 필요 시 `Stream Stop -> Stream Start` 순으로 재시도

## 10. 긴급 정지 시나리오

### 10.1 발생 조건

- 충돌 위험
- 통신 이상
- 경로 이탈 심화
- 비정상 동작
- 운용자 수동 정지 판단

### 10.2 절차

1. 운용자는 어느 화면에서든 `E-Stop`을 누른다.
2. 앱은 leader robot에 즉시 긴급 정지 명령을 보낸다.
3. leader robot은 follower robot에 정지 명령을 relay한다.
4. 앱은 모든 일반 제어를 잠근다.
5. 운용자는 현장 상태를 확인한다.
6. 복귀가 가능하면 `Stop/Reset` 후 `Idle`로 전환한다.

## 11. 임무 종료 시나리오

### 11.1 정상 종료

1. mission이 완료되어 `Mission Status = Reached`가 된다.
2. 앱은 완료 배지를 표시한다.
3. 운용자는 결과를 확인한다.
4. 필요 시 `Return to Home`를 수행한다.
5. 또는 새 경로를 업로드하거나 `Idle`로 복귀한다.

### 11.2 중도 종료

1. 운용자는 `Stop`을 누른다.
2. leader robot은 이동을 중단한다.
3. follower robot도 정지 또는 hold 상태로 전환한다.
4. 앱은 `Idle` 또는 재시작 준비 화면으로 복귀한다.

## 12. 다중 로봇 운용 규칙

### 12.1 제어 규칙

- 앱은 leader robot만 직접 제어한다.
- leader robot은 follower robot의 이동 명령을 생성하고 분배한다.
- follower robot은 direct manual drive 대상이 아니다.

### 12.2 표시 규칙

- 지도는 leader 기준으로 route와 formation을 표시한다.
- follower robot은 상태, 위치, slot 유지 여부를 중심으로 표시한다.
- follower robot 상태는 leader robot이 집계한 값으로 표시한다.
- event log에는 leader와 follower 이벤트를 모두 기록하되 source robot ID를 포함한다.

### 12.3 안전 규칙

- `E-Stop`은 swarm 전체에 우선한다.
- leader 연결이 끊기면 follower는 fail-safe 정지 정책을 적용한다.

## 13. 운용 요약

제품 운용의 핵심은 아래와 같다.

1. Device Check에서 leader robot을 선택한다.
2. 앱 명령은 leader robot에만 전달한다.
3. Recon과 Assault는 waypoint 기반으로 운용한다.
4. Recon 또는 Assault 종료 후 필요 시 `Return to Home`로 출발지에 복귀한다.
5. 세부 진행 상태는 항상 `Mission Status`로 표시한다.
6. 스트리밍은 고정 파이프라인 `start/stop`만 제어한다.
7. 긴급 상황에서는 leader를 통해 follower까지 즉시 정지시킨다.
8. follower 상태는 leader robot이 집계해 앱에 전달한다.
