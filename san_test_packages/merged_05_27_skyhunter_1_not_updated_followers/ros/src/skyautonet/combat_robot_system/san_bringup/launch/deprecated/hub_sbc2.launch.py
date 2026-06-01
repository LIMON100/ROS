# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

r"""
Hub UGV SBC #2 — fixed-role launch wrapper (DCN-2026-011 D-034).

Equivalent to:
  ros2 launch san_bringup squadron.launch.py \\
      robot_id:=2 robot_role:=hub sbc_id:=2 \\
      hub_features:=true include_regression:=auto

Identical to hub_sbc1.launch.py except for sbc_id=2 — both SBCs of the
Hub UGV share the same robot_id (2) and robot_role (hub), differing
only in the physical slot identifier so RobotStatus.sbc{1,2}_healthy
(filled by DCN-2026-011 D-033) reports the correct slot.
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

    squadron = PathJoinSubstitution([
        FindPackageShare("san_bringup"), "launch", "squadron.launch.py",
    ])

    return LaunchDescription([
        DeclareLaunchArgument(
            "deployment_mode", default_value="production",
            description="production | demo | lab_test | bench | development",
        ),
        DeclareLaunchArgument(
            "include_regression", default_value="auto",
            description="auto | true | false",
        ),
        LogInfo(msg=["[hub_sbc2] Launching Hub UGV SBC #2 profile"]),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(squadron),
            launch_arguments={
                "robot_id":            "2",
                "robot_role":          "hub",
                "sbc_id":              "2",
                "hub_features":        "true",
                "include_regression":  include_regression,
                "deployment_mode":     deployment_mode,
            }.items(),
        ),
    ])
