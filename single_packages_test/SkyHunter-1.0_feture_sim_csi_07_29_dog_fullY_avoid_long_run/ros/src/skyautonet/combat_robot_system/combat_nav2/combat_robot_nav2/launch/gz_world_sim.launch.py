"""Shared Gazebo world for the swarm sim — one gz instance, one /clock bridge.

Per-robot models/sensors/nav2 are brought up separately by robot_bringup_sim.launch.py
so that N robots share this single world.
"""
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (DeclareLaunchArgument, IncludeLaunchDescription,
                            OpaqueFunction, SetEnvironmentVariable)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _setup(context, *args, **kwargs):
    pkg_nav = get_package_share_directory('combat_robot_nav2')
    pkg_desc = get_package_share_directory('combat_robot_description')

    gz_resource_path = SetEnvironmentVariable(
        name='GZ_SIM_RESOURCE_PATH',
        value=os.path.join(pkg_desc, '..'))

    # world 인자: world/ 아래 파일명(예 sejong.world, poc_runway_world.sdf).
    # 모든 월드는 동일 spherical_coordinates datum 을 써야 GPS 스택이 호환된다.
    world_name = LaunchConfiguration('world').perform(context)
    world_file = os.path.join(pkg_nav, 'world', world_name)

    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('ros_gz_sim'), 'launch', 'gz_sim.launch.py')),
        launch_arguments={'gz_args': f'-r {world_file}'}.items())

    # /clock is global and bridged exactly once (shared by all robots).
    clock_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        name='clock_bridge',
        arguments=['/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock'],
        parameters=[{'use_sim_time': True}],
        output='screen')

    return [gz_resource_path, gazebo, clock_bridge]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('world', default_value='sejong.world',
                              description='world/ 아래 월드 파일명(sejong.world | poc_runway_world.sdf)'),
        OpaqueFunction(function=_setup),
    ])
