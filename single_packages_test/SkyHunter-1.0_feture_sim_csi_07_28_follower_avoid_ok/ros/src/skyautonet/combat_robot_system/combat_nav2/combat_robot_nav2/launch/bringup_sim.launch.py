import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, TimerAction, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    pkg_dir = get_package_share_directory('combat_robot_nav2')
    launch_dir = os.path.join(pkg_dir, 'launch')

    use_sim_time = LaunchConfiguration('use_sim_time')
    with_gnss = LaunchConfiguration('with_gnss')

    common_args = {'use_sim_time': use_sim_time}
    localization_args = {'use_sim_time': use_sim_time, 'with_gnss': with_gnss}

    gazebo_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(launch_dir, 'gazebo.launch.py')),
        launch_arguments=common_args.items()
    )

    localization_launch = TimerAction(
        period=5.0,
        actions=[IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'localization.launch.py')),
            launch_arguments=localization_args.items())]
    )

    navigation_launch = TimerAction(
        period=10.0,
        actions=[IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'navigation_sim.launch.py')),
            launch_arguments=common_args.items())]
    )

    map_rviz_launch = TimerAction(
        period=15.0,
        actions=[IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(launch_dir, 'map_rviz_sim.launch.py')),
            launch_arguments=common_args.items())]
    )

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        DeclareLaunchArgument('with_gnss', default_value='false',
                              description='Enable navsat_transform (requires /fix data)'),
        gazebo_launch,
        localization_launch,
        navigation_launch,
        map_rviz_launch
    ])
