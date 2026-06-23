# 4-Layer Local Cost Map (SAN v1.3 §6.4 / §9.6)

> Source of truth:
> - `mapping/obstacle_layer.py` (L2)
> - `mapping/traversability_layer.py` (L3)
> - `mapping/inflation_layer.py` (L4)
> - `mapping/cost_map.py` (compositor + PNG codec)
> - `core/messages.py` (cost-cell constants, `CostMapUpdate`)
> - `config/system.yaml` § `cost_map`

## 1. Layer stack

The master cost map is composited from four layers using the
**worst-cost-wins** rule (per-cell `max`):

| Layer | Class | Cost values |
|---|---|---|
| L1 static          | OSM static + SLAM persistent (existing PHASE 0 layers) | 0 / lethal-from-static |
| L2 obstacle        | `ObstacleLayer`   | 0 / **200** / **254** |
| L3 traversability  | `TraversabilityLayer` (slope + ditch) | 0 / **100** (slope warn) / **254** |
| L4 inflation       | `InflationLayer`  | 0 .. 200 (exp falloff) |

Cell-value constants live in `core.messages`:
`COST_FREE = 0`, `COST_WARN_LOW = 100`, `COST_WARN_HIGH = COST_WARN = 200`,
`COST_LETHAL = 254`, `COST_UNKNOWN = 255`.

## 2. UGV chassis-derived thresholds

Every threshold below traces back to the UGV survivability spec
(SAN v1.3 §4.5). Do **not** tune them without coordinating with the
chassis / payload teams.

| Threshold | Value | Source |
|---|---|---|
| Obstacle lethal height | **235 mm** | UGV max step-over |
| Obstacle warn height   | 200 mm     | high-risk band      |
| Slope lethal           | **30°**    | UGV climbing limit  |
| Slope warn             | 25°        |                     |
| Ditch lethal width     | **220 mm** | UGV crossing limit  |
| Inflation radius       | **1.0 m**  | chassis-width / 2 + margin |
| Chassis size           | 1300 × 850 mm | mechanical spec |
| Max speed              | **10 km/h (2.78 m/s)** (v1.5) | AVTBOT TinS-13 chassis HW spec; SDD-SWARM v1.5 §4.5 |
| LiDAR mount height     | 500 mm above ground | Robosense E1 mount |
| LiDAR forward dead-zone | 250 mm  | chassis self-occlusion |

## 3. Grid geometry

```
14 m × 14 m square, 50 mm cells → 280 × 280 master grid
   robot at center (140, 140), facing +X (rows decrease)
   forward coverage = 7 m (KPP minimum)
```

A `CostMapUpdate` carries the resolved geometry: `width`, `height`,
`resolution_m`, `origin_xy` (world coord of grid (0, 0)).

## 4. Acceptance gates (verified by tests)

| Gate | Test |
|---|---|
| obstacle 200 / 235 / 270 mm → 200 / 254 / 254 | `test_cost_thresholds.test_obstacle_height_classification` |
| slope 25° / 30° / 35° → 100 / 254 / 254       | `test_cost_thresholds.test_traversability_slope_classification` |
| ditch 150 / 220 / 300 mm → free / 254 / 254   | `test_cost_thresholds.test_traversability_ditch_classification` |
| inflation 1.0 m radius                        | `test_cost_thresholds.test_inflation_radius` |
| master grid 14 × 14 m, 280 × 280              | `tests/kpp/test_cost_map_coverage.test_grid_size_280x280_at_50mm_resolution` |
| forward coverage ≥ 7 m                        | `tests/kpp/test_cost_map_coverage.test_forward_coverage_geometric_invariant` |
| 1 Hz publish, ≥ 10% headroom                  | `tests/kpp/test_cost_map_latency.test_cost_map_publish_rate_1hz_pm_10pct` |
| KPP `cost_map_latency_p99` ≤ 5 s              | `tests/kpp/test_cost_map_latency.test_cost_map_latency_p99_within_5s` |
| 4-layer composition lethal beats warn         | `test_cost_thresholds.test_compose_lethal_obstacle_gets_inflation_halo` |
| `CostMapUpdate` raw round-trip + queue        | `test_cost_map_message.test_message_flows_through_queue` |
| `CostMapUpdate` PNG round-trip                | `test_cost_map_message.test_png_encoding_round_trip` |
| 3-strike avoidance failure → `operator_alert` | `test_cost_map_alarm.test_three_failures_emits_one_alert` |
| streak resets on success                      | `test_cost_map_alarm.test_success_resets_streak_then_refires` |

## 5. Wiring

Producer (in `MapFusionProcess` follow-up — not in this PR):

```python
from mapping import CostMap, CostMapConfig
cm = CostMap(CostMapConfig.from_cfg(self.cfg))

# Every LiDAR tick (or 1 Hz batched):
master, latency_s = cm.compose(points_xyz, static_layer_window=osm_local)
msg = cm.to_message(producer_latency_s=latency_s,
                    origin_xy=pose_to_origin_xy(self._latest_pose),
                    encoding="png")
publish(self.queues.cost_map_update, msg)
```

Consumer (Mission, Tier 1.5 reroute hook — coordination with `tier_manager`):

```python
from mission.cost_map_alarm import CostMapAvoidanceAlarm
alarm = CostMapAvoidanceAlarm(max_failures=cfg.get("cost_map",
                                                   "avoidance_max_failures",
                                                   default=3))
# On each reroute attempt around a lethal cell:
alarm.record(success=reroute_ok, reason=reroute_failure_reason)
alert = alarm.poll_alert()
if alert is not None:
    publish(queues.operator_alert, alert)
```

## 6. Out-of-scope for PHASE 1

- ROS2 `nav2_costmap_2d` plugin port — this repo is pure Python; the
  Python `CostMap` class exposes the same `master` grid the planner
  needs.
- Gazebo S17-1~5 scenario suite — requires a sim setup that is not
  wired in this branch.
- HIL regression (real Robosense E1, 30-min lab_test) — physical
  hardware required.
- Full RANSAC ground-plane segmentation — the per-cell `min(z)`
  proxy in `build_ground_grid()` covers the v1.3 thresholds with
  the chosen depth floor. RANSAC is a precision upgrade for follow-up.

## 7. Adding a new layer

1. Implement a class with `update(...) → uint8 grid of self.shape`,
   mirroring `ObstacleLayer` (no IPC, no threading).
2. Compose into `CostMap.compose()` via `np.maximum(master, new_layer)`.
3. Add a layer-index constant to `core.messages` if it needs to
   carry an explicit ID across the IDS.
4. Document the costs you emit here.
