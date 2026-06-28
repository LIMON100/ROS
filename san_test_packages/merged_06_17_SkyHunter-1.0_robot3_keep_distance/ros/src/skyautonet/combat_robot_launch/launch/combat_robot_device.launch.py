# SAN v1.3 PHASE 0 - combat robot device launch.
#
# Resolves a deployment_mode to its overlay yaml and launches the
# operation_system + swarm_coordinator stack.

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

# Mode -> overlay yaml. production runs the base params.yaml only.
_OVERLAYS = {
    "production":  None,
    "demo":         "params.demo.yaml",
    "lab_test":     "params.lab_test.yaml",      # v1.3 신규
    "bench":        "params.bench.yaml",
    "development":  "params.development.yaml",   # v1.3 신규
}


def select_params_overlay(deployment_mode: str):
    """Return the overlay yaml filename (or None) for `deployment_mode`.

    Raises RuntimeError on unknown mode so a typo at launch time is
    caught immediately instead of silently dropping to production.
    """
    if deployment_mode not in _OVERLAYS:
        raise RuntimeError(
            "Invalid deployment_mode: '{}'. Allowed: {}".format(
                deployment_mode, list(_OVERLAYS.keys())))
    return _OVERLAYS[deployment_mode]


def _launch_setup(context, *args, **kwargs):
    mode = LaunchConfiguration("deployment_mode").perform(context)
    robot_id = LaunchConfiguration("robot_id").perform(context)
    overlay = select_params_overlay(mode)

    op_share = get_package_share_directory("combat_robot_operation_system")
    params_base = os.path.join(op_share, "config", "params.yaml")
    params = [params_base]
    if overlay is not None:
        params.append(os.path.join(op_share, "config", overlay))

    params.append({
        "deployment_mode": mode,
        "robot_id": int(robot_id) if robot_id.isdigit() else 0,
    })

    return [
        Node(
            package="combat_robot_operation_system",
            executable="combat_robot_operation_system",
            name="combat_robot_operation_system",
            output="screen",
            parameters=params,
        ),
        Node(
            package="swarm_coordinator",
            executable="swarm_coordinator",
            name="swarm_coordinator",
            output="screen",
        ),
    ]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            "deployment_mode",
            default_value="production",
            description="production | demo | lab_test | bench | development",
        ),
        DeclareLaunchArgument(
            "robot_id",
            default_value="0",
            description="This SBC's swarm id (1..8)",
        ),
        OpaqueFunction(function=_launch_setup),
    ])
