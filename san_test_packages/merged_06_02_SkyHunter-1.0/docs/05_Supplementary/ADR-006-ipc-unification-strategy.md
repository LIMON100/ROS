# ADR-006 — IPC Unification on ROS 2 (DDS) as the SAN Standard Communication Substrate

> **Status**: Accepted
> **Date**: 2026-05-12
> **Decider**: 김태근 대표이사 (㈜스카이오토넷)
> **Consulted**: Phase 2 작업 진행 중 발견된 코드 ↔ 권원 분절 보고
> **Supersedes**: 없음 (신규 ADR)
> **Related**: DCN-2026-002 (이 ADR 의 governance 측 동반 문서)

---

## 1. 컨텍스트 (Context)

### 1.1 현 상태의 문제 (Problem Statement)

SkyHunter v1.5 의 SH_Unitree_Patrol 코드베이스 분석에서 다음을 발견:

#### 1.1.1 IPC 메커니즘 3-way 분절

`core/ipc.py` 의 첫 7 줄이 자백:
```python
"""
Inter-process communication.

Two channels:
  • mp.Queue: small messages (pose, status, commands, refs)
  • SharedMemory pool: large data (point clouds, encoded video)
"""
```

실제 codebase 정밀 분석 결과 **3 가지** IPC 메커니즘이 혼용 중:

| # | 메커니즘 | 사용처 | 토픽 수 |
|---|---|---|---|
| 1 | `multiprocessing.Queue` | pose, twist, robot_status, commands, refs | ~40 |
| 2 | `ShmPool` (multiprocessing.shared_memory) | lidar point cloud, IMX678 4K 카메라, thermal | 4 |
| 3 | `multiprocessing.Manager().dict()` | auth_state_proxy, shared config flags | 1 |

각 메커니즘은 서로 호환되지 않으며, 새 노드 추가 시 `main.py` 의
process 생성자 인자를 모두 수정해야 한다 (`UnitreeGo2Adapter(queues,
shutdown, cfg, lidar_shm, camera_shm, **diag)` 등).

#### 1.1.2 ROS 2 미사용

```bash
$ grep -rn "^from rclpy\|^import rclpy" --include="*.py" .
(0 matches)
```

전체 codebase 에서 `rclpy` 사용 **0건**. 즉 ROS 2 권원 (IDS v1.5
§5 35+ 토픽 정의) 이 코드와 무의미한 mapping 상태.

#### 1.1.3 권원 정합 불가능 항목

| v1.5 권원 요구 | mp.Queue 기반에서 | 비고 |
|---|---|---|
| SDD-SWARM §5.5 LTE 이중화 (multi-host) | **불가능** | mp.Queue 는 host-local |
| SDD-SWARM §5.6 Leader 승계 (mesh broadcast) | **불가능** | mp.Queue 는 P2P |
| IDS v1.5 §5 권원 토픽 정의 | **무의미** | mp.Queue 는 토픽 개념 없음 |
| IDS v1.5 §7 QoS (RELIABLE/BEST_EFFORT 등) | **표현 수단 없음** | mp.Queue 는 maxsize 만 |
| 토픽 기반 회귀 시험 (`ros2 bag record`) | **불가능** | 외부 도구 사용 불가 |

#### 1.1.4 DCN-2026-001 D-007 의 잘못된 진단

원안 D-007 "Python 프로토타입 폐기" 는 위 문제의 **표면만 본 진단**.
실제 근본 원인은 IPC 의 표준 미준수이며, 언어 (Python vs C++) 는
부차적 문제.

근거:
- 동일한 IPC 구조를 C++ 로 전환해도 (자체 IPC 라이브러리 사용 시)
  ROS 2 권원 정합은 여전히 불가능
- 반대로 IPC 만 ROS 2 로 통일하면 Python 코드도 즉시 권원 정합 가능
  (Boston Dynamics Spot SDK, NASA JPL Mars 2020 등 동일 패턴)

### 1.2 외부 표준의 권고

| 표준 | 권고 |
|---|---|
| ROS 2 design.ros2.org | DDS 단일 backend, rclcpp/rclpy 동일 IPC |
| MIL-STD-882E (System Safety) | safety-critical zone 명확화 + IPC trust boundary |
| DO-178C (Airborne Software) | high-integrity zone 분리 (LRDC) |
| IEC 61508 (Functional Safety) | safety integrity level 별 격리 |
| 한화에어로스페이스 / 보스턴 다이내믹스 등 산업 사례 | hybrid 언어 + 단일 IPC |

모두 "**IPC 통일이 우선, 언어는 zone 별 선택**" 패턴 권고.

---

## 2. 결정 (Decision)

### 2.1 결정문 (One-Liner)

> **SkyHunter v1.5 의 모든 inter-process 통신은 ROS 2 (DDS) 로
> 통일한다. `multiprocessing.{Process, Queue, shared_memory, Manager}`
> 사용은 전면 금지하며, 신규 노드는 `rclcpp::Node` (C++) 또는
> `rclpy.node.Node` (Python) 만 사용한다.**

### 2.2 구체적 결정 사항

#### 결정 D-1: ROS 2 (DDS) 가 표준 IPC 백엔드

- **통신**: `rclcpp::Publisher` / `rclpy.Publisher` 단일 인터페이스
- **백엔드**: ROS 2 의 기본 DDS 구현 (Fast DDS / Cyclone DDS — 사업 표준은 Cyclone DDS, 결정론적 latency 우위)
- **분배**: 단일 호스트 / 다중 호스트 / mesh 모두 동일 코드로 동작

#### 결정 D-2: IPC ↔ 언어 분리

언어 선택은 IPC 와 독립:
- C++ 노드 ↔ Python 노드 자유 통신 (동일 DDS topic 공유)
- 단일 호스트 내 같은 process 에 C++ + Python 노드 composable
- 언어 선택은 모듈의 안전 등급 + 성능 요구 + 개발 속도 기준

#### 결정 D-3: 토픽 분류 표준

`<robot_id>_<scope>/<topic_name>` 패턴:
- `robot_1/swarm/leader_announce` — leader 승계 (P0)
- `robot_3/sensor/lidar_scan` — LiDAR raw 데이터 (sensor)
- `swarm/fire/authorization_request` — broadcast 발사 인가 요청 (P1)

- robot 별 namespace 분리 → 시뮬레이션 다중 robot 자연 분리
- scope 분리 (`swarm/`, `sensor/`, `state/`, `cmd/`) → rqt_graph 시각화 명료

#### 결정 D-4: QoS 강제 명시

모든 publisher/subscriber 는 QoS 명시:

| 토픽 등급 | QoS | 사용처 |
|---|---|---|
| P0 (안전 critical) | RELIABLE depth=10 + TRANSIENT_LOCAL | leader_announce, hub_announce, fire/* |
| P1 (제어 명령) | RELIABLE depth=5 | cmd_vel, mission/command |
| P2 (상태 보고) | BEST_EFFORT depth=1 | operation_state, robot_status |
| Sensor (대역폭) | BEST_EFFORT depth=2 | lidar_scan, camera/* |

기본값 (`rclcpp::QoS::SystemDefault`) 사용 **금지**. 모든 노드의 QoS 는
IDS v1.5 §7 권원 표에서 명시 인용.

#### 결정 D-5: Intra-process zero-copy 활용

같은 process 내 노드 간 통신은 `IntraProcessSetting::Enable` 사용:
- camera frame (4K Bayer ~ 20 MB) → ShmPool 대체
- LiDAR point cloud (~ 5 MB) → ShmPool 대체
- ROS 2 가 내부적으로 shared_ptr move 만 수행 → ShmPool 대비 동일 성능

#### 결정 D-6: Composable node container 도입

성능 요구 모듈은 `rclcpp_components::ComponentManager` 로 묶음:
- `san_perception_container`: imx678 → perception → mission 동일 process
- `san_safety_container`: limp_mode_manager + fire_authorization 동일 process

이로써:
- TCP loopback 없이 같은 process 메모리 공간 → 1μs 미만 latency
- 별 process 가 필요한 모듈만 분리 → systemd 관리 단순화

#### 결정 D-7: systemd + ROS 2 launch 의 역할 분담

| 책임 | 도구 |
|---|---|
| Process 생명주기 (restart on failure) | systemd unit |
| 노드 그룹 기동 (composable container) | ROS 2 launch (Python) |
| 노드 파라미터 주입 | launch arg + yaml |
| 환경 변수 / 시크릿 마운트 | systemd Environment / EnvironmentFile |

`main.py` 의 multiprocessing 부트스트랩 책임은 둘로 분리:
- 1. systemd: `san-squadron.service` (실패 재시작)
- 2. ROS 2 launch: `squadron.launch.py` (노드 그룹 기동)

#### 결정 D-8: deprecation 일정

| 단계 | 기간 | 행동 |
|---|---|---|
| Phase 2-E (v1.5) | 3개월 | rclpy/rclcpp 신규 작성, Python multiprocessing adapter 유지 |
| 회귀 검증 (v1.5 종료) | 1개월 | rclpy 노드와 Python adapter 동시 운용 비교 |
| Archive (v1.5.1) | 즉시 | 정합 완료 모듈의 Python adapter 를 `archive/` 로 이동 |
| 완전 삭제 (v1.6) | — | archive 디렉토리 git history 제외 삭제 |

---

## 3. 결과 (Consequences)

### 3.1 긍정적 결과 (Positive)

#### 3.1.1 권원 정합 즉시 달성

| 권원 항목 | 정합 효과 |
|---|---|
| SDD-SWARM §5.5 LTE 이중화 | DDS multi-domain → 즉시 정합 |
| SDD-SWARM §5.6 Leader 승계 | DDS broadcast → 즉시 정합 |
| IDS v1.5 §5 권원 토픽 | 35+ 토픽 모두 rclcpp/rclpy 로 1:1 매핑 가능 |
| IDS v1.5 §7 QoS | rclcpp::QoS 로 강제 명시 |
| SDD-SWARM §10.1.1 표준 툴체인 | 3-Tier 정합 |

#### 3.1.2 작업량 감소

전면 C++ 전환 vs IPC 통일 비교:

| 작업 | 전면 C++ | IPC 통일 (rclpy 허용) |
|---|---|---|
| HW 어댑터 (8개) | 16 turn | 16 turn (동일, Tier 1) |
| Mission / Perception | 20 turn | 0 (rclpy 유지) |
| Sensor 융합 | 8 turn | 4 turn (rclpy 옵션) |
| 시뮬 / 시험 | 10 turn | 5 turn (rclpy 전환만) |
| 회귀 검증 | 10 turn | 5 turn |
| **합계** | **~64 turn (6+개월)** | **~30 turn (3개월)** |

#### 3.1.3 표준 도구 즉시 사용 가능

ROS 2 통일 후 다음 도구가 즉시 사용 가능:

```bash
ros2 topic list                          # 모든 토픽 발견
ros2 topic echo /robot_1/swarm/leader_announce  # 실시간 관찰
ros2 topic hz /robot_3/sensor/lidar_scan        # 주기 측정
ros2 bag record -a                       # 모든 토픽 녹화 (사고 사후분석)
ros2 bag play <bag>                      # 회귀 시험 재현
rqt_graph                                # 노드 ↔ 토픽 시각화 (사업평가 자료)
rqt_plot /robot_2/state/twist            # 실시간 차트
rviz2                                    # 3D 시각화 (SLAM 결과 등)
```

기존 `mp.Queue` 기반에서는 모두 자체 작성 (`debug_dashboard.py`,
`/tmp/patrol-metrics.json` 폴링 등) 필요했음.

#### 3.1.4 안전 분석 명료성

- Tier 1 (C++, safety-critical) ↔ Tier 2/3 (rclpy 가능) 경계 = trust boundary
- 사고 분석 시 `ros2 bag` 으로 모든 통신 100% 재현 가능
- 평가위원 질의 대응: "왜 Python?" → "Tier 3, 안전-critical 아님"

#### 3.1.5 다중 host / mesh 자연 지원

- Leader Go2 (192.168.10.1) ↔ Hub UGV (192.168.10.2) ↔ Follower UGV (192.168.10.3~8)
- 모두 동일 ROS_DOMAIN_ID 로 자동 발견
- 코드 변경 0 — DDS 가 처리

#### 3.1.6 시뮬레이션 정합

- Gazebo / Ignition 의 표준 인터페이스 = ROS 2 topic
- 동일 노드 코드가 실기 / 시뮬 모두에서 동작
- 시뮬 시 robot 별 namespace 분리만 추가

### 3.2 부정적 결과 (Negative)

#### 3.2.1 DDS 메타데이터 오버헤드

DDS discovery 트래픽이 mp.Queue 보다 큼:
- 16-byte mp.Queue.put() vs ~100-byte DDS header
- 100 Hz 토픽 기준: 10 kB/s vs 8.4 kB/s 증가 (0.84 KB/s)
- LTE 60 Mbps 대역폭 대비 무시 가능 (0.014%)
- Mesh 토폴로지에서는 multicast 활용 → broadcast 토픽 대역폭 절감

#### 3.2.2 첫 publish latency

DDS discovery 가 publisher↔subscriber matching 에 ~50ms 소요:
- mp.Queue 는 0ms (선행 부모 process)
- 영향: 노드 기동 후 최초 메시지 50ms 지연
- 안전 critical: P0 토픽에 TRANSIENT_LOCAL durability 적용 → 늦게 join 한 subscriber 도 마지막 메시지 받음

#### 3.2.3 학습 곡선

- 기존 개발자가 `mp.Queue.put()` → `publisher_->publish()` 전환 학습 필요
- rclcpp API 가 mp 보다 복잡 (Node 라이프사이클, executor, callback group)
- 1주차에 가이드 문서 (developer playbook) 작성 권장

#### 3.2.4 Python rclpy 의 GIL 한계

- 단일 process 다중 rclpy 노드 → GIL 경합
- 완화: 노드별 process 분리 (composable container 사용 안 함)
- Tier 1 (성능 critical) 은 어차피 C++ → 영향 없음
- Tier 3 (perception) 은 GIL 가능 (NPU 추론 동안 다른 thread 동작 가능)

### 3.3 위험 (Risks)

#### 3.3.1 Migration 중 부분 정합 상태

Phase 2-E 작업 기간 동안 일부 노드는 rclpy/rclcpp 로 전환되고
일부는 mp.Queue 로 잔존 → 부분 정합 상태에서 회귀 발생 위험.

완화책:
- `compat/queue_to_topic_bridge.py` 작성 — mp.Queue ↔ ROS 2 topic 브릿지
- 노드 단위 전환, 각 전환 후 회귀 시험 통과 의무
- 부분 정합 상태 추적 표 (`PATCH` 문서) 매 commit 갱신

#### 3.3.2 DDS QoS mismatch silent failure

publisher RELIABLE + subscriber BEST_EFFORT → silent no-op:
- 권원 토픽 표 (IDS v1.5 §7) 1:1 매핑 의무
- CI 에 `ros2 doctor` 호출 추가하여 QoS 부정합 사전 검출

#### 3.3.3 Cyclone DDS specific behavior

사업 표준 (Cyclone DDS) 가 Fast DDS 와 동작 차이:
- 멀티캐스트 동작 differ
- discovery timing differ
- 회귀 시험을 Cyclone DDS 환경에서 수행 의무 (`RMW_IMPLEMENTATION=rmw_cyclonedds_cpp`)

---

## 4. 대안 검토 (Alternatives Considered)

### 4.1 대안 A: 전면 C++ 전환 (DCN-2026-001 D-007 원안)

**거부 사유**:
- 작업량 6~12개월 → 사업 일정 (24개월 중 50%) 점유 과대
- AI/perception 개발 속도 50%↓
- 권원 정합 측면 동일 (IPC 가 mp.Queue 면 언어 무관하게 정합 불가)
- 평가위원 역질문 위험 ("왜 application 도 C++?")

### 4.2 대안 B: Python rclpy 단일화 (no C++ at all)

**거부 사유**:
- safety-critical 모듈에 Python GC pause 부적합
- 발사 인가 100ms 응답 요구 (HMAC + audit fsync) → C++ 우위
- DO-178C / IEC 61508 high-integrity zone 정합 어려움
- 산업 표준 (BD, Anduril, JPL 등) 모두 hybrid 사용

### 4.3 대안 C: ZeroMQ 또는 Cap'n Proto 기반 자체 IPC

**거부 사유**:
- ROS 2 권원 (IDS v1.5 §5/§7) 정합 불가능
- Gazebo / rviz2 / ros2 bag 등 표준 도구 미사용
- 자체 IPC 유지보수 부담
- 시뮬레이션 환경 정합 어려움

### 4.4 대안 D: 하이브리드 (Tier 1 C++, Tier 2/3 Python, IPC 그대로 mp.Queue)

**거부 사유**:
- IPC 통일이 핵심인데 이를 회피
- C++ Tier 1 ↔ Python Tier 2/3 통신 시 bridge 필요 (`mp.Queue` ↔ rclcpp)
- 권원 정합 여전히 불가능
- 사업 평가에서 가장 안 좋게 보임 (혼란스러운 아키텍처)

### 4.5 결정 — 본 ADR-006 (대안 B 거부, 대안 A 부분 채택)

**채택**: 3-Tier 언어 정책 + **IPC 통일 (ROS 2)**

- Tier 1 (C++) = 대안 A 의 critical path 부분 채택
- Tier 2/3 (Python rclpy) = 대안 B 의 application 부분 채택
- IPC (ROS 2) = 본 ADR 의 핵심 결정 (대안 C/D 거부)

---

## 5. 구현 가이드 (Implementation Guide)

### 5.1 마이그레이션 패턴 — Before / After

#### 패턴 1: Adapter → Node

**Before** (`adapters/rtk_gnss.py`):
```python
class RtkGnssAdapter(BaseProcess):
    def __init__(self, queues, shutdown, cfg, **diag):
        super().__init__(...)
        self.queues = queues

    def step(self):
        fix = self.read_uart()
        self.queues.rtk.put(fix)
        self.queues.gga_latest.put(fix.gga)
```

**After** (`san_rtk_gnss/rtk_gnss_node.py`):
```python
class RtkGnssNode(rclpy.node.Node):
    def __init__(self):
        super().__init__('rtk_gnss_node')
        self.fix_pub = self.create_publisher(
            NavSatFix, '~/fix',
            qos_profile=qos_profile_sensor_data)
        self.gga_pub = self.create_publisher(
            String, '~/gga_latest',
            qos_profile=QoSProfile(depth=1, reliability=RELIABLE))
        self.create_timer(0.1, self.tick)

    def tick(self):
        fix = self.read_uart()
        self.fix_pub.publish(self._to_navsatfix(fix))
        self.gga_pub.publish(String(data=fix.gga))
```

#### 패턴 2: ShmPool → intra-process zero-copy

**Before** (`adapters/imx678_camera.py`):
```python
slot_idx = camera_shm.acquire_slot(buf_size)
buf = camera_shm.write_slot(slot_idx, frame_bytes)
queues.imx678_ref.put(CameraFrameRef(slot_idx, ts, ...))
```

**After** (`san_camera_imx678/imx678_node.cpp`):
```cpp
auto msg = std::make_unique<sensor_msgs::msg::Image>();
msg->width = 3840; msg->height = 2160;
msg->step = 3840 * 3; msg->encoding = "rgb8";
msg->data = std::move(frame_bytes);  // move, no copy
camera_pub_->publish(std::move(msg));  // zero-copy if subscriber in same process
```

#### 패턴 3: manager.dict() → service / parameter

**Before** (`main.py`):
```python
auth_state_proxy = manager.dict()
auth_state_proxy["developer_mode"] = False
auth_state_proxy["weapons_safe"] = True
```

**After** (`san_safety/safety_node.cpp`):
```cpp
// Read-only state for subscribers: parameter
declare_parameter("developer_mode", false);
declare_parameter("weapons_safe", true);

// Mutation: service
mutation_srv_ = create_service<SetSafetyState>(
    "~/set_state",
    [this](const auto req, auto resp) {
      set_parameter(rclcpp::Parameter("weapons_safe", req->weapons_safe));
      resp->ok = true;
    });
```

### 5.2 QoS 권장 매핑 (IDS v1.5 §7 정합)

```cpp
// P0 안전 critical (leader announce, fire authorization)
const auto qos_p0 = rclcpp::QoS(10)
    .reliable()
    .transient_local();  // late-join subscriber 도 받음

// P1 제어 명령 (cmd_vel, mission command)
const auto qos_p1 = rclcpp::QoS(5).reliable();

// P2 상태 보고 (1Hz 하트비트)
const auto qos_p2 = rclcpp::QoS(1).best_effort();

// Sensor (대역폭 우선)
const auto qos_sensor = rclcpp::SensorDataQoS();  // best_effort, depth=5
```

### 5.3 systemd unit 표준 (`san-squadron.service`)

```ini
[Unit]
Description=SAN SkyHunter Squadron Bootstrap
After=network-online.target
Wants=network-online.target

[Service]
Type=exec
User=san
Group=san
EnvironmentFile=/etc/san/squadron.env
ExecStart=/usr/bin/ros2 launch san_bringup squadron.launch.py
Restart=on-failure
RestartSec=5s
TimeoutStartSec=30s
KillSignal=SIGINT
TimeoutStopSec=10s

[Install]
WantedBy=multi-user.target
```

### 5.4 회귀 시험 패턴

기존 (`pytest test_perception.py`):
```python
queues = TopicQueues()
proc = PerceptionProcess(queues, ...)
proc.step()
result = queues.detections.get()
```

신규 (`pytest test_perception_node.py`):
```python
rclpy.init()
node = PerceptionNode()

# Helper publisher
helper = rclpy.create_node("test_helper")
img_pub = helper.create_publisher(Image, "~/image", 10)
det_sub = helper.create_subscription(
    DetectionArray, "~/detections", lambda m: detections.append(m), 10)

# Publish + spin
img_pub.publish(test_image)
rclpy.spin_once(node, timeout_sec=1.0)
rclpy.spin_once(helper, timeout_sec=1.0)

# Verify
assert len(detections) == 1
```

---

## 6. 검증 (Validation)

### 6.1 정합 측정 기준 (DCN-2026-002 D-009 인용)

본 ADR 채택 후 다음 측정으로 진행 추적:

| 측정 항목 | 목표 (PDR 시점) | 측정 명령 |
|---|---|---|
| `^from multiprocessing` import | 0 | `grep -rn "^from multiprocessing\|^import multiprocessing" --include="*.py"` |
| `ShmPool` / `shared_memory` 사용 | 0 | `grep -rn "ShmPool\|shared_memory" --include="*.py"` |
| Tier 1 모듈의 비-C++ 코드 | 0 | 패키지별 `*.py` line count |
| QoS 미명시 publisher | 0 | grep + AST 분석 |
| 권원 토픽 ↔ 코드 누락 | 0 | IDS v1.5 §5 표 ↔ ros2 topic list 비교 |

### 6.2 마일스톤

| 단계 | 일자 | 검증 |
|---|---|---|
| ADR-006 채택 | 2026-05-12 | (본 문서) |
| Phase 2-E Turn 1 (san_bringup) | +2주 | squadron.launch.py 부분 동작 |
| Phase 2-E Turn 5 (HW 어댑터) | +6주 | 모든 HW 어댑터 ROS 2 노드 화 |
| Phase 2-E Turn 9 (Application) | +10주 | Perception/Mission rclpy 전환 |
| PDR 사전 검토 | +12주 | 위 정합 측정 모두 0 / 100% |

---

## 7. 결론

**문제**: SkyHunter v1.5 의 mp.Queue + ShmPool + manager.dict() 3-way
IPC 분절이 ROS 2 권원 (IDS v1.5) 정합을 불가능하게 함.

**원인**: DCN-2026-001 D-007 의 "Python 폐기" 진단이 표면적 원인 (언어)
만 짚고 근본 원인 (IPC 표준 미준수) 을 놓침.

**해결**: ROS 2 (DDS) 로 모든 IPC 통일. 언어는 3-Tier 정책에 따라
모듈별 선택 (C++/Python). 작업량 6~12개월 → 3개월 단축.

**효과**:
1. v1.5 권원 즉시 정합 (SDD-SWARM §5.5, §5.6, IDS §5/§7)
2. 표준 도구 (ros2 bag, rqt) 즉시 활용
3. 안전 분석 trust boundary 명료화
4. PDR 평가 시 hybrid 아키텍처 정당화

본 ADR-006 은 DCN-2026-002 의 기술적 근거 문서로서, 사업 일정
정합 + 권원 정합 + 안전 분석 정합을 동시 달성한다.

---

## 8. 참조 (References)

### 8.1 내부
- DCN-2026-001 (원안 D-007)
- DCN-2026-002 (D-007 개정 + D-008 + D-009)
- SDD-SWARM v1.5 §5.5, §5.6, §10.1.1
- IDS v1.5 §1, §5, §7
- OPS-SOP v1.5 §3.1

### 8.2 외부
- ROS 2 design.ros2.org — DDS 단일 backend
- Cyclone DDS documentation — 사업 표준 RMW
- Boston Dynamics Spot SDK Architecture (hybrid 사례)
- NASA JPL F´ Framework (multi-language hybrid)
- MIL-STD-882E §4.4 — System Safety
- DO-178C §6.4 — Software Architecture
- IEC 61508 Part 3 §7.4 — Software Architecture Design

### 8.3 코드 분석 자료
- `/home/claude/work/v15_code_patch/SH_Unitree_Patrol/main.py` (446 lines)
- `/home/claude/work/v15_code_patch/SH_Unitree_Patrol/core/ipc.py` (260+ lines)
- Phase 2-D 결과 (san_fire_authorization C++ 노드 — Tier 1 reference)

---

## 9. 승인 (Approval)

| 역할 | 이름 | 일자 |
|---|---|---|
| 시스템 아키텍트 | (CTO / 외부 검토) | 2026-05-12 |
| 사업 책임 | 김태근 대표이사 | 2026-05-12 |
| 안전 책임 | (Safety lead) | 2026-05-12 |
| 권원 책임 | (PM) | 2026-05-12 |

---

> 본 ADR 은 채택 시점 즉시 발효되며, DCN-2026-002 와 함께 SkyHunter
> v1.5 의 기술 거버넌스 정문서로 등재된다.
