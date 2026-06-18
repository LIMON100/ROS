# PATCH Phase 1 — Sensor stub surfacing (v1.5.2)

> **작업일**: 2026-05-13
> **대상**: san_imu_driver, san_cameras, san_rtk_gnss, san_lidar, san_unitree_driver
> **권원**: codebase review systemic "silent stub fallback" antipattern (Phase 0 PR-B 후속)

---

## 1. 배경 (Phase 0 → Phase 1)

Phase 0 PR-B (#120) 는 sensor 계층의 stub fallback 을 fail-loud (parsing safety + 3-layer gate) 로 만들었음. 그러나 driver 가 STUB 모드로 진입했다는 사실 자체를 **downstream consumer 가 인지** 할 수 있어야 함 — perception_node 가 stub camera frame 으로 inference 돌리지 않고, fire_authorization 이 stub IMU/GNSS pose 를 신뢰하지 않게.

Phase 1 은 각 driver 에 **latched `~/stub_status` Bool publisher** 를 추가. 일관된 패턴으로 5개 driver 모두 동일 topic 명 사용.

---

## 2. 추가 패턴

각 driver:
```cpp
stub_status_pub_ = create_publisher<std_msgs::msg::Bool>(
    "~/stub_status",
    rclcpp::QoS(1).reliable().transient_local());

// after open() decides mode:
std_msgs::msg::Bool m;
m.data = <stub flag>;
stub_status_pub_->publish(m);
```

- **QoS**: `keep_last(1) + reliable + transient_local` → 늦게 join 한 subscriber 도 즉시 현재값 수신.
- **Topic 명 일관**: 모든 driver 가 `~/stub_status`. Downstream 은 driver 별 namespace 만 다르게 subscribe.

---

## 3. 적용 대상

| Driver | stub 조건 | 본 PR 변경 |
|---|---|---|
| `san_imu_driver/imu_driver_node` | `stub_mode_` true (serial open 실패 + `stub_on_no_serial`) | stub_status_pub_ 추가 + 시작 시 publish |
| `san_cameras/camera_node_base` | `stub_mode_` true (V4L2 open 실패) — 단 PR-B 의 runtime gate 가 `allow_stub_v4l2=false` 인 production 에서는 ctor throw → 여기 도달 안 함 | stub_status_pub_ in base class (IMX678/Thermal 둘 다 자동 적용) |
| `san_rtk_gnss/rtk_gnss_node` | `!serial_->isOpen()` (serial open 실패) | stub_status_pub_ 추가 + open 결과로 latch |
| `san_lidar/lrf_node` | `stub_mode_` true (serial open 실패) | stub_status_pub_ 추가 |
| `san_unitree_driver/unitree_go2_node` | `sdk_->isStub()` (Go2SdkInterface::isStub() — Phase 0 PR-C 에서 추가) | stub_status_pub_ in wireSdkAndRos |

---

## 4. 파일 변경 요약

```
san_imu_driver/
├── include/.../imu_driver_node.hpp                ★ stub_status_pub_ 멤버 + std_msgs/Bool include
├── src/imu_driver_node.cpp                         ★ publisher 생성 + initial publish
├── CMakeLists.txt                                  ★ std_msgs dep
└── package.xml                                     ★ std_msgs dep

san_cameras/
├── include/san_cameras/camera_node_base.hpp        ★ stub_status_pub_ 멤버 (Base — IMX678/Thermal 자동)
├── src/camera_node_base.cpp                        ★ publisher 생성 + initial publish (startCapture 후)
├── CMakeLists.txt                                  ★ std_msgs dep (4 lines)
└── package.xml                                     ★ std_msgs dep

san_rtk_gnss/
├── include/san_rtk_gnss/rtk_gnss_node.hpp          ★ stub_status_pub_ 멤버 + Bool include
└── src/rtk_gnss_node.cpp                            ★ publisher 생성 + initial publish

san_lidar/
├── include/san_lidar/lrf_node.hpp                  ★ stub_status_pub_ 멤버 + Bool include
├── src/lrf_node.cpp                                 ★ publisher 생성 + initial publish
└── CMakeLists.txt                                   ★ lrf_node target std_msgs dep

san_unitree_driver/
├── include/san_unitree_driver/unitree_go2_node.hpp ★ stub_status_pub_ 멤버 + Bool include
└── src/unitree_go2_node.cpp                         ★ wireSdkAndRos publisher + initial publish
```

---

## 5. 호환성

| 항목 | Before | After |
|---|---|---|
| Topic / QoS / msg schema | 동일 | 동일 + 신규 `~/stub_status` (driver당) |
| Public API | 동일 | `Go2SdkInterface::isStub()` 와 `V4l2CaptureInterface::isStub()` 는 Phase 0 PR-B/PR-C 에서 추가됨 |
| 빌드 의존성 | — | std_msgs 추가 (san_imu_driver, san_cameras 만; 나머지는 이미 있음) |
| Consumer 거동 변화 | — | 없음 (지금은 publisher 만; 본 PR 은 consumer 측 변경 X) |

---

## 6. Next steps (out of scope)

본 PR 은 publisher 만 추가. Consumer 측 변경은 후속 작업:
- `human_detector`: 자체 `~/backend_name` String + `~/stub_status` Bool 발행 (`isStub()` API 추가)
- `perception_node`: 카메라 `~/stub_status` 구독 → stub=true 시 inference skip + diagnostic 발행
- `fire_authorization`: IMU/GNSS `~/stub_status` 구독 → stub=true 시 결정 reason_detail 에 명시
- `mission_node`: 다중 sensor stub 종합 → BT가 발사 권한 path 진입 거부

Phase 2/3/4 진행 중 적절한 시점에 wire-up.

---

## 7. 결론

Phase 1 은 sensor 계층의 stub 상태를 표준화된 latched topic 으로 표면화하여 downstream consumer 가 인지할 수 있도록 함. 본 PR 은 publisher 측만 (5 driver) — consumer wiring 은 후속 Phase 에서 driver별로 적절한 정책과 함께 추가.

Phase 0 → Phase 1 진행 상황:
- ✅ PR-A (#118) — fire authorization
- ✅ PR-B (#120) — sensor stubs (fail-loud parsing + 3-layer gate)
- ✅ PR-C (#121) — driver safety (Unitree gate, cmd_vel watchdog, pan-tilt clamp)
- ✅ PR-D (#122) — operator command auth (interim CommandAuthGate)
- ✅ Phase 1 (본 PR) — stub surfacing (latched ~/stub_status)
- ⏳ Phase 2 — concurrency Tier 1
- ⏳ Phase 3 — SLAM correctness
- ⏳ Phase 4 — tests/tools fix
- ⏳ Phase 5 — medium tier
