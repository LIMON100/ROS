"""Leader (Unitree Go2) — fixed-role launch wrapper (DCN-2026-011 D-034).

Equivalent to:
  ros2 launch san_bringup squadron.launch.py \\
      robot_id:=1 robot_role:=leader sbc_id:=0 \\
      hub_features:=false include_regression:=auto

Pin reasoning:
  - robot_id=1, robot_role=leader : Leader slot in the v1.5 IDS topology
  - sbc_id=0                      : "N/A" sentinel — leader has a single SBC,
                                    not the dual-SBC redundancy of the Hub
  - hub_features=false            : explicit; hub-only group stays disabled
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
        LogInfo(msg=["[leader_go2] Launching Leader (Unitree Go2) profile"]),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(squadron),
            launch_arguments={
                "robot_id":            "1",
                "robot_role":          "leader",
                "sbc_id":              "0",
                "hub_features":        "false",
                "include_regression":  include_regression,
                "deployment_mode":     deployment_mode,
            }.items(),
        ),
    ])
