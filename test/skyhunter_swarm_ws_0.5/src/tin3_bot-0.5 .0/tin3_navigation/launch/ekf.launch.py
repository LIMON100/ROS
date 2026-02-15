# =============================================================================
# Standalone robot_localization EKF node.
# Fuses: odom (wheel odometry) + IMU
# Publishes: /odom_filtered + TF (odom → base_link)
# =============================================================================

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    pkg_tin3_navigation = get_package_share_directory("tin3_navigation")
    

    ekf_config = os.path.join(pkg_tin3_navigation, "config", "ekf.yaml")

    ekf_node = Node(
        package="robot_localization",
        executable="ekf_node",
        name="ekf_filter_node",
        output="screen",
        parameters=[ekf_config],
        remappings=[
            ("odometry/filtered", "odom_filtered"),
        ],
    )

    return LaunchDescription(
        [

            ekf_node,
           
        ]
    )