import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')
    pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
    pkg_yolobot = get_package_share_directory('yolobot_description')

    # 1. Start Gazebo with the SDF world
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')),
        launch_arguments={'gz_args': f'-r {os.path.join(pkg_skyhunter_gazebo, "worlds", "rough_terrain.sdf")}'}.items(),
    )

    # 2. Spawn Robot
    # Note: In Harmonic, we use 'create' instead of 'spawn_entity.py'
    spawn_robot = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=[
            '-name', 'skyhunter_1',
            '-topic', 'robot_description',
            '-x', '0', '-y', '0', '-z', '0.5'
        ],
        output='screen'
    )

    # 3. State Publisher
    urdf_file = os.path.join(pkg_yolobot, 'robot', 'skyhunter_pro_v3.urdf.xacro')
    # (Assuming you process xacro via command or read file)
    with open(urdf_file, 'r') as f:
        robot_desc = f.read()

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[{'robot_description': robot_desc, 'use_sim_time': True}],
        output='screen'
    )

    # 4. Bridge (Manual for now to test)
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '/model/skyhunter_1/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist',
            '/model/skyhunter_1/odometry@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            '/world/rough_terrain/model/skyhunter_1/link/lidar_link/sensor/lidar_sensor/scan/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked'
        ],
        output='screen'
    )

    return LaunchDescription([
        gz_sim,
        robot_state_publisher,
        spawn_robot,
        bridge
    ])