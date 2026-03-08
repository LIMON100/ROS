# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import IncludeLaunchDescription
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch_ros.actions import Node

# def generate_launch_description():
#     pkg_tin3_gz_simulation = get_package_share_directory('tin3_gz_simulation')
#     pkg_tin3_navigation = get_package_share_directory('tin3_navigation')
    
#     # Define the robot's namespace
#     robot_namespace = 'robot_01'

#     # 1. Launch the Gazebo simulation with a single robot
#     sim_launch = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')
#         ),
#         launch_arguments={
#             'num_robots': '1',
#             # This spawn_robot.launch.py will automatically handle namespacing
#         }.items()
#     )

#     # 2. Launch the Nav2 stack for that single robot
#     nav2_launch = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_tin3_navigation, 'launch', 'nav2.launch.py')
#         ),
#         launch_arguments={
#             'use_sim_time': 'true',
#             'autostart': 'true',
#         }.items()
#     )
    
#     # 3. Launch the Leader Node from the skyhunter_formation package
#     leader_node = Node(
#         package='skyhunter_formation',
#         executable='leader_node',
#         namespace=robot_namespace,
#         output='screen',
#         parameters=[{'use_sim_time': True}],
#         remappings=[
#             # Remap the generic 'odom' topic to the robot's namespaced odom
#             ('odom', f'/{robot_namespace}/odom'),
#             # The output topic will be automatically namespaced to /robot_01/leader_state
#         ]
#     )

#     return LaunchDescription([
#         sim_launch,
#         nav2_launch,
#         leader_node
#     ])



import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    pkg_tin3_gz_simulation = get_package_share_directory('tin3_gz_simulation')
    pkg_tin3_navigation = get_package_share_directory('tin3_navigation')
    
    # Define the robot's namespace
    robot_namespace = 'robot_01'
    
    # Argument for simulation time
    use_sim_time = LaunchConfiguration('use_sim_time', default='true')

    # 1. Launch the Gazebo simulation with a single robot
    sim_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_tin3_gz_simulation, 'launch', 'sim.launch.py')
        ),
        launch_arguments={
            'num_robots': '1',
            'lidar_mode': 'half',
            'use_sim_time': use_sim_time
        }.items()
    )

    # 2. Launch the Nav2 stack for that single robot
    # Note: We include the namespaced nav2 launch if available, or the standard one
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
    
    # 3. Launch the Leader Node (Phase 2 Updated)
    leader_node = Node(
        package='skyhunter_formation',
        executable='leader_node',
        namespace=robot_namespace,
        output='screen',
        parameters=[{
            'use_sim_time': use_sim_time,
            'lookahead_dist_1': 2.0,  # 2 Meters ahead
            'lookahead_dist_2': 5.0   # 5 Meters ahead (Increased for stability)
        }],
        remappings=[
            # Remap generic inputs to the namespaced topics
            # ('odom', f'/{robot_namespace}/odom'),
            ('odom', '/odom'),
            # CRITICAL: Remap 'plan' to the topic Nav2 publishes the global path on
            # Usually /plan or /robot_01/plan depending on Nav2 config
            ('plan', '/plan'),
            # Output topic
            ('leader_state', f'/{robot_namespace}/leader_state')
        ]
    )

    return LaunchDescription([
        sim_launch,
        nav2_launch,
        leader_node
    ])