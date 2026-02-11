import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    pkg_tin3_gz_simulation = get_package_share_directory('tin3_gz_simulation')
    pkg_tin3_navigation = get_package_share_directory('tin3_navigation')
    
    use_sim_time = LaunchConfiguration('use_sim_time', default='true')

    nodes = []

    # ====================================================
    # 1. WORLD & LEADER (Global Namespace)
    # ====================================================
    # We launch sim with num_robots=1. This creates the world + Robot 1 (Global)
    sim_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')
        ),
        launch_arguments={
            'num_robots': '1', 
            'lidar_mode': 'full',
            'use_sim_time': use_sim_time
        }.items()
    )
    nodes.append(sim_launch)

    # ====================================================
    # 2. NAV2 (Global Namespace - GUARANTEED TO WORK)
    # ====================================================
    nav2_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')
        ),
        launch_arguments={
            'use_sim_time': use_sim_time,
            'autostart': 'true',
            'params_file': os.path.join(pkg_tin3_navigation, 'config', 'nav2_params.yaml')
        }.items()
    )
    nodes.append(nav2_launch)

    # ====================================================
    # 3. LEADER NODE (Global Namespace)
    # ====================================================
    leader_node = Node(
        package='skyhunter_formation',
        executable='leader_node',
        name='leader_node',
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
        remappings=[
            ('odom', '/odom'),           # Global Odom
            ('leader_state', '/leader_state') # Global Topic
        ]
    )
    nodes.append(leader_node)

    # ====================================================
    # 4. FOLLOWER (Robot 2) - Manually Spawned
    # ====================================================
    # We delay this slightly to let the world load
    # ====================================================
    # 4. FOLLOWER (Robot 2) - Robust Version
    # ====================================================
    spawn_follower = TimerAction(
        period=8.0, # Give leader more time to start AMCL
        actions=[
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")
                ),
                launch_arguments={
                    "robot_ns": "robot_02",
                    "pose": "-2.0 2.0 0.5 0 0 0",
                    "use_sim_time": use_sim_time,
                    "use_ekf": "true" # Disable EKF to prevent TF conflicts
                }.items(),
            ),

            # LINK 1: map -> robot_02/odom (The global connection)
            Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name='map_to_robot02_odom',
                arguments=['0', '0', '0', '0', '0', '0', 'map', 'robot_02/odom'],
                parameters=[{'use_sim_time': True}]
            ),

            # LINK 2: robot_02/odom -> robot_02/base_footprint (The body connection)
            # This "glues" the robot body to the odom frame so it's not lost in space
            Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name='robot02_odom_to_base',
                arguments=['0', '0', '0', '0', '0', '0', 'robot_02/odom', 'robot_02/base_footprint'],
                parameters=[{'use_sim_time': True}]
            ),
            
            # Follower Logic
            Node(
                package='skyhunter_formation',
                executable='follower_node',
                namespace='robot_02',
                parameters=[{
                    'use_sim_time': True,
                    'offset_x': -2.0,
                    'offset_y': 2.0,
                    'leader_topic': '/leader_state', 
                    'robot_width': 0.9,
                    'map_topic': '' 
                }],
                remappings=[
                    ('odom', '/robot_02/odom'),
                    ('cmd_vel', '/robot_02/cmd_vel'),
                    ('leader_state', '/leader_state')
                ]
            )
        ]
    )
    nodes.append(spawn_follower)

    return LaunchDescription(nodes)

# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, TimerAction
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch.substitutions import LaunchConfiguration
# from launch_ros.actions import Node

# def generate_launch_description():
#     pkg_tin3_gz_simulation = get_package_share_directory('tin3_gz_simulation')
#     pkg_tin3_navigation = get_package_share_directory('tin3_navigation')
#     pkg_formation = get_package_share_directory('skyhunter_formation')

#     use_sim_time = LaunchConfiguration('use_sim_time', default='true')

#     nodes = []

#     # ====================================================
#     # 1. WORLD & LEADER (Global Namespace)
#     # ====================================================
#     sim_launch = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')
#         ),
#         launch_arguments={
#             'num_robots': '1', 
#             'lidar_mode': 'full',
#             'use_sim_time': use_sim_time
#         }.items()
#     )
#     nodes.append(sim_launch)

#     # ====================================================
#     # 2. LEADER NAV2 & LOGIC
#     # ====================================================
#     nav2_launch = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')
#         ),
#         launch_arguments={
#             'use_sim_time': use_sim_time,
#             'autostart': 'true',
#             'params_file': os.path.join(pkg_tin3_navigation, 'config', 'nav2_params.yaml')
#         }.items()
#     )
#     nodes.append(nav2_launch)

#     leader_node = Node(
#         package='skyhunter_formation',
#         executable='leader_node',
#         namespace='robot_01', # Namespace helps organize
#         name='leader_node',
#         output='screen',
#         parameters=[{
#             'use_sim_time': use_sim_time,
#             'lookahead_dist_1': 2.0,
#             'lookahead_dist_2': 4.0
#         }],
#         remappings=[
#             ('odom', '/odom'),           
#             ('plan', '/plan'),           
#             ('leader_state', '/leader_state') 
#         ]
#     )
#     nodes.append(leader_node)

#     # ====================================================
#     # 3. SPAWN FOLLOWER (Robot 2) sequence
#     # ====================================================
#     follower_sequence = TimerAction(
#         period=5.0,
#         actions=[
#             # A. Spawn Entity & State Publisher
#             IncludeLaunchDescription(
#                 PythonLaunchDescriptionSource(
#                     os.path.join(pkg_tin3_gz_simulation, 'launch', 'spawn_robot.launch.py')
#                 ),
#                 launch_arguments={
#                     'robot_ns': 'robot_02',
#                     'pose': '-2.0 2.0 0.5 0 0 0', 
#                     'use_sim_time': use_sim_time
#                 }.items()
#             ),

#             # B. Static TF: Map -> Robot 2 Odom
#             # This anchors the follower in the world
#             Node(
#                 package='tf2_ros',
#                 executable='static_transform_publisher',
#                 name='static_tf_map_r2',
#                 arguments=['0', '0', '0', '0', '0', '0', 'map', 'robot_02/odom'],
#                 parameters=[{'use_sim_time': use_sim_time}],
#                 output='screen'
#             ),

#             # C. EKF: Robot 2 Odom -> Robot 2 Base_Footprint
#             # THIS FIXES THE "INVALID FRAME ID" ERROR
#             Node(
#                 package='robot_localization',
#                 executable='ekf_node',
#                 name='ekf_filter_node',
#                 namespace='robot_02',
#                 output='screen',
#                 parameters=[
#                     os.path.join(pkg_tin3_navigation, 'config', 'follower_ekf.yaml'),
#                     {
#                         'use_sim_time': use_sim_time,
#                         'odom_frame': 'robot_02/odom',
#                         'base_link_frame': 'robot_02/base_footprint',
#                         'world_frame': 'robot_02/odom',
#                     }
#                 ],
#                 remappings=[
#                     ('odometry/filtered', 'odom_filtered'),
#                     # Feed the raw odom topic into the EKF
#                     ('odom', '/robot_02/odom') 
#                 ]
#             ),
            
#             # D. Follower Logic Node
#             Node(
#                 package='skyhunter_formation',
#                 executable='follower_node',
#                 namespace='robot_02',
#                 name='follower_node',
#                 output='screen',
#                 parameters=[{
#                     'use_sim_time': use_sim_time,
#                     'offset_x': -2.0,
#                     'offset_y': 2.0,
#                     'leader_topic': '/leader_state', 
#                     'robot_width': 0.9,
#                     # Disable map checks for now to get basic following working
#                     'map_topic': '' 
#                 }],
#                 remappings=[
#                     ('odom', '/robot_02/odom'), 
#                     # Note: We use raw odom for logic, EKF handles the TF tree
#                     ('cmd_vel', '/robot_02/cmd_vel'),
#                     ('leader_state', '/leader_state')
#                 ]
#             )
#         ]
#     )
#     nodes.append(follower_sequence)

#     return LaunchDescription(nodes)