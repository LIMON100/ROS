# Phase D Multi-robot Simulation

Gazebo Classic 11 + ROS 2 Humble + CycloneDDS for swarm simulation.

## Prerequisites

- Ubuntu 22.04 LTS (host or WSL2)
- ROS 2 Humble
- Gazebo Classic 11
- CycloneDDS 0.10+

## Quick Start

```bash
# Build sim docker image
docker compose -f sim/docker-compose.gazebo.yml build

# Launch 5-robot sim (1 leader + 1 hub + 3 followers)
docker compose -f sim/docker-compose.gazebo.yml up
```

Wait ~30 s for all robots to come up. Verify:

```bash
ros2 topic list | grep /robot
# Should see /robot1/cmd_vel, /robot2/cmd_vel, etc.
```

## Scenarios

| Scenario | Launch | Purpose |
|---|---|---|
| Empty field | `launch/multi_robot_5.launch.py world:=empty_field` | Baseline KPP |
| Seoul urban | `launch/multi_robot_9.launch.py world:=seoul_urban_2km` | UC-2 mission test |
| Narrow corridor | `launch/multi_robot_5.launch.py world:=narrow_corridor` | UC-11 + P2-3 terrain switch |

## KPP Measurement

Run with KPP integration:

```bash
python sim/scripts/measure_kpp_in_sim.py \
    --world seoul_urban_2km \
    --robots 9 \
    --duration 600 \
    --output /tmp/kpp_$(date +%s).json
```

Outputs JSON compatible with `tests/kpp/` (P2-13 KPP CI).

## Failure Injection (UC-4, UC-6)

```bash
# Kill leader → measure UC-6 reconfiguration time
sim/scripts/inject_failure.sh leader_kill robot1

# WiFi drop for 10 s → measure UC-4 LTE failover
sim/scripts/inject_failure.sh wifi_drop 10

# RTK loss → measure UC-5 Tier degrade
sim/scripts/inject_failure.sh rtk_loss
```

## RK3588J HW Bring-up

After sim validation, deploy to RK3588J:

```bash
ssh robot1 "sudo systemctl start patrol.service"
ssh robot1 "sudo journalctl -u patrol -f"
```

Run all calibrations first per `doc/calibration_procedure.md` (P2-12) and the
full HW bring-up sequence in `doc/phase_d_hw_bringup.md`.
