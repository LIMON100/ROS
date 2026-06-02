# PATCH Phase 7 — Medium tier residual (v1.5.2)

> **작업일**: 2026-05-13
> **대상**: san_hub_orchestrator, san_role_management, san_follower_tier, san_mission
> **권원**: codebase review medium-tier residual

---

## 1. 식별된 이슈 4건

### 🟡 P7-1 — Hub aggregator clock-skew (publisher timestamp_ms)

**현상**:
- `HubOrchestratorNode::onRobotStatus` 가 `s.last_heartbeat_ms = msg->timestamp_ms` (publisher's wall clock)
- `SwarmAggregator::aggregate` 가 `now_ms - last_heartbeat_ms` 로 disconnect 판정 (hub's wall clock)
- Publisher clock leads → 영구 fresh; lags → 즉시 disconnect
- 동일 패턴이 `ThreatAggregator` 에서도 발생 (window_start_ms 가 publisher ts 기준)

**수정**:
- `hub_orchestrator_node`: `last_heartbeat_ms = now().nanoseconds()/1e6` (hub 로컬)
- `threat_aggregator_node`: `in.timestamp_ms = hub now()` (publisher ts 무시 — payload 보존은 별도 field 로 후속 작업)

### 🟡 P7-2 — `hub_role_manager` lifecycle service `future.get()->success` 무시

**현상**:
- `activate{Slam,Video}` + `deactivate{Slam,Video}` 4 곳이 `wait_for==ready` 만 체크
- `ChangeState::Response.success=false` (e.g. node 이미 active, wrong state) 도 OK 로 신뢰
- Full Takeover guard (`hub_role_manager.cpp:135`) 가 거짓말 신뢰 → 잘못된 promotion

**수정**:
- 4개 service call 모두 `future.get(); return resp && resp->success` 로 갱신

### 🟡 P7-3 — `tier_fsm::step()` hardcoded 100ms dt

**현상**:
- `tier_node` 의 `tick_period_ms` parameter (default 100, configurable) 이 fsm 에 전달 안 됨
- `fsm.step()` 안에서 `stepWithDt(100)` hardcoded → user 가 `tick_period_ms=200` 설정 시 anti-flap dwell 이 wall time 의 1/2 만 측정 → flap 가능

**수정**:
- `TierFsm::step(in, dt_ms=0)` overload — 0 은 legacy 100ms default (back-compat)
- `TierNode::onTick()` 가 `fsm_->step(in, tick_period_ms_)` 호출

### 🟡 P7-4 — `mission_node._on_pose` pose/yaw lock 누락

**현상**:
- `_on_pose` 가 `pose_xy` 와 `yaw_rad` 를 두 statement 로 write
- MultiThreadedExecutor 하에서 BT tick 가 사이에 read 시 msg N 의 pose + msg N+1 의 yaw 관측 가능
- R-13 patch 가 `priority.*` 만 lock 추가; pose/yaw 는 미커버

**수정**:
- `MissionContext.lock = threading.RLock()` 추가 (R-13 의 `ctx.lock` 명명과 일치 — merge 시 충돌 없음)
- `_on_pose` 가 yaw 계산 후 `with self._ctx.lock:` 하에 pose_xy + yaw_rad 동시 set

---

## 2. 파일 변경 요약

```
san_hub_orchestrator/src/
├── hub_orchestrator_node.cpp           ★ last_heartbeat_ms = hub now()
└── threat_aggregator_node.cpp          ★ in.timestamp_ms = hub now()

san_role_management/src/
└── hub_role_manager.cpp                ★ 4 lifecycle calls: future.get()->success

san_follower_tier/
├── include/san_follower_tier/tier_fsm.hpp  ★ step(in, dt_ms=0)
├── src/tier_fsm.cpp                         ★ advance_ms = dt_ms ?: 100
└── src/tier_node.cpp                         ★ fsm_->step(in, tick_period_ms_)

san_mission/san_mission/
├── mission_context.py                  ★ ctx.lock = threading.RLock
└── mission_node.py                     ★ _on_pose under lock
```

---

## 3. 호환성

| 항목 | Before | After |
|---|---|---|
| Topic / QoS / msg schema | 동일 | 동일 |
| `TierFsm::step(in)` | 1-arg | 1-arg or 2-arg (default dt_ms=0 → legacy 100ms) |
| `MissionContext` 신규 field | — | `lock: threading.RLock` |
| 거동 변화 | 동일 | hub aggregator/threat aggregator 가 publisher clock skew 영향 무 |

---

## 4. 결론

codebase review 의 모든 Critical (35건) + Tier 1 medium (대표적인 패턴) 처리 완료.

Phase 진행 종합:
- ✅ Phase 0 (PR-A/B/C/D) — Tier 0 field-deploy blockers
- ✅ Phase 1 — sensor stub surfacing
- ✅ Phase 2 — concurrency Tier 1
- ✅ Phase 3 — SLAM correctness
- ✅ Phase 4 — tests + tooling
- ✅ Phase 5 — hub health hysteresis + skew
- ✅ Phase 6 — threat_aggregator double-publish + tier_node KPP-2
- ✅ Phase 7 (본 PR) — clock-skew, lifecycle service, tier dt, mission pose lock

미해결 (deferred — separate PRs / 별도 추적):
- pose_graph_optimizer full g2o population (Phase 3 에서 sentinel 만 추가)
- HubSlamNode 의 PNG encode-inside-mutex (R-3 review finding)
- HubSlamNode 의 동적 producer discovery
- combat_robot_msgs schema breaking changes (FormationCommand vs FormationStatus enum 통합)
- S20-3/4/5 always-pass soft-fallback test 재작성
- Full HMAC + nonce integration for kinetic operator commands (Phase 0 PR-D 의 interim 후속)
