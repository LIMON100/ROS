# Deployment Modes (SAN v1.3 §11)

> Source of truth: `core/deployment.py` (enum, constants, transition table),
> `config/system.yaml` (base) + `config/system.<mode>.yaml` (overlays).

The platform supports five mutually exclusive deployment tiers. The active
tier is resolved once at boot and broadcast at 1 Hz on the `operation_state`
queue (`OperationState.deployment_mode`).

## 1. Five tiers

| Mode          | Live robots | Stubs allowed | Auth gate                | Use case                                   |
|---------------|:-----------:|:-------------:|--------------------------|--------------------------------------------|
| `production`  | yes         | no            | none (default)           | Real operations, full safety               |
| `demo`        | yes         | no            | none                     | Customer / trade-show demo, slowed cadence |
| `lab_test`    | yes         | no            | none                     | Indoor lab, real robots, reduced envelope  |
| `bench`       | partial     | yes           | none                     | Single-robot HIL bench                     |
| `development` | no          | yes           | `DEVELOPER_AUTH_TOKEN`   | Laptop / CI / dev container                |

### Robot ID mapping (v1.4: 4..8 robots, gracefully variable)

| ID  | Role           | Constant              | Notes                                                            |
|----:|----------------|-----------------------|------------------------------------------------------------------|
| 1   | Leader Go2     | `LEADER_ROBOT_ID`     | Predictive planner + breadcrumbs.                                |
| 2   | Hub UGV        | `HUB_ROBOT_ID`        | SLAM fuse + comm gateway. **Leader succession 2nd priority** (v1.4). |
| 3   | Deputy UGV     | `DEPUTY_ROBOT_ID`     | **v1.4 신규**. Hardware-identical to Hub. **Leader succession 1st** + Hub backup 1st (LTE + SLAM + video). |
| 4..8 | Follower UGV  | `FOLLOWER_ROBOT_IDS`  | 1~5 followers (variable). Local Nav2 + tier escape.              |

**v1.3 legacy** `DEFAULT_DEPUTY_CHAIN = (2, 3, 4, 5, 6, 7, 8)` is preserved
for compatibility but **deprecated** — v1.3 placed Hub at the head of the
chain. v1.4 callers should use:

```
DEFAULT_LEADER_SUCCESSION_CHAIN = [
    DEPUTY_ROBOT_ID,    # 1st: Deputy UGV (battery ≥ 20%, both SBCs healthy)
    HUB_ROBOT_ID,       # 2nd: Hub UGV (Deputy failed)
    -1,                 # 3rd: battery-max follower (runtime-picked)
    -2,                 # 4th: Limp Mode entry
]
```

Source of truth lives in
[`swarm_coordinator/swarm_coordinator.hpp`](../../ros/src/skyautonet/combat_robot_system/swarm_coordinator/include/swarm_coordinator/swarm_coordinator.hpp).

## 2. Mode transition policy

```
                ┌──────────────┐                ┌──────────────┐
                │  production  │ ◀────────────▶ │     demo     │
                └──────┬───────┘                └──────┬───────┘
                       │                               │
                       │   (denied: live robots must   │
                       ✗   not silently drop safety)   │
                       │                               │
                       ▼                               ▼
                ┌──────────────┐                ┌──────────────┐
                │   lab_test   │ ◀────────────▶ │    bench     │
                └──────────────┘                └──────────────┘

   any mode ── DEVELOPER_AUTH_TOKEN ──▶ development
   development ── reboot only ────────▶ any other mode
```

Encoded in `core.deployment.is_mode_transition_allowed(old, new)`:

| From → To       | Allowed?      | Rationale                                                  |
|-----------------|:--------------|------------------------------------------------------------|
| identity        | always        | no-op                                                      |
| production ↔ demo  | yes        | operator authority                                         |
| production → lab_test | **denied** | real robots must not silently drop into reduced-safety mode without a reboot |
| any → development | conditional | requires `DEVELOPER_AUTH_TOKEN`; safety relaxed             |
| development → any  | **denied** | dev session leaves residual relaxed state — must reboot    |
| bench ↔ lab_test   | yes        | normal lab bring-up workflow                                |
| anything else      | denied     | explicit allow-list, no implicit transitions                |

## 3. Boot-time resolution order

1. `--deployment-mode <mode>` CLI flag (highest precedence)
2. `PATROL__SYSTEM__DEPLOYMENT_MODE` env var
3. `system.deployment_mode` in base yaml
4. baked-in default = `production`

After resolution, the overlay file `<base_dir>/<base_stem>.<mode>.yaml` is
merged on top of the base yaml. A missing overlay file for any non-production
mode is a **hard error** (we refuse to silently fall back to production).

## 4. Development mode safety gate

`development` is the only mode that disables safety expectations for live
hardware. To prevent accidental entry on a real robot:

```bash
# Refused: FATAL log, exit code 2
python3 main.py --deployment-mode development

# Accepted: WARN banner displayed, hardware stubs accepted
DEVELOPER_AUTH_TOKEN=$(cat ~/.san_dev_token) \
    python3 main.py --deployment-mode development
```

The check lives in `core.deployment.validate_developer_auth()` and runs
before any process is spawned. The token's value is not validated against a
secret store — its presence alone is the gate; this is a deliberate trade-off
to keep dev-machine setup low-friction while still preventing the
"forgot-the-flag" accident on a robot.

## 5. Observability

The 1 Hz `OperationState` heartbeat carries:

- `deployment_mode` (string, matches `DeploymentMode.value`)
- `robot_id`, `robot_role`
- `leader_robot_id`, `hub_robot_id`, `deputy_chain`
- `n_alive_followers`

Consumers: WS telemetry (operator app badge), audit log (mode-change audit),
debug dashboard. To inspect from a shell:

```bash
# Equivalent of `ros2 topic echo /operation_state` for this Python platform:
python3 scripts/debug_dashboard.py | grep deployment_mode
```

## 6. Adding a new overlay

1. Create `config/system.<new_mode>.yaml` carrying only the **deltas** from
   `config/system.yaml`. Always set `system.deployment_mode: "<new_mode>"`
   explicitly so a copy-paste error is loud.
2. Add the literal value to `core.deployment.DeploymentMode`.
3. Decide the transition policy: edit `_ALLOWED_TRANSITIONS` in
   `core/deployment.py`. Default-deny is the safe choice.
4. Update the table in §1 here.
5. Add a parametrized boot test to `tests/test_deployment_mode.py`.
