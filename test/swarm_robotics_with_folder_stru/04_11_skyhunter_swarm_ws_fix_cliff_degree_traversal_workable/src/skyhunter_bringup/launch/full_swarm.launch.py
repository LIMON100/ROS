import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, TimerAction, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
import math

def launch_setup(context, *args, **kwargs):
    # --- 1. Setup Paths and Config ---
    pkg_tin3_gz_simulation = get_package_share_directory('skyhunter_gazebo')
    pkg_tin3_navigation = get_package_share_directory('skyhunter_navigation')
    pkg_skyhunter_comm = get_package_share_directory('skyhunter_comm')
    
    num_robots = int(context.perform_substitution(LaunchConfiguration('num_robots')))
    use_sim_time = LaunchConfiguration('use_sim_time')
    world_file = context.perform_substitution(LaunchConfiguration('world'))
    formation_type = context.perform_substitution(LaunchConfiguration('formation'))
    
    # --- 2. AUTOMATIC WORLD DETECTION ---
    pose_str = context.perform_substitution(LaunchConfiguration('pose'))
    
    # Auto-setting Route 66 coordinates if world is selected
    if 'route_66' in world_file and pose_str == '0.0 0.0 0.5 0.0 0.0 0.0':
        pose_str = "2445.0 293.5 53.2 0 0 -3.023"
        print(f"[INFO] Route 66 detected. Auto-setting pose to: {pose_str}")

    pose_parts = pose_str.split()
    b_x = float(pose_parts[0])
    b_y = float(pose_parts[1])
    b_z = float(pose_parts[2])
    b_yaw = float(pose_parts[5]) if len(pose_parts) > 5 else 0.0

    nodes_to_start = []

    map_yaml = LaunchConfiguration('map_yaml', default=os.path.join(
        pkg_tin3_navigation, 'maps', 'empty_world_map.yaml'))


    use_map = context.perform_substitution(LaunchConfiguration('use_map')).lower() == 'true'


    if use_map:
        nodes_to_start.append(Node(
            package='nav2_map_server',
            executable='map_server',
            name='map_server',
            output='screen',
            parameters=[{'yaml_filename': map_yaml, 'use_sim_time': use_sim_time}]
        ))

        nodes_to_start.append(Node(
            package='nav2_lifecycle_manager',
            executable='lifecycle_manager',
            name='lifecycle_manager_map',
            output='screen',
            parameters=[
                {'use_sim_time': use_sim_time},
                {'autostart': True},
                {'node_names': ['map_server']}
            ]
        ))

    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')),
        launch_arguments={'num_robots': '1', 'lidar_mode': 'half', 'use_sim_time': use_sim_time, 'world': world_file, 'pose': pose_str}.items()
    ))


    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'autostart': 'true',
            'namespace': '',
            'use_rviz': 'true',
        }.items()
    ))

    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('skyhunter_navigation'), 'launch', 'ekf.launch.py')
        ),
        launch_arguments={'namespace': ''}.items() # Empty for leader
    ))

    # 3. Leader Node (Produces waypoints for R1)
    nodes_to_start.append(Node(
        package='skyhunter_control', executable='leader_node',
        output='screen', parameters=[{
            'use_sim_time': True,
            'initial_formation': int(formation_type)
        }],
        remappings=[('odom', '/odom_filtered'), ('leader_state', '/leader_state')]
    ))

    nodes_to_start.append(Node(
        package='skyhunter_perception', executable='yolo_detector_node',
        name='leader_yolo_node',
        output='screen',
        parameters=[{'robot_ns': ''}] # Empty string = Leader
    ))

    # ==========================================
    # --- SWARM LIDAR FILTER  ---
    # ==========================================
    nodes_to_start.append(Node(
        package='skyhunter_perception', 
        executable='swarm_lidar_filter',
        name='leader_lidar_filter',
        output='screen',
        parameters=[{
            'use_sim_time': True,
            'stealth_radius': 1.0  # Erase points within 1m of followers
        }],
        remappings=[
            ('scan/points', '/scan/points'),           # Input: Raw
            ('scan/points_filtered', '/scan/points_filtered') # Output: Clean
        ]
    ))

    # 4. Global Leadership Manager (Heartbeat + Relay for R1)
    # Note: We only use THIS, not the 'leader_relay' script.
    nodes_to_start.append(Node(
        package='skyhunter_control', executable='leadership_manager',
        name='leadership_manager_R1',
        parameters=[{'robot_int_id': 1}],
        remappings=[
            ('leader_state', '/leader_state'), 
            ('/swarm/virtual_leader/state', '/swarm/virtual_leader/state')
        ]
    ))

    # ====================================================
    # SWARM COMMUNICATION & MESH SIMULATION
    # ====================================================

    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_skyhunter_comm, 'launch', 'network.launch.py'))
    ))

    # ====================================================
    #  DYNAMIC FOLLOWERS (SH_02 TO SH_07)
    # ====================================================
    for i in range(2, num_robots + 1):
        follower_ns = f'SH_{i:02d}' # SH_02, SH_03...
        
        row = i // 2
        side = 1 if i % 2 == 0 else -1

        # Spawning coordinates
        spawn_dist_back = -2.5 * row 
        spawn_dist_side = 1.5 * side
        spawn_x = b_x + (spawn_dist_back * math.cos(b_yaw) - spawn_dist_side * math.sin(b_yaw))
        spawn_y = b_y + (spawn_dist_back * math.sin(b_yaw) + spawn_dist_side * math.cos(b_yaw))
        
        # Driving Formation Offsets
        off_dist = -3.0 * row  # Distance behind Leader path
        off_side = 1.2 * side  # Lateral offset (V-Shape)

        drive_off_back = -3.0 * row  # 3m behind for first row
        drive_off_side = 1.5 * side  # 1.5m to the side

        # STAGGERED SPAWN (5s Delay to prevent CPU crash)
        delay = float(i) * 5.0 

        follower_action = TimerAction(
            period=float(i) * 1.5 + 5.0,
            actions=[
                # A. Spawn Robot
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")),
                    launch_arguments={
                        "robot_ns": follower_ns, 
                        "pose": f"{spawn_x} {spawn_y} {b_z + 0.15} 0 0 {b_yaw}", 
                        "use_sim_time": use_sim_time, 
                        "lidar_mode": "low", 
                        "use_ekf": "true"}.items(),
                ),

                # B. Nav2 (NAMESPACED & NO RVIZ)
                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
                    launch_arguments={
                        'use_sim_time': use_sim_time,
                        'autostart': 'true',
                        # 'params_file': os.path.join(pkg_tin3_navigation, 'config', 'nav2_params.yaml'),
                        'params_file': os.path.join(pkg_tin3_navigation, 'config', 'nav2_follower_params.yaml'),
                        'namespace': follower_ns,
                        'use_namespace': 'true',
                        'use_rviz': 'false',
                        # 'map': map_yaml 
                    }.items()
                ),

                IncludeLaunchDescription(
                    PythonLaunchDescriptionSource(
                        os.path.join(get_package_share_directory('skyhunter_navigation'), 'launch', 'ekf.launch.py')
                    ),
                    launch_arguments={'namespace': follower_ns}.items()
                ),

                # C. Leadership Manager (The ID switcher)
                Node(
                    package='skyhunter_control', executable='leadership_manager',
                    namespace=follower_ns,
                    parameters=[{'robot_int_id': i}],
                    remappings=[
                        ('leader_state', f'/{follower_ns}/leader_state'), # In: My local logic
                        ('/swarm/virtual_leader/state', '/swarm/virtual_leader/state') # Out: Global Virtual
                    ]
                ),

                # D. Local Leader Node
                Node(
                    package='skyhunter_control', executable='leader_node',
                    namespace=follower_ns,
                    remappings=[
                        ('odom', f'/{follower_ns}/odom_filtered'),
                        ('leader_state', f'/{follower_ns}/leader_state'),
                        ('plan', f'/{follower_ns}/plan') 
                    ]
                ),

                # E. Follower Node
                Node(
                    package='skyhunter_control', executable='follower_node',
                    namespace=follower_ns,
                    parameters=[{
                        'use_sim_time': True,
                        'offset_dist': float(drive_off_back),
                        'offset_lateral': float(drive_off_side), 
                    }],
                    remappings=[
                        # This is why they didn't move: they must look at the VIRTUAL topic
                        ('leader_state', '/swarm/virtual_leader/state'), 
                        ('odom', f'/{follower_ns}/odom_filtered'),
                        # ('cmd_vel', f'/{follower_ns}/cmd_vel'),
                        ('cmd_vel', f'/{follower_ns}/cmd_vel_nav'),
                        ('scan/points', f'/{follower_ns}/scan/points'),
                        ('/tf', '/tf'), ('/tf_static', '/tf_static')
                    ]
                ),

                # Node(
                #     package='nav2_collision_monitor',
                #     executable='collision_monitor',
                #     namespace=follower_ns,
                #     output='screen',
                #     parameters=[os.path.join(pkg_tin3_navigation, 'config', 'collision_monitor.yaml')],
                #     remappings=[
                #         # This node listens to the 'nav' topic we just created
                #         ('cmd_vel_in', f'/{follower_ns}/cmd_vel_nav'), 
                #         # And outputs to the REAL motor topic
                #         ('cmd_vel_out', f'/{follower_ns}/cmd_vel'),
                #         # Use the filtered scan from your C++ node
                #         ('scan/points_filtered', f'/{follower_ns}/scan/points_filtered')
                #     ]
                # ),

                Node(
                    package='nav2_collision_monitor',
                    executable='collision_monitor',
                    namespace=follower_ns,
                    output='screen',
                    parameters=[
                        os.path.join(pkg_tin3_navigation, 'config', 'collision_monitor.yaml'),
                        {
                            'base_frame_id': f'{follower_ns}/base_footprint',
                            'odom_frame_id': f'{follower_ns}/odom'
                        }
                    ],
                    remappings=[
                        ('cmd_vel_in', f'/{follower_ns}/cmd_vel_nav'), 
                        ('cmd_vel_out', f'/{follower_ns}/cmd_vel'),
                        ('scan/points_filtered', f'/{follower_ns}/scan/points') 
                    ]
                ),

                # F. Perception Node (Standby Mode)
                Node(
                    package='skyhunter_perception', 
                    executable='yolo_detector_node',
                    namespace=follower_ns,
                    parameters=[{'robot_ns': follower_ns}]
                ),
            ]
        )
        nodes_to_start.append(follower_action)

    # SECTION 3: GOAL SENDER
    nodes_to_start.append(Node(
        package='skyhunter_nav_tools', executable='waypoint_sender',
        output='screen', parameters=[{'use_sim_time': use_sim_time}]
    ))
    swarm_monitor_node = Node(
        package='skyhunter_control',
        executable='swarm_monitor_node',
        output='screen',
        parameters=[{'use_sim_time': True}]
    )
    nodes_to_start.append(swarm_monitor_node)

    return nodes_to_start

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('num_robots', default_value='7'),
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        DeclareLaunchArgument('use_map', default_value='false'), 
        DeclareLaunchArgument('world', default_value='empty_world.sdf'),
        DeclareLaunchArgument('pose', default_value='0.0 0.0 0.5 0.0 0.0 0.0'),
        DeclareLaunchArgument('formation', default_value='0'), 
        OpaqueFunction(function=launch_setup)
    ])

