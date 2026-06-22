# SkyHunter Docker 네트워크 / 격리 정책

**위치**: `infra/docker/`
**관련 통제**: DCN-2026-004 D-009 (C-9 — host network 보안 모델 문서화)
**발행일**: 2026-05-13
**적용 대상**: SkyHunter v1.5.1 이후

---

## 1. 현재 정책: `network_mode: host`

`infra/docker/sbc1/docker-compose.yml` 및 `infra/docker/sbc2/docker-compose.yml` 의 모든 서비스는 `network_mode: host` 로 설정되어 있다 (9개 컨테이너 합계 — 자세한 위치는 §4).

```yaml
services:
  san_hub_comm:
    network_mode: host       # ← 본 정책
    environment:
      - ROS_DOMAIN_ID=42
```

호스트 네트워크 모드는 컨테이너가 호스트의 네트워크 스택을 직접 공유하므로 **컨테이너 ↔ 호스트 ↔ 외부 네트워크 사이의 네트워크 격리가 없다**.

---

## 2. 이 선택의 이유 (의도된 trade-off)

| 요인 | 호스트 네트워크 선택 사유 |
|---|---|
| **DDS multicast 발견** | CycloneDDS 의 SPDP (Simple Participant Discovery Protocol) 가 multicast 를 사용. Docker 의 기본 bridge 모드에서는 multicast 가 격리됨. host 모드만이 NetworkInterface eth0/wlan0 양쪽에서 DDS 가 정상 동작 |
| **LTE/Wi-Fi 6 mesh 직접 접근** | LTE PPP 인터페이스 (ppp0) 와 Wi-Fi 6 mesh (wlan0) 가 동적 IP/route 변경을 수반. Docker bridge 의 NAT 가 이 동적 라우팅 (mwan3) 과 충돌 |
| **GStreamer SRT/UDP** | 영상 relay 가 UDP/SRT 로 외부 운영자 태블릿과 직접 통신. port forwarding 보다 host 모드가 단순/안정적 |
| **NTRIP 클라이언트** | RTK 보정 데이터 수신을 위한 NTRIP HTTP 요청. bridge 모드에서도 가능하나 RTCM packet의 timing jitter 가 증가 |
| **저지연 mesh routing** | 분대 단위 mesh 노드 간 L2/L3 routing 이 분당 단위로 변할 수 있음 (DAWN/802.11s). host 모드만이 OpenWrt 의 routing 변화를 컨테이너에 즉시 반영 |
| **RK3588 디바이스 노드 접근** | `/dev/dri`, `/dev/mpp_service`, `/dev/rknn` 등 NPU/VPU/GPU 디바이스 접근. host 모드가 권장 (devices: 매핑은 가능하나 검증 부담 큼) |

---

## 3. 위협 모델 (Threat Model)

`network_mode: host` 의 보안 영향:

### 3.1 의도된 노출 (운용 범위 내)

| 노출 | 평가 | 완화 |
|---|---|---|
| 컨테이너가 호스트의 모든 인터페이스에 listen 가능 | **수용** | 각 컨테이너 application 이 listen port 명시 (binding IP 제한 가능) |
| 컨테이너 process 가 host 의 다른 process 와 port 충돌 가능 | **수용** | 각 service 의 port 가 사업 IDS §7 통신표에 명시되어 중복 없음 |
| 컨테이너가 host 의 DDS 트래픽 가시 가능 | **수용** | DDS 자체에 인증 / 암호화 미적용, ROS 2 SROS 미사용 (v1.6 후속 작업) |

### 3.2 비의도된 노출 (수용 불가)

| 노출 | 평가 | 완화 / 통제 |
|---|---|---|
| 컨테이너가 host 의 모든 인터페이스의 트래픽을 sniff/spoof 가능 | **고위험** | 컨테이너 image 가 신뢰된 빌드 (CI 빌드만 deploy). `apt install` 으로 임의 패키지 설치 불가 (image 가 read-only) |
| 컨테이너가 host 의 ssh / web 서비스 port 점유 가능 | **중위험** | host 의 ssh (port 22) / web 관리 (80/443) 는 외부 firewall (OpenWrt) 에서 mesh 측면 차단. 컨테이너가 port 22 binding 시도 시 EADDRINUSE 로 즉시 실패 |
| 컨테이너가 host 의 LTE/Wi-Fi 설정을 직접 변경 가능 | **고위험** | 컨테이너에 CAP_NET_ADMIN 명시 부여한 service 만 (san_lte_redundancy). 나머지는 cap_drop: ALL |
| 컨테이너 escape → host 권한 획득 | **고위험** | host 격리 손실. 본 시스템은 **물리적으로 격리된 SBC** (방산용 군용 차량 내부, ssh 외부 접근 차단) 라 격리 손실의 운용 영향 제한적 |

### 3.3 SBOM / Image trust

| 항목 | 정책 |
|---|---|
| Base image | `ros:humble-ros-base` (Open Robotics 공식, GPG 검증) |
| 빌드 위치 | GitHub Actions CI (PR merge 시 자동 빌드) |
| Image registry | 사내 registry, push 권한은 PM + Tech Lead 만 |
| Image scan | `trivy` (CI step) — HIGH / CRITICAL CVE 발견 시 build fail |
| 운영 환경 image rotation | 매 sprint (~2주) — base image 의 CVE patch 반영 |

---

## 4. 대안 검토 — 왜 채택하지 않았는가

### 4.1 Bridge mode + port forwarding

```yaml
ports:
  - "8888:8888"
  - "5000-5008:5000-5008/udp"
```

**거부 이유**:
- DDS multicast 가 작동하지 않아 ROS 2 discovery 불가 (CycloneDDS 의 SPDP 가 multicast 의존)
- 정적 port mapping 이라 Wi-Fi mesh 의 동적 peer 추가 (DAWN/802.11s roaming) 수용 불가
- Docker NAT 가 LTE PPP 의 동적 IP 변경 (mwan3 fail-over) 시 ~30초 통신 단절

### 4.2 Macvlan / IPvlan

```yaml
network_mode: macvlan
```

**거부 이유**:
- Multicast 가 LAN 안에서 동작하나 wlan0 의 IPv6 link-local 가 깨짐 (mesh 측면 통신 불가)
- 일부 BSP 의 mali GPU driver 가 macvlan 환경에서 framebuffer 접근 불안정

### 4.3 SROS 2 (Secure ROS) + bridge

**거부 이유**:
- SROS 2 의 DDS-Security (인증서 기반) 가 multicast 와 호환되지 않음 (필수 unicast)
- 보드 부팅 시 인증서 chain 검증 (~3초) 추가 — 작전 투입 즉시성 영향
- v1.6 후속 작업으로 별도 평가 예정

---

## 5. 운영 통제

| # | 통제 | 책임자 | 빈도 |
|---|---|---|---|
| OC-1 | 컨테이너 image scan (trivy) | DevOps | CI 매 PR + 매주 |
| OC-2 | 컨테이너 base image rotation | DevOps | 매 sprint (~2주) |
| OC-3 | CAP/SECCOMP profile 점검 | Tech Lead | 분기별 |
| OC-4 | host firewall (iptables/nftables) rule 점검 | OpenWrt admin | 분기별 |
| OC-5 | 컨테이너 process 외 host process 모니터링 | Ops | 상시 (auditd) |
| OC-6 | network_mode 변경 시 IRB 승인 | PM | 발생 시 |

---

## 6. 본 정책의 적용 위치 (참조용)

| 파일 | 호스트 모드 컨테이너 수 |
|---|---|
| `infra/docker/sbc1/docker-compose.yml` | 4 (lines 15, 30, 45, 61) |
| `infra/docker/sbc2/docker-compose.yml` | 5 (lines 16, 30, 49, 64, 87) |

각 컨테이너의 역할은 사업 IDS §7 통신표 참조.

---

## 7. 변경 이력

| Rev | 일자 | 변경 | 발행자 |
|---|---|---|---|
| 1.0 | 2026-05-13 | 초판 (DCN-2026-004 D-009 발행) | PM |

— 끝 —
