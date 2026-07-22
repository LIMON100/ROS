# Hub UGV 듀얼 SBC 운용 가이드 (SAN v1.3 PHASE 4)

> Source: SAN-SDD-SWARM-001 v1.3 §3.4, SDD-SUR §4, TST §S15-3

Hub UGV (S2) 는 두 개의 SBC 를 탑재합니다. 한쪽 장애 시에도 임무가
계속되도록 노드 분담이 강제되어 있고, `HubHealthMonitor` 가
`/swarm/robot_status` 의 `sbc1_healthy` / `sbc2_healthy` 를 1 Hz 로
관찰해 deputy chain 포함 여부를 결정합니다.

## 1. 하드웨어 구성

| 장치 | 사양 | 인터페이스 |
|---|---|---|
| **SBC #1** (SLAM 전담) | RK3588J 16 GB / 256 GB NVMe (또는 Jetson Orin) | 내부 GbE (`eth0`) |
| **SBC #2** (Comm / Video / LTE) | RK3588J 8 GB / 128 GB NVMe | 내부 GbE (`eth0`) + Wi-Fi 6 mesh (`wlan0`) + LTE (`lte0`) |
| **내부 GbE 직결** | 짧은 cat6 패치 | 양쪽 `eth0` 직결 |
| **DDS 도메인** | `ROS_DOMAIN_ID=42` | `CycloneDDS` 양쪽에서 동일 |

## 2. 노드 분담

### SBC #1 (`infra/docker/sbc1/docker-compose.yml`)

| Container | 패키지 | 역할 |
|---|---|---|
| `san_hub_slam` | `san_hub_slam` (PHASE 3) | 8 robot delta merge + 5 s aggregation |
| `san_lidar_sbc1` | `san_lidar` (PHASE 1) | Hub 자체 Robosense E1 driver |
| `san_costmap_sbc1` | `san_costmap` (PHASE 1) | Hub local cost map |
| `san_local_slam_sbc1` | `san_slam` (PHASE 3) | Hub local SLAM delta producer |

### SBC #2 (`infra/docker/sbc2/docker-compose.yml`)

| Container | 패키지 | 역할 |
|---|---|---|
| `san_hub_comm` | `san_hub_comm` (PHASE 5) | GStreamer SRT relay |
| `san_lte_redundancy` | `san_lte_redundancy` (PHASE 2) | LTE 1차 게이트웨이 + Mwan3 |
| `san_operation_ctrl` | `san_operation_control` (PHASE 7) | DEMO sequencer + watchdog |
| `san_role_mgmt` | `san_role_management` (PHASE 8) | Hub-Deputy + 4-tier + Limp |

## 3. CycloneDDS NIC binding

- `infra/docker/cyclonedds/cyclonedds_sbc1.xml` — `eth0` 만 (내부 GbE 만)
- `infra/docker/cyclonedds/cyclonedds_sbc2.xml` — `eth0` (default) + `wlan0` (priority 50)

이렇게 priority 를 분리하면 내부 토픽은 GbE 로 흐르고 외부 mesh 트래픽만
Wi-Fi 6 로 흐릅니다.

## 4. 부분 운용 시나리오 (TST §S15-3)

### Case A — SBC #1 장애 (SLAM 정전)

| 항목 | 동작 |
|---|---|
| 글로벌 SLAM aggregation | ❌ 중단 |
| Local SLAM (각 follower) | ✅ 계속 (1 Hz) |
| Cost map | ✅ 계속 (1 Hz / 10 Hz publish) |
| LTE 게이트웨이 | ✅ 정상 (SBC #2) |
| GStreamer 영상 | ✅ 정상 |
| Leader 승계 chain | Hub 유지 (SBC #2 alive) |
| Operator banner | "Hub SLAM SBC 장애 — 글로벌 맵 갱신 중단" |
| 임무 | ✅ 계속 |

`RobotStatus.sbc1_healthy = false` 가 broadcast 되고 `HubHealthMonitor::classify()` 가 `CASE_A` 를 반환합니다.

### Case B — SBC #2 장애 (Comm/Video/LTE)

| 항목 | 동작 |
|---|---|
| LTE 외부 통제 | ❌ → PHASE 2 LTE 백업 자동 활성 (S3 promote) |
| GStreamer relay | ❌ → mesh 로만 영상 시청 |
| 내부 mesh routing | 다른 Wi-Fi 6 peer 로 보조 |
| 글로벌 SLAM | ✅ 정상 (SBC #1 alive) |
| Leader 승계 chain | Hub 유지 (SBC #1 alive) |
| Operator banner | "Hub 통신 SBC 장애 — LTE 백업 활성화 (S3)" |
| 임무 | ✅ 계속 |

### Case C — 양쪽 SBC 장애 (Hub UGV 전체 손상)

| 항목 | 동작 |
|---|---|
| Hub 전부 기능 | ❌ |
| Leader 승계 chain | **Hub 제외** (`hubExcludedFromLeaderChain() == true`) |
| Deputy UGV (S3) | v1.4 PHASE 8 의 4-tier 정책에 따라 Leader 승계 1순위로 승격 |
| Limp Mode | Hub + Deputy 모두 불능 시 PHASE 8 LimpModeManager 가 진입 |
| Operator alert | "Hub UGV 전체 손상 — Leader 승계 대기 / Deputy 활성" |

## 5. 빌드 + 배포

### SBC #1

```bash
cd infra/docker/sbc1
docker compose build           # Multi-stage build (~10-15 min on RK3588J)
docker compose up -d
docker compose logs -f san_hub_slam
```

### SBC #2

```bash
cd infra/docker/sbc2
docker compose build
docker compose up -d
docker compose logs -f san_lte_redundancy
```

### 검증

```bash
# 1) 양쪽 SBC 에서 동일한 토픽이 보여야 함 (eth0 internal share)
ssh sbc1 'docker exec san_hub_slam ros2 topic list'
ssh sbc2 'docker exec san_hub_comm ros2 topic list'

# 2) HubHealthMonitor 분류
ros2 topic echo /swarm/robot_status --field "[sbc1_healthy, sbc2_healthy]"
```

## 6. 운영 정책

- **양쪽 SBC 동시 재부팅 금지** — 운용 중 reboot 은 항상 한쪽씩, 1 분 간격
- **재시작 정책** — `restart: unless-stopped` (Docker 자동 복구)
- **로그 위치** — `/var/log/san/*.log` (양쪽 SBC bind-mount)
- **NIC binding 검증** — 부팅 직후 `ros2 topic hz /swarm/robot_status` 가 양쪽에서 동일 cadence 이어야 함

## 7. 후속 작업

- PHASE 5 GStreamer relay 통합 (`san_hub_comm` 컨테이너 활성화)
- PHASE 8 LimpModeManager 와 `HubHealthMonitor::classify() == CASE_C` 연동 (`san_role_management` 가 별도로 `/swarm/hub/role_announce` 발행)
