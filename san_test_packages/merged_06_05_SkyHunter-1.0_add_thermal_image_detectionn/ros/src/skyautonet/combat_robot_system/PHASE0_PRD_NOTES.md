# PATCH Phase 0 PR-D — Operator command authorization (v1.5.2)

> **작업일**: 2026-05-13
> **대상**: san_operator_tools (Python)
> **권원**: codebase review Tier 0 #11 — kinetic-area operator tools 의 anonymous publish
> **유형**: Phase 0 Tier 0 — interim compensating control (full HMAC integration → Phase 1+)

---

## 1. 배경

`san_fire_authorization` 은 HMAC + Two-key + audit 로 무장 발사를 보호 (#118 PR-A). 그러나 동일한 운용 효과를 갖는 **kinetic-area** 명령 (`/swarm/waypoint_command`, `/swarm/formation_command`) 은 **anonymous publish** 가 가능했음 — DDS domain 의 어떤 node 도 mass waypoint 변경 / formation 변경 publish 가능.

전체 HMAC integration 은 (1) `WaypointCommand`/`FormationCommand` msg 에 `hmac_signature` field 추가, (2) operator-side signing key 배포, (3) gate-side verifier 구축 이 필요 — Tier 0 단일 PR 범위 초과. 본 PR 은 **compensating control** 로 anonymous publish 만 차단.

---

## 2. PD1 — CommandAuthGate (interim compensating control)

**도입**:
- 신규 모듈 `san_operator_tools/command_auth.py` 의 `CommandAuthGate` 헬퍼
- Node 두 parameter:
  - `operator_id` (string, default `""`)
  - `production_mode` (bool, default `false`)
- `check_and_log(summary, target_id)`:
  - `production_mode=true` + `operator_id` empty → **refuse to publish**, ERROR 로그
  - 그 외엔 publish 허용 + WARN 로그 (operator_id, publish counter, target 포함)

**적용 대상**:
- `waypoint_sender.py` — `_publish_waypoints()` 전에 gate 체크. 거부 시 publish skip.
- `formation_switcher.py` — 5회 publish 루프 진입 전 gate 체크. 거부 시 ERROR + early shutdown.

**미해결 사항** (Phase 1+):
- `combat_robot_msgs/WaypointCommand`, `FormationCommand` 에 `hmac_signature`, `nonce`, `operator_id` field 추가 (schema change)
- Gate-side verifier (e.g. `san_operation_control` 새 callback) — HMAC 검증 후 forwarding
- Operator-side signing key 배포 (`/etc/san/operator_secret.bin`)

`HMAC_INTEGRATION_AVAILABLE = False` sentinel 이 모듈에 있음 — 위 작업 완료 시 True 로 flip + 코드 가드.

---

## 3. 파일 변경 요약

```
san_operator_tools/
├── san_operator_tools/
│   ├── command_auth.py                  ★ NEW (CommandAuthGate)
│   ├── waypoint_sender.py               ★ patched (gate before publish)
│   └── formation_switcher.py            ★ patched (gate before publish loop)
└── test/
    └── test_command_auth.py             ★ NEW (5 pytests)
```

---

## 4. 호환성

| 항목 | Before | After |
|---|---|---|
| Topic / QoS / msg schema | 동일 | 동일 |
| 신규 parameter | — | `operator_id` (default ""), `production_mode` (default false) |
| Default 거동 (dev) | publish 무인증 | publish + WARN 로그 (operator_id="<anon>" 표기) |
| Default 거동 (production_mode=true + empty operator_id) | publish | **refuse** + ERROR + 0 publishes |
| Audit trail | log 없음 | WARN 로그에 publish counter + operator_id + target_id |

운용 절차:
- Dev / lab: `ros2 run san_operator_tools waypoint_sender` 기존대로. WARN 로그 한 줄만 추가.
- Production: `... -p operator_id:=op_alpha -p production_mode:=true`. operator_id 누락 시 publish 안 됨.

---

## 5. 검증

신규 테스트: `test/test_command_auth.py` (5건)
- `test_publishes_with_operator_id` — `op_alpha` + production → True
- `test_refuses_without_operator_id_in_production` — empty + production → False (3회 연속)
- `test_allows_without_operator_id_in_dev_mode` — empty + dev → True (WARN 로그만)
- `test_publish_counter_increments` — accepted call 마다 counter ++
- `test_refusal_does_not_increment_counter` — refused 시 counter 유지

---

## 6. 결론

본 patch 는 Tier 0 #11 의 anonymous kinetic command publish 를 차단하는 interim compensating control 을 도입.

Phase 0 완료:
- ✅ PR-A (#118) — fire authorization
- ✅ PR-B (#120) — sensor stubs
- ✅ PR-C (#121) — driver safety
- ✅ PR-D (본 PR) — operator command auth

Phase 0 (Tier 0 field-deploy blockers) 처리 완료. 다음 단계: Phase 1 (sensor stub 표면화 — 모든 stub backend 가 is_stub/diagnostic 발행, runtime gate), Phase 2 (concurrency Tier 1), Phase 3 (SLAM aggregator + pose_graph_optimizer), Phase 4 (test/tooling), Phase 5 (medium tier).
