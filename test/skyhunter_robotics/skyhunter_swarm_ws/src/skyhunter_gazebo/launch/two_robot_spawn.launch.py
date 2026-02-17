# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import IncludeLaunchDescription, GroupAction
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch_ros.actions import Node, PushRosNamespace
# from launch.substitutions import LaunchConfiguration

# def generate_launch_description():
    
#     # --- Paths ---
#     pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
#     pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
#     pkg_yolobot_description = get_package_share_directory('yolobot_description')
    
#     # --- World File ---
#     # We use the existing nav_test.world, or you can create an empty one
#     world_path = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'rough_terrain.world')
    
#     # --- Robot Description (URDF) ---
#     urdf_file = os.path.join(pkg_yolobot_description, 'robot', 'skyhunter_pro_v3.urdf')
#     with open(urdf_file, 'r') as infp:
#         robot_desc = infp.read()

#     # --- Gazebo Server & Client ---
#     gz_server = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_gazebo_ros, 'launch', 'gzserver.launch.py')
#         ),
#         launch_arguments={'world': world_path}.items()
#     )

#     gz_client = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_gazebo_ros, 'launch', 'gzclient.launch.py')
#         )
#     )

#     # ====================================================
#     # ROBOT 1 (LEADER) - Spawning at (0, 0, 0.3)
#     # ====================================================
#     robot1_group = GroupAction([
#         PushRosNamespace('robot1'),
        
#         # 1. Robot State Publisher (Publishes TF /robot1/base_link, etc.)
#         Node(
#             package='robot_state_publisher',
#             executable='robot_state_publisher',
#             name='robot_state_publisher',
#             output='screen',
#             parameters=[{
#                 'use_sim_time': True,
#                 'robot_description': robot_desc,
#                 'frame_prefix': 'robot1/'  # <--- CRITICAL: Prefixes all TF frames
#             }]
#         ),

#         # 2. Spawn Entity in Gazebo
#         Node(
#             package='gazebo_ros',
#             executable='spawn_entity.py',
#             name='spawn_robot1',
#             arguments=[
#                 '-topic', 'robot_description',
#                 '-entity', 'skyhunter_1',
#                 '-x', '0.0', '-y', '0.0', '-z', '0.3',
#                 '-robot_namespace', 'robot1'
#             ],
#             output='screen'
#         )
#     ])

#     # ====================================================
#     # ROBOT 2 (FOLLOWER) - Spawning at (-2.0, 2.0, 0.3)
#     # This is the "V-Formation" Left Wing position
#     # ====================================================
#     robot2_group = GroupAction([
#         PushRosNamespace('robot2'),

#         # 1. Robot State Publisher
#         Node(
#             package='robot_state_publisher',
#             executable='robot_state_publisher',
#             name='robot_state_publisher',
#             output='screen',
#             parameters=[{
#                 'use_sim_time': True,
#                 'robot_description': robot_desc,
#                 'frame_prefix': 'robot2/' # <--- Unique Prefix
#             }]
#         ),

#         # 2. Spawn Entity
#         Node(
#             package='gazebo_ros',
#             executable='spawn_entity.py',
#             name='spawn_robot2',
#             arguments=[
#                 '-topic', 'robot_description',
#                 '-entity', 'skyhunter_2',
#                 '-x', '-2.0', '-y', '2.0', '-z', '0.3',
#                 '-robot_namespace', 'robot2'
#             ],
#             output='screen'
#         )
#     ])

#     return LaunchDescription([
#         gz_server,
#         gz_client,
#         robot1_group,
#         robot2_group
#     ])


import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, GroupAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node, PushRosNamespace

def generate_launch_description():

    # --- Paths ---
    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
    pkg_yolobot_description = get_package_share_directory('yolobot_description')
    
    # --- Config ---
    world_path = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'rough_terrain.world')
    
    # --- Gazebo Server & Client ---
    gz_server = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gzserver.launch.py')
        ),
        launch_arguments={'world': world_path}.items()
    )

    gz_client = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gzclient.launch.py')
        )
    )

    # --- Read URDF ---
    urdf_file = os.path.join(pkg_yolobot_description, 'robot', 'skyhunter_pro_v3.urdf')
    with open(urdf_file, 'r') as infp:
        robot_desc_original = infp.read()

    # ====================================================
    # ROBOT 1 (LEADER)
    # ====================================================
    robot1_name = 'robot1'
    
    # Unique Plugin Names
    robot1_desc = robot_desc_original.replace('name="gps_plugin"', f'name="{robot1_name}_gps_plugin"')
    robot1_desc = robot1_desc.replace('name="imu_plugin"', f'name="{robot1_name}_imu_plugin"')
    robot1_desc = robot1_desc.replace('name="lidar_plugin"', f'name="{robot1_name}_lidar_plugin"')
    robot1_desc = robot1_desc.replace('name="differential_drive_controller"', f'name="{robot1_name}_diff_drive"')
    robot1_desc = robot1_desc.replace('name="joint_state_publisher"', f'name="{robot1_name}_joint_state"')
    robot1_desc = robot1_desc.replace('name="camera_rgb_controller"', f'name="{robot1_name}_camera_rgb_controller"')
    robot1_desc = robot1_desc.replace('name="camera_ir_controller"', f'name="{robot1_name}_camera_ir_controller"')

    robot1_group = GroupAction([
        PushRosNamespace(robot1_name),
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            parameters=[{'use_sim_time': True, 'robot_description': robot1_desc, 'frame_prefix': f'{robot1_name}/'}]
        ),
        Node(
            package='skyhunter_formation',
            executable='leader_node',
            # FIX: Tell leader node to listen to local 'odom', not global '/odom'
            parameters=[{'use_sim_time': True, 'odom_topic': 'odom'}] 
        ),
        # EKF for Robot 1
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            parameters=[
                os.path.join(get_package_share_directory('skyhunter_localization'), 'config', 'ekf.yaml'),
                {'use_sim_time': True, 
                 'odom_frame': f'{robot1_name}/odom', 
                 'base_link_frame': f'{robot1_name}/base_link', 
                 'world_frame': f'{robot1_name}/odom'}
            ],
            # FIX: Remap the inputs defined in ekf.yaml (/odom) to the local topic (odom)
            remappings=[
                ('/odometry/filtered', 'odometry/filtered'),
                ('/odom', 'odom'),
                ('/imu/data', 'imu/data')
            ]
        )
    ])

    spawn_robot1 = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=['-topic', f'/{robot1_name}/robot_description', '-entity', robot1_name, '-x', '0.0', '-y', '0.0', '-z', '0.3'],
        output='screen'
    )

    # ====================================================
    # ROBOT 2 (FOLLOWER)
    # ====================================================
    robot2_name = 'robot2'

    # Unique Plugin Names
    robot2_desc = robot_desc_original.replace('name="gps_plugin"', f'name="{robot2_name}_gps_plugin"')
    robot2_desc = robot2_desc.replace('name="imu_plugin"', f'name="{robot2_name}_imu_plugin"')
    robot2_desc = robot2_desc.replace('name="lidar_plugin"', f'name="{robot2_name}_lidar_plugin"')
    robot2_desc = robot2_desc.replace('name="differential_drive_controller"', f'name="{robot2_name}_diff_drive"')
    robot2_desc = robot2_desc.replace('name="joint_state_publisher"', f'name="{robot2_name}_joint_state"')
    robot2_desc = robot2_desc.replace('name="camera_rgb_controller"', f'name="{robot2_name}_camera_rgb_controller"')
    robot2_desc = robot2_desc.replace('name="camera_ir_controller"', f'name="{robot2_name}_camera_ir_controller"')

    robot2_group = GroupAction([
        PushRosNamespace(robot2_name),
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            parameters=[{'use_sim_time': True, 'robot_description': robot2_desc, 'frame_prefix': f'{robot2_name}/'}]
        ),
        # Perception
        Node(
            package='skyhunter_perception',
            executable='elevation_mapper_node',
            parameters=[{'use_sim_time': True, 'base_frame': f'{robot2_name}/base_link', 'map_frame': f'{robot2_name}/odom'}]
        ),
        # Control
        Node(
            package='skyhunter_formation',
            executable='follower_node',
            parameters=[{
                'use_sim_time': True,
                'offset_x': -2.0, 'offset_y': 2.0,
                'leader_topic': '/robot1/leader_state', # Listen to Robot 1's state
                'map_topic': 'elevation_map'
            }]
        ),
        # EKF for Robot 2
        Node(
            package='robot_localization',
            executable='ekf_node',
            name='ekf_filter_node',
            parameters=[
                os.path.join(get_package_share_directory('skyhunter_localization'), 'config', 'ekf.yaml'),
                {'use_sim_time': True, 
                 'odom_frame': f'{robot2_name}/odom', 
                 'base_link_frame': f'{robot2_name}/base_link', 
                 'world_frame': f'{robot2_name}/odom'}
            ],
            # FIX: Remap the inputs to local topics
            remappings=[
                ('/odometry/filtered', 'odometry/filtered'),
                ('/odom', 'odom'),
                ('/imu/data', 'imu/data')
            ]
        )
    ])

    spawn_robot2 = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=['-topic', f'/{robot2_name}/robot_description', '-entity', robot2_name, '-x', '-2.0', '-y', '2.0', '-z', '0.3'],
        output='screen'
    )

    return LaunchDescription([
        gz_server,
        gz_client,
        robot1_group,
        spawn_robot1,
        robot2_group,
        spawn_robot2
    ])