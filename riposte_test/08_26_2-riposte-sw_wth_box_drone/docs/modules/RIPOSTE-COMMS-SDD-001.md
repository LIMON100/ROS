# RIPOSTE-COMMS-SDD-001
## Riposte — 통신·배포(L1 / 플랫폼) 모듈 설계서

| 항목 | 내용 |
|---|---|
| 문서 ID | RIPOSTE-COMMS-SDD-001 |
| 버전 | 1.0 |
| 대상 | `riposte-sw/config/`, `riposte-sw/deploy/`, mavlink-router 연동 |
| 상위 문서 | RIPOSTE-SAD-001 (§5 통신 아키텍처) |
| 계층 | L1 통신 + L0 플랫폼 |

---

## 1. 목적 및 범위

RK3588 상의 MAVLink 라우팅, IP/장치 계획, systemd 기동 순서, udev 장치 명명을 정의한다. 애플리케이션 코드가 아닌 **통합·배포 계층**의 설계다.

---

## 2. MAVLink 라우팅 (mavlink-router)

단일 라우터가 3개 엔드포인트를 중계. **어느 노드도 서로 직결하지 않는다.**

```
[UartEndpoint sik]  /dev/riposte-sik @57600   ← GCS (SiK, USB)
[UdpEndpoint  fc ]  Server 0.0.0.0:14550       ← PX4 (Ethernet)
[UdpEndpoint  obc]  Normal 127.0.0.1:14540     ← riposte-obc (MAVSDK)
```

**설계 결정 N-1 (단일 라우팅 허브)**: OBC·GCS·FC 트래픽을 한 라우터에서 관리. OBC는 FC 직결이 아닌 로컬 UDP(`udpin://0.0.0.0:14540`)에 접속 → 링크 구성이 애플리케이션과 분리되고, GCS/정비 PC 추가가 설정만으로 가능.

**SiK 대역 관리(57.6kbps)**: 라우터는 필터링하지 않으므로 FC의 SiK행 스트림 레이트는 PX4 프로파일로 제한, OBC 상태보고는 1Hz로 최소화.

---

## 3. 연결 토폴로지 (재설계 핵심)

```
GCS ─SiK 433MHz(RF)─▶ [SiK 모뎀]─USB─▶ RK3588 ─Ethernet─▶ Pixhawk 6X ◀─UART─ GPS
```

| 링크 | 물리 | 프로토콜 |
|---|---|---|
| GCS ↔ RK3588 | **USB** (CP210x/FTDI) | MAVLink v2, 57600 8N1 |
| RK3588 ↔ Pixhawk | **Ethernet** 100M | MAVLink v2 / UDP |
| Pixhawk ↔ GPS | UART (GPS1) | u-blox UBX |

**설계 결정 N-2 (GPS는 FC 직결)**: GPS(GNSS+콤파스)는 Pixhawk GPS1에 직결, RK3588 아님. 근거: 비행안전 독립성(RK3588 다운 시에도 EKF2·RTL·지오펜스 정상), EKF2 하드웨어 타임스탬프 융합 품질, 유도는 기체 상대좌표만 사용. 상세는 SAD-001 §4 트레이드 스터디. RK3588은 위치를 GPS가 아닌 **FC의 `LOCAL_POSITION_NED`(이더넷)** 로 수신.

---

## 4. IP / 장치 계획

| 노드 | IP | 비고 |
|---|---|---|
| RK3588 | 192.168.144.10/24 | 정적, eth0 |
| Pixhawk 6X | 192.168.144.11/24 | 정적 |
| 정비 PC | 192.168.144.20/24 | 지상 정비 시 |

**udev 고정명** (`deploy/99-riposte.rules`): USB 열거 순서 비의존. SiK를 벤더/제품 ID로 `/dev/riposte-sik`에 고정.

---

## 5. 프로세스 기동 순서 (systemd)

```
mavlink-router → riposte-obc → riposte-seeker → riposte-supervisor
```

| 유닛 | 의존 | 재시작 정책 |
|---|---|---|
| mavlink-router | network-online | always (최우선) |
| riposte-obc | Requires mavlink-router | on-failure, StartLimitBurst=3, CAP_SYS_NICE |
| riposte-seeker | After obc | on-failure |
| riposte-supervisor | After obc | always, WatchdogSec=5 |

**설계 결정 N-3 (OBC 우선·무한재시작 금지)**: OBC는 TrackBus 부재에도 READY까지 진입 가능(시커 없는 지상점검 허용). OBC 반복 실패 시 `StartLimitBurst`로 재시작을 멈춰 PX4 페일세이프(D-1)를 가리지 않는다.

---

## 6. 프로세스 간 IPC 매핑 (요약)

| 채널 | 방식 | Writer→Reader |
|---|---|---|
| TrackBus | shm SeqSlot `SHM_TRACK` | seeker → obc·supervisor |
| ObcStatusBus | shm SeqSlot `SHM_OBC_STATUS` | obc → supervisor |
| SeekerHealthBus | shm SeqSlot `SHM_SEEKER_HEALTH` | seeker → supervisor |
| 명령 채널 | UDS datagram `OBC_CMD_SOCKET` | engage-cli → obc |

상세 원시요소는 RIPOSTE-COMMON-SDD-001 참조.

---

## 7. ASSUMPTION / DEFERRED

| 태그 | 항목 |
|---|---|
| ASSUMPTION | mavlink-router systemd 서비스로 설치·선기동 |
| ASSUMPTION | RK3588↔Pixhawk 이더넷 물리링크 안정 |
| DEFERRED | 영상 GCS 전송용 보조 IP 링크(SiK 저대역 한계) |
| DEFERRED | 시각 동기 PPS 정밀화 |
