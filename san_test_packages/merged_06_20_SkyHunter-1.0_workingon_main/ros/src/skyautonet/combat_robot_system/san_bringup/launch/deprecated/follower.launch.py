# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

r"""
Follower UGV — fixed-role launch wrapper (DCN-2026-011 D-034).

Equivalent to:
  ros2 launch san_bringup squadron.launch.py \\
      robot_id:=<N> robot_role:=follower sbc_id:=0 \\
      hub_features:=false include_regression:=auto

The follower fleet uses robot_id values 4-8 (v1.5 IDS topology); the
default here is 4. Operators provisioning followers 5/6/7/8 override
robot_id on the command line or in the systemd service Environment.
"""

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    LogInfo,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    deployment_mode = LaunchConfiguration("deployment_mode")
    include_regression = LaunchConfiguration("include_regression")
    robot_id = LaunchConfiguration("robot_id")

    squadron = PathJoinSubstitution([
        FindPackageShare("san_bringup"), "launch", "squadron.launch.py",
    ])

    return LaunchDescription([
        DeclareLaunchArgument(
            "robot_id", default_value="4",
            description="Follower slot in the squadron (4-8 by v1.5 IDS)",
        ),
        DeclareLaunchArgument(
            "deployment_mode", default_value="production",
            description="production | demo | lab_test | bench | development",
        ),
        DeclareLaunchArgument(
            "include_regression", default_value="auto",
            description="auto | true | false",
        ),
        LogInfo(msg=["[follower] Launching Follower UGV profile"]),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(squadron),
            launch_arguments={
                "robot_id":            robot_id,
                "robot_role":          "follower",
                "sbc_id":              "0",
                "hub_features":        "false",
                "include_regression":  include_regression,
                "deployment_mode":     deployment_mode,
            }.items(),
        ),
    ])
