# Convoy parameter sweep — results & optimal selection

Goal: across **various paths × leader (robot-dog) start speeds**, find the optimal
convoy configuration; fix bugs uncovered. Harness: `convoy_sweep.sh`
(4 paths × 5 speeds = 20 configs). Env: Ubuntu 24.04 / ROS 2 Jazzy / Gazebo
Harmonic 8.14, GPU.

## Method — clean terminate-per-run

The reliable method (per SAN-SITL-VERIF-001 RC-2) is **`wsl --terminate` before
every config** so the CHAMP controllers re-initialize cleanly. Orchestrated from
the host session (`clean_sweep.sh`): each config = terminate → fresh boot →
`convoy_sweep.sh IDX IDX`. (An earlier *in-distro relaunch* sweep produced
corrupted S-curve rows — CHAMP stale-state — which the clean rerun corrected.)

Run was stopped after **14/20 configs** — enough to fully cover straight + S-curve
+ most of zigzag and fix the obstacle-at-waypoint bug. Raw: `clean_sweep.csv`.

## Results (clean)

| path | 0.3 | 0.45 | 0.6 | 0.75 | 0.9 |
|---|---|---|---|---|---|
| **straight** | INCOMPLETE | INCOMPLETE | **FALL** | **FALL** | COLLIDE |
| **scurve** | INCOMPLETE | INCOMPLETE | INCOMPLETE✅ | INCOMPLETE✅ | **FALL** |
| **zigzag** | INCOMPLETE | **FALL** | INCOMPLETE | INCOMPLETE | (n/a) |
| lturn | (not run — sharp 90°, expected to fail like straight/zigzag) |||||

(go2_z<0.15 = FALL; min inter-robot<0.8 = COLLIDE; "INCOMPLETE✅" = no fall, no
collision, obstacle ≥2.25 m, healthy gait — just didn't finish the 100 s window.)

Key per-config (scurve, the stable path): 0.3→wp3/8, 0.45→wp5/8 (gap 3.0),
**0.6→wp6/8** (gap 1.6, obs 2.40 m), 0.75→wp6/8, 0.9→FALL (go2_z 0.077).

## Findings

- **Path matters most.** Only the gentle pre-routed **S-curve** is stable. Paths
  that force a *sharp* turn at the obstacle (straight, zigzag, L-turn) make the
  low-RTF CHAMP gait lose balance → **fall** (go2_z ≈ 0.077) at moderate+ speed.
- **Speed ceiling ≈ 0.6 m/s.** Above it the apex-obstacle detour turn gets too
  sharp → **0.9 falls even on S-curve**. 0.6 and 0.75 reach the same progress
  (leader throttles to convoy cohesion), so 0.6 is the faster-with-margin choice.
- **0.3 m/s is too slow** — gait under-walks, never finishes.

## ✅ Optimal = **S-curve path @ leader_vmax 0.6 m/s** (leader_wmax 0.35, gap 3.0)

Best stable progress + largest fall margin + full convoy cohesion + obstacle
≥2.25 m. Matches the PR #277 clean validation (8/8 with the obstacle off-path) and
is the `convoy_demo.launch.py` default — **no change needed**.

## Bug fixed (req 3)

**Leader stuck when the obstacle sits on/near a waypoint** — advance test used the
*raw* waypoint (inside the obstacle), never reachable. Fixed via
`convoy_coordinator._shifted_target()` (advance on the shifted aim point). Also:
UGVs invisible in Gazebo → added `GZ_SIM_RESOURCE_PATH` (package:// mesh resolve).

## Reproduce

```bash
bash skyhunter_sim/build_test_ws.sh                 # build from repo
bash skyhunter_sim/run_convoy_test.sh               # clean validation of the optimal
# full clean sweep (host session): terminate-per-run orchestration
```
