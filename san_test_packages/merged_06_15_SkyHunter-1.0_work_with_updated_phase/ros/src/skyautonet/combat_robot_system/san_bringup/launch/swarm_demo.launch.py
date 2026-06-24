# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""SAN v1.5 — One-command swarm demo orchestrator.

Brings up the WHOLE leader-follower sim from a SINGLE launch, the way the
`skyhunter_ws_code` prototype's `full_swarm.launch.py` does — instead of
running 7 terminals by hand:

    1x  swarm_sim.launch.py        (Gazebo world + N bodies)
    1x  squadron.launch.xml        (robot_1 = leader, hub_features=true)
    Nx  squadron.launch.xml        (robot_2..N = followers)
    1x  waypoint_to_nav2 + waypoint_sender   (the mission glue)

The decisive difference from launching those by hand: each heavy stack is
STAGGERED with a TimerAction. Firing four ~30-node squadrons simultaneously
swamps DDS discovery + CPU and followers silently lose their leader/odom
connection ("only 2 robots move"). Staggering is exactly what the prototype
does (`TimerAction period = i*1.5 + 5.0`) and what fixes it here.

Usage (defaults reproduce the 4-robot dev sim):
    ros2 launch san_bringup swarm_demo.launch.py
    ros2 launch san_bringup swarm_demo.launch.py num_robots:=5 world:=empty_world.sdf

If the box still saturates with full follower stacks, run the followers
lightweight (only the pursuit node — proven to move reliably):
    ros2 launch san_bringup swarm_demo.launch.py follower_mode:=light

Tuning:
    num_robots        total bodies incl. leader (default 4 → leader + 3 followers)
    world             Gazebo world file in san_sim_gazebo/worlds
    pose              leader spawn "x y z roll pitch yaw"
    follower_mode     full (per-follower squadron) | light (pursuit node only)
    run_mission       true | false — start the waypoint mission glue
    sim_settle_s      delay before the leader launches (Gazebo warm-up)
    stagger_s         gap between each follower stack (the anti-storm knob)
"""
import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    LogInfo,
    OpaqueFunction,
    TimerAction,
)
from launch.launch_description_sources import (
    AnyLaunchDescriptionSource,
    PythonLaunchDescriptionSource,
)
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _ensure_mesh_secret(path: str) -> None:
    """Create the 32-byte mesh secret if missing.

    `fire_authorization` aborts without it. Written directly with os.urandom
    — NO shell-out (DCN-2026-002 / ADR-006 forbids system()/popen/subprocess).
    """
    if os.path.exists(path):
        return
    parent = os.path.dirname(path)
    if parent and not os.path.isdir(parent):
        os.makedirs(parent, exist_ok=True)
    with open(path, "wb") as fh:
        fh.write(os.urandom(32))
    os.chmod(path, 0o600)


def launch_setup(context, *args, **kwargs):
    sim_share = get_package_share_directory("san_sim_gazebo")
    bringup_share = get_package_share_directory("san_bringup")

    def cfg(name):
        return LaunchConfiguration(name).perform(context)

    num_robots = int(cfg("num_robots"))
    world = cfg("world")
    pose = cfg("pose")
    deployment_mode = cfg("deployment_mode")
    mesh_secret = cfg("mesh_secret_path")
    fire_audit = cfg("fire_audit_log_path")
    include_rosbridge = cfg("include_rosbridge")
    follower_mode = cfg("follower_mode").lower()
    run_mission = cfg("run_mission").lower() == "true"
    sim_settle = float(cfg("sim_settle_s"))
    stagger = float(cfg("stagger_s"))
    mission_extra = float(cfg("mission_extra_s"))

    _ensure_mesh_secret(mesh_secret)

    swarm_sim = os.path.join(sim_share, "launch", "swarm_sim.launch.py")
    squadron = os.path.join(bringup_share, "launch", "squadron.launch.xml")

    def squadron_include(robot_id, role, hub):
        return IncludeLaunchDescription(
            AnyLaunchDescriptionSource(squadron),
            launch_arguments={
                "robot_id": str(robot_id),
                "robot_role": role,
                "hub_features": hub,
                "deployment_mode": deployment_mode,
                "use_sim_time": "true",
                "mesh_secret_path": mesh_secret,
                "fire_audit_log_path": fire_audit,
                "include_rosbridge": include_rosbridge,
            }.items(),
        )

    def light_follower(robot_id):
        # Minimal follower: pursuit (movement + encircle + gimbal aim) PLUS the
        # gimbal driver so the PanTiltCommand actually slews the Gazebo gimbal.
        # Both per-robot; no perception/Nav2/SLAM (that stays on the leader).
        return [
            Node(
                package="san_follower_tier",
                executable="follower_pursuit_node",
                name="follower_pursuit_node",
                namespace=f"robot_{robot_id}",
                output="screen",
                parameters=[{
                    "robot_id": robot_id,
                    "use_sim_time": True,
                    "obstacle_brake_dist_m": 4.0,
                    "obstacle_stop_dist_m": 1.2,
                    "obstacle_cone_rad": 0.7,
                }],
            ),
            Node(
                package="san_surveillance",
                executable="pan_tilt_driver",
                name="pan_tilt_driver_node",
                namespace=f"robot_{robot_id}",
                output="screen",
                parameters=[{
                    "robot_id": robot_id,
                    "use_sim_time": True,
                }],
            ),
        ]

    def medium_follower(robot_id):
        # light + per-follower PERCEPTION + threat + vote gate, so the follower
        # detects on its own camera and contributes to the voting system —
        # WITHOUT the heavy Nav2/SLAM/costmap/comm/mission stack of `full`.
        # Sim detector config mirrors squadron.launch.xml (use_sim_time path):
        #   backend=onnx, image=rgb_camera/image_raw, model=yolov8m.onnx.
        ns = f"robot_{robot_id}"
        hd_model = os.path.join(
            get_package_share_directory("human_detector"),
            "models", "yolov8m.onnx")
        squadron_yaml = os.path.join(
            get_package_share_directory("san_bringup"),
            "config", "squadron.yaml")
        surveillance_yaml = os.path.join(
            get_package_share_directory("san_surveillance"),
            "config", "surveillance.yaml")
        return light_follower(robot_id) + [
            Node(
                package="human_detector",
                executable="human_detector_node",
                name="human_detector_node",
                namespace=ns,
                output="screen",
                parameters=[squadron_yaml, {
                    "image_mode": "raw",
                    "decoded_topic": "rgb_camera/image_raw",
                    "detections_topic": "~/detections",
                    "max_inference_hz": 15,
                    "drop_when_busy": True,
                    "inference_backend": "onnx",
                    "model_path": hd_model,
                    "robot_id": robot_id,
                    "use_sim_time": True,
                    "deployment_mode": deployment_mode,
                    "thermal_topic": "thermal_camera_node/image",
                }],
            ),
            Node(
                package="san_hub_orchestrator",
                executable="detection_to_threat_node",
                name="detection_to_threat_node",
                namespace=ns,
                output="screen",
                parameters=[{
                    "fused_topic": "human_detector_node/detections",
                    "rgb_topic": "human_detector_node/detections",
                    "output_topic": "/swarm/threat_alert_raw",
                    "source_robot_id": str(robot_id),
                    "base_frame": "base_footprint",
                    "focal_px": 550.0,
                    "img_cx": 320.0,
                    "img_cy": 240.0,
                    "use_sim_time": True,
                }],
            ),
            Node(
                package="san_surveillance",
                executable="surveillance_node",
                name="surveillance_node",
                namespace=ns,
                output="screen",
                parameters=[surveillance_yaml, squadron_yaml, {
                    "robot_id": robot_id,
                    "use_sim_time": True,
                    "deployment_mode": deployment_mode,
                }],
            ),
        ]

    actions = []

    # t=0 — Gazebo world + N bodies (leader = root ns, followers = robot_N).
    actions.append(LogInfo(
        msg=f"[san_demo] world={world} bodies={num_robots} "
            f"follower_mode={follower_mode} stagger={stagger:.0f}s"))
    actions.append(IncludeLaunchDescription(
        PythonLaunchDescriptionSource(swarm_sim),
        launch_arguments={
            "num_robots": str(num_robots),
            "world": world,
            "pose": pose,
            "use_sim_time": "true",
        }.items(),
    ))

    # CRITICAL: bridge Gazebo's clock into ROS. swarm_sim.launch.py omits this
    # (its sibling launches sim_nav / swarm_nav / leader_follower_demo all have
    # it). Without /clock, every use_sim_time:=true node freezes at t=0 — Nav2
    # never plans, the waypoint sender never ticks, followers never move.
    actions.append(Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        name="clock_bridge",
        output="screen",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"],
    ))

    # t=sim_settle — leader (robot_1), full squadron + hub features.
    actions.append(TimerAction(
        period=sim_settle,
        actions=[
            LogInfo(msg=f"[san_demo] t={sim_settle:.0f}s  LEADER robot_1"),
            squadron_include(1, "leader", "true"),
        ]))

    # Followers, STAGGERED — the anti-discovery-storm knob.
    last_t = sim_settle
    for idx, rid in enumerate(range(2, num_robots + 1), start=1):
        t = sim_settle + stagger * idx
        last_t = t
        if follower_mode == "light":
            bodies = light_follower(rid)
        elif follower_mode == "medium":
            bodies = medium_follower(rid)
        else:
            bodies = [squadron_include(rid, "follower", "auto")]
        actions.append(TimerAction(
            period=t,
            actions=[
                LogInfo(msg=f"[san_demo] t={t:.0f}s  FOLLOWER robot_{rid} "
                            f"({follower_mode})"),
                *bodies,
            ]))

    # Mission glue — waypoint->Nav2 bridge + waypoint sender — after everyone up.
    if run_mission:
        t_mission = last_t + mission_extra
        actions.append(TimerAction(
            period=t_mission,
            actions=[
                LogInfo(msg=f"[san_demo] t={t_mission:.0f}s  waypoint mission"),
                Node(
                    package="san_operator_tools",
                    executable="waypoint_to_nav2",
                    name="waypoint_to_nav2",
                    output="screen",
                    parameters=[{
                        "robot_id": 1,
                        "announce_leader": True,
                        "goal_frame": "map",
                        "use_sim_time": True,
                    }],
                ),
                Node(
                    package="san_operator_tools",
                    executable="waypoint_sender",
                    name="waypoint_sender",
                    output="screen",
                    parameters=[{"use_sim_time": True}],
                ),
            ]))

    return actions


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("num_robots", default_value="4"),
        DeclareLaunchArgument("world", default_value="empty_world.sdf"),
        DeclareLaunchArgument("pose", default_value="0.0 0.0 0.5 0 0 0"),
        DeclareLaunchArgument("deployment_mode", default_value="development"),
        DeclareLaunchArgument("mesh_secret_path",
                              default_value="/tmp/mesh_secret.bin"),
        DeclareLaunchArgument("fire_audit_log_path",
                              default_value="/tmp/fire_audit.log"),
        DeclareLaunchArgument("include_rosbridge", default_value="false"),
        DeclareLaunchArgument(
            "follower_mode", default_value="full",
            description="full = per-follower squadron.launch.xml (heavy); "
                        "medium = pursuit+gimbal+perception+threat+vote (no "
                        "Nav2/SLAM/comm); light = pursuit+gimbal only (lean)"),
        DeclareLaunchArgument("run_mission", default_value="true"),
        DeclareLaunchArgument("sim_settle_s", default_value="8.0",
                              description="delay before leader (Gazebo warm-up)"),
        DeclareLaunchArgument("stagger_s", default_value="12.0",
                              description="gap between follower stacks (anti-storm)"),
        DeclareLaunchArgument("mission_extra_s", default_value="12.0",
                              description="extra delay after last follower before mission"),
        OpaqueFunction(function=launch_setup),
    ])