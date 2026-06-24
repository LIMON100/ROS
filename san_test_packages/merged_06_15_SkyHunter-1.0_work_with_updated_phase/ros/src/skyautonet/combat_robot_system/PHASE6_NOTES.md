# PATCH Phase 6 — Critical residual (v1.5.2)

> **작업일**: 2026-05-13
> **대상**: san_hub_orchestrator/threat_aggregator, san_follower_tier/tier_node
> **권원**: codebase review role+coordination Critical (#21, #22)

본 PR 은 codebase review 의 Critical 2건 중 Phase 0-5 에서 누락된 부분을 처리.

---

## 1. P6-1 — ThreatAggregator CRITICAL/FATAL fast-path double-publish

**현상** (`threat_aggregator_node.cpp:90`):
```cpp
auto p = aggregator_.peek(in.source_robot_id, in.threat_type);
if (p) publish(*p);     // ← peek 만, slot 은 그대로 남음
```
- Fast-path 가 publish 후 slot 을 erase 안 함
- dedup window (5s) 지나면 `pollReady()` 가 같은 slot 을 다시 return → operator 에게 같은 CRITICAL/FATAL 두 번 보임
- Reproducible by existing test fixtures

**수정**:
- 신규 `ThreatAggregator::pop(source_robot_id, threat_type)` 추가 — peek + erase atomic
- Node fast-path 이 `peek` → `pop` 으로 전환

## 2. P6-2 — TierNode `~/robot_status` private topic (KPP-2 evidence 무효)

**현상** (`tier_node.cpp:33`):
```cpp
status_sub_ = create_subscription<...>(
    "~/robot_status",     // = /tier_node/robot_status (private)
    ...);
```
- Fleet 전체는 `/swarm/robot_status` 발행 (leader_role_manager, hub_health_monitor 등)
- TierNode 가 자기 private topic 만 구독 → producer 없음 → `current_x_/current_y_` 영구 0 → `delta_m = 0` → FSM 가 T0/T1 stuck → T2/T3/T4 진입 불가
- **KPP-2 (follower tier transition) 검증 evidence 자체가 invalid**

**수정**:
- Subscribe → `/swarm/robot_status`
- 기존 `onRobotStatus` 가 이미 `msg->robot_id != robot_id_` 로 self-filter → 동작 변경 없음

---

## 3. 파일 변경 요약

```
san_hub_orchestrator/
├── include/san_hub_orchestrator/threat_aggregator.hpp  ★ pop() 신규
├── src/threat_aggregator.cpp                            ★ pop() 구현
└── src/threat_aggregator_node.cpp                       ★ peek → pop in fast-path

san_follower_tier/
└── src/tier_node.cpp                                     ★ /swarm/robot_status
```

---

## 4. 호환성

| 항목 | Before | After |
|---|---|---|
| Topic / QoS / msg schema | 동일 | 동일 (tier_node 는 외부 topic 으로 전환 — 외부에는 신규 subscriber 추가만) |
| ThreatAggregator API | 동일 | `pop()` 신규 추가 (기존 `peek` 유지) |
| 거동 변화 | 동일 | TierNode 가 처음으로 pose 수신 → FSM 가 작동, CRITICAL/FATAL alert 가 단일 publish |

---

## 5. 결론

- ✅ P6-1: operator 가 CRITICAL/FATAL 알람을 두 번 받는 문제 제거
- ✅ P6-2: KPP-2 follower tier transition 검증 가능

Phase 진행:
- ✅ Phase 0-5
- ✅ Phase 6 (본 PR)
- ⏳ Phase 7 — medium residual (clock-skew, mission pose lock, lifecycle service check, tier dt)
