"""Hub UGV SBC #1 — fixed-role launch wrapper (DCN-2026-011 D-034).

Equivalent to:
  ros2 launch san_bringup squadron.launch.py \\
      robot_id:=2 robot_role:=hub sbc_id:=1 \\
      hub_features:=true include_regression:=auto

Pin reasoning:
  - robot_id=2 / robot_role=hub : Hub UGV in the v1.5 IDS topology
  - sbc_id=1                    : primary RK3588 slot (matches
                                  /etc/skyautonet/sbc_id on the board
                                  provisioned by infra/systemd/install.sh)
  - hub_features=true           : explicit (not 'auto') so the
                                  hub-only group is enabled
                                  unconditionally on this profile

Only deployment_mode + include_regression are operator-overridable;
the identity parameters above are pinned to prevent provisioning errors.
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
        LogInfo(msg=["[hub_sbc1] Launching Hub UGV SBC #1 profile"]),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(squadron),
            launch_arguments={
                "robot_id":            "2",
                "robot_role":          "hub",
                "sbc_id":              "1",
                "hub_features":        "true",
                "include_regression":  include_regression,
                "deployment_mode":     deployment_mode,
            }.items(),
        ),
    ])
