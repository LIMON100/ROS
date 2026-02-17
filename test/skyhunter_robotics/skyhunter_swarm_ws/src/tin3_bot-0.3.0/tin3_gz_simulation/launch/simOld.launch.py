# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import (
#     DeclareLaunchArgument, IncludeLaunchDescription, SetEnvironmentVariable, 
#     TimerAction, OpaqueFunction
# )
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
# from launch_ros.actions import Node

# def spawn_robots(context, num_robots, use_ekf, lidar_mode):
#     pkg_tin3_bot = get_package_share_directory("tin3_gz_simulation")
#     num = int(context.perform_substitution(num_robots))
#     spawn_actions = []
    
#     for i in range(num):
#         ns = f"robot_{i + 1:02d}"
#         x = 0.0 if i == 0 else -2.0 * i
#         y = 0.0 if i == 0 else (2.0 if i % 2 != 0 else -2.0)
        
#         spawn = TimerAction(
#             period=float(i) * 2.0,
#             actions=[
#                 IncludeLaunchDescription(
#                     PythonLaunchDescriptionSource(
#                         os.path.join(pkg_tin3_bot, "launch", "spawn_robot.launch.py")
#                     ),
#                     launch_arguments={
#                         "robot_ns": ns,
#                         "x": str(x), "y": str(y), "z": "0.5",
#                         "use_ekf": "true",
#                         "lidar_mode": "half",
#                     }.items(),
#                 )
#             ],
#         )
#         spawn_actions.append(spawn)
#     return spawn_actions

# def launch_swarm_logic(context, num_robots):
#     num = int(context.perform_substitution(num_robots))
#     pkg_tin3_bot = get_package_share_directory("tin3_gz_simulation")
#     nodes = []

#     for i in range(num):
#         robot_ns = f"robot_{i + 1:02d}"

#         # 1. Perception Node (Elevation Mapper) - for ALL robots
#         perception_node = Node(
#             package='skyhunter_perception', 
#             executable='elevation_mapper_node',
#             namespace=robot_ns,
#             name='elevation_mapper',
#             parameters=[{
#                 'use_sim_time': True,
#                 'base_frame': f'{robot_ns}/base_footprint',
#                 'map_frame': f'{robot_ns}/odom',
#                 'cloud_topic': 'scan/points',
#                 'map_topic': 'elevation_map'
#             }],
#             output='screen'
#         )
#         nodes.append(perception_node)

#         # 2. Logic Splitting
#         if i == 0:
#             # ==========================
#             # LEADER (Robot 01)
#             # ==========================
#             leader_node = Node(
#                 package='skyhunter_formation', 
#                 executable='leader_node',
#                 namespace=robot_ns,
#                 name='leader_node',
#                 parameters=[{
#                     'use_sim_time': True,
#                     'odom_topic': 'odom',
#                     'leader_state_topic': '/leader_state' 
#                 }],
#                 output='screen'
#             )
#             nodes.append(leader_node)

#             # Launch Nav2 Stack (This handles the map->odom TF for the leader)
#             nav2_launch = IncludeLaunchDescription(
#                 PythonLaunchDescriptionSource(
#                     os.path.join(pkg_tin3_bot, "launch", "nav2_test.launch.py")
#                 ),
#                 launch_arguments={'robot_ns': robot_ns}.items()
#             )
#             nodes.append(nav2_launch)

#         else:
#             # ==========================
#             # FOLLOWERS (Robot 02+)
#             # ==========================
            
#             # *** FIX: Only spawn Static TF for followers ***
#             # The Leader gets its TF from nav2_test.launch.py
#             static_tf_publisher = Node(
#                 package='tf2_ros',
#                 executable='static_transform_publisher',
#                 name=f'map_to_{robot_ns}_odom_tf',
#                 arguments=['0', '0', '0', '0', '0', '0', 'map', f'{robot_ns}/odom'],
#                 parameters=[{'use_sim_time': True}],
#             )
#             nodes.append(static_tf_publisher)

#             # Follower Logic
#             offsets = [(-2.0, 2.0), (-2.0, -2.0), (-4.0, 4.0), (-4.0, -4.0), (-6.0, 6.0), (-6.0, -6.0)]
#             off_x, off_y = offsets[(i-1) % len(offsets)]
            
#             follower_node = Node(
#                 package='skyhunter_formation', 
#                 executable='follower_node',
#                 namespace=robot_ns,
#                 name='follower_node',
#                 parameters=[{
#                     'use_sim_time': True,
#                     'offset_x': off_x,
#                     'offset_y': off_y,
#                     'leader_topic': '/leader_state',
#                     'map_topic': 'elevation_map',
#                     'k_x': 1.5, 
#                     'k_theta': 4.0 
#                 }],
#                 output='screen'
#             )
#             nodes.append(follower_node)

#     return nodes

# def generate_launch_description():
#     pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")
#     pkg_tin3_bot = get_package_share_directory("tin3_gz_simulation")

#     gz_resource_path = SetEnvironmentVariable(
#         name="GZ_SIM_RESOURCE_PATH",
#         value=[os.environ.get("GZ_SIM_RESOURCE_PATH", ""), ":", os.path.dirname(pkg_tin3_bot)]
#     )

#     return LaunchDescription([
#         gz_resource_path,
#         DeclareLaunchArgument("world", default_value="empty_world.sdf"),
#         DeclareLaunchArgument("num_robots", default_value="2"),
#         DeclareLaunchArgument("use_ekf", default_value="true"),
#         DeclareLaunchArgument("lidar_mode", default_value="half"),
        
#         IncludeLaunchDescription(
#             PythonLaunchDescriptionSource(os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")),
#             launch_arguments={"gz_args": [PathJoinSubstitution([pkg_tin3_bot, "worlds", LaunchConfiguration("world")]), " -r"]}.items(),
#         ),
        
#         Node(package="ros_gz_bridge", executable="parameter_bridge",
#              name="clock_bridge", arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"], output="screen"),
             
#         OpaqueFunction(function=spawn_robots, args=[LaunchConfiguration("num_robots"), LaunchConfiguration("use_ekf"), LaunchConfiguration("lidar_mode")]),
#         TimerAction(period=8.0, actions=[OpaqueFunction(function=launch_swarm_logic, args=[LaunchConfiguration("num_robots")])])
#     ])



# =================================================================================
# FINAL INTEGRATED LAUNCH FILE
# 
# Combines the flexible robot spawner with the multi-robot formation logic.
# - Spawns 'num_robots' using the specified pattern.
# - If num_robots > 1, it waits for spawning to complete, then launches:
#   - An elevation mapper for EVERY robot.
#   - The leader_node for robot_01.
#   - The Nav2 stack for robot_01 (for autonomous goal following).
#   - A follower_node for every other robot (robot_02, robot_03, etc.).
# =================================================================================

import os
import random
import math

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
    TimerAction,
    OpaqueFunction,
)
from launch.launch_context import LaunchContext
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

# ==================== Package Paths ====================
pkg_tin3_description = get_package_share_directory("tin3_description")
pkg_tin3_gz_simulation = get_package_share_directory("tin3_gz_simulation")
pkg_tin3_gz_navigation = get_package_share_directory("tin3_navigation")
pkg_tin3_gz_worlds = get_package_share_directory("tin3_gz_worlds")
pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")

# Added package paths for formation and perception nodes
pkg_skyhunter_perception = get_package_share_directory("skyhunter_perception")
pkg_skyhunter_formation = get_package_share_directory("skyhunter_formation")


# =============================================================================
# SECTION 1: ROBOT SPAWNING LOGIC (FROM YOUR NEW sim.launch.py)
# This function dynamically spawns N robots based on launch arguments.
# =============================================================================
def spawn_robots(
    context: LaunchContext,
    num_robots_config: LaunchConfiguration,
    lidar_mode_config: LaunchConfiguration,
    pose_config: LaunchConfiguration,
    pattern_config: LaunchConfiguration,
    spacing_config: LaunchConfiguration,
):
    """Dynamically spawn N robots with different patterns"""

    num_robots = int(context.perform_substitution(num_robots_config))
    lidar_mode = context.perform_substitution(lidar_mode_config)
    pose_str = context.perform_substitution(pose_config)
    pattern = context.perform_substitution(pattern_config)
    spacing = float(context.perform_substitution(spacing_config))

    pose_parts = pose_str.split()
    base_x = float(pose_parts[0]) if len(pose_parts) > 0 else 0.0
    base_y = float(pose_parts[1]) if len(pose_parts) > 1 else 0.0
    base_z = float(pose_parts[2]) if len(pose_parts) > 2 else 0.5
    base_roll = float(pose_parts[3]) if len(pose_parts) > 3 else 0.0
    base_pitch = float(pose_parts[4]) if len(pose_parts) > 4 else 0.0
    base_yaw = float(pose_parts[5]) if len(pose_parts) > 5 else 0.0
    spawn_actions = []

    for i in range(num_robots):
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
        else:
            cols = int(math.sqrt(num_robots)) + 1
            row = i // cols
            col = i % cols
            x = base_x + col * spacing
            y = base_y + row * spacing

        if num_robots == 1:
            robot_ns = ""
            delay = 2.0
        else:
            robot_ns = f"robot_{i + 1:02d}"
            delay = float(i) * 2.0 + 2.0

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
                    }.items(),
                )
            ],
        )
        spawn_actions.append(spawn)

    return spawn_actions


# =================================================================================
# SECTION 2: SWARM INTELLIGENCE LOGIC (FROM simOld.launch.py)
# This function starts the leader, follower, and perception nodes.
# =================================================================================
def launch_swarm_logic(context, num_robots_config):
    """Launch leader, follower, and perception nodes for the swarm."""
    num = int(context.perform_substitution(num_robots_config))
    
    # Do not launch swarm logic for a single robot
    if num <= 1:
        return []

    nodes = []

    for i in range(num):
        robot_ns = f"robot_{i + 1:02d}"

        # 1. Perception Node (Elevation Mapper) - for ALL robots
        perception_node = Node(
            package='skyhunter_perception', 
            executable='elevation_mapper_node',
            namespace=robot_ns,
            name='elevation_mapper',
            parameters=[{
                'use_sim_time': True,
                'base_frame': f'{robot_ns}/base_footprint',
                'map_frame': f'{robot_ns}/odom',
                'cloud_topic': 'scan/points',
                'map_topic': 'elevation_map'
            }],
            output='screen'
        )
        nodes.append(perception_node)

        # 2. Logic Splitting
        if i == 0:
            # === LEADER (Robot 01) ===
            leader_node = Node(
                package='skyhunter_formation', 
                executable='leader_node',
                namespace=robot_ns,
                name='leader_node',
                parameters=[{
                    'use_sim_time': True,
                    'odom_topic': 'odom',
                    'leader_state_topic': '/leader_state' 
                }],
                output='screen'
            )
            nodes.append(leader_node)

            # Launch Nav2 Stack for the leader
            # This handles the map->odom TF for the leader, allowing it to navigate
            nav2_launch = IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(pkg_tin3_gz_navigation, "launch", "nav2_test.launch.py")
                ),
                launch_arguments={'robot_ns': robot_ns}.items()
            )
            nodes.append(nav2_launch)

        else:
            # === FOLLOWERS (Robot 02+) ===
            
            # This static TF publisher is crucial for followers. It provides the
            # map -> odom link that they don't get from a full Nav2 stack.
            static_tf_publisher = Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name=f'map_to_{robot_ns}_odom_tf',
                arguments=['0', '0', '0', '0', '0', '0', 'map', f'{robot_ns}/odom'],
                parameters=[{'use_sim_time': True}],
            )
            nodes.append(static_tf_publisher)

            # Follower Logic Node
            # V-Formation Offsets
            offsets = [(-2.0, 2.0), (-2.0, -2.0), (-4.0, 4.0), (-4.0, -4.0), (-6.0, 6.0), (-6.0, -6.0)]
            off_x, off_y = offsets[(i-1) % len(offsets)]
            
            follower_node = Node(
                package='skyhunter_formation', 
                executable='follower_node',
                namespace=robot_ns,
                name='follower_node',
                parameters=[{
                    'use_sim_time': True,
                    'offset_x': off_x,
                    'offset_y': off_y,
                    'leader_topic': '/leader_state',
                    'map_topic': 'elevation_map',
                    'k_x': 1.5, 
                    'k_theta': 4.0 
                }],
                output='screen'
            )
            nodes.append(follower_node)

    return nodes


# =================================================================================
# SECTION 3: MAIN LAUNCH DESCRIPTION
# This assembles the final launch file.
# =================================================================================
def generate_launch_description():
    # Environment Setup
    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[
            os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
            ":", os.path.dirname(pkg_tin3_description),
            ":", pkg_tin3_gz_worlds,
        ],
    )

    # Launch Arguments
    world_arg = DeclareLaunchArgument(
        "world",
        default_value=os.path.join(pkg_tin3_gz_worlds, "worlds", "empty_world.sdf"),
        description="World file path",
    )
    num_robots_arg = DeclareLaunchArgument("num_robots", default_value="1")
    lidar_mode_arg = DeclareLaunchArgument("lidar_mode", default_value="full")
    pose_arg = DeclareLaunchArgument("pose", default_value="0 0 0.5")
    pattern_arg = DeclareLaunchArgument("pattern", default_value="grid")
    spacing_arg = DeclareLaunchArgument("spacing", default_value="3.0")

    # Gazebo Simulation
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")),
        launch_arguments={"gz_args": LaunchConfiguration("world")}.items(),
    )

    # Clock Bridge
    clock_bridge = Node(
        package="ros_gz_bridge", executable="parameter_bridge",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock]"], output="screen"
    )

    # Spawn Robots Action
    spawn_robots_action = OpaqueFunction(
        function=spawn_robots,
        args=[
            LaunchConfiguration("num_robots"),
            LaunchConfiguration("lidar_mode"),
            LaunchConfiguration("pose"),
            LaunchConfiguration("pattern"),
            LaunchConfiguration("spacing"),
        ],
    )

    # Launch Swarm Logic Action (with a delay)
    # The delay ensures Gazebo and all robot models are fully loaded before starting the control nodes.
    launch_swarm_logic_action = TimerAction(
        period=10.0, # Increased delay to be safe with many robots
        actions=[
            OpaqueFunction(
                function=launch_swarm_logic,
                args=[LaunchConfiguration("num_robots")]
            )
        ]
    )

    return LaunchDescription([
        gz_resource_path,
        world_arg,
        num_robots_arg,
        lidar_mode_arg,
        pose_arg,
        pattern_arg,
        spacing_arg,
        gz_sim,
        clock_bridge,
        spawn_robots_action,
        launch_swarm_logic_action, # <-- This is the added logic
    ])