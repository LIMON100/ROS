import os

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription, LogInfo, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_xml.launch_description_sources import XMLLaunchDescriptionSource


ROBOT_NAME_TO_ID = {f"S{i}": i for i in range(1, 9)}

PLATFORM_TO_RTSP_CONFIG = {
    "rk3588": "rtsp_server_rk3588.yaml",
    "rpi": "rtsp_server_rpi.yaml",
    "pc": "rtsp_server_pc.yaml",
}

MODE_PRESETS = {
    "production": {
        "launch_file": "combat_robot.launch.xml",
        "overlay_yaml": None,
        "launch_arguments": {
            "use_operator": "true",
            "use_detector": "true",
            "use_sensor": "false",
            "use_visualization": "false",
            "use_camera_motor": "false",
            "use_robot_server": "true",
            "use_teleop": "true",
            "use_trigger": "true",
            "use_gnss": "true",
        },
    },
    "demo": {
        "launch_file": "combat_robot.launch.xml",
        "overlay_yaml": "params.demo.yaml",
        "launch_arguments": {
            "use_operator": "true",
            "use_detector": "true",
            "use_sensor": "false",
            "use_visualization": "false",
            "use_camera_motor": "false",
            "use_robot_server": "true",
            "use_teleop": "true",
            "use_trigger": "false",
        },
    },
    "office_test": {
        "launch_file": "combat_robot_test.launch.xml",
        "overlay_yaml": "params.office_test.yaml",
        "launch_arguments": {
            "use_operator": "true",
            "use_pan_tilt_controller": "false",
            "use_detector": "false",
            "use_sensor": "false",
            "use_visualization": "false",
            "use_camera_motor": "false",
            "use_robot_server": "true",
            "use_teleop": "false",
            "use_trigger": "false",
            "use_dummy_data": "true",
            "load_dummy_swarm_data": "true",
        },
    },
}


def _load_profile(profile_path):
    if not os.path.exists(profile_path):
        raise FileNotFoundError(f"device profile not found: {profile_path}")

    with open(profile_path, "r", encoding="utf-8") as stream:
        data = yaml.safe_load(stream) or {}

    device = data.get("device")
    if not isinstance(device, dict):
        raise RuntimeError("device profile must contain a 'device' mapping")

    return device


def _resolve_string(value):
    return str(value).strip() if value is not None else ""


def _build_launch(context):
    profile_path = LaunchConfiguration("profile").perform(context)
    robot_name_override = _resolve_string(LaunchConfiguration("robot_name").perform(context))
    platform_override = _resolve_string(LaunchConfiguration("platform").perform(context))
    deployment_mode_override = _resolve_string(
        LaunchConfiguration("deployment_mode").perform(context)
    )

    device = _load_profile(profile_path)

    robot_name = (robot_name_override or _resolve_string(device.get("robot_name"))).upper()
    platform = (platform_override or _resolve_string(device.get("platform"))).lower()
    deployment_mode = (
        deployment_mode_override or _resolve_string(device.get("deployment_mode"))
    ).lower()

    if robot_name not in ROBOT_NAME_TO_ID:
        raise RuntimeError(f"unsupported robot_name '{robot_name}', expected S1~S8")
    if platform not in PLATFORM_TO_RTSP_CONFIG:
        raise RuntimeError(f"unsupported platform '{platform}', expected rk3588|rpi|pc")
    if deployment_mode not in MODE_PRESETS:
        raise RuntimeError(
            "unsupported deployment_mode "
            f"'{deployment_mode}', expected demo|office_test|production"
        )

    launch_pkg_share = get_package_share_directory("combat_robot_launch")
    robot_server_share = get_package_share_directory("robot_server")
    op_system_share = get_package_share_directory("combat_robot_operation_system")
    preset = MODE_PRESETS[deployment_mode]
    launch_file = os.path.join(launch_pkg_share, "launch", preset["launch_file"])
    rtsp_config_file = os.path.join(
        robot_server_share, "config", PLATFORM_TO_RTSP_CONFIG[platform]
    )

    launch_arguments = dict(preset["launch_arguments"])
    launch_arguments["robot_id"] = str(ROBOT_NAME_TO_ID[robot_name])
    launch_arguments["deployment_mode"] = deployment_mode
    launch_arguments["rtsp_config_file"] = rtsp_config_file

    overlay_yaml = preset.get("overlay_yaml")
    if overlay_yaml:
        overlay_path = os.path.join(op_system_share, "config", overlay_yaml)
        launch_arguments["params_overlay_file"] = overlay_path

    return [
        LogInfo(
            msg=(
                "[combat_robot_device] "
                f"robot={robot_name} id={ROBOT_NAME_TO_ID[robot_name]} "
                f"platform={platform} mode={deployment_mode}"
            )
        ),
        IncludeLaunchDescription(
            XMLLaunchDescriptionSource(launch_file),
            launch_arguments=launch_arguments.items(),
        ),
    ]


def generate_launch_description():
    default_profile = os.path.join(
        get_package_share_directory("combat_robot_launch"),
        "config",
        "device_profile.yaml",
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument("profile", default_value=default_profile),
            DeclareLaunchArgument("robot_name", default_value=""),
            DeclareLaunchArgument("platform", default_value=""),
            DeclareLaunchArgument("deployment_mode", default_value=""),
            OpaqueFunction(function=_build_launch),
        ]
    )
