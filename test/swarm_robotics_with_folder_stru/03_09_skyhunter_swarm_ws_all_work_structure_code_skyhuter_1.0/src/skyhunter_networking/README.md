# skyhunter_networking

WiFi6 mesh simulation and swarm communication package for skyhunter robots.

## Structure

```
skyhunter_networking/
├── CMakeLists.txt
├── package.xml
├── include/skyhunter_networking/    # C++ headers
├── src/                        # C++ source files
├── scripts/                    # Python nodes
├── config/                     # Configuration files
└── launch/                     # Launch files
```

## Dependencies

- **skyhunter_msgs** — Message and service definitions

## Nodes (To be implemented)

| Node | Language | Description |
|------|----------|-------------|
| wifi6_mesh_sim | Python/C++ | WiFi6 mesh RF simulation |
| swarm_comm_manager | Python/C++ | Communication tier FSM |
| lte_simulator | Python | LTE backup simulation |
| lora_sim_stub | Python | LoRa emergency link stub |

## Build

```bash
colcon build --packages-select skyhunter_msgs skyhunter_networking
```

## Related Documents

- WiFi6_Mesh_Design.md
- 802.11s_Config_Spec.md
- wifi6_mesh_sim_Architecture.md
