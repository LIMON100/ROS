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
    
#     use_sim_time = LaunchConfiguration('use_sim_time', default='true')

#     nodes = []

#     # ====================================================
#     # 1. WORLD & LEADER (Global Namespace)
#     # ====================================================
#     # We launch sim with num_robots=1. This creates the world + Robot 1 (Global)
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
#     # 2. NAV2 (Global Namespace - GUARANTEED TO WORK)
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

#     # ====================================================
#     # 3. LEADER NODE (Global Namespace)
#     # ====================================================
#     leader_node = Node(
#         package='skyhunter_formation',
#         executable='leader_node',
#         name='leader_node',
#         output='screen',
#         parameters=[{'use_sim_time': use_sim_time}],
#         remappings=[
#             ('odom', '/odom'),           # Global Odom
#             ('leader_state', '/leader_state') # Global Topic
#         ]
#     )
#     nodes.append(leader_node)

#     # 4. FOLLOWER (Robot 2) - Robust Version
#     # ====================================================
#     # Inside swarm_robust.launch.py -> TimerAction:
#     follower_ns = 'robot_02' 
#     spawn_follower = TimerAction(
#         period=8.0,
#         actions=[
#             IncludeLaunchDescription(
#                 PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")),
#                 launch_arguments={
#                     "robot_ns": "robot_02",
#                     "pose": "-2.0 2.0 0.5 0 0 0",
#                     "use_sim_time": use_sim_time,
#                     "use_ekf": "true" # Keep EKF ON to publish odom -> base_footprint
#                 }.items(),
#             ),

#             # Link the GLOBAL map to this robot's LOCAL starting point
#             Node(
#                 package='tf2_ros',
#                 executable='static_transform_publisher',
#                 name='map_to_robot02_link',
#                 # This connects the two unconnected trees
#                 arguments=['0', '0', '0', '0', '0', '0', 'map', 'robot_02/odom'],
#                 parameters=[{'use_sim_time': True}],
#                 output='screen'
#             ),

#             # 3. Perception Node for Follower (Produces the map)
#             Node(
#                 package='skyhunter_perception',
#                 executable='elevation_mapper_node',
#                 namespace='robot_02',
#                 parameters=[{
#                     'use_sim_time': True,
#                     'base_frame': 'robot_02/base_footprint',
#                     'map_frame': 'map',
#                     'cloud_topic': '/robot_02/scan/points',
#                     'map.length': 8.0,
#                     'map.resolution': 0.15
#                 }]
#             ),
            
#             # The Follower Node
#             # Node(
#             #     package='skyhunter_formation',
#             #     executable='follower_node',
#             #     namespace='robot_02',
#             #     output='screen',
#             #     parameters=[{
#             #         'use_sim_time': True,
#             #         'offset_x': -2.5,
#             #         'offset_y': 2.0,
#             #         'leader_topic': '/leader_state'
#             #     }],
#             #     remappings=[
#             #         ('cmd_vel', '/robot_02/cmd_vel'),
#             #         ('elevation_map', '/robot_02/elevation_map'), # Connect Mapper to Follower
#             #         ('/tf', '/tf'),
#             #         ('/tf_static', '/tf_static')
#             #     ]
#             # )

#             Node(
#                 package='skyhunter_formation',
#                 executable='follower_node',
#                 namespace=follower_ns,
#                 output='screen',
#                 parameters=[{
#                     'use_sim_time': True,
#                     'offset_x': -2.5,
#                     'offset_y': 2.0,
#                     'leader_topic': '/leader_state'
#                 }],
#                 remappings=[
#                     ('scan/points', f'/{follower_ns}/scan/points'), 
#                     ('cmd_vel', f'/{follower_ns}/cmd_vel'),
#                     ('odom', f'/{follower_ns}/odom'),
                    
#                     # Keep this global so everyone hears the one Leader
#                     ('leader_state', '/leader_state'),
                    
#                     # Keep these global for TF tree consistency
#                     ('/tf', '/tf'),
#                     ('/tf_static', '/tf_static')
#                 ]
#             )
#         ]
#     )
#     nodes.append(spawn_follower)

#     return LaunchDescription(nodes)



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

    # 4. FOLLOWER (Robot 2) - Robust Version
    # ====================================================
    # Inside swarm_robust.launch.py -> TimerAction:
    follower_ns = 'robot_02' 
    spawn_follower = TimerAction(
        period=8.0,
        actions=[
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")),
                launch_arguments={
                    "robot_ns": "robot_02",
                    "pose": "-2.0 2.0 0.5 0 0 0",
                    "use_sim_time": use_sim_time,
                    "use_ekf": "true" # Keep EKF ON to publish odom -> base_footprint
                }.items(),
            ),

            # Link the GLOBAL map to this robot's LOCAL starting point
            Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name='map_to_robot02_link',
                # This connects the two unconnected trees
                arguments=['0', '0', '0', '0', '0', '0', 'map', 'robot_02/odom'],
                parameters=[{'use_sim_time': True}],
                output='screen'
            ),

            # 3. Perception Node for Follower (Produces the map)
            Node(
                package='skyhunter_perception',
                executable='elevation_mapper_node',
                namespace='robot_02',
                parameters=[{
                    'use_sim_time': True,
                    'base_frame': 'robot_02/base_footprint',
                    'map_frame': 'map',
                    'cloud_topic': '/robot_02/scan/points',
                    'map.length': 8.0,
                    'map.resolution': 0.15
                }]
            ),
            
            # The Follower Node
            Node(
                package='skyhunter_formation',
                executable='follower_node',
                namespace=follower_ns,
                output='screen',
                parameters=[{
                    'use_sim_time': True,
                    'offset_x': -2.5,
                    'offset_y': 2.0,
                    'leader_topic': '/leader_state'
                }],
                remappings=[
                    ('scan/points', f'/{follower_ns}/scan/points'), 
                    ('cmd_vel', f'/{follower_ns}/cmd_vel'),
                    ('odom', f'/{follower_ns}/odom'),
                    
                    # Keep this global so everyone hears the one Leader
                    ('leader_state', '/leader_state'),
                    
                    # Keep these global for TF tree consistency
                    ('/tf', '/tf'),
                    ('/tf_static', '/tf_static')
                ]
            )
        ]
    )
    nodes.append(spawn_follower)

    return LaunchDescription(nodes)
