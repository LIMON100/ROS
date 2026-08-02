import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.conditions import IfCondition, UnlessCondition
from launch_ros.actions import Node


def generate_launch_description():
    pkg_dir = get_package_share_directory('combat_robot_nav2')
    ekf_config_file = os.path.join(pkg_dir, 'config', 'ekf.yaml')

    use_sim_time = LaunchConfiguration('use_sim_time')
    with_gnss = LaunchConfiguration('with_gnss')

    # gnss_base_link → gps static TF
    tf_gnss_fix = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        arguments=['0', '0', '0', '0', '0', '0', 'gnss_base_link', 'gps'],
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
        condition=IfCondition(with_gnss)
    )

    # GNSS 미사용 시 map → odom 고정
    static_map_to_odom = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='static_map_to_odom',
        arguments=['0', '0', '0', '0', '0', '0', 'map', 'odom'],
        output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
        condition=UnlessCondition(with_gnss)
    )

    # EKF odom
    ekf_local_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node_odom',
        output='screen',
        parameters=[ekf_config_file, {'use_sim_time': use_sim_time}],
        remappings=[('odometry/filtered', 'odometry/local')]
    )

    # EKF map
    ekf_global_node = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node_map',
        output='screen',
        parameters=[ekf_config_file, {'use_sim_time': use_sim_time}],
        remappings=[('odometry/filtered', 'odometry/global')],
        condition=IfCondition(with_gnss)
    )

    # gps_to_map: /fix → /odometry/gps_map (ENU, frame=map) 직접 변환.
    # navsat_transform_node 가 datum/transform 은 계산하고도 /odometry/gps 위치를
    # 하드 0 으로 발행하는 버그(주행해도 map 위치 고정)를 우회한다.
    gps_to_map_node = Node(
        package='combat_robot_nav2',
        executable='gps_to_map_node',
        name='gps_to_map',
        output='screen',
        parameters=[{
            'use_sim_time': use_sim_time,
            'fix_topic': '/fix',
            'output_topic': '/odometry/gps_map',
            'map_frame': 'map',
            'base_frame': 'base_footprint',
        }],
        condition=IfCondition(with_gnss)
    )

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('with_gnss', default_value='true',
                              description='Enable GNSS-based global localization'),
        tf_gnss_fix,
        static_map_to_odom,
        ekf_local_node,
        ekf_global_node,
        gps_to_map_node,
    ])