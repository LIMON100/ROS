import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, RegisterEventHandler
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.event_handlers import OnProcessExit
from launch_ros.actions import Node
import xacro

def generate_launch_description():
    pkg_name = 'my_bot_nav2'
    pkg_share = get_package_share_directory(pkg_name)
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')
    
    # Paths
    world_file = os.path.join(pkg_share, 'worlds', 'obstacles.sdf')
    urdf_file = os.path.join(pkg_share, 'urdf', 'robot.urdf.xacro')
    bridge_config = os.path.join(pkg_share, 'config', 'bridge_config.yaml')
    slam_config = os.path.join(pkg_share, 'config', 'mapper_params_online_async.yaml')
    nav2_params = os.path.join(get_package_share_directory('nav2_bringup'), 'params', 'nav2_params.yaml')
    
    # Process URDF
    doc = xacro.process_file(urdf_file)
    robot_description = doc.toxml()

    # 1. Start Gazebo (Using the official ros_gz_sim launch)
    # This prevents the crash by handling resources correctly
    start_gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')
        ),
        launch_arguments={'gz_args': f'-r {world_file}'}.items(),
    )

    # 2. Robot State Publisher
    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description, 'use_sim_time': True}]
    )

    # 3. Spawn Robot
    spawn_robot = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=['-topic', 'robot_description', 
                   '-name', 'my_bot', 
                   '-z', '0.5'],
        output='screen'
    )

    # 4. ROS-GZ Bridge
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=['--ros-args', '-p', f'config_file:={bridge_config}'],
        output='screen'
    )

    # 5. SLAM Toolbox
    slam = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(get_package_share_directory('slam_toolbox'), 'launch', 'online_async_launch.py')
        ]),
        launch_arguments={
            'params_file': slam_config, 
            'use_sim_time': 'true'
        }.items()
    )

    # 6. Nav2 Bringup
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource([
            os.path.join(get_package_share_directory('nav2_bringup'), 'launch', 'navigation_launch.py')
        ]),
        launch_arguments={
            'use_sim_time': 'true', 
            'params_file': nav2_params
        }.items()
    )

    return LaunchDescription([
        start_gazebo,
        node_robot_state_publisher,
        spawn_robot,
        bridge,
        slam,
        nav2
    ])