# Sprint Backlog — SDD Rev.A.6 §16.2 P1 Implementation
**Project**: Swarm Platform — Q3 2026  
**Total**: 19 stories across 4 sprints (8 weeks)  
**Story Points**: 107 SP  
**Critical items (⭐)**: 3 (PTP, Audit Log, Health Monitoring)

---

## Epic Overview

| Epic | Sprint | Stories | Story Points |
|---|---|---|---|
| **Map + Swarm Core** (EPIC-1) | S1 (Wk 1-2) | 5 | 26 |
| **Predictive Control + Hub UGV** (EPIC-2) | S2 (Wk 3-4) | 4 | 34 |
| **Operator Interface** (EPIC-3) | S3 (Wk 5-6) | 5 | 24 |
| **Critical Infrastructure** (EPIC-4) | S4 (Wk 7-8) | 5 | 23 |

---

## Map + Swarm Core (EPIC-1) — S1 (Wk 1-2)

_OSM PBF + SLAM 3-layer fusion + 5-Tier escape system + Breadcrumb buffer. Foundation for autonomous obstacle handling and cumulative mapping._

### `P1-6` OSM PBF rasterizer (20 cm cell)

**Story Points**: 5 | **Priority**: Highest | **SDD**: §4.7.1, §10.5

**Components**: mapping  
**Labels**: mapping, P1-critical

**Description**:

Implement OsmStaticLayer that loads .pbf files and rasterizes to 20 cm occupancy grid. Replaces HD map dependency. Used as L1 (Static) layer in the 3-layer cost map fusion (SDD §4.7).

Implementation:
- mapping/osm_static_layer.py — OsmStaticLayer class
- pyosmium for PBF parse, GDAL/rasterio for raster ops
- bbox extraction → 20 cm grid (~50 MB for 10×10 km)
- Cost mapping: building/water=1.0, road=0.2, unknown=0.5

**Acceptance Criteria**:

- [ ] OsmStaticLayer.load_from_pbf(path, bbox) returns numpy 2D array
- [ ] Cell size 20 cm verified via shape check
- [ ] building/water cells have cost ≥ 0.95
- [ ] 10×10 km PBF rasterizes within 60 s on RK3588J
- [ ] Unit tests: 5+ cases (empty area, urban, water, road)

---

### `P1-7` SLAM Persistent Layer with Bayesian update (α=0.95)

**Story Points**: 5 | **Priority**: Highest | **SDD**: §4.7.5

**Components**: mapping  
**Labels**: mapping, P1-critical, algorithm

**Blocked by**: P1-6

**Description**:

L2 layer of 3-layer cost map. Receives LiDAR scans and applies Bayesian occupancy update with persistence parameter α=0.95.

P_new(cell) = α·P_prev(cell) + (1-α)·P_observed(cell)

Implementation:
- mapping/slam_persistent_layer.py — SlamPersistentLayer class
- Tile-based storage for memory efficiency
- Reset on RTK FIXED reacquisition

**Acceptance Criteria**:

- [ ] Bayesian update converges to 0.95+ after 20 observations of obstacle
- [ ] Free → occupied transition takes ~2 s at 10 Hz
- [ ] Reset() snaps cells to 0.5 (uncertain)
- [ ] Unit tests: 5 cases including hysteresis verification

---

### `P1-8` Anomaly detection (OSM-SLAM mismatch threshold 0.7)

**Story Points**: 3 | **Priority**: Highest | **SDD**: §4.7.5, §15.6

**Components**: mapping  
**Labels**: mapping, P1-critical, perception

**Blocked by**: P1-7

**Description**:

Compare L1 OSM Static and L2 SLAM Persistent — when difference > 0.7, publish AnomalyEvent for operator notification (§4.7.5).

Cases:
- OSM=Free, SLAM=Occupied → unmapped_obstacle
- OSM=Building, SLAM=Free → structure_changed
- OSM=Road, SLAM=Occupied → road_blocked

**Acceptance Criteria**:

- [ ] detect_anomaly() identifies 3 categories correctly
- [ ] False positive rate < 5% in static scenes (no real obstacles)
- [ ] AnomalyEvent published with type, location, confidence
- [ ] Unit tests: 8 cases covering each transition type

---

### `P1-1` 5-Tier escape system (T0/T1.5/T1-T4) state machine

**Story Points**: 8 | **Priority**: Highest | **SDD**: §6.7

**Components**: swarm  
**Labels**: swarm, P1-critical, fsm

**Description**:

Implement Tier transition logic per SDD §6.7. Each follower has a tier state that drives behavior:

  T0 PREDICTIVE — track 1s prediction
  T1.5 AUTO_REROUTE — local ±2m avoidance
  T1 NORMAL — body-frame PID
  T2 CATCH_UP — 1.2x speed
  T3 HARD_CATCH_UP — max speed
  T4 BREADCRUMB_RECOVERY — follow leader's recent path

Hysteresis required to prevent chattering.

**Acceptance Criteria**:

- [ ] TierManager state machine with 6 states + transitions
- [ ] T2→T1 hysteresis at δ ≤ 1.2 d₀, T3→T2 at δ ≤ 1.5 d₀
- [ ] No chattering: 100 cycles → ≤ 2 transitions in steady state
- [ ] Tier exposed in telemetry message (used by Android UI color coding)
- [ ] Unit tests: 12+ cases covering all transitions + hysteresis

---

### `P1-2` Breadcrumb buffer (1.2 km, 1m or 0.5s sample)

**Story Points**: 5 | **Priority**: Highest | **SDD**: §6.7.1

**Components**: swarm  
**Labels**: swarm, P1-critical

**Blocked by**: P1-1

**Description**:

Leader records position every 1m moved or 0.5s elapsed. Buffer holds ~1200 entries (~60 KB). T4 followers use breadcrumbs to retrace leader's safe path.

DDS topic: sw/breadcrumb (P0 RELIABLE, leader broadcast).

**Acceptance Criteria**:

- [ ] BreadcrumbBuffer class with circular buffer (~1200 entries)
- [ ] Sample triggered by distance ≥ 1m OR time ≥ 0.5s
- [ ] DDS publish at 10 Hz when leader is moving
- [ ] T4 follower can retrace from any past breadcrumb to leader
- [ ] Unit tests: 5 cases covering buffer overflow, T4 retrace

---

## Predictive Control + Hub UGV (EPIC-2) — S2 (Wk 3-4)

_1-second predictive broadcast (Hybrid A*) + Leader rollback policy + Hub UGV adapter + Modified Raft election. Core swarm coordination._

### `P1-3` 1-second predictive broadcast (Hybrid A* lookahead)

**Story Points**: 13 | **Priority**: Highest | **SDD**: §7.3

**Components**: mission, swarm  
**Labels**: swarm, P1-critical, algorithm, differentiator

**Blocked by**: P1-13

**Description**:

Leader publishes its predicted position at t+1.0 every 100 ms (10 Hz). NOT linear extrapolation — uses Hybrid A* path planner's lookahead. Followers compute their P_F_i(t+1) using V-shape offset and arrive within 850 ms via Nav2 local planner.

This is the central differentiator from the reference document (§7.3).

**Acceptance Criteria**:

- [ ] PredictiveLeader runs at 10 Hz
- [ ] hybrid_a_star_lookahead(pose, goal, costmap, 1.0s) returns P_L(t+1)
- [ ] FollowerTargetMessage includes valid_until field
- [ ] Follower reaches P_F_i within 850 ms in 95% of cases
- [ ] KPP §2.1.1 「control latency ≤ 150 ms」 met in integration test
- [ ] Unit tests: 8+ cases including curved leader path

---

### `P1-4` Leader rollback policy (≥50% follower struggling → retreat + replan)

**Story Points**: 5 | **Priority**: Highest | **SDD**: §7.6

**Components**: mission  
**Labels**: swarm, P1-critical, user-decision

**Blocked by**: P1-1, P1-3

**Description**:

When 50%+ followers are in T3/T4 (struggling), leader stops + retreats 30s to last stable position + Hybrid A* re-plan with avoid_corridors constraint. If new path also fails, alert operator (§7.6, user decision #6).

**Acceptance Criteria**:

- [ ] LeaderRollbackChecker runs at 1 Hz
- [ ] ratio ≥ 0.5 → leader stops + retreat 30s
- [ ] Hybrid A* re-plan with avoid_corridors=[recent_failed_path]
- [ ] If no viable path → AlertEvent("NO_VIABLE_PATH")
- [ ] ratio < 0.3 hysteresis: exit rollback mode
- [ ] Unit tests: 5 cases covering 50% boundary, no-viable-path

---

### `P1-5` Hub UGV adapter (SLAM fusion + comm gateway + leader takeover)

**Story Points**: 8 | **Priority**: Highest | **SDD**: §2.2, §6.5

**Components**: adapters  
**Labels**: adapter, P1-critical, hub-ugv

**Blocked by**: P1-7

**Description**:

New process for tracked Hub UGV platform. Three roles:
1. SLAM fusion: receives sw/follower_map from each follower, merges per    multirobot_map_merge pattern, broadcasts sw/shared_map
2. Comm gateway: relays DDS traffic + LTE backhaul
3. Leader takeover: priority highest among non-Go2 nodes

**Acceptance Criteria**:

- [ ] HubUgvAdapter process with role='hub' in config/system.yaml
- [ ] SLAM fusion from 3 followers verified via test
- [ ] Leader takeover after election: pose published within 5 s
- [ ] LTE backhaul mode when WiFi6 mesh unavailable
- [ ] Unit tests: 10+ cases (init, fusion, takeover, gateway)

---

### `P1-13` Modified Raft + DDS Liveliness QoS

**Story Points**: 8 | **Priority**: Highest | **SDD**: §6, §6.5

**Components**: swarm  
**Labels**: swarm, P1-critical, election, kpp

**Description**:

Replace simple priority broadcast with Modified Raft election + DDS Liveliness QoS (lease 2s, deadline 200ms × 3) for Leader detection.

Priority 4-tuple: [Battery 40% | RTK 30% | Sensor 20% | RobotID 10%]

Recovery time goal ≤ 10 s (KPP §2.1.1).

**Acceptance Criteria**:

- [ ] DDS Liveliness QoS configured (lease 2s, deadline 200ms × 3)
- [ ] Modified Raft election: term, vote, priority broadcast
- [ ] Leader recovery from kill -9 within 10 s (KPP)
- [ ] Tested with 5-robot Gazebo (kill leader, verify takeover)
- [ ] Unit tests: 8+ cases (election, vote, term increment)

---

## Operator Interface (EPIC-3) — S3 (Wk 5-6)

_BLE GATT API + WebSocket JSON-RPC + Telemetry 30 Hz + Anomaly Event push + operational mode rules. Android app integration surface._

### `P1-9` BLE GATT API (UUID 0xFF00-0xFF05)

**Story Points**: 8 | **Priority**: Highest | **SDD**: §15.2

**Components**: control  
**Labels**: control, P1-critical, ble, android

**Description**:

Six characteristic UUIDs per SDD §15.2:
  0xFF00 R: Device info (32 byte)
  0xFF01 W: WiFi provisioning JSON
  0xFF02 N: Phase status string
  0xFF03 W: Direct cmd_vel (BLE fallback)
  0xFF04 N: Status notification 5 Hz
  0xFF05 W: PIN auth challenge

**Acceptance Criteria**:

- [ ] All 6 UUIDs implemented in BleControl process
- [ ] 0xFF01 provisioning: triggers WiFi connect within 5 s
- [ ] 0xFF04 notifications at 5 Hz when subscribed
- [ ] PIN auth via 0xFF05 with 32-byte challenge-response
- [ ] Tested via BleSim TCP simulator (no hardware needed)

---

### `P1-10` WebSocket JSON-RPC API (11 method)

**Story Points**: 8 | **Priority**: Highest | **SDD**: §15.3

**Components**: control  
**Labels**: control, P1-critical, websocket, android

**Description**:

WS server on :5001, JSON-RPC 2.0:
  formation.set, mission.start, mission.abort, telemetry.subscribe,
  state.set, leader.takeover, map.upload_brief,
  + notifications: telemetry, anomaly, alert
(§15.3)

**Acceptance Criteria**:

- [ ] All 8 methods + 3 notification types implemented
- [ ] JSON-RPC 2.0 compliance: id correlation, error codes
- [ ] Subscribe/unsubscribe to telemetry stream
- [ ] Concurrent client support (≥ 2 simultaneous)
- [ ] Unit + integration tests: 15+ cases

---

### `P1-11` Telemetry 30 Hz format (robots[] + tier color codes)

**Story Points**: 3 | **Priority**: Highest | **SDD**: §15.5

**Components**: control  
**Labels**: control, P1-critical, telemetry

**Blocked by**: P1-10, P1-1

**Description**:

30 Hz broadcast over WS. Includes all robots' pose, battery, tier, RTK quality. Used by Android UI for color-coded tier display.

Tier colors per §15.5: T0 cyan, T1.5 purple, T1 green, T2 yellow, T3 orange, T4 red+blink.

**Acceptance Criteria**:

- [ ] 30 Hz publish rate sustained without packet loss
- [ ] Schema includes all required fields (robots[], swarm_health)
- [ ] Tier field included for each robot
- [ ] JSON serialization < 5 ms per snapshot
- [ ] End-to-end latency (robot → app) < 50 ms

---

### `P1-12` Anomaly Event push format with image_url + severity

**Story Points**: 2 | **Priority**: Highest | **SDD**: §15.6

**Components**: control, comm  
**Labels**: control, P1-critical, perception

**Blocked by**: P1-10, P1-8

**Description**:

Anomaly notifications pushed to Android app. Includes type, location, confidence, severity, optional image_url for snapshot retrieval via C4 HTTP channel (§15.6).

**Acceptance Criteria**:

- [ ] AnomalyEvent JSON schema matches §15.6
- [ ] image_url generated for ai_detection type
- [ ] Severity field (info/warn/critical) properly set
- [ ] Image accessible via HTTP C4 within 1 s
- [ ] Unit tests: 5 cases covering each anomaly type

---

### `P1-14` Operational mode single rule (Test 3m / Move 5m / Assault 15m)

**Story Points**: 3 | **Priority**: Highest | **SDD**: §7.1, §15.7

**Components**: mission  
**Labels**: mission, P1-critical, user-decision

**Blocked by**: P1-10

**Description**:

Single-button presets per §7.1 + user decision #7:
  Test/Dev: d=3 m, 60°, 1.0 m/s forced (PIN required)
  Narrow:   d=3 m, 40°, 1.3 m/s
  Recon:    d=5 m, 90°, 1.3 m/s ← default
  Wide:     d=7 m, 120°, 1.3 m/s
  Assault:  d=15 m, 60°, 1.3 m/s

**Acceptance Criteria**:

- [ ] 5 presets exposed via formation.set RPC
- [ ] dev_mode=true forces leader.max_speed = 1.0 m/s
- [ ] PIN auth required for dev_mode (via 0xFF05)
- [ ] Transition between presets smooth (3-5 s)
- [ ] Unit tests: 5 cases for each preset

---

## Critical Infrastructure (EPIC-4) — S4 (Wk 7-8)

_PTP time sync, audit log, health monitoring, geofence, battery RTH. Cross-cutting infrastructure required for KPP compliance and field deployment._

### `P1-15` ⭐ Time Synchronization (PTP / GNSS PPS)

**Story Points**: 5 | **Priority**: Highest | **SDD**: §5.8

**Components**: core  
**Labels**: infrastructure, P1-critical, infra-x1, ptp

**Description**:

PTP IEEE-1588 (linuxptp) with Hub UGV as master, all followers as slaves. Target ±1 ms accuracy. GNSS PPS for absolute reference (±100 ns).

WITHOUT THIS, 1-second predictive broadcast (P1-3) is inaccurate and KPP §2.1.1 ≤ 150 ms is violated. CRITICAL.

**Acceptance Criteria**:

- [ ] ptp4l + phc2sys configured on all robots
- [ ] Hub UGV is best-clock master (GNSS PPS lock)
- [ ] All slaves synchronized within ±1 ms verified
- [ ] Mission start PreCheck blocks if offset > 5 ms
- [ ] Fallback: quorum consensus when master fails (≤ 5 ms)

---

### `P1-16` ⭐ Audit Log (hash-chained jsonl)

**Story Points**: 5 | **Priority**: Highest | **SDD**: §9.7

**Components**: core, safety  
**Labels**: infrastructure, P1-critical, infra-x2, compliance

**Description**:

Append-only JSON Lines audit trail with cryptographic hash chain. Records mission lifecycle, FSM transitions, permission events, safety events, AI decisions, election, time sync events.

Required for KRIT verification + military security audit (§9.7).

**Acceptance Criteria**:

- [ ] AuditLogger writes to /var/log/patrol/audit/<date>.jsonl
- [ ] Each entry includes prev_hash + self_hash (sha256)
- [ ] Tampering detection: modify any line → chain validation fails
- [ ] Mission end: leader collects all robots' logs → patrol_server
- [ ] Retention: 7 GB rolling buffer (~1 year)

---

### `P1-17` ⭐ Health Monitoring + Degraded Mode FSM

**Story Points**: 8 | **Priority**: Highest | **SDD**: §9.6

**Components**: safety  
**Labels**: infrastructure, P1-critical, infra-x3, safety

**Blocked by**: P1-15

**Description**:

Per-component health checks (RTK, LiDAR, camera, IMU, LTE, mesh, battery, RK3588J temp). Auto-transition to DEGRADED_L1/L2/CRITICAL with appropriate behavior changes. 1 Hz publish on sw/health.

Without this, sensor failures force full mission abort (§9.6).

**Acceptance Criteria**:

- [ ] All 10 components monitored per §9.6.1
- [ ] Degraded FSM (NORMAL → L1 → L2 → CRITICAL)
- [ ] Hysteresis: degrade immediate, recover after 30 s
- [ ] sw/health published at 1 Hz with structured JSON
- [ ] Unit tests: 12+ cases for transitions

---

### `P1-18` Geofence violation detection (E3 event)

**Story Points**: 3 | **Priority**: Highest | **SDD**: §9.3

**Components**: safety  
**Labels**: safety, P1-critical

**Description**:

Per-mission geofence polygon defined in config. SafetyProcess checks robot pose 1 Hz. Buffer (2 m) → operator warning, hard_stop (0.5 m) → force stop + Stand mode (§9.3).

**Acceptance Criteria**:

- [ ] Polygon-in-point check at 1 Hz for each robot
- [ ] Buffer warn at ≤ 2 m, hard stop at ≤ 0.5 m
- [ ] Operator alert + UI indication on violation
- [ ] Override only in dev_mode (PIN-authenticated)
- [ ] Unit tests: 5 cases (interior, buffer, boundary)

---

### `P1-19` Battery 20% → automatic RTH trigger

**Story Points**: 2 | **Priority**: Highest | **SDD**: §8

**Components**: safety, mission  
**Labels**: safety, P1-critical

**Blocked by**: P1-17

**Description**:

When battery SoC drops below 20%, automatically trigger RTH (return-to-home). Below 10%, force emergency Stand mode (§8).

**Acceptance Criteria**:

- [ ] BatteryMonitor publishes SoC at 1 Hz
- [ ] 20% threshold triggers RTH behavior tree node
- [ ] 10% threshold forces Stand + alert
- [ ] Operator can override RTH only in dev_mode
- [ ] Unit tests: 4 cases (20%, 10%, override, recovery)

---

