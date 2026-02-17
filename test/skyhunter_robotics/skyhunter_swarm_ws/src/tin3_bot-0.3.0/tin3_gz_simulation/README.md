# tin3_gz_simulation

Gazebo simulation for TIN3 robot swarm.

## Dependencies

- `tin3_description`
- `tin3_gz_worlds`


## Usage
```bash
# Single robot
ros2 launch tin3_gz_simulation sim.launch.py

# Multi-robot
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=2
ros2 launch tin3_gz_simulation simOld.launch.py num_robots:=2

# With pattern
ros2 launch tin3_gz_simulation sim.launch.py num_robots:=6 pattern:=circle

# With custom world
ros2 launch tin3_gz_simulation sim_launch.py world:=/path/to/world.sdf

# Performance mode (no LiDAR)
ros2 launch tin3_gz_simulation sim_launch.py num_robots:=8 lidar_mode:=none
```

## Arguments

| Argument | Default | Options |
|----------|---------|---------|
| `world` | `empty_world.sdf` | Any `.sdf` file |
| `num_robots` | `1` | 1, 2, 4, 8... |
| `pose` | `0 0 0.5` | `"x y z"` |
| `pattern` | `grid` | `grid`, `line_x`, `line_y`, `random`, `circle` |
| `spacing` | `3.0` | meters |
| `lidar_mode` | `full` | `full`, `half`, `low`, `none` |

# Note

Adjust `z` in `pose` based on terrain height. Some worlds have elevated or rotated terrain — if robot falls through, increase z value (e.g., `pose:="0 0 2.0"`).
