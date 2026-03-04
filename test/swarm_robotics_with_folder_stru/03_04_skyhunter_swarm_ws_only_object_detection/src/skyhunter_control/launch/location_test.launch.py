import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    pkg_tin3_gz_simulation = get_package_share_directory('tin3_gz_simulation')
    pkg_tin3_navigation = get_package_share_directory('tin3_navigation')
    
    use_sim_time = LaunchConfiguration('use_sim_time', default='true')

    # 1. WORLD & LEADER (Robot 01 - Global)
    sim_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')),
        launch_arguments={'num_robots': '1', 'use_sim_time': use_sim_time}.items()
    )

    nav2_leader = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')),
        launch_arguments={'use_sim_time': use_sim_time, 'autostart': 'true'}.items()
    )

    # 2. FOLLOWER (Robot 02 - Namespaced)
    spawn_follower = TimerAction(
        period=8.0, # Increased delay to ensure EKF starts properly
        actions=[
            # A. Spawn Robot 02 + EKF Node
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(os.path.join(pkg_tin3_gz_simulation, 'launch', 'spawn_robot.launch.py')),
                launch_arguments={
                    'robot_ns': 'robot_02',
                    'pose': '-2.0 2.0 0.5 0 0 0',
                    'use_sim_time': use_sim_time,
                    'use_ekf': 'true' # <--- This starts the EKF inside robot_02 namespace
                }.items()
            ),

            # B. Static TF: map -> robot_02/odom
            Node(
                package='tf2_ros',
                executable='static_transform_publisher',
                name='map_to_robot02_odom',
                arguments=['-2', '2', '0', '0', '0', '0', 'map', 'robot_02/odom'],
                parameters=[{'use_sim_time': True}]
            ),
            
            # C. FOLLOWER LOGIC NODE (With Remappings)
            Node(
                package='skyhunter_formation',
                executable='follower_node',
                namespace='robot_02',
                output='screen',
                parameters=[{
                    'use_sim_time': True,
                    'offset_dist': -2.5,
                    'leader_topic': '/leader_state'
                }],
                # ============================================================
                # REMAPPINGS ARE ADDED HERE:
                # ============================================================
                remappings=[
                    # 1. Use EKF Output (Filtered) instead of raw Odom (Wheel slip)
                    ('odom', '/robot_02/odometry/filtered'), 
                    
                    # 2. Map local motor commands to namespaced topic
                    ('cmd_vel', '/robot_02/cmd_vel'),
                    
                    # 3. Connect namespaced node to global TF tree
                    ('/tf', '/tf'),
                    ('/tf_static', '/tf_static'),
                    
                    # 4. Listen to global leader
                    ('leader_state', '/leader_state')
                ]
                # ============================================================
            )
        ]
    )

    return LaunchDescription([
        sim_launch,
        nav2_leader,
        spawn_follower
    ])