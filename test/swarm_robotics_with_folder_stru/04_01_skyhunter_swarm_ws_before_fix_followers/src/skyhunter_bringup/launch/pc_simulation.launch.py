import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, TimerAction, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
import math

def launch_setup(context, *args, **kwargs):
    pkg_tin3_gz_simulation = get_package_share_directory('skyhunter_gazebo')
    pkg_tin3_navigation = get_package_share_directory('skyhunter_navigation')
    pkg_skyhunter_comm = get_package_share_directory('skyhunter_comm')
    
    num_robots = int(context.perform_substitution(LaunchConfiguration('num_robots')))
    use_sim_time = LaunchConfiguration('use_sim_time')
    world_file = context.perform_substitution(LaunchConfiguration('world'))
    pose_str = context.perform_substitution(LaunchConfiguration('pose'))
    
    # Auto-setting Route 66 coordinates
    if 'route_66' in world_file and pose_str == '0.0 0.0 0.5 0.0 0.0 0.0':
        pose_str = "2445.0 293.5 53.2 0 0 -3.023"

    pose_parts = pose_str.split()
    b_x = float(pose_parts[0])
    b_y = float(pose_parts[1])
    b_z = float(pose_parts[2])
    b_yaw = float(pose_parts[5]) if len(pose_parts) > 5 else 0.0

    nodes_to_start = []
    map_yaml = LaunchConfiguration('map_yaml', default=os.path.join(pkg_tin3_navigation, 'maps', 'empty_world_map.yaml'))
    use_map = context.perform_substitution(LaunchConfiguration('use_map')).lower() == 'true'

    if use_map:
        nodes_to_start.append(Node(
            package='nav2_map_server', executable='map_server', name='map_server',
            output='screen', parameters=[{'yaml_filename': map_yaml, 'use_sim_time': use_sim_time}]))
        nodes_to_start.append(Node(
            package='nav2_lifecycle_manager', executable='lifecycle_manager', name='lifecycle_manager_map',
            output='screen', parameters=[{'use_sim_time': use_sim_time}, {'autostart': True}, {'node_names': ['map_server']}]))

    # Leader Physics
    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')),
        launch_arguments={'num_robots': '1', 'lidar_mode': 'half', 'use_sim_time': use_sim_time, 'world': world_file, 'pose': pose_str}.items()
    ))

    # Leader Nav2
    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
        launch_arguments={'use_sim_time': use_sim_time, 'autostart': 'true', 'namespace': '', 'use_rviz': 'true'}.items()
    ))

    # Network Simulation
    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_skyhunter_comm, 'launch', 'network.launch.py'))
    ))

    # Swarm Monitor (Tracks Odometry on the PC side)
    nodes_to_start.append(Node(
        package='skyhunter_control', executable='swarm_monitor_node',
        output='screen', parameters=[{'use_sim_time': True}]
    ))

    # Follower Physics and Nav2
    for i in range(2, num_robots + 1):
        follower_ns = f'SH_{i:02d}'
        row = i // 2
        side = 1 if i % 2 == 0 else -1

        spawn_dist_back = -2.5 * row 
        spawn_dist_side = 1.5 * side
        spawn_x = b_x + (spawn_dist_back * math.cos(b_yaw) - spawn_dist_side * math.sin(b_yaw))
        spawn_y = b_y + (spawn_dist_back * math.sin(b_yaw) + spawn_dist_side * math.cos(b_yaw))

        follower_action = TimerAction(
            period=float(i) * 1.5 + 5.0,
            actions=[
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")),
                    launch_arguments={"robot_ns": follower_ns, "pose": f"{spawn_x} {spawn_y} {b_z + 0.15} 0 0 {b_yaw}", 
                                      "use_sim_time": use_sim_time, "lidar_mode": "low", "use_ekf": "true"}.items(),
                ),
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
                    launch_arguments={'use_sim_time': use_sim_time, 'autostart': 'true',
                                      'params_file': os.path.join(pkg_tin3_navigation, 'config', 'nav2_follower_params.yaml'),
                                      'namespace': follower_ns, 'use_namespace': 'true', 'use_rviz': 'false'}.items()
                )
            ]
        )
        nodes_to_start.append(follower_action)

    return nodes_to_start

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('num_robots', default_value='3'),
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        DeclareLaunchArgument('use_map', default_value='false'), 
        DeclareLaunchArgument('world', default_value='empty_world.sdf'),
        DeclareLaunchArgument('pose', default_value='0.0 0.0 0.5 0.0 0.0 0.0'),
        OpaqueFunction(function=launch_setup)
    ])