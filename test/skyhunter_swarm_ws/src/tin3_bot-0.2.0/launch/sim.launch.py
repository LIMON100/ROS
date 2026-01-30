import os

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
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node


def spawn_robots(
    context: LaunchContext,
    num_robots: LaunchConfiguration,
    use_ekf: LaunchConfiguration,
    lidar_mode: LaunchConfiguration
):
    """Dynamically spawn N robots in a grid layout"""

    pkg_tin3_bot = get_package_share_directory("tin3_bot")
    num = int(context.perform_substitution(num_robots))
    ekf_flag = context.perform_substitution(use_ekf)
    lidar_flag = context.perform_substitution(lidar_mode)

    spawn_actions = []

    # Grid layout settings
    cols = 4  # robots per row
    spacing = 3.0  # meters between robots

    for i in range(num):
        row = i // cols
        col = i % cols
        x = col * spacing
        y = row * spacing

        # Namespace: "" for single robot, "robot_01" etc for multi
        if num == 1:
            ns = ""
            delay = 2.0  # Wait for Gazebo to start
        else:
            ns = f"robot_{i + 1:02d}"
            delay = float(i) * 2.0 + 2.0  # Stagger spawns

        spawn = TimerAction(
            period=delay,
            actions=[
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(
                        os.path.join(pkg_tin3_bot, "launch", "spawn_robot.launch.py")
                    ),
                    launch_arguments={
                        "robot_ns": ns,
                        "x": str(x),
                        "y": str(y),
                        "z": "0.5",
                        "use_ekf": ekf_flag,
                        "lidar_mode": lidar_flag,
                    }.items(),
                )
            ],
        )
        spawn_actions.append(spawn)

    return spawn_actions


def generate_launch_description():
    # Package paths
    pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")
    pkg_tin3_bot = get_package_share_directory("tin3_bot")

    # ==================== Environment ====================
    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[
            os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
            ":",
            os.path.dirname(pkg_tin3_bot),
        ],
    )

    # ==================== Arguments ====================
    world_arg = DeclareLaunchArgument(
        "world",
        default_value="empty_world.sdf",
        description="World file name (in worlds/ folder)",
    )

    num_robots_arg = DeclareLaunchArgument(
        "num_robots",
        default_value="1",
        description="Number of robots to spawn",
    )

    use_ekf_arg = DeclareLaunchArgument(
        "use_ekf",
        default_value="false",
        description="Enable EKF sensor fusion for all robots",
    )
    lidar_mode_arg = DeclareLaunchArgument(
        "lidar_mode",
        default_value="full",
        description="LiDAR resolution: full, half, low",
    )
    # ==================== Gazebo ====================
    gz_sim = IncludeLaunchDescription(
    PythonLaunchDescriptionSource(
        os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")
    ),
    launch_arguments={
        "gz_args": [
            PathJoinSubstitution([
                pkg_tin3_bot,
                "worlds",
                LaunchConfiguration("world")
            ]),
            " -r"  # Auto-run simulation
        ],
    }.items(),
)
    # ==================== Clock Bridge ====================
    clock_bridge = Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="clock_bridge",
        arguments=[
            "/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock",
        ],
        output="screen",
    )

    # ==================== Spawn Robots ====================
    spawn_robots_action = OpaqueFunction(
        function=spawn_robots,
        args=[
            LaunchConfiguration("num_robots"),
            LaunchConfiguration("use_ekf"),
            LaunchConfiguration("lidar_mode"),
        ],
    )

    return LaunchDescription(
        [
            gz_resource_path,
            world_arg,
            num_robots_arg,
            use_ekf_arg,
            lidar_mode_arg, 
            gz_sim,
            clock_bridge,
            spawn_robots_action,
        ]
    )