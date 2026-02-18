import os
import random
import math

from ament_index_python.packages import get_package_share_directory

from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
    TimerAction,
    OpaqueFunction,
)
from launch.launch_description import LaunchDescription
from launch.launch_context import LaunchContext
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

# ==================== Package Paths ====================
pkg_tin3_description = get_package_share_directory("tin3_description")
pkg_tin3_gz_simulation = get_package_share_directory("tin3_gz_simulation")
pkg_tin3_gz_worlds = get_package_share_directory("tin3_gz_worlds")
pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")


def spawn_robots(
    context: LaunchContext,
    num_robots_config: LaunchConfiguration,
    lidar_mode_config: LaunchConfiguration,
    pose_config: LaunchConfiguration,
    pattern_config: LaunchConfiguration,
    spacing_config: LaunchConfiguration,
    use_sim_time_config: LaunchConfiguration
):
    """Dynamically spawn N robots with different patterns"""

    num_robots = int(context.perform_substitution(num_robots_config))
    lidar_mode = context.perform_substitution(lidar_mode_config)
    pose_str = context.perform_substitution(pose_config)
    pattern = context.perform_substitution(pattern_config)
    spacing = float(context.perform_substitution(spacing_config))

    # Parse "x y z" string
    pose_parts = pose_str.split()
    base_x = float(pose_parts[0]) if len(pose_parts) > 0 else 0.0
    base_y = float(pose_parts[1]) if len(pose_parts) > 1 else 0.0
    base_z = float(pose_parts[2]) if len(pose_parts) > 2 else 0.5
    base_roll = float(pose_parts[3]) if len(pose_parts) > 3 else 0.0
    base_pitch = float(pose_parts[4]) if len(pose_parts) > 4 else 0.0
    base_yaw = float(pose_parts[5]) if len(pose_parts) > 5 else 0.0
    spawn_actions = []

    for i in range(num_robots):
        # Calculate position based on pattern
        if num_robots == 1:
            x, y = base_x, base_y
        elif pattern == "line_x":
            x = base_x + i * spacing
            y = base_y
        elif pattern == "line_y":
            x = base_x
            y = base_y + i * spacing
        elif pattern == "random":
            area_size = spacing * (num_robots ** 0.5)
            x = base_x + random.uniform(0, area_size)
            y = base_y + random.uniform(0, area_size)
        elif pattern == "circle":
            angle = 2 * math.pi * i / num_robots
            radius = spacing * num_robots / (2 * math.pi)
            x = base_x + radius * math.cos(angle)
            y = base_y + radius * math.sin(angle)
        else:  # grid (default)
            cols = int(num_robots ** 0.5) + 1
            row = i // cols
            col = i % cols
            x = base_x + col * spacing
            y = base_y + row * spacing

        # Namespace
        if num_robots == 1:
            robot_ns = ""
            delay = 2.0
        else:
            robot_ns = f"robot_{i + 1:02d}"
            delay = float(i) * 2.0 + 2.0

        # Pass pose as "x y z" string
        robot_pose = f"{x} {y} {base_z} {base_roll} {base_pitch} {base_yaw}"

        spawn = TimerAction(
            period=delay,
            actions=[
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(
                        os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")
                    ),
                    launch_arguments={
                        "robot_ns": robot_ns,
                        "pose": robot_pose,
                        "lidar_mode": lidar_mode,
                        "use_sim_time": use_sim_time_config,
                    }.items(),
                )
            ],
        )
        spawn_actions.append(spawn)

    return spawn_actions


def generate_launch_description():
    # ==================== Environment ====================
    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[
            os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
            ":",
            os.path.dirname(pkg_tin3_description),
            ":",
            pkg_tin3_gz_worlds,
        ],
    )

    # ==================== Arguments ====================
    use_sim_time_arg = DeclareLaunchArgument(
                "use_sim_time",
                default_value="true",
                description="Use simulation (Gazebo) clock if true"
            )
   
    world_arg = DeclareLaunchArgument(
        "world",
        default_value=os.path.join(pkg_tin3_gz_worlds, "worlds", "empty_world.sdf"),
        description="World file path",
    )

    num_robots_arg = DeclareLaunchArgument(
        "num_robots",
        default_value="1",
        description="Number of robots to spawn",
    )

    lidar_mode_arg = DeclareLaunchArgument(
        "lidar_mode",
        default_value="full",
        description="LiDAR resolution: full, half, low,none",
    )

    pose_arg = DeclareLaunchArgument(
        "pose",
        default_value="0 0 0.5",
        description="Spawn origin 'x y z' or 'x y z roll pitch yaw'",
    )

    pattern_arg = DeclareLaunchArgument(
        "pattern",
        default_value="grid",
        description="Pattern: grid, line_x, line_y, random, circle",
    )

    spacing_arg = DeclareLaunchArgument(
        "spacing",
        default_value="3.0",
        description="Distance between robots (meters)",
    )

    # ==================== Gazebo ====================
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")
        ),
        launch_arguments={"gz_args": LaunchConfiguration("world")}.items(),
    )

    # ==================== Clock Bridge ====================
    clock_bridge = Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="clock_bridge",
        arguments=[
            "/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock",
        ],
        parameters=[{"use_sim_time": True}],
        output="screen",
    )

    # ==================== Spawn Robots ====================
    spawn_robots_action = OpaqueFunction(
        function=spawn_robots,
        args=[
            LaunchConfiguration("num_robots"),
            LaunchConfiguration("lidar_mode"),
            LaunchConfiguration("pose"),
            LaunchConfiguration("pattern"),
            LaunchConfiguration("spacing"),
            LaunchConfiguration("use_sim_time"),
        ],
    )

    return LaunchDescription(
        [
            gz_resource_path,
            use_sim_time_arg, 
            world_arg,
            num_robots_arg,
            lidar_mode_arg,
            pose_arg,
            pattern_arg,
            spacing_arg,
            gz_sim,
            clock_bridge,
            spawn_robots_action,
        ]
    )