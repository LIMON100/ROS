# skyhunter_comm

WiFi6 mesh simulation and swarm communication package for the SkyHunter 8-robot UGV swarm.

## Structure

```
skyhunter_comm/
├── CMakeLists.txt
├── package.xml
├── scripts/                        # Python nodes (executable entry points)
│   ├── wifi6_mesh_sim              # WiFi6 RF simulation node
│   └── jammer_service              # Jammer service node
├── skyhunter_comm/                 # Python source
│   ├── __init__.py
│   ├── wifi6_mesh_sim.py           # Main mesh sim node
│   ├── jammer_service.py           # Standalone jammer node
│   └── models/                     # Modular RF model components
│       ├── robot_discovery.py      # TF-based robot discovery
│       └── path_loss.py            # Log-distance path loss model
├── config/
│   └── wifi6_mesh_sim.yaml         # Networking parameters (SSID, freq, tx power, thresholds)
```

## Dependencies

- **skyhunter_msgs** — Message and service definitions
- **skyhunter_gazebo** — Provides `robot_params.yaml` (robot naming, namespaces)

## Nodes

| Node | Description |
|------|-------------|
| wifi6_mesh_sim | WiFi6 RF simulation — TF2 distance tracking, log-distance path loss, delay injection, RSSI publishing |
| jammer_service | Standalone jammer node — publishes `/jammed_links`, supports multiple simultaneous jams |

## Key Design Decisions

- **Auto-discovery:** Robots are discovered from the TF tree — no hardcoded `num_robots`
- **Separation of concerns:** `jammer_service` is a standalone node; not embedded in `wifi6_mesh_sim`
- **Single source of truth:** All networking params live in `config/wifi6_mesh_sim.yaml`
- **Robot params ownership:** `robot_params.yaml` lives in `skyhunter_gazebo` (robot naming is a Gazebo concern)

## Configuration

`config/wifi6_mesh_sim.yaml` — networking parameters (frequency, tx power, path loss exponent, RSSI thresholds, delay range).

`skyhunter_gazebo/config/robot_params.yaml` — robot namespace definitions shared across packages.

## Usage

**Run wifi6_mesh_sim:**
```bash
ros2 run skyhunter_comm wifi6_mesh_sim
```

**Run jammer_service:**
```bash
ros2 run skyhunter_comm jammer_service
```

> Both nodes must be running for jam/unjam services to work. `jammer_service` publishes `/jammed_links` which `wifi6_mesh_sim` subscribes to.

## Build

```bash
colcon build --packages-select skyhunter_msgs skyhunter_comm
```

## Related Documents

- `WiFi6_Mesh_Design.md`
- `802.11s_Config_Spec.md`
- `wifi6_mesh_sim_Architecture.md`