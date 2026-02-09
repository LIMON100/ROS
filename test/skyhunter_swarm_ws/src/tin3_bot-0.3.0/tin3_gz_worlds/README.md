# tin3_gz_worlds

Gazebo simulation worlds for TIN3 robot testing.

## Package Structure

```
tin3_gz_worlds/
├── worlds/              # SDF world files
│   ├── empty_world.sdf  # Basic empty world with ground plane
│   └── test_world.sdf   # Test world with terrain and objects
├── meshes/
│   ├── terrain/         # Terrain meshes (STL/DAE)
│   └── objects/         # Object meshes (poles, signs, etc.)
├── models/              # Reusable Gazebo models
├── launch/
│   └── gz_world.launch.py
├── config/              # World configuration files
└── env-hooks/           # Gazebo resource path setup
```

## Usage

### Launch world only (no robot)

```bash
# Default empty world
ros2 launch tin3_gz_worlds gz_world.launch.py

# Specific world
ros2 launch tin3_gz_worlds gz_world.launch.py world:=test_world.sdf

# Headless (no GUI)
ros2 launch tin3_gz_worlds gz_world.launch.py gui:=false
```

### Launch with robot (from tin3_bot package)

```bash
# Use tin3_bot's sim.launch.py with world argument
ros2 launch tin3_bot sim.launch.py world:=test_world.sdf
```

## Adding New Worlds

1. Create SDF file in `worlds/` directory
2. Use `package://tin3_gz_worlds/meshes/...` for mesh references
3. Include required plugins (Physics, Sensors, etc.)

### Required Plugins

```xml
<plugin filename="gz-sim-physics-system" name="gz::sim::systems::Physics"/>
<plugin filename="gz-sim-sensors-system" name="gz::sim::systems::Sensors">
  <render_engine>ogre2</render_engine>
</plugin>
<plugin filename="gz-sim-user-commands-system" name="gz::sim::systems::UserCommands"/>
<plugin filename="gz-sim-scene-broadcaster-system" name="gz::sim::systems::SceneBroadcaster"/>
<plugin filename="gz-sim-imu-system" name="gz::sim::systems::Imu"/>
<plugin filename="gz-sim-navsat-system" name="gz::sim::systems::NavSat"/>
```

## Adding Meshes from Blender

### Export Pipeline

1. Select mesh in Blender
2. Apply transforms: `Ctrl+A` → All Transforms
3. Reset cursor: `Shift+C`
4. Set origin: Right-click → Set Origin → Origin to 3D Cursor
5. Decimate if > 100k faces
6. Export: File → Export → STL → ☑️ Selection Only

### Face Count Guidelines

| Faces | Action |
|-------|--------|
| < 100,000 | Export directly |
| 100,000 - 500,000 | Consider decimating |
| > 500,000 | Must decimate |

## Dependencies

- ROS 2 Humble/Jazzy
- Gazebo Harmonic (gz-sim)
- ros_gz_sim
- ros_gz_bridge

## Build

```bash
cd ~/ros2_ws
colcon build --packages-select tin3_gz_worlds
source install/setup.bash
```
