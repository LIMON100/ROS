# PATCH Phase 0 PR-C — Driver / actuation safety (v1.5.2)

> **작업일**: 2026-05-13
> **대상**: san_unitree_driver, san_surveillance
> **권원**: codebase review Tier 0 — driver / actuation findings
> **유형**: Phase 0 Tier 0 — field-deploy blocker safety fixes

---

## 1. 식별된 이슈 3건 (모두 Critical / Medium)

### 🔴 PC1 (SAFETY) — Unitree stub SDK silent linking + no run-time gate

**현상**:
- `CMakeLists.txt` `find_package(unitree_sdk2 QUIET)` + 누락 시 stub 으로 silent link (`message(WARNING)` 만)
- `StubGo2Sdk` 에 `isStub()` virtual 없음
- Node ctor 가 stub 임에도 정상 시작 — `ros2 node list` 에 "running" 으로 표시
- 운용자는 driver 가 동작 중으로 오해 → 페이퍼 타이거 deploy

**수정 (3-layer gate)**:
1. **Compile-time**: `SAN_UNITREE_ALLOW_STUB` CMake option — OFF 빌드는 `unitree_sdk2` 없을 시 `FATAL_ERROR`
2. **Link-time**: `Go2SdkInterface::isStub()` virtual 추가, `StubGo2Sdk::isStub() = true`
3. **Run-time**: `UnitreeGo2Node` ctor 가 `sdk_->isStub() && !allow_stub_sdk_` 일 때 `throw std::runtime_error`. Param default false (production-safe).

### 🔴 PC2 (SAFETY) — `cmd_vel` watchdog 부재

**현상**: `UnitreeGo2Node::onCmdVel` 이 stale-input timeout 없음. Nav2 crash 시 마지막 명령 (예: 0.3 m/s forward) 이 SDK 에 영구 유지 → robot 이 무한 주행 가능.

**수정**:
- 신규 parameter `cmd_vel_watchdog_ms` (default 500 ms — 약 5 tick at typical 10 Hz Nav2 cadence)
- 10 Hz wall_timer `onCmdVelWatchdog` 추가
- `last_cmd_vel_steady_ms_` + `last_cmd_vel_nonzero_` atomics 로 stale 판정
- Stale + 마지막 명령 non-zero 시 zero-velocity 자동 송신 + WARN + `watchdog_zero_count_` 증가
- 0 ms 설정 시 watchdog 비활성화 (NOT recommended)

### 🟡 PC3 (SAFETY) — pan-tilt Track 모드 clamp 무단 widening

**현상**: `pan_tilt_controller.cpp::stepTrack()` 의 tilt clamp 가 `[cfg.tilt_min_deg - 20, cfg.tilt_max_deg + 50]` — Sweep 모드의 `[tilt_min_deg, tilt_max_deg]` 와 불일치. 운용 envelope 가 +10° 인 weapon-mount 가 Track 모드에서 **+60°** 까지 swing 가능 → 운용자 머리 위로 muzzle.

**수정**:
- `stepTrack()` clamp 를 `[cfg.tilt_min_deg, cfg.tilt_max_deg]` 로 통일 (Sweep 모드와 동일)
- Engage 모드가 lead prediction 위해 더 넓은 envelope 가 필요하다면 `engage_tilt_min/max` 별도 config + operator mode gate 로 명시화 — 본 patch scope 외 (Phase 1+ 추적)

---

## 2. 파일 변경 요약

```
san_unitree_driver/
├── CMakeLists.txt                                ★ SAN_UNITREE_ALLOW_STUB option
├── include/san_unitree_driver/
│   ├── go2_sdk_interface.hpp                     ★ isStub() virtual
│   └── unitree_go2_node.hpp                       ★ watchdog fields + atomics
├── src/
│   ├── stub_go2_sdk.cpp                           ★ isStub() = true
│   └── unitree_go2_node.cpp                       ★ stub gate + watchdog
└── test/test_unitree_go2_node.cpp                 ★ PC_* tests

san_surveillance/
├── src/pan_tilt_controller.cpp                    ★ Track clamp envelope
└── test/test_surveillance.cpp                     ★ PC_TrackTilt* tests
```

---

## 3. 호환성

| 항목 | Before | After |
|---|---|---|
| Topic / QoS / msg | 동일 | 동일 |
| `unitree_sdk2` 없는 빌드 | silent stub | `SAN_UNITREE_ALLOW_STUB=ON`: stub link + node refuse-to-start. OFF: FATAL_ERROR |
| `UnitreeGo2Node` 신규 param | — | `cmd_vel_watchdog_ms` (default 500), `allow_stub_sdk` (default false) |
| `Go2SdkInterface` interface | 없음 | `isStub()` virtual (default false in base — 기존 MockGo2Sdk 호환) |
| Track-mode tilt 한계 | `[min-20, max+50]` | `[min, max]` (Sweep 와 동일) |

**Default 거동 변화**:
- Unitree: stub SDK 환경에서 노드 시작 거부 (이전엔 silent stub 시작). Bringup 시 `allow_stub_sdk:=true` 로 허용.
- cmd_vel: 500ms 이상 stale 시 zero-velocity 자동 송신 (이전엔 마지막 명령 영구 유지).
- pan-tilt: Track 모드 envelope = config envelope (이전엔 widened).

---

## 4. 검증

신규 테스트:
- `test_unitree_go2_node.cpp`
  - `PC_StubSdkRefusedWhenAllowStubSdkFalse` — stub-flagged mock 으로 ctor throw
  - `PC_StubSdkAcceptedWhenAllowStubSdkTrue` — param override 시 정상 시작
  - `PC_CmdVelWatchdogZerosOnStaleInput` — non-zero cmd 후 watchdog 100ms+ 경과 → zero cmd
- `test_surveillance.cpp`
  - `PC_TrackTiltClampedToConfiguredEnvelope` — Track target +60° 이지만 tilt ≤ tilt_max_deg
  - `PC_TrackTiltClampedToLowerEnvelope` — symmetric lower bound check

기존 테스트: Mock의 default `isStub()=false` 로 그대로 통과.

---

## 5. 결론

본 patch 는 codebase review 의 driver / actuation Tier 0 3건 (#8 stub SDK gate, #9 cmd_vel watchdog, #10 pan-tilt clamp) 을 해소.

Phase 0 진행 상황:
- ✅ PR-A (#118) fire authorization
- ✅ PR-B (#120) sensor stubs
- ✅ PR-C (본 PR) driver safety
- ⏳ PR-D operator command auth — 다음 단계

Phase 0 완료 후 Phase 1 → 5 sequential 진행.
