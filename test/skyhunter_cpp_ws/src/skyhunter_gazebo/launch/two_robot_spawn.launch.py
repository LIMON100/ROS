import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, GroupAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node, PushRosNamespace
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    
    # --- Paths ---
    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
    pkg_yolobot_description = get_package_share_directory('yolobot_description')
    
    # --- World File ---
    # We use the existing nav_test.world, or you can create an empty one
    world_path = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'rough_terrain.world')
    
    # --- Robot Description (URDF) ---
    urdf_file = os.path.join(pkg_yolobot_description, 'robot', 'skyhunter_pro_v3.urdf')
    with open(urdf_file, 'r') as infp:
        robot_desc = infp.read()

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

    # ====================================================
    # ROBOT 1 (LEADER) - Spawning at (0, 0, 0.3)
    # ====================================================
    robot1_group = GroupAction([
        PushRosNamespace('robot1'),
        
        # 1. Robot State Publisher (Publishes TF /robot1/base_link, etc.)
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{
                'use_sim_time': True,
                'robot_description': robot_desc,
                'frame_prefix': 'robot1/'  # <--- CRITICAL: Prefixes all TF frames
            }]
        ),

        # 2. Spawn Entity in Gazebo
        Node(
            package='gazebo_ros',
            executable='spawn_entity.py',
            name='spawn_robot1',
            arguments=[
                '-topic', 'robot_description',
                '-entity', 'skyhunter_1',
                '-x', '0.0', '-y', '0.0', '-z', '0.3',
                '-robot_namespace', 'robot1'
            ],
            output='screen'
        )
    ])

    # ====================================================
    # ROBOT 2 (FOLLOWER) - Spawning at (-2.0, 2.0, 0.3)
    # This is the "V-Formation" Left Wing position
    # ====================================================
    robot2_group = GroupAction([
        PushRosNamespace('robot2'),

        # 1. Robot State Publisher
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            name='robot_state_publisher',
            output='screen',
            parameters=[{
                'use_sim_time': True,
                'robot_description': robot_desc,
                'frame_prefix': 'robot2/' # <--- Unique Prefix
            }]
        ),

        # 2. Spawn Entity
        Node(
            package='gazebo_ros',
            executable='spawn_entity.py',
            name='spawn_robot2',
            arguments=[
                '-topic', 'robot_description',
                '-entity', 'skyhunter_2',
                '-x', '-2.0', '-y', '2.0', '-z', '0.3',
                '-robot_namespace', 'robot2'
            ],
            output='screen'
        )
    ])

    return LaunchDescription([
        gz_server,
        gz_client,
        robot1_group,
        robot2_group
    ])