import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, TimerAction, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def launch_setup(context, *args, **kwargs):
    # --- 1. Setup Paths and Config ---
    pkg_tin3_gz_simulation = get_package_share_directory('tin3_gz_simulation')
    pkg_tin3_navigation = get_package_share_directory('tin3_navigation')
    
    # Convert LaunchConfiguration to actual Python types
    num_robots = int(context.perform_substitution(LaunchConfiguration('num_robots')))
    use_sim_time = LaunchConfiguration('use_sim_time')

    nodes_to_start = []

    # ====================================================
    # 1. WORLD & LEADER (Always Global Namespace)
    # ====================================================
    sim_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')),
        launch_arguments={'num_robots': '1', 'lidar_mode': 'half', 'use_sim_time': use_sim_time}.items()
    )
    nodes_to_start.append(sim_launch)

    nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
        launch_arguments={'use_sim_time': use_sim_time, 'autostart': 'true'}.items()
    )
    nodes_to_start.append(nav2_launch)

    leader_node = Node(
        package='skyhunter_formation', executable='leader_node',
        output='screen', parameters=[{'use_sim_time': True}],
        remappings=[('odom', '/odom'), ('leader_state', '/leader_state')]
    )
    nodes_to_start.append(leader_node)

    # ====================================================
    # 2. DYNAMIC FOLLOWERS (Robot 02 to Robot N)
    # ====================================================
    # We loop from 2 up to num_robots
    for i in range(2, num_robots + 1):
        follower_ns = f'robot_{i:02d}'
        
        # Calculate a V-Shape Formation position for spawning
        # Robot 2: (-2, 2), Robot 3: (-2, -2), Robot 4: (-4, 4), etc.
        row = i // 2
        side = 1 if i % 2 == 0 else -1
        spawn_x = -2.5 * row
        spawn_y = 2.5 * side
        
        # Calculate Following Offset (where they should be while moving)
        # We want them in a V-shape behind the leader
        off_x = -3.0 * row
        off_y = 2.0 * side

        # Stagger the spawn delay so Gazebo doesn't crash (2 seconds between robots)
        delay = float(i) * 2.0 + 5.0

        follower_action = TimerAction(
            period=delay,
            actions=[
                # A. Spawn Robot Model
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")),
                    launch_arguments={
                        "robot_ns": follower_ns,
                        "pose": f"{spawn_x} {spawn_y} 0.1 0 0 0",
                        "use_sim_time": use_sim_time,
                        "lidar_mode": "half",
                        "use_ekf": "true"
                    }.items(),
                ),

                # B. Map Link (map -> robot_XX/odom)
                Node(
                    package='tf2_ros', executable='static_transform_publisher',
                    name=f'link_{follower_ns}',
                    arguments=['0', '0', '0', '0', '0', '0', 'map', f'{follower_ns}/odom'],
                    parameters=[{'use_sim_time': True}]
                ),

                # C. Follower Node (The Brain)
                Node(
                    package='skyhunter_formation',
                    executable='follower_node',
                    namespace=follower_ns,
                    output='screen',
                    parameters=[{
                        'use_sim_time': True,
                        'offset_dist': float(off_x), # Note: using offset_dist from your robust C++
                        'leader_topic': '/leader_state'
                    }],
                    remappings=[
                        ('scan/points', f'/{follower_ns}/scan/points'), 
                        ('cmd_vel', f'/{follower_ns}/cmd_vel'),
                        # ('odom', f'/{follower_ns}/odom'),
                        ('odom', f'/{follower_ns}/odometry/filtered'), 
                        ('leader_state', '/leader_state'),
                        ('/tf', '/tf'),
                        ('/tf_static', '/tf_static')
                    ]
                )
            ]
        )
        nodes_to_start.append(follower_action)

    goal_sender_node = Node(
        package='skyhunter_nav_tools',
        executable='waypoint_sender',
        name='waypoint_sender',
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
    )


    nodes_to_start.append(goal_sender_node)

    return nodes_to_start

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('num_robots', default_value='2', description='Total robots (Leader + Followers)'),
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        OpaqueFunction(function=launch_setup)
    ])