# P2 Sprint Backlog — SDD Rev.A.6 §16.3 (Q4 2026)

**Project**: Swarm Platform — Q4 2026  
**Total**: 14 stories across 3 sprints (6 weeks)  
**Story Points**: 68 SP  
**P1 dependencies**: P1-3 (S2), P1-6/P1-7/P1-8 (S1), P1-13/P1-15 (S4)

---

## Epic Overview

| Epic | Sprint | Stories | Story Points |
|---|---|---|---|
| **Formations + Operational Modes** (EPIC-5) | S5 (Wk 9-10) | 4 | 20 |
| **Map + Lab Development** (EPIC-6) | S6 (Wk 11-12) | 5 | 19 |
| **Tooling + Compliance** (EPIC-7) | S7 (Wk 13-14) | 5 | 29 |

---

## Formations + Operational Modes (EPIC-5) — S5 (Wk 9-10)

_9-formation library + Hungarian slot assignment + auto terrain switch + M4 assault mode. Completes the swarm formation framework started in S1/S2._

### `P2-1` 9 formations library (column, line, V, diamond, echelon, box, vee, free)

**Story Points**: 8 | **Priority**: High | **SDD**: §7.2

**Components**: mission  
**Labels**: mission, P2, formation

**Blocked by**: P1-3

**Description**:

Implement all 9 formation types per SDD §7.2 + reference SAN-SDD-SWARM-001 v1.0 §5:

1. 1열 종대 (Column)
2. 1열 횡대 (Line)
3. V형 (V-shape)
4. 다이아몬드 (Diamond)
5. 에셸론 좌 (Echelon Left)
6. 에셸론 우 (Echelon Right)
7. 박스 (Box)
8. 역 V형 (Vee Inverted)
9. 자유 산개 (Free Spread)

Each formation provides offset_xy generator for given d/theta/N parameters.
Replace V-shape-only formation_offsets dict in P1-3 PredictivePlanner.

**Acceptance Criteria**:

- [ ] FormationLibrary class with 9 formation types
- [ ] compute_offsets(formation_type, n_followers, d_m, theta_deg) → list of (x, y)
- [ ] All 9 formations validated visually (matplotlib output in tests/data/)
- [ ] Hand-off to PredictivePlanner via formation_offsets dict
- [ ] Unit tests: 18+ cases (9 types × 2 sizes each)

---

### `P2-2` Hungarian slot assignment (formation transition)

**Story Points**: 5 | **Priority**: High | **SDD**: §15.4

**Components**: swarm  
**Labels**: swarm, P2, algorithm

**Blocked by**: P2-1

**Description**:

When formation changes, optimally assign each follower to nearest target slot to minimize total movement. Currently P1-1 uses simple index matching which causes unnecessary crossings during transitions.

Implementation:
- swarm/slot_assignment.py — HungarianAssigner class
- scipy.optimize.linear_sum_assignment for cost matrix
- Cost = euclidean distance between current and target positions

**Acceptance Criteria**:

- [ ] HungarianAssigner.assign(current_positions, target_slots) → mapping
- [ ] Returns optimal pairing minimizing total distance
- [ ] Handles N=2..9 followers
- [ ] Smooth transition: no two followers swap paths
- [ ] Unit tests: 6+ cases (small/large groups, edge cases)

---

### `P2-3` Auto terrain switch (자동 종대 전환)

**Story Points**: 5 | **Priority**: High | **SDD**: §7.5

**Components**: mission  
**Labels**: mission, P2, auto

**Blocked by**: P2-1, P1-3

**Description**:

Auto-switch to column formation when terrain becomes restrictive (SDD §7.5).

Triggers (any one):
- LiDAR passable width < D_across + 2 m safety margin
- Cost map: untraversable cells within 0.5 m on both sides
- DEM: elevation change > 1 m within 5 m laterally
- Tier 4 follower ratio ≥ 1/3

Operator notification 5 s before auto-switch (manual override).

**Acceptance Criteria**:

- [ ] TerrainAnalyzer evaluates 4 trigger conditions at 1 Hz
- [ ] 5-second operator confirmation window with manual override
- [ ] Auto-revert to previous formation on terrain clearance
- [ ] Hysteresis: 10 s clearance required before reverting
- [ ] Unit tests: 8+ cases (each trigger + override + revert)

---

### `P2-8` M4 assault mode (d=15m, 60°)

**Story Points**: 2 | **Priority**: High | **SDD**: §7.8

**Components**: mission  
**Labels**: mission, P2, user-decision

**Blocked by**: P1-14

**Description**:

Add M4 assault mission type to tactical_missions.yaml schema. User decision #7: 돌격 = 15 m / 60°.

Builds on P1-14 operational modes — wires assault preset into mission framework.

**Acceptance Criteria**:

- [ ] tactical_missions.yaml supports type: assault
- [ ] M4 mission spec validates d=15m, theta=60°
- [ ] Behavior tree handles assault mode with formation_set
- [ ] Unit tests: 3+ cases (config validation, BT integration)

---

## Map + Lab Development (EPIC-6) — S6 (Wk 11-12)

_AHD camera adapter for Lab dev (replaces IMX678 for PoC), hybrid OSM-SLAM static layer update, SRTM 30m DEM, mission_brief.sh, iptime mesh router config._

### `P2-4` AHD camera adapter (Lab dev mode)

**Story Points**: 5 | **Priority**: High | **SDD**: §2.2

**Components**: adapters  
**Labels**: adapter, P2, user-decision

**Description**:

Add adapters/ahd_camera.py for FHD AHD camera (Lab dev mode, IMX678 alternative).

Per user decision #2: Phase B-D uses inexpensive AHD modules ($30) instead of Sony IMX678 ($300+). V4L2 driver interface preserved (USB or AHD-to-USB capture).

Implementation:
- adapters/ahd_camera.py — AhdCameraAdapter class
- Same interface as IMX678Adapter (drop-in replacement)
- config flag: payload.camera_type = 'imx678' | 'ahd'

**Acceptance Criteria**:

- [ ] AhdCameraAdapter implements same interface as IMX678Adapter
- [ ] Configurable resolution: 1080p default, 720p option
- [ ] V4L2 device path configurable (/dev/video0 or USB capture)
- [ ] Streaming/AI fan-out works identically
- [ ] Unit tests: 5+ cases (init, capture, fan-out)

---

### `P2-5` Hybrid static layer update (SLAM → OSM)

**Story Points**: 5 | **Priority**: High | **SDD**: §4.7.6

**Components**: mapping  
**Labels**: mapping, P2, algorithm

**Blocked by**: P1-6, P1-7

**Description**:

When SLAM persistent layer accumulates high confidence (>0.9, 100+ observations), update OSM static layer with the new info (SDD §4.7.6).

This addresses OSM stale data — point of difference becomes feedback into the prior.

Algorithm:
1. SLAM persistent cell count_observations >= 100
2. SLAM persistent confidence >= 0.9
3. update OsmStaticLayer.cell_value = SLAM value
4. Mark cell as 'hybrid_updated' for audit
5. Mission end: upload updated OSM tile to patrol_server

**Acceptance Criteria**:

- [ ] SLAM cell observation counter implemented
- [ ] Hybrid update threshold (0.9 conf, 100 obs) configurable
- [ ] OsmStaticLayer.update_cell() called on threshold
- [ ] Updated cells marked in audit log (P1-16)
- [ ] Unit tests: 6+ cases (threshold boundary, count, audit)

---

### `P2-6` SRTM 30m DEM loader

**Story Points**: 3 | **Priority**: Medium | **SDD**: §10.5.1

**Components**: mapping  
**Labels**: mapping, P2, terrain

**Blocked by**: P1-6

**Description**:

Load NASA SRTM 30m elevation tiles for terrain analysis.

Used by:
- P2-3 Auto terrain switch (DEM elevation triggers)
- Mission planning (slope-aware path)
- Geofence 3D extension

Implementation:
- mapping/srtm_dem_loader.py
- HGT file format (16-bit big-endian, 1201×1201 per tile)
- Bilinear interpolation for arbitrary lat/lon

**Acceptance Criteria**:

- [ ] SrtmDemLoader.load_tile(lat_deg, lon_deg) → 1201×1201 grid
- [ ] elevation_at(lat, lon) bilinear interpolation
- [ ] Korea coverage: ~50 MB onboard preload
- [ ] Slope computation (gradient magnitude) supported
- [ ] Unit tests: 5+ cases (load, interp, slope)

---

### `P2-7` mission_brief.sh — LTE map download automation

**Story Points**: 3 | **Priority**: Medium | **SDD**: §10.5.2

**Components**: scripts  
**Labels**: tooling, P2, shell

**Blocked by**: P1-6, P2-6

**Description**:

Bash script to download OSM PBF + SRTM DEM for operation area before mission start (SDD §10.5.2).

Workflow:
1. Operator inputs polygon (lat/lon corners or KML import)
2. Compute bbox
3. wget OSM PBF (Geofabrik)
4. osmium extract bbox
5. Download SRTM HGT tiles for bbox
6. Rasterize PBF → 20 cm grid
7. Save to /opt/patrol_maps/<mission_id>/
8. SCP to all robots in mesh

**Acceptance Criteria**:

- [ ] scripts/mission_brief.sh accepts bbox or KML input
- [ ] Idempotent: re-runs skip already-downloaded files
- [ ] All robots in mesh receive same map data via rsync
- [ ] Total time < 5 min for 10×10 km area on 4G/5G
- [ ] Integration test: end-to-end with sample area

---

### `P2-9` iptime mesh router auto-config

**Story Points**: 3 | **Priority**: Medium | **SDD**: §2.2

**Components**: scripts  
**Labels**: network, P2, user-decision

**Description**:

Bash script to provision iptime AX2004M / AX5000M mesh router for first-boot (user decision #3 + SDD §2.2).

Auto-config:
- SSID + WPA3-SAE password
- EasyMesh enable
- IGMP snooping v2/v3 (multicast for DDS)
- 5 GHz fallback (6 GHz preferred for backhaul)
- DHCP range 192.168.42.10-50

**Acceptance Criteria**:

- [ ] scripts/iptime_provision.sh applies all config via web API
- [ ] Idempotent: re-running doesn't break existing config
- [ ] Verifies IGMP snooping enabled (curl probe)
- [ ] Documented for AX2004M + AX5000M models
- [ ] Manual fallback: web UI step-by-step in doc/

---

## Tooling + Compliance (EPIC-7) — S7 (Wk 13-14)

_OTA update mechanism, configuration management with snapshots, calibration tools, KPP auto-measurement, AI permission invariants verification._

### `P2-10` OTA Update mechanism (A/B partition)

**Story Points**: 8 | **Priority**: High | **SDD**: §10.6.1

**Components**: scripts, core  
**Labels**: infrastructure, P2, infra-x4

**Description**:

Phase D+ field-deployable firmware/model/config update (SDD §10.6).

Architecture:
- A/B partition switch with auto-rollback on boot failure
- digital signature verification (gpg + pre-registered keys)
- SHA256 checksum + dependency check (kernel ABI, DDS lib)
- Operator confirms, robot downloads via LTE/WiFi6
- Mission-blocking: no update during active mission
- Rolling update: 1 robot at a time in swarm

**Acceptance Criteria**:

- [ ] scripts/ota_update.sh handles full A/B partition flow
- [ ] Manifest validation (sha256, signature, kernel ABI)
- [ ] Auto-rollback: 5-min stability window, then commit
- [ ] Mission-active block: returns error during active mission
- [ ] Rolling update: ensures only 1 robot per swarm at a time
- [ ] Integration test with mock OTA server

---

### `P2-11` Configuration Management (snapshot + rollback)

**Story Points**: 5 | **Priority**: High | **SDD**: §10.6.3

**Components**: core  
**Labels**: infrastructure, P2, infra-x5

**Blocked by**: P2-10, P1-16

**Description**:

Mission-level config snapshots with audit trail + rollback support (SDD §10.6.3).

Per-mission snapshot:
- system.yaml, tactical_missions.yaml, geofence.yaml
- model versions (.rknn)
- NTRIP credentials (encrypted)
- Stored under git stash pattern

Lock during mission (immutable). On abort/complete, audit log entry.

**Acceptance Criteria**:

- [ ] core/config_manager.py — ConfigManager with snapshot()
- [ ] Mission start: snapshot all config files + version
- [ ] Mission active: writes blocked (raises immutable error)
- [ ] Mission end: snapshot archived + linked in audit log
- [ ] rollback_to(mission_id) restores config from snapshot
- [ ] Unit tests: 8+ cases (snapshot, immutable, rollback)

---

### `P2-12` Calibration tools (IMU/camera/RTK)

**Story Points**: 8 | **Priority**: Medium | **SDD**: §16.3

**Components**: scripts  
**Labels**: tooling, P2, infra-x8

**Description**:

Calibration utility scripts for sensor alignment (SDD §16.3 X8).

Scripts:
- IMU boresight (gravity alignment, ZUPT)
- Camera intrinsics (chessboard or charuco)
- LiDAR-IMU extrinsic (hand-eye)
- RTK base station survey-in alignment
- Output: YAML calibration files in /etc/patrol/calibration/

**Acceptance Criteria**:

- [ ] scripts/calibrate_imu.py — gravity + ZUPT bias
- [ ] scripts/calibrate_camera.py — chessboard intrinsics
- [ ] scripts/calibrate_lidar_imu.py — hand-eye extrinsic
- [ ] scripts/calibrate_rtk_base.py — survey-in
- [ ] Output: /etc/patrol/calibration/<sensor>.yaml
- [ ] Documentation: doc/calibration_procedure.md

---

### `P2-13` KPP 5종 자동 측정 (CI integration)

**Story Points**: 5 | **Priority**: High | **SDD**: §5.6.5

**Components**: tests, scripts  
**Labels**: testing, P2, kpp, ci

**Blocked by**: P1-3, P1-13, P1-15

**Description**:

Automate KPP measurement in CI pipeline (SDD §5.6.5).

5 KPPs:
1. 대열 유지 평균 오차 ≤ 2 m
2. 근접 위험 회피 ≤ 300 ms
3. 군집 제어 통신 지연 ≤ 150 ms
4. 리더 이탈 재구성 ≤ 10 s
5. 집결 성공률 ≥ 95%

Each runs as integration test on RK3588J board (or Gazebo) and emits metric to .github/workflows/kpp_report.yml. Failure blocks PR merge.

**Acceptance Criteria**:

- [ ] tests/kpp/ — 5 integration test scripts
- [ ] .github/workflows/kpp.yml — runs on PR/main
- [ ] kpp_report.json artifact with measured values
- [ ] PR comment with PASS/FAIL summary
- [ ] Failures block merge (status check)

---

### `P2-14` AI 권한 분리 invariants 자동 검증

**Story Points**: 3 | **Priority**: High | **SDD**: §8

**Components**: safety, core  
**Labels**: safety, P2, compliance

**Blocked by**: P1-8, P1-16

**Description**:

Verify AI permission separation invariants (SDD §8) at runtime.

Invariants:
- AI detection results never directly invoke cmd_vel changes
- All AI outputs route through Mission BT or operator alert only
- DDS topic ACL: AI process can publish to anomaly_events but not cmd_vel

Implementation: runtime guard in queues.cmd_vel publisher rejects messages with source='ai_detection' (audit logged).

**Acceptance Criteria**:

- [ ] core/permission_guard.py — PermissionGuard class
- [ ] Runtime check: cmd_vel publisher rejects AI-source messages
- [ ] Audit log entry on any invariant violation
- [ ] DDS XML profile defines ACL per process role
- [ ] Unit tests: 6+ cases (allowed/blocked combinations)

---

## P1 vs P2 Comparison

| Metric | P1 (Q3) | P2 (Q4) |
|---|---|---|
| Stories | 19 | 14 |
| Story Points | 107 | 68 |
| Sprints | 4 | 3 |
| Critical (⭐) | 3 (PTP, Audit, Health) | 0 |
| Focus | KPP critical infra | Standard features + tooling |
| Duration | 8 weeks | 6 weeks |
| Phase | B-D | D-E |

