# Convoy — Follower nav2 Integration (Design Proposal)

**Status:** Ratified as **DCN-2026-029** (APPROVED 2026-06-23). This file is the engineering
design source; the formal change record is `docs/04_Change_Management/DCN-2026-029_Convoy_Follower_Nav2_Integration.md`.
**P1 (leader-side reference-path publish) is implemented**; P2–P4 follow.
**Scope:** POC = 1× Unitree Go2 (leader) + 4× UGV. Layered guidance + per-robot autonomy.

## 1. Why
The POC architecture is **two layers**:
- **Global (reference):** the Go2 uses its 4D lidar to lead the formation, generates a
  **reference driving path**, and provides it to every robot **@5 Hz (0.2 s)**. Every
  robot reports its position to the Go2 **@5 Hz**. (Comm rate landed: `comm_period_s=0.2`.)
- **Local (autonomy):** the Go2 path is a **reference, not a command**. Each UGV runs its
  **own nav2** to track the reference *and* autonomously handle 돌발상황 (sudden obstacles,
  deviation, recovery).

Current `convoy_ugv` is a **simple breadcrumb pure-pursuit** (look-ahead on the leader's
recorded trail) with a reactive `avoid_override` safety net — it is **not** nav2-driven and
has no local costmap / planner / recovery. **Gap to close:** drive each follower with nav2,
using the Go2 reference as the global plan.

## 2. Architecture (2 layers)
```
 Go2 (leader)                                   each UGV (follower)
 ┌─────────────────────────────┐                ┌──────────────────────────────┐
 │ convoy_costmap (4D lidar)   │                │ local costmap (own lidar)    │
 │ convoy_coordinator          │  /convoy/      │ nav2: planner+controller     │
 │  ├ reference path/slot  ────┼─ target/r{n} ─▶│  (DWB|MPPI) + recovery       │
 │  └ formation throttle       │   @5 Hz        │  ├ follows reference         │
 │                             │◀─ report/r{n} ─┤  └ avoids 돌발 obstacle      │
 └─────────────────────────────┘   @5 Hz        └──────────────────────────────┘
   GLOBAL: route + slot + formation               LOCAL: track + obstacle + recovery
```
- **Global responsibility (Go2):** route guidance, per-robot slot (~3 m along-path), and
  formation-level avoidance (`convoy_costmap`) + formation throttle (wait for stragglers).
- **Local responsibility (per-UGV nav2):** stay on the reference within its lane; build a
  local costmap from its **own** sensors; local controller for tracking + 돌발 obstacle
  avoidance; nav2 recovery behaviors (clear-costmap / backup / spin), then re-acquire the
  reference.

## 3. Reuse, do not fork (reconciliation)
- Per-robot nav2 = **SkyHunter `san_nav2` / `san_costmap` / `san_localization`** — single-
  source. Do **not** stand up a second nav2 from CombatRobot_1/test_nav2; migrate any
  test_nav2-only asset into `san_nav2`.
- Leader reference + lidar = the merged **`convoy_coordinator` / `convoy_costmap`** —
  reuse, extend as a DCN delta. No second Go2 stack.

## 4. Interface design (leader reference → follower nav2)
The leader already publishes `/convoy/target/r{n}` (predecessor pose+vel) @5 Hz. For nav2,
add a **reference path** per follower:
- **Leader side (new):** publish `nav_msgs/Path` on `/convoy/ref_path/r{n}` @5 Hz — the
  robot's reference lane = leader breadcrumb trail, offset to the robot's slot (the
  along-path `arc` follow logic already computes this). Keep `/convoy/target/r{n}` for
  backward-compat / fallback.
- **Follower side (new node, replaces convoy_ugv's direct cmd_vel):** a thin
  `convoy_nav2_follower` that feeds the reference Path into nav2:
  - **Option A (recommended):** nav2 `FollowPath` (controller_server) on the reference Path
    — controller (DWB/MPPI) tracks it; local costmap injects 돌발 obstacles → controller
    deviates and re-converges. Simplest, keeps the leader as the global planner.
  - **Option B:** `NavigateThroughPoses` with the reference points as poses (uses nav2's
    global planner per segment — heavier, better for large detours).
  - Recommend **A** for the POC (the Go2 *is* the global planner); escalate to B only if
    a 돌발 obstacle requires a global re-route the local controller can't solve.

## 5. domain_bridge alignment (Task B)
This layering is the network-load argument:
- **Heavy data stays on-board:** each UGV's nav2 local costmap + raw lidar/cam never leave
  its board.
- **Only coordination crosses the domain boundary @5 Hz:** `/convoy/ref_path/r{n}` (leader→
  robot) + `/convoy/report/r{n}` (robot→leader). These are the **exact topics domain_bridge
  forwards**; everything else is local.
- Baseline (all-shared-domain): sensor data broadcast board-to-board. After-bridge: only the
  5 Hz coordination topics. → expect a large reduction; this is the measurement target.

## 6. 돌발상황 (contingency) handling
- Local costmap (own lidar) detects a sudden obstacle the reference path crosses.
- nav2 controller deviates within the lane; if blocked, nav2 **recovery behaviors** fire
  (clear costmap → backup → spin), then the robot **re-acquires the reference** at its slot.
- Formation tolerance: the leader's gap throttle (floor 0.5) slows the Go2 while a follower
  resolves a contingency, so the convoy stays together.

## 7. Phased plan + test mapping
1. **P1 — reference Path publish:** leader emits `/convoy/ref_path/r{n}` @5 Hz from the
   breadcrumb/slot. (No follower change yet; verify Path content + rate.)
2. **P2 — per-UGV nav2 bringup:** `san_nav2` + local costmap from the UGV lidar in the
   convoy sim (5 units). Verify each UGV navigates a static goal.
3. **P3 — reference follow:** `convoy_nav2_follower` feeds ref Path → nav2 `FollowPath`;
   retire convoy_ugv's direct cmd_vel (keep as fallback flag). Verify formation hold ~3 m.
4. **P4 — contingency:** inject a sudden obstacle on the path; verify local avoid + recovery
   + reference re-acquire, formation preserved.
5. **Test mapping:** P3/P4 → **TST S20-1…9** scenarios + **L5 Gate-1 (L5_26…33)**; not
   ad-hoc. Sensors-under-load pass/fail (lidar/cam/IMU rate + **RTF** + dropped frames) on
   **real RK3588/Linux** (WSL RTF ~10% is non-representative).

## 8. Open decisions
- **Controller:** DWB vs MPPI (MPPI smoother for dynamic following; heavier on RK3588).
- **Localization/frames:** convoy is tf2-free on `/odom_gt`; nav2 needs a tf tree
  (`map`→`odom`→`base_link`). Reconcile via `san_localization` per robot.
- **Reference offset:** keep the existing along-path `arc` slot logic (proven ~3 m) as the
  Path generator; nav2 only tracks + avoids.
- **Fallback:** keep convoy_ugv pure-pursuit behind a flag for sim/no-nav2 runs.
