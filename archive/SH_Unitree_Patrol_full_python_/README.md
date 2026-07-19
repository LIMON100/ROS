# SkyHunter Swarm Platform

[![CI](https://github.com/adasone/SH_Unitree_Patrol/actions/workflows/ci.yml/badge.svg)](https://github.com/adasone/SH_Unitree_Patrol/actions/workflows/ci.yml)
[![ARM64 build](https://github.com/adasone/SH_Unitree_Patrol/actions/workflows/arm64.yml/badge.svg)](https://github.com/adasone/SH_Unitree_Patrol/actions/workflows/arm64.yml)
[![Lint](https://github.com/adasone/SH_Unitree_Patrol/actions/workflows/lint.yml/badge.svg)](https://github.com/adasone/SH_Unitree_Patrol/actions/workflows/lint.yml)
![Tests](https://img.shields.io/badge/tests-270%20passed-brightgreen)

**Hardware target**: Unitree Go2 EDU + RK3588 (Orange Pi 5 Plus / Khadas Edge2 등)
**Mission focus**: 군집제어 소형전술 로봇 (swarm-controlled small tactical robot) — recon / formation move / area sweep
**Architecture**: SDD Rev.A.5 — leader / follower / hub 3-role topology, 5-Tier escape FSM
**OS**: Ubuntu 22.04 (ARM64)

---

## 1. 설계 철학

| 원칙 | 적용 |
|---|---|
| **저수준 제어는 Go2에 위임** | 1kHz 모터/균형/gait는 Go2 EDU 내장. RK3588은 지능 layer만. |
| **점군은 numpy + SharedMemory** | mp.Queue + pickle 경로 회피, 50ms → 0.1ms |
| **CPU big.LITTLE 인지** | A76(4-7) = RT/compute, A55(0-3) = I/O |
| **NPU는 perception 전용** | RKNN으로 YOLOv5 추론, CPU 부담 격리 |
| **외부 라이브러리 활용** | Point-LIO (SLAM), Nav2 (waypoint), GStreamer (스트리밍) |
| **Always-on BLE control plane** | WiFi/스트리밍 별개 — BLE는 항상 켜져 있는 control 채널 (AIRYS SAN-BLE-WIFI-001) |
| **5-Tier 군집 복구** | T0 predictive → T4 breadcrumb 자동 강등, follower 이탈 회복 |
| **Hub UGV 이중화** | 리더 통신 단절 시 hub가 leadership takeover (SDD §6.7) |
| **3-layer cost map** | OSM 정적 + SLAM Bayesian persistent (α=0.95) + anomaly detection (SDD §4.7) |

---

## 2. 프로세스 / 코어 매핑

`main.py`는 **17개 프로세스**를 spawn하고, hub 노드일 때 `HubUgvAdapter` 1개를 추가로 기동한다 (`system.robot_role: hub`).

### 2.1 Hardware adapters (8)

| Process | 역할 | Core (RK3588) | RT prio |
|---|---|---|---|
| **UnitreeGo2Adapter** | Go2 EDU SDK ↔ IPC, LiDAR/IMU/Camera/Status, /goal_pose, /cmd_vel | A76 #4 | 50 |
| **RtkGnssAdapter** | u-blox NMEA/RTCM 파싱, RTK Fixed/Float quality tier | A76 #5 | normal |
| **NtripClientAdapter** | NTRIP caster 연결, RTCM3 corrections → RtkGnssAdapter | A55 #0 | normal |
| **LteModemAdapter** | AT 명령 기반 RSRP/RSRQ/quota 모니터링 | A55 #0 | normal |
| **ExternalImuAdapter** | 200Hz 외장 IMU (보조 fallback) | A76 #5 | 40 |
| **LrfAdapter** | Laser range finder 5Hz | A55 #0 | normal |
| **IMX678Adapter** | 4K RGB camera (h265 인코딩, SHM fanout) | A55 #3 | normal |
| **ThermalCameraAdapter** | 열화상 카메라 9 fps, RGB와 융합 | A55 #3 | normal |

### 2.2 State estimation & mapping (4)

| Process | 역할 | Core | RT prio |
|---|---|---|---|
| **LocalizationProcess** | RTK + IMU + dead-reckoning fallback, 100Hz /pose | A76 #5 | 30 |
| **SLAMBridgeProcess** | LiDAR SHM accumulator → Point-LIO bridge | A76 #6,7 | normal |
| **SharedMapReceiverProcess** | follower↔hub 분산 SLAM map 청크 수신 | A55 #1 | normal |
| **MapFusionProcess** | OSM 정적 + SLAM persistent (Bayesian α=0.95) + anomaly | A55 #1 | normal |

### 2.3 Mission, perception, swarm (4)

| Process | 역할 | Core | RT prio |
|---|---|---|---|
| **PerceptionProcess** | YOLOv5 (NPU 3 cores), thermal/RGB fusion | A55 #3 + NPU | normal |
| **MissionProcess** | Behavior Tree, recon/formation/area_sweep dispatch, 5Hz | A55 #2 | normal |
| **SwarmBridgeProcess** | TierManager + breadcrumbs + leader rollback | A55 #2 | normal |
| *(hub-only)* **HubUgvAdapter** | N개 follower SLAM 청크 fuse, 리더십 takeover | A55 #1 | normal |

### 2.4 Comm, safety (2)

| Process | 역할 | Core | RT prio |
|---|---|---|---|
| **CommProcess** | 엣지 필터 + 서버 업로드 + LTE/WiFi failover | A55 #0 | normal |
| **SafetyProcess** | E1~E5 감시 (fall, comm loss, …) | A55 #0 | normal |

### 2.5 Control plane (`control/` — BLE/WiFi 7-Phase)

본 모듈은 SDD §3.2 / AIRYS SAN-BLE-WIFI-001을 따르며, `main.py` 외부에서도 단독 기동 가능. BLE는 항상 켜져 있고, WiFi/스트리밍은 BLE 명령(WIFI_ON/OFF)으로만 켜고 끈다.

```
BOOT → BLE_ADV → BLE_CONN → WIFI_BRINGUP → WIFI_READY → STREAMING → TEARDOWN
                                                                       │
                                                                       └→ BLE_ADV
```

| Process | 역할 |
|---|---|
| **OrchestratorProcess** | 7-Phase FSM 진행 / 명령 dispatch |
| **BleControlProcess** | BLE GATT 서버, opcode 핸들링 |
| **WifiControlProcess** | hostapd + dnsmasq 라이프사이클 |
| **WsTelemetryProcess** | 운용 앱 ws:// 텔레메트리 / 알람 |
| **HttpRecordingsProcess** | 녹화본 카탈로그 + HTTP serve |
| **DisplayProcess** | dev 모드 KMS 디스플레이 |
| *(streaming)* **StreamingProcess** | `gst-launch-1.0` 서브프로세스로 H.265 UDP/SRT 파이프라인 관리 |

---

## 3. 데이터 흐름

```
Go2 EDU                           RK3588
──────                            ──────
  4D LiDAR ─┐                     ┌─→ Localization ─→ /pose ─┐
  IMU      ─┼─DDS/Eth─→ UnitreeGo2Adapter                     ├─→ Mission ─→ /goal_pose ─┐
  Camera   ─┘    │      │  ├─ SHM(LiDAR) → SLAMBridge → /cumulative ─┐         │         │
  Sport state ───┘      │                                              ↓         │         │
                        │                                          MapFusion     │         │
External payload                                                       ↑         │         │
  RTK + NTRIP ─→ RtkGnss/NtripClient ─→ /rtk_fix ─→ Localization      │         │         │
  LTE modem  ─→ LteModem        ─→ /lte_status ─→ Comm (failover)     │         │         │
  Ext IMU    ─→ ExternalImu     ─→ /imu_ext ─→ Localization (fallback)│         │         │
  LRF        ─→ Lrf             ─→ /range ─→ Safety                   │         │         │
  IMX678     ─→ IMX678(SHM)     ─┐                                    │         │         │
  Thermal    ─→ Thermal(SHM)    ─┴→ Perception(NPU) ─→ /anomaly ─→ Comm ─→ 서버 │         │
                                                                                  │         │
OsmStaticLayer ─→ MapFusion (cell cost lookup) ─→ /fused_tile ────────────────────┘         │
                                                                                            │
SharedMapReceiver ──→ HubUgvAdapter (hub-only fuse) ─→ /shared_map ─→ followers            │
SwarmBridge ───→ /follower_target ─→ TierManager ─→ Mission                                │
                                                              ←──────────────── Adapter ←──┘
                                                              cmd_vel / sport command

Control plane  ──  BLE always-on ──  Orchestrator ↔ {Wifi, Streaming, Recordings, Telemetry}
```

---

## 4. 디렉토리 구조

```
SH_Unitree_Patrol/
├── main.py                        # 진입점 (17+1 프로세스 기동)
├── requirements.txt               # 공통 (dev/CI)
├── requirements-arm64.txt         # RK3588 device only (rknn, cyclonedds, ...)
├── README.md
│
├── config/
│   ├── system.yaml                # CPU 매핑, robot_role, RTK/NTRIP, LTE, perception, ...
│   ├── tactical_missions.yaml     # recon / formation_move / area_sweep 정의
│   └── patrol_routes.yaml         # 레거시 routine patrol 경로
│
├── core/                          # 공통 인프라
│   ├── messages.py                # numpy 기반 메시지 (slots dataclass) + Tier 상수
│   ├── shm_pool.py                # SharedMemory ring buffer + reaper
│   ├── ipc.py                     # TopicQueues + per-topic sizing
│   ├── base_process.py            # CPU affinity + RT prio + lifecycle + diag
│   ├── diag.py                    # per-process logger, crash dump, schema check
│   └── config.py                  # YAML loader (env override + validate_required)
│
├── adapters/                      # Hardware ↔ IPC 브리지
│   ├── unitree_go2.py             # Go2 EDU SDK (★핵심)
│   ├── rtk_gnss.py                # u-blox NMEA + RTCM tier
│   ├── ntrip_client.py            # NTRIP caster client
│   ├── ntrip_process.py           # NTRIP을 BaseProcess로 래핑
│   ├── lte_modem.py               # AT command modem
│   ├── payload_sensors.py         # ExtImu / Lrf / IMX678 / Thermal
│   └── hub_ugv.py                 # Hub UGV (follower SLAM fuse, leader takeover)
│
├── localization/
│   ├── localization_process.py    # 100Hz pose publish (RTK + IMU + DR fallback)
│   └── dead_reckoning.py          # RTK 손실시 odom drift 추정
│
├── mapping/
│   ├── osm_static_layer.py        # L1 OSM 정적 레이어 (20cm raster, SDD §4.7.1)
│   ├── slam_persistent_layer.py   # L2 Bayesian persistent (α=0.95, SDD §4.7.5)
│   ├── cumulative_map.py          # 시간 인지 누적, 4계층 자동 분류
│   ├── shared_map_receiver.py     # follower↔hub map chunk RX
│   └── processes.py               # SLAMBridge / MapFusion (OSM+SLAM 통합)
│
├── perception/
│   ├── rknn_inference.py          # RKNN NPU wrapper (3 cores)
│   ├── thermal_rgb_fusion.py      # 열화상 ↔ RGB 정합
│   └── perception_process.py      # YOLOv5 추론 (COCO 80-class, classes_of_interest 필터)
│
├── mission/
│   ├── behavior_tree.py           # 최소 BT 구현
│   ├── patrol_planner.py          # YAML 경로 + 스케줄
│   ├── predictive_planner.py      # 1초 예측 leader path (Rev.A.5 §7.6)
│   └── mission_process.py         # 전술 mission BT (★recon/formation/sweep)
│
├── swarm/                         # SDD Rev.A.5 §6.7, §7.3, §7.6
│   ├── tier_manager.py            # 5-Tier escape FSM (T0 → T4 + 히스테리시스)
│   ├── breadcrumb.py              # leader path breadcrumb buffer
│   ├── leader_rollback.py         # leadership takeover decision
│   └── swarm_bridge.py            # SwarmBridgeProcess (3개 통합)
│
├── control/                       # BLE/WiFi 7-Phase orchestration
│   ├── state_machine.py           # ConnectionFsm (BOOT → … → STREAMING)
│   ├── orchestrator_process.py
│   ├── ble_control_process.py
│   ├── wifi_control_process.py
│   ├── ws_telemetry_process.py
│   ├── http_recordings_process.py
│   └── display_process.py
│
├── streaming/
│   ├── streaming_process.py       # gst-launch UDP/SRT 파이프라인 lifecycle
│   ├── nv12_pool.py               # AI consumer NV12 SHM pool
│   ├── include/, cpp/             # 네이티브 헬퍼
│
├── comm/
│   └── comm_process.py            # 엣지 필터 + 업로드 + WiFi/LTE failover
│
├── safety/
│   └── safety_process.py          # E1~E5 감시
│
├── scripts/                       # 검증/유틸
│   ├── debug_dashboard.py         # 라이브 메트릭 시각화 (cat /tmp/patrol-metrics.json)
│   ├── debug_smoke.sh
│   ├── hw_poc.py
│   ├── queue_inspect.py
│   ├── verify_arm64_build.sh
│   └── build_unitree_stack_rk3588.sh
│
└── tests/                         # pytest (270 tests, ~45s on CI)
```

---

## 5. RK3588 최적화 적용

### 5.1 CPU 친화도 (BaseProcess)
```python
class BaseProcess:
    def _apply_cpu_affinity(self):
        os.sched_setaffinity(0, set(self.cpu_affinity))   # A76 또는 A55
```

### 5.2 RT 스케줄링
```python
def _apply_rt_priority(self):
    os.sched_setscheduler(0, os.SCHED_FIFO, os.sched_param(self.rt_priority))
```

### 5.3 SharedMemory zero-copy + reaper
```python
# 생산자 (UnitreeGo2Adapter / IMX678Adapter / Thermal)
slot = lidar_shm.acquire()
view = ShmPool.view_array(slot, (n, 4), np.float32)
view[:] = points
publish(queues.lidar_ref, LidarScanRef(shm_name=slot.name, n_points=n))

# 소비자 (SLAMBridge / Perception)
shm = ShmPool.attach(ref.shm_name)
arr = ShmPool.view_array(shm, (ref.n_points, 4), np.float32)
... use arr ...
lidar_shm.release(ref.shm_name)
# 소비자 미수신 시 reaper 스레드가 만료된 슬롯을 회수 — leak window 봉인
```

### 5.4 NPU 활용 (RKNN, 3 cores)
```python
detector = RknnRunner(model_path="models/yolov5s-640-640_rk3588_*.rknn", core="auto")
# COCO 80-class 추론, classes_of_interest = [person, bicycle, car, …]만 후처리
```

### 5.5 numpy vectorization
```python
# SLAM persistent layer Bayesian update (mapping/slam_persistent_layer.py)
self.grid = (alpha * self.grid + (1 - alpha) * observed).clip(0.0, 1.0)
```

---

## 6. 실행

### 6.1 RK3588 device
```bash
# 시스템 의존성
sudo apt install python3-numpy python3-yaml libcyclonedds-dev hostapd dnsmasq \
                 gstreamer1.0-tools gstreamer1.0-plugins-{good,bad,ugly}

# Python 의존성 (공통 + ARM64 device-only)
pip install -r requirements.txt
pip install -r requirements-arm64.txt
# Unitree SDK는 별도 빌드: github.com/unitreerobotics/unitree_sdk2_python

# RT 권한
sudo setcap 'cap_sys_nice=eip' /usr/bin/python3.10

# 실행 (모든 진단 기능 기본 ON)
python3 main.py --config config/system.yaml

# 라이브 대시보드 (별도 터미널)
python3 scripts/debug_dashboard.py
```

### 6.2 개발 머신 (stub 모드)
```bash
pip install -r requirements.txt
python3 main.py --config config/system.yaml
# Unitree SDK / NPU / GNSS 등이 없으면 자동으로 stub 데이터 생성
```

### 6.3 진단 / 디버그 옵션
| 플래그 | 설명 |
|---|---|
| `--log-dir /var/log/patrol` | 프로세스별 회전 로그 (50 MB × 5) |
| `--crash-dir /var/log/patrol/crashes` | unhandled exception / SIGUSR1 → JSON dump |
| `--metrics-file /tmp/patrol-metrics.json` | 1초 주기 라이브 메트릭 (대시보드용) |
| `--log-json` | one-JSON-per-line 로그 |
| `--profile` | 모든 프로세스 cProfile (5~15% CPU 오버헤드) |
| `--hang-timeout-s 5` | step()이 임계치 초과시 자동 crash dump |
| `--schema-check` | 발행 메시지 dataclass 필드 검증 (느림, 디버그용) |

`kill -USR1 <pid>` → 해당 프로세스 상태 풀덤프.

---

## 7. 전술 mission 동작 흐름

`config/tactical_missions.yaml` 의 mission `type` 별로 BT 가지가 달라진다:

```
recon (단독/소그룹 covert):
  Sequence(
    Cond(pose available?), Cond(battery >= 60%?),
    Foreach(waypoint):
       NavigateToWaypoint  (silent, no map broadcast)
       DwellAndObserve     (perception → /anomaly → ws telemetry)
  )

formation_move (leader-follower):
  PredictivePlanner → /follower_target ─┐
  TierManager(δ) ─→ T0..T4 의사결정      ├→ Mission → /goal_pose
  BreadcrumbBuffer (T4 fallback) ──────┘

area_sweep (그리드 커버리지):
  Polygon → boustrophedon track 생성 (track_spacing_m)
  → recon 와 동일 dwell/observe 반복
```

이탈/통신 손실 시 `LeaderRollbackChecker`가 hub takeover 결정 → `HubUgvAdapter`가 leadership 인계.

---

## 8. 구성 핵심 (`config/system.yaml`)

| 섹션 | 키 | 메모 |
|---|---|---|
| `system.robot_role` | `leader` / `follower` / `hub` | hub일 때만 `HubUgvAdapter` 기동 |
| `system.cpu_affinity` | 프로세스 → CPU 코어 | A76(4-7) / A55(0-3) |
| `go2.*` | Go2 SDK DDS topic / IP | `eth0`, `192.168.123.x` |
| `rtk.*` + `rtk.ntrip.*` | u-blox device + NTRIP caster | 한국 NGII VRS 기본값 |
| `shm.lidar_*` / `shm.camera_*` | SharedMemory 슬롯 크기/개수 | 16 MB×8 / 4 MB×16 |
| `perception.*` | YOLOv5 모델 경로, classes_of_interest | COCO 인덱스 (person, vehicle, …) |
| `mission.routes_file` | `tactical_missions.yaml` | 전술 mission 정의 |
| `comm.enable_patrol_server` | bool | false면 ws telemetry로 우회 (Phase F까지) |
| `safety.*` | comm_loss_timeout_s, fall_detect_acc_g | E1~E5 임계값 |

---

## 9. 다음 단계

| Priority | 작업 | 기간 |
|---|---|---|
| P1 | Go2 EDU + RK3588 실 연결 PoC (LiDAR 수신 + waypoint 송신) | 1~2주 |
| P1 | Point-LIO 통합 (SLAMBridge에서 외부 노드 구독) | 2주 |
| P1 | YOLOv5 RKNN 운용 모델 검증 + classes_of_interest 튜닝 | 2주 |
| P1 | NTRIP RTK Fixed 안정화 + dead-reckoning fallback PoC | 1주 |
| P2 | 5-Tier 실차 검증 (T0→T4 시나리오) | 2주 |
| P2 | Hub UGV leadership takeover 시연 | 2주 |
| P2 | 충전 도크 자동 도킹 | 1주 |
| P2 | Patrol Server (`comm.enable_patrol_server=true`) + 대시보드 | 3주 |
| P3 | 다대 운용 (Open-RMF) | 4주+ |
