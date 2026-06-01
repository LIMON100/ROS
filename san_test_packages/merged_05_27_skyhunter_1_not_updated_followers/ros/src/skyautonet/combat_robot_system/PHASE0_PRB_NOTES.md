# PATCH Phase 0 PR-B — Sensor stub safety (v1.5.2)

> **작업일**: 2026-05-13
> **대상**: san_imu_driver, san_cameras, san_rtk_gnss, san_lidar
> **권원**: codebase review Tier 0 — "silent stub fallback" antipattern (5 Critical 중 4건; 5번째인 perception 은 R-14 에서 이미 해결됨)
> **유형**: Phase 0 Tier 0 — field-deploy blocker safety fixes

---

## 1. 배경

전체 codebase review (perception+sensors agent) 결과 sensor 계층 전반에 "stub fallback 이 silent" antipattern 이 systemic 으로 존재. 각 driver 는 (a) 실패 시 stub 로 영구 lock-in 되거나 (b) parsing safety 가 없어 garbage 가 downstream 으로 전파. 4건 모두 EKF / SLAM / fire authorization 의 입력을 오염시킬 수 있어 field deploy 차단 사유.

---

## 2. 식별된 이슈 4건 (모두 Critical)

### 🔴 PB1 (SAFETY) — IMU `publishStubSample()` in REAL mode

**현상**: `ImuDriverNode::publishFromPayload()` 가 binary frame parse 성공 후 (실제 IMU 가 framing 정상 → 진짜 데이터 수신 중) 그대로 `publishStubSample()` 호출 → static-platform 모델 (gravity in +Z, near-zero gyro, identity quaternion) publish.

**영향**: EKF / Nav2 가 정지된 평탄 robot 으로 인식. 실제 움직임 무시 → catastrophic for nav. Decoder TODO 가 운영 단계에서도 stub 로 silent fallback 됨.

**수정**:
- 신규 parameter `require_decoded_payload` (default **true**) — production 안전
- `publishFromPayload()` 가 decoder 없을 때:
  - true (default): 프레임 DROP + WARN_THROTTLE (5s) + `undecoded_drop_count_` 증가
  - false: 기존 stub-republish behavior (bringup test 전용)
- `onHealthTick()` 로그에 `undecoded_drops` 추가

### 🔴 PB2 (SAFETY) — `makeRealV4l2()` 무조건 stub 반환

**현상**: `san_cameras/src/stub_v4l2.cpp::makeRealV4l2()` 가 `make_unique<StubV4l2>()` 반환 (= 항상 stub). `Imx678Node` / `ThermalNode` default-ctor 가 production 에서 영구 stub.

**영향**: Build green + `ros2 node list` 에서 running 으로 보임 + 1Hz "UNHEALTHY" 로그만 출력. 운용자는 카메라 동작 중으로 오해 가능.

**수정 (3-layer gate)**:
1. **Compile-time**: `SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK` CMake option — OFF 빌드는 `makeRealV4l2()` 호출 시 `#error` (real backend 강제 link 필요)
2. **Link-time**: 신규 `makeStubV4l2()` 명시적 stub factory + `isStub()` virtual method 추가
3. **Run-time**: `CameraNodeBase::startCapture()` 가 `v4l2_->isStub() && !allow_stub_v4l2` 일 때 `throw std::runtime_error`. Launch param `allow_stub_v4l2` default false.

**Note**: SubclassDefaults / atomic / timestamp validation 등 다른 san_cameras 개선은 R-16 (#119) 에서 진행 — 본 PR 은 stub safety 만.

### 🔴 PB3 (PARSING) — NMEA NaN / unbounded lat/lon

**현상**:
- `toDouble()` 가 `std::strtod("nan")` / `"inf"` 통과 → `parseDmToDeg` 의 minutes 가 NaN
- `parseDmToDeg` deg integer 무제한 — "359912.0" → 3599° 통과
- `parseGga` lat/lon 가 ±90/±180 검증 없이 publish

**영향**: 글리치 receiver / spoofed sentence 가 NaN 또는 3599° lat 를 `NavSatFix` 로 발행 → SLAM map / Nav2 transform 가 garbage 로 오염.

**수정**:
- `toDouble()` 에 `std::isfinite(v)` 검사 추가 (NaN/Inf reject)
- `parseDmToDeg()` 에 `deg ∈ [0, 180]` + `std::isfinite(result)` + `|result| ≤ 180` 검사
- `parseGga()` 의 lat 에 `|lat| ≤ 90` 검사, lon 에 `|lon| ≤ 180` 검사
- 추가: 같은 NaN 거부 idiom 을 `san_lidar/lrf_parser.cpp::toFloat()` 에 적용

### 🔴 PB4 (SAFETY) — `StubSerial::write()` fake-success

**현상**: `san_rtk_gnss/src/stub_serial.cpp::write()` 가 `return bytes.size();` (pretend success). `RtkGnssNode::onRtcm` 이 이를 신뢰하고 `rtcm_inject_count_` 증가 → 운용자 대시보드가 "RTCM 정상 흐름" 으로 보임 (실제로는 receiver 없음).

**수정**:
- `StubSerial::write()` 가 `return 0` (real failure)
- `RtkGnssNode::onRtcm` 가 short-write 감지 → `rtcm_inject_drop_count_` 증가 + WARN_THROTTLE(5s) 로그
- 신규 `rtcm_inject_drop_count_` atomic 추가
- regression test 신규: `test_stub_serial.cpp` (StubSerial::write returns 0)

---

## 3. 파일 변경 요약

```
san_imu_driver/
├── include/san_imu_driver/imu_driver_node.hpp   ★ require_decoded_payload_, undecoded_drop_count_
└── src/imu_driver_node.cpp                       ★ publishFromPayload no-op + health log

san_cameras/
├── CMakeLists.txt                                ★ SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK
├── include/san_cameras/v4l2_interface.hpp        ★ isStub() + makeStubV4l2()
└── src/
    ├── stub_v4l2.cpp                             ★ #error gate + makeStubV4l2
    └── camera_node_base.cpp                      ★ runtime allow_stub_v4l2 check

san_rtk_gnss/
├── CMakeLists.txt                                ★ test_stub_serial 등록
├── include/san_rtk_gnss/rtk_gnss_node.hpp        ★ rtcm_inject_drop_count_
├── src/
│   ├── nmea_parser.cpp                            ★ NaN reject + deg bound
│   ├── rtk_gnss_node.cpp                          ★ onRtcm short-write WARN
│   └── stub_serial.cpp                            ★ write returns 0
└── test/
    ├── test_nmea_parser.cpp                       ★ NaN / OOB tests
    └── test_stub_serial.cpp                       ★ NEW

san_lidar/
├── src/lrf_parser.cpp                             ★ toFloat NaN reject
└── test/test_lrf_parser.cpp                       ★ L11-L13 NaN/Inf tests
```

---

## 4. 호환성

| 항목 | Before | After |
|---|---|---|
| Topic / QoS / msg | 동일 | 동일 |
| `ImuDriverNode` parameter `require_decoded_payload` | 없음 | 신규 (default true, 안전) |
| `CameraNodeBase` parameter `allow_stub_v4l2` | 없음 | 신규 (default false, 안전) |
| `V4l2CaptureInterface::isStub()` virtual | 없음 | 신규 (default false in base) |
| `makeRealV4l2()` 빌드 | 항상 성공 | `SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK=OFF` 일 때 link 실패 (의도) |
| Public API breaking | — | `V4l2CaptureInterface` 새 virtual (default impl 제공 → 기존 구현 호환) |

**Default 거동 변화**:
- IMU: REAL 모드에서 decoder 없으면 **DROP** (이전엔 stub 발행). Bringup 시 `require_decoded_payload=false` 로 legacy 거동 복원.
- 카메라: stub 로 fallback 시 노드가 **start 거부**. Bringup 시 `allow_stub_v4l2:=true` 로 허용.
- NMEA: NaN/OOB sentence **drop** (이전엔 publish). 정상 sentence 영향 없음.
- RTCM: stub-only 환경에서 `rtcm_inject_count_` 증가 안 함 (이전엔 fake-increment). 실제 receiver 환경 영향 없음.

---

## 5. 검증

신규 테스트:
- `test_lrf_parser.cpp` L11/L12/L13 — NaN/Inf range + nan strength field
- `test_nmea_parser.cpp` — NaN token, out-of-range degrees, GGA with out-of-bounds lat
- `test_stub_serial.cpp` (신규 파일) — StubSerial::write returns 0

기존 테스트: 모두 동일하게 통과 (정상 sentence + 정상 range 거동 변화 없음).

CI 빌드: `SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK=ON` (default) 으로 link OK. Production 빌드는 OFF + real V4L2 backend 필요 (CI 환경에서는 ON 유지).

---

## 6. 결론

본 patch 는 codebase review 의 sensor 계층 "silent stub fallback" antipattern 4건을 systemic 으로 해소:

- ✅ **PB1**: IMU EKF 오염 차단 (REAL mode 에서 stub publish 금지)
- ✅ **PB2**: 카메라 build-green-but-stub 차단 (3-layer compile/link/runtime gate)
- ✅ **PB3**: NMEA NaN/OOB 차단 (SLAM/Nav2 입력 정상화)
- ✅ **PB4**: RTCM fake-success 차단 (대시보드 정확성 회복)

Phase 0 의 PR-A (fire authorization, #118), R-16 (san_cameras deep-dive, #119) 와 함께 field-deploy blocker 완료. 남은 PR-C (driver safety: unitree stub gate, cmd_vel watchdog, pan-tilt clamp), PR-D (operator command HMAC) 는 별도 진행.
