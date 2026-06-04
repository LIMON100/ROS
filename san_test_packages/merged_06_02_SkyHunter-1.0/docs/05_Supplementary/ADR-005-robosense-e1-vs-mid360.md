# ADR-005 — Robosense E1 over Livox Mid360 (follower LiDAR)

- **Status**: Accepted (v1.1, 2026-05-11)
- **Spec**: SAN-SDD-CMD-001 v1.1 §7 (LiDAR), SAN-IDS-CMD-001 v1.1 (cliff alert)
- **Implementation**: `config/`, `safety/cliff_detector.py`,
  `tests/test_cliff_detector.py`

## Context

The Leader's LiDAR is **Unitree L1** (integrated with the Go2 EDU). v1.0 did
not specify the follower-side LiDAR; field tests used a mix of Mid360 units
borrowed from an adjacent project. As we standardized for v1.1 we needed a
follower LiDAR that satisfied:

1. **Tight vertical FOV at the close range** — followers operate in a V
   formation and need to detect cliffs / step-downs at body length (~0.6m
   ahead of the foot), where Mid360's hemispherical scan has its weakest
   point density at the very forward-down vector.
2. **Compact form factor** — must mount on the follower's pan-tilt head
   carriage without altering the gimbal envelope.
3. **Single-power-cable integration** — Go2-compatible 12V harness, < 8W
   sustained.
4. **Onboard IMU** — the cliff detector needs IMU pitch to gate
   range-derived height (avoid false-positive cliff alerts during normal
   gait pitching).
5. **Cost** — fleet-of-8 follower deployment; per-unit cost matters.

| LiDAR | Vertical FOV (forward-down) | Form factor | Onboard IMU | Approx unit cost |
|---|---|---|---|---|
| **Livox Mid360** | Hemispherical, weak density near 0° forward-down | Cylindrical, top-mount preferred | No (external) | $750 |
| **Robosense E1** | Dense across full forward-down hemisphere | Compact rectangular, side-mount OK | **Yes** — built-in 6-axis IMU | $620 |

## Decision

Adopt **Robosense E1** as the standard follower LiDAR. Leader keeps the
Unitree L1 (it is integrated with the Go2 EDU and not changeable without
breaking warranty).

The E1's built-in IMU drives the cliff detector directly — pitch hysteresis
and de-bounce live in `safety/cliff_detector.py`, gated by IMU samples from
the LiDAR's own data stream (no separate IMU harness to break).

## Consequences

**Positive**

- Cliff detection at body-length range works (S15 test suite + bench
  validation). Mid360 in equivalent mounting saw ~30% false negatives at
  0.5m–0.7m forward-down because of point sparsity there.
- One fewer external sensor to wire and calibrate (the IMU is on the LiDAR
  bus, not a separate I²C harness).
- Per-fleet BOM ~$1k cheaper across 8 followers.

**Negative**

- E1 is a newer product than Mid360. Driver maturity in the open-source
  ecosystem is lower; our integration in `config/` includes some
  vendor-firmware-version pinning. Tracked in
  `config/README-lidar.md`.
- The E1 ROS / Linux driver is supported by Robosense rather than the
  larger Livox community. Long-term support risk is non-zero but acceptable
  given the form-factor and cliff-detection wins.
- Existing Mid360 units in the project's spare-parts inventory remain
  usable only as bench rigs; they are not deployed on followers.

**Migration**

- All v1.1 followers ship with E1. Field units that received Mid360 during
  v1.0 trials will be retrofitted during the next service rotation.
- Cliff-detector unit tests use synthetic IMU streams (`test_cliff_detector
  .py`) and remain hardware-agnostic; the integration risk is at the driver
  layer, not the detection logic.
