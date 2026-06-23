# PATCH Phase 5 — Medium tier hardening (v1.5.2)

> **작업일**: 2026-05-13
> **대상**: swarm_coordinator/hub_health_monitor
> **권원**: codebase review medium-tier findings (hub_health_monitor future heartbeat + SBC flapping)

---

## 1. 배경

Phase 0-4 가 Critical / Tier 0 findings 를 처리했고, Phase 5 는 codebase review 가 medium 으로 분류한 patterns 을 단계적으로 처리. 본 PR 은 systemic clock-skew 와 flapping 패턴 중 가장 영향 큰 `hub_health_monitor` 부분에 집중.

(Medium tier 의 나머지 — `threat_aggregator` / `swarm_aggregator` 의 publisher-timestamp clock-skew, `mission_node._on_pose` lock, `hub_role_manager` lifecycle `future.get()->success` — 는 별도 후속 PR. 본 PR 은 hub health 단일 영역만.)

---

## 2. 식별된 이슈 2건

### 🟡 P5-1 — Future heartbeat 가 fresh 로 인식됨

**현상**: `isFresh()` 의 `if (now_ms < last_heartbeat_ms_) return true;` — clock-skew / bag-replay / spoofed timestamp 가 영구 fresh.

**수정**:
- 신규 상수 `kHubMaxSkewMs = 500` (ms)
- future timestamp 이 ≤ 500ms 면 accept, > 500ms 면 stale 처리
- Test: `FutureHeartbeatBeyondSkewIsStale` + `FutureHeartbeatWithinSkewToleranceFresh`

### 🟡 P5-2 — SBC flapping protection 부재

**현상**: `update()` 가 raw `sbc1_healthy` / `sbc2_healthy` 직접 write. Producer 가 10 Hz 로 노이즈 alternate 하면 `classify()` 가 매 tick NORMAL↔CASE_A 진동.

**수정**:
- `FlappingPolicy { bad_threshold, good_threshold }` struct 추가
- Default `{1, 1}` (즉시 응답, pre-patch 호환). Production tuning: `{3, 5}` (3 연속 bad → unhealthy flip, 5 연속 good → recover).
- Hysteresis 카운터 (sbc1/sbc2 각각 bad/good)
- "Warm start" semantic: 첫 heartbeat 은 hysteresis 우회 — baseline 설정. 이후 sample 부터 hysteresis 적용. 기존 테스트 호환.
- Test: `HysteresisIgnoresSingleSpike`, `HysteresisFlipsAfterNConsecutiveBad`
- 보너스: `hasEverSeenHub()` accessor — UNKNOWN 가 "never seen" 인지 "stale" 인지 구분

---

## 3. 파일 변경 요약

```
swarm_coordinator/
├── include/swarm_coordinator/hub_health_monitor.hpp  ★ FlappingPolicy, kHubMaxSkewMs, hysteresis counters, hasEverSeenHub
├── src/hub_health_monitor.cpp                          ★ applyHysteresis, future-skew bound, warm-start
└── test/test_hub_health.cpp                            ★ +4 tests (skew + hysteresis)
```

---

## 4. 호환성

| 항목 | Before | After |
|---|---|---|
| `HubHealthMonitor` 생성자 | `(hub_id, stale_ms)` | `(hub_id, stale_ms, FlappingPolicy{1,1})` — 3rd arg default |
| Default 거동 | 즉시 응답 | 즉시 응답 (FlappingPolicy{1,1}) — pre-patch 와 동일 |
| Production tuning | — | `FlappingPolicy{3, 5}` 권장 |
| Future heartbeat | fresh 처리 | ≤500ms skew accept, > 500ms stale |
| 신규 API | — | `hasEverSeenHub()` |

---

## 5. 미해결 (out of scope — separate PR)

본 PR 외 Phase 5 medium-tier 후보:
- `san_hub_orchestrator/threat_aggregator` — publisher `timestamp_ms` 로 window 계산 (clock skew vulnerable); CRITICAL/FATAL fast-path 이 slot erase 안 함 → dedup window 후 재발행
- `san_hub_orchestrator/swarm_aggregator` — publisher `timestamp_ms` 로 disconnect 판정
- `san_mission/mission_node._on_pose` — pose_xy/yaw_rad lock 누락 (R-13 에서 priority만 lock)
- `san_role_management/hub_role_manager` lifecycle service `future.get()->success` 무시 (M11/M12 가 limp_mode_manager 만 처리)
- `san_follower_tier/tier_node` `breadcrumb_available` field 미사용 + `~/robot_status` private topic 버그
- `san_follower_tier/tier_fsm` `step()` 가 `tick_period_ms` 무시 (hardcoded 100ms)

각각 별도 PR 로 진행.

---

## 6. 결론

본 PR 은 Phase 5 medium-tier 중 hub health monitor 부분만 처리:
- ✅ P5-1: future heartbeat 의 무한-fresh bug — bounded skew
- ✅ P5-2: SBC flapping protection — hysteresis 정책 (production tunable)

Phase 진행:
- ✅ Phase 0 / 1 / 2 / 3 / 4 / 5 (본 PR)
- 추후: 위 미해결 medium-tier findings 별도 PR

---

## 7. 전체 6 Phase 정리 (codebase review 후속 작업 종합)

| Phase | PR | 처리 내용 |
|---|---|---|
| 0 PR-A | #118 | Fire authorization (KEY1↔KEY2 binding + audit fail-closed + nonce window) |
| 0 PR-B | #120 | Sensor stub safety (IMU/cameras/RTK/LRF — fail-loud + 3-layer gate) |
| 0 PR-C | #121 | Driver safety (Unitree stub gate + cmd_vel watchdog + pan-tilt clamp) |
| 0 PR-D | #122 | Operator command auth (interim CommandAuthGate) |
| 1 | #123 | Sensor stub status surfacing (latched ~/stub_status × 5 driver) |
| 2 | #124 | Concurrency Tier 1 (costmap shared_ptr race + CommandEcho torn read) |
| 3 | #125 | SLAM correctness (snapshot race + per-robot fragment + optimize status) |
| 4 | #126 | Tests + tooling (operator topics + S20-2 executable + swarm_sim args + token entropy) |
| 5 | 본 PR | Medium tier — hub health hysteresis + bounded skew |

R-series 보조 PR:
- #117 (R-15) san_hub_comm + san_lte_redundancy concurrency
- #119 (R-16) san_cameras parameter override + atomics + timestamp
