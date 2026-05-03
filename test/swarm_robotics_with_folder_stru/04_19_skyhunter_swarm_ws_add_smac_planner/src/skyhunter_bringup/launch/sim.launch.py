"""
tin3_bringup — sim.launch.py

Includes common_config (Domain 42 + FastDDS), then launches
the Gazebo simulation. All sim.launch.py arguments are passed through.

Usage:
    ros2 launch tin3_bringup sim.launch.py num_robots:=8
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
    sim_share = get_package_share_directory('skyhunter_gazebo')

    return LaunchDescription([
        # ── Common config (Domain 42 + FastDDS) ──
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(bringup_share, 'launch', 'common_config.launch.py'),
            ),
        ),

        # ── Pass-through sim arguments ──
        DeclareLaunchArgument('num_robots', default_value='1'),
        DeclareLaunchArgument('world', default_value='empty_world.sdf'),
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
                'world': LaunchConfiguration('world'), 
                'pose': LaunchConfiguration('pose'),
                'pattern': LaunchConfiguration('pattern'),
                'spacing': LaunchConfiguration('spacing'),
                'use_sim_time': LaunchConfiguration('use_sim_time'),
            }.items(),
        ),
    ])