# ### work all world + v-shape + lateral_offset - 02-19
# # import os
# # from ament_index_python.packages import get_package_share_directory
# # from launch import LaunchDescription
# # from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, TimerAction, OpaqueFunction
# # from launch.launch_description_sources import PythonLaunchDescriptionSource
# # from launch.substitutions import LaunchConfiguration
# # from launch_ros.actions import Node
# # import math

# # def launch_setup(context, *args, **kwargs):
# #     # --- 1. Setup Paths and Config ---
# #     pkg_tin3_gz_simulation = get_package_share_directory('tin3_gz_simulation')
# #     pkg_tin3_navigation = get_package_share_directory('tin3_navigation')
    
# #     num_robots = int(context.perform_substitution(LaunchConfiguration('num_robots')))
# #     use_sim_time = LaunchConfiguration('use_sim_time')
# #     world_file = context.perform_substitution(LaunchConfiguration('world'))
    
# #     # --- 2. AUTOMATIC WORLD DETECTION ---
# #     # Get the pose string from the launch argument
# #     pose_str = context.perform_substitution(LaunchConfiguration('pose'))
    
# #     # Logic: If world is Route 66 and the user didn't provide a custom pose, 
# #     # we automatically use the Route 66 starting point.
# #     if 'route_66' in world_file and pose_str == '0.0 0.0 0.5 0.0 0.0 0.0':
# #         pose_str = "2445.0 293.5 55.0 0 0 -3.023"
# #         print(f"[INFO] Route 66 detected. Auto-setting pose to: {pose_str}")

# #     # Parse the final pose string
# #     pose_parts = pose_str.split()
# #     b_x = float(pose_parts[0])
# #     b_y = float(pose_parts[1])
# #     b_z = float(pose_parts[2])
# #     b_yaw = float(pose_parts[5]) if len(pose_parts) > 5 else 0.0

# #     nodes_to_start = []

# #     # ====================================================
# #     # 1. WORLD & LEADER (Always Global Namespace)
# #     # ====================================================
# #     sim_launch = IncludeLaunchDescription(
# #         PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')),
# #         launch_arguments={
# #             'num_robots': '1', 
# #             'lidar_mode': 'half', 
# #             'use_sim_time': use_sim_time,
# #             'world': world_file,
# #             'pose': pose_str 
# #         }.items()
# #     )
# #     nodes_to_start.append(sim_launch)

# #     nav2_launch = IncludeLaunchDescription(
# #         PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
# #         launch_arguments={'use_sim_time': use_sim_time, 'autostart': 'true'}.items()
# #     )
# #     nodes_to_start.append(nav2_launch)

# #     leader_node = Node(
# #         package='skyhunter_formation', executable='leader_node',
# #         output='screen', parameters=[{'use_sim_time': True}],
# #         remappings=[('odom', '/odom'), ('leader_state', '/leader_state')]
# #     )
# #     nodes_to_start.append(leader_node)

# #     # ====================================================
# #     # 2. DYNAMIC FOLLOWERS
# #     # ====================================================
# #     for i in range(2, num_robots + 1):
# #         follower_ns = f'robot_{i:02d}'
# #         row = i // 2
# #         side = 1 if i % 2 == 0 else -1

# #         spawn_dist_back = -2.5 * row 
# #         spawn_dist_side = 1.5 * side
# #         spawn_x = b_x + (spawn_dist_back * math.cos(b_yaw) - spawn_dist_side * math.sin(b_yaw))
# #         spawn_y = b_y + (spawn_dist_back * math.sin(b_yaw) + spawn_dist_side * math.cos(b_yaw))
        
# #         # Driving Offsets (The C++ parameters)
# #         # Tight tactical spacing
# #         drive_off_back = -3.0 * row  # 3m behind for first row
# #         drive_off_side = 1.5 * side  # 1.5m to the side

# #         follower_action = TimerAction(
# #             period=float(i) * 1.5 + 4.0,
# #             actions=[
# #                 IncludeLaunchDescription(
# #                     PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")),
# #                     launch_arguments={
# #                         "robot_ns": follower_ns,
# #                         "pose": f"{spawn_x} {spawn_y} {b_z + 0.15} 0 0 {b_yaw}", # Lower drop
# #                         "use_sim_time": use_sim_time,
# #                         "lidar_mode": "half", # Use LOW for 7 robots to prevent CPU lag
# #                         "use_ekf": "true"
# #                     }.items(),
# #                 ),
# #                 Node(
# #                     package='tf2_ros', executable='static_transform_publisher',
# #                     name=f'link_{follower_ns}',
# #                     arguments=['0', '0', '0', '0', '0', '0', 'map', f'{follower_ns}/odom'],
# #                     parameters=[{'use_sim_time': True}]
# #                 ),
# #                 Node(
# #                     package='skyhunter_formation',
# #                     executable='follower_node',
# #                     namespace=follower_ns,
# #                     output='screen',
# #                     parameters=[{
# #                         'use_sim_time': True,
# #                         'offset_dist': float(drive_off_back),
# #                         'offset_lateral': float(drive_off_side), 
# #                         'leader_topic': '/leader_state'
# #                     }],
# #                     remappings=[
# #                         ('scan/points', f'/{follower_ns}/scan/points'), 
# #                         ('cmd_vel', f'/{follower_ns}/cmd_vel'),
# #                         ('odom', f'/{follower_ns}/odom_filtered'), 
# #                         ('leader_state', '/leader_state'),
# #                         ('/tf', '/tf'),
# #                         ('/tf_static', '/tf_static')
# #                     ]
# #                 )
# #             ]
# #         )
# #         nodes_to_start.append(follower_action)

# #     # 3. GOAL SENDER
# #     nodes_to_start.append(Node(
# #         package='skyhunter_nav_tools', executable='waypoint_sender',
# #         output='screen', parameters=[{'use_sim_time': use_sim_time}]
# #     ))

# #     return nodes_to_start

# # def generate_launch_description():
# #     return LaunchDescription([
# #         DeclareLaunchArgument('num_robots', default_value='1'),
# #         DeclareLaunchArgument('use_sim_time', default_value='true'),
# #         # Set default world to obstacle world
# #         DeclareLaunchArgument('world', default_value='obstacle_world.sdf'),
# #         # Set default pose to 0,0,0.5
# #         DeclareLaunchArgument('pose', default_value='0.0 0.0 0.5 0.0 0.0 0.0'),
# #         OpaqueFunction(function=launch_setup)
# #     ])










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
    
    num_robots = int(context.perform_substitution(LaunchConfiguration('num_robots')))
    use_sim_time = LaunchConfiguration('use_sim_time')
    world_file = context.perform_substitution(LaunchConfiguration('world'))
    
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

    # WITHOUT using the leader brain switch
    # ====================================================
    # SECTION 1: THE LEADER (Global Namespace for Stability)
    # ====================================================
    # Sim + Robot 1
    # nodes_to_start.append(IncludeLaunchDescription(
    #     PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')),
    #     launch_arguments={
    #         'num_robots': '1', 
    #         'lidar_mode': 'half', 
    #         'use_sim_time': use_sim_time,
    #         'world': world_file,
    #         'pose': pose_str 
    #     }.items()
    # ))

    # # Nav2 (The Brain)
    # nodes_to_start.append(IncludeLaunchDescription(
    #     PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
    #     launch_arguments={'use_sim_time': use_sim_time, 'autostart': 'true'}.items()
    # ))

    # # Leader Node (Global)
    # nodes_to_start.append(Node(
    #     package='skyhunter_formation', executable='leader_node',
    #     output='screen', parameters=[{'use_sim_time': True}],
    #     remappings=[('odom', '/odom'), ('leader_state', '/leader_state')]
    # ))

    # # ====================================================
    # # SECTION 2: DYNAMIC FOLLOWERS (SH_02 TO SH_07)
    # # ====================================================
    # # We loop to create a 7-unit swarm total
    # for i in range(2, num_robots + 1):
    #     follower_ns = f'SH_{i:02d}' # SH_02, SH_03...
        
    #     row = i // 2
    #     side = 1 if i % 2 == 0 else -1

    #     # Spawning coordinates
    #     spawn_dist_back = -2.5 * row 
    #     spawn_dist_side = 1.5 * side
    #     spawn_x = b_x + (spawn_dist_back * math.cos(b_yaw) - spawn_dist_side * math.sin(b_yaw))
    #     spawn_y = b_y + (spawn_dist_back * math.sin(b_yaw) + spawn_dist_side * math.cos(b_yaw))
        
    #     # Driving Formation Offsets
    #     off_dist = -3.0 * row  # Distance behind Leader path
    #     off_side = 1.2 * side  # Lateral offset (V-Shape)

    #     # STAGGERED SPAWN (5s Delay to prevent CPU crash)
    #     delay = float(i) * 5.0 

    #     follower_action = TimerAction(
    #         # period=delay,
    #         period=float(i) * 1.5 + 4.0,
    #         actions=[
    #             # A. Spawn physical model in namespace SH_02...
    #             IncludeLaunchDescription(
    #                 PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")),
    #                 launch_arguments={
    #                     "robot_ns": follower_ns,
    #                     "pose": f"{spawn_x} {spawn_y} {b_z + 0.15} 0 0 {b_yaw}",
    #                     "use_sim_time": use_sim_time,
    #                     "lidar_mode": "half", # Low density for followers saves CPU
    #                     "use_ekf": "true"
    #                 }.items(),
    #             ),
    #             # B. Static TF link (map -> SH_02/odom)
    #             Node(
    #                 package='tf2_ros', executable='static_transform_publisher',
    #                 name=f'link_{follower_ns}',
    #                 arguments=['0', '0', '0', '0', '0', '0', 'map', f'{follower_ns}/odom'],
    #                 parameters=[{'use_sim_time': True}]
    #             ),
    #             # Node(
    #             #     package='tf2_ros', executable='static_transform_publisher',
    #             #     name=f'link_{follower_ns}',
    #             #     # <-- FIX THIS LINE TO USE SPAWN COORDS:
    #             #     arguments=[str(spawn_x), str(spawn_y), '0', '0', '0', str(b_yaw), 'map', f'{follower_ns}/odom'],
    #             #     parameters=[{'use_sim_time': True}]
    #             # ),
    #             # C. Follower Node
    #             Node(
    #                 package='skyhunter_formation', executable='follower_node',
    #                 namespace=follower_ns, output='screen',
    #                 parameters=[{
    #                     'use_sim_time': True, 
    #                     'offset_dist': float(off_dist),
    #                     'offset_lateral': float(off_side) 
    #                 }],
    #                 remappings=[
    #                     ('scan/points', f'/{follower_ns}/scan/points'), 
    #                     ('cmd_vel', f'/{follower_ns}/cmd_vel'),
    #                     ('odom', f'/{follower_ns}/odom_filtered'), 
    #                     ('leader_state', '/leader_state'),
    #                     ('/tf', '/tf'), ('/tf_static', '/tf_static')
    #                 ]
    #             ),
    #         ]
    #     )
    #     nodes_to_start.append(follower_action)

    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')),
        launch_arguments={'num_robots': '1', 'lidar_mode': 'half', 'use_sim_time': use_sim_time, 'world': world_file, 'pose': pose_str}.items()
    ))


    nodes_to_start.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'autostart': 'true',
            'namespace': '', # LEAVE EMPTY FOR GLOBAL
            'use_rviz': 'true'
        }.items()
    ))

    # 3. Leader Node (Produces waypoints for R1)
    nodes_to_start.append(Node(
        package='skyhunter_control', executable='leader_node',
        output='screen', parameters=[{'use_sim_time': True}],
        remappings=[('odom', '/odom'), ('leader_state', '/leader_state')]
    ))

    nodes_to_start.append(Node(
        package='skyhunter_perception', executable='yolo_detector_node',
        name='leader_yolo_node',
        output='screen',
        parameters=[{'robot_ns': ''}] # Empty string = Leader
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
    # SECTION 4: SWARM COMMUNICATION & MESH SIMULATION
    # ====================================================
    # 1. WiFi6 Mesh RF Simulator
    nodes_to_start.append(Node(
        package='skyhunter_comm',
        executable='wifi6_mesh_sim',
        name='wifi6_mesh_sim',
        output='screen'
    ))

    # 2. Jammer Service (Allows you to dynamically break links)
    nodes_to_start.append(Node(
        package='skyhunter_comm',
        executable='jammer_service',
        name='jammer_service',
        output='screen'
    ))

    # 3. (Optional) RViz Mesh Visualizer - draws lines between robots
    nodes_to_start.append(Node(
        package='skyhunter_comm',
        executable='mesh_visualizer',
        name='mesh_visualizer',
        output='screen'
    ))

    # ====================================================
    # SECTION 2: DYNAMIC FOLLOWERS (SH_02 TO SH_07)
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
                        'use_rviz': 'false' # FIX: Only Robot-01 gets RViz
                    }.items()
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

                # D. Local Leader Node (CRITICAL FIX: Missing in your code!)
                # This node MUST be ready if this robot is elected as leader
                Node(
                    package='skyhunter_control', executable='leader_node',
                    namespace=follower_ns,
                    remappings=[
                        ('odom', f'/{follower_ns}/odom_filtered'),
                        ('leader_state', f'/{follower_ns}/leader_state'),
                        ('plan', f'/{follower_ns}/plan')  # <--- ADD THIS LINE !!!
                    ]
                    # remappings=[
                    #     ('odom', f'/{follower_ns}/odom'), # <--- FIXED: Removed _filtered
                    #     ('leader_state', f'/{follower_ns}/leader_state'),
                    #     ('plan', f'/{follower_ns}/plan')  
                    # ]
                ),

                # Node(
                #     package='tf2_ros', executable='static_transform_publisher',
                #     name=f'link_{follower_ns}',
                #     arguments=[str(spawn_x), str(spawn_y), '0', '0', '0', str(b_yaw), 'map', f'{follower_ns}/odom'],
                #     parameters=[{'use_sim_time': True}]
                # ),

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
                        ('cmd_vel', f'/{follower_ns}/cmd_vel'),
                        ('scan/points', f'/{follower_ns}/scan/points'),
                        ('/tf', '/tf'), ('/tf_static', '/tf_static')
                    ]
                ),
                # F. Perception Node (Standby Mode)
                Node(
                    package='skyhunter_perception', 
                    executable='yolo_detector_node',
                    namespace=follower_ns,
                    parameters=[{'robot_ns': follower_ns}] 
                    # No remappings needed! The C++ code builds the correct topics automatically.
                )
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
        DeclareLaunchArgument('num_robots', default_value='2'),
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        DeclareLaunchArgument('world', default_value='empty_world.sdf'),
        DeclareLaunchArgument('pose', default_value='0.0 0.0 0.5 0.0 0.0 0.0'),
        OpaqueFunction(function=launch_setup)
    ])

