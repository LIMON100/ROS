import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    pkg_share = get_package_share_directory('simple_nav_bot')
    pkg_ros_gz = get_package_share_directory('ros_gz_sim')
    pkg_nav2 = get_package_share_directory('nav2_bringup')
    
    # 1. Gazebo (Empty World)
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_ros_gz, 'launch', 'gz_sim.launch.py')),
        launch_arguments={'gz_args': '-r empty.sdf'}.items()
    )

    # 2. Spawn Robot
    # We pass the robot description as a string to the spawn node
    xacro_file = os.path.join(pkg_share, 'urdf', 'robot.urdf.xacro')
    # Simple way to read xacro content
    import xacro
    doc = xacro.process_file(xacro_file)
    robot_desc = doc.toxml()

    spawn_robot = Node(package='ros_gz_sim', executable='create',
        arguments=['-name', 'simple_bot', '-string', robot_desc, '-x', '0', '-y', '0', '-z', '0.5'],
        output='screen')

    # 3. Robot State Publisher
    rsp = Node(package='robot_state_publisher', executable='robot_state_publisher',
        parameters=[{'robot_description': robot_desc, 'use_sim_time': True}],
        output='screen')

    # 4. Bridge
    bridge_config = os.path.join(pkg_share, 'config', 'bridge.yaml')
    bridge = Node(package='ros_gz_bridge', executable='parameter_bridge',
        parameters=[{'config_file': bridge_config, 'use_sim_time': True}],
        output='screen')

    # 5. SLAM Toolbox (Async)
    slam = Node(package='slam_toolbox', executable='async_slam_toolbox_node',
        parameters=[{'use_sim_time': True, 
                     'base_frame': 'base_link', 
                     'odom_frame': 'odom', 
                     'map_frame': 'map', 
                     'scan_topic': '/scan',
                     'mode': 'mapping'}],
        output='screen')

    # 6. Nav2 Bringup
    nav2_params = os.path.join(pkg_share, 'config', 'nav2_params.yaml')
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_nav2, 'launch', 'navigation_launch.py')),
        launch_arguments={
            'use_sim_time': 'True',
            'params_file': nav2_params,
            'autostart': 'True',
            'use_composition': 'False' # Run nodes separately for easier debug
        }.items()
    )

    # 7. RViz
    # You'll need to manually add Map, RobotModel, TF, Goal, Path in RViz initially
    rviz = Node(package='rviz2', executable='rviz2',
        arguments=['-d', os.path.join(pkg_share, 'config', 'default.rviz')], # Create empty rviz first
        parameters=[{'use_sim_time': True}])

    return LaunchDescription([
        gz_sim,
        spawn_robot,
        rsp,
        bridge,
        slam,
        nav2,
        rviz
    ])