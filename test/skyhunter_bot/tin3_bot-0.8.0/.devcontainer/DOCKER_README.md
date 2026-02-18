# Docker Setup — ROS 2 Humble + Gazebo Harmonic

Test and run tin3_bot packages in a ROS 2 Humble container with Gazebo Harmonic.

---

## Prerequisites

- Docker installed
- VS Code with [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers) extension
- X11 display (for Gazebo/RViz GUI)

```bash
# Allow X11 forwarding (run on host)
xhost +local:docker
```

---

## Quick Start (VS Code Dev Container)

1. Place `.devcontainer/` folder at the workspace root alongside the packages:

```
<your_workspace>/
├── .devcontainer/
│   ├── Dockerfile
│   └── devcontainer.json
├── tin3_description/
├── tin3_gz_simulation/
├── tin3_gz_worlds/
└── tin3_navigation/
```

2. Open workspace in VS Code:
   ```bash
   code <your_workspace>/
   ```

3. Reopen in Container:
   - `Ctrl+Shift+P` → "Dev Containers: Reopen in Container"
   - First build takes ~5-10 min

4. Test (terminal opens inside container at `/ros2_ws`):
   ```bash
   source install/setup.bash
   ros2 launch tin3_gz_simulation sim.launch.py
   ```

---

## Environment

| Component | Version |
|-----------|---------|
| Ubuntu | 22.04 |
| ROS 2 | Humble |
| Gazebo | Harmonic |

---

## What Gets Installed

The Dockerfile installs all dependencies. The `postCreateCommand` in `devcontainer.json` then runs `rosdep install` and `colcon build` automatically on first open.

---

## Notes

- GUI uses X11 forwarding — no VNC or web desktop needed
- `--privileged` and `--network=host` are set for GPU access and ROS 2 DDS discovery
- `use_sim_time: true` is already configured in the navigation configs
- See the global README for launch arguments, sensor topics, and multi-robot usage