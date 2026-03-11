"""
skyhunter_bringup — networking_demo.launch.py

Includes common_config (Domain 42 + FastDDS), launches Gazebo simulation,
and launches networking nodes (sim + networking, no nav2).

Usage:
    ros2 launch skyhunter_bringup networking_demo.launch.py num_robots:=8
"""

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration


def generate_launch_description():
    bringup_share = get_package_share_directory('skyhunter_bringup')
    sim_share = get_package_share_directory('skyhunter_gz_simulation')

    return LaunchDescription([
        # ── Common config (Domain 42 + FastDDS) ──
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(bringup_share, 'launch', 'common_config.launch.py'),
            ),
        ),

        # ── Sim arguments ──
        DeclareLaunchArgument('num_robots', default_value='8'),
        DeclareLaunchArgument('world', default_value=''),
        DeclareLaunchArgument('lidar_mode', default_value='full'),
        DeclareLaunchArgument('pose', default_value='0 0 0.5'),
        DeclareLaunchArgument('pattern', default_value='grid'),
        DeclareLaunchArgument('spacing', default_value='3.0'),
        DeclareLaunchArgument('use_sim_time', default_value='true'),

        # ── Launch simulation ──
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(sim_share, 'launch', 'sim.launch.py'),
            ),
            launch_arguments={
                'num_robots': LaunchConfiguration('num_robots'),
                'lidar_mode': LaunchConfiguration('lidar_mode'),
                'pose': LaunchConfiguration('pose'),
                'pattern': LaunchConfiguration('pattern'),
                'spacing': LaunchConfiguration('spacing'),
                'use_sim_time': LaunchConfiguration('use_sim_time'),
            }.items(),
        ),

        # ── TODO: Networking nodes (skyhunter_networking) ──
        # IncludeLaunchDescription(
        #     PythonLaunchDescriptionSource(
        #         os.path.join(
        #             get_package_share_directory('skyhunter_networking'),
        #             'launch', 'networking.launch.py',
        #         ),
        #     ),
        # ),
    ])