#!/usr/bin/env python3
# Convoy 추종자 per-UGV Nav2 bringup (DCN-2026-029 P2) — convoy_demo 위 오버레이.
#
# convoy_demo.launch.py 가 이미 띄운 UGV(robot_3/4/5/2) + gz 위에 각 UGV 의 Nav2 스택을
# 올린다(로봇/gz spawn 안 함 — 비파괴 오버레이). 로봇별 제어 사슬:
#
#     ros2 action send_goal /robot_<id>/navigate_to_pose ...  → /robot_<id> Nav2
#         → /robot_<id>/cmd_vel → ros_gz_bridge → DiffDrive → 로봇 이동.
#
# P2 범위 = per-UGV Nav2 가 정적 goal 을 항법(swarm_nav.launch.py 패턴 재사용). swarm_nav 와 동일:
#   - 로봇별 tf prefix(robot_<id>/base_footprint, robot_<id>/odom) + RewrittenYaml(root_key=ns).
#   - map→odom: static_transform_publisher(sim 지상진실 identity).
#   - odom→base_footprint: convoy_odom_tf(지상진실 /robot_<id>/odom 중계) — convoy 가 tf-free
#     이므로 필요(DCN-2026-029 §9 O-2). 실HW 는 san_localization dual-EKF.
# P3/P4 범위(follow_ref_path:=true) = 로봇별 convoy_nav2_follower 추가 — 리더 참조경로
#   (/convoy/ref_path/r{n})를 nav2 로 추종. convoy_ugv(직접 cmd_vel)와 택일. follower_mode:
#   follow_path(P3, controller 단독) | navigate_poses(P4, bt_navigator recovery+참조 재획득).
#
# ⚠ 검증 보류(Linux/CI): WSLg 에서 gz 중도종료. Path 발행율·tf 정합·정적 goal 항법·참조
#   추종·돌발 recovery 는 Ubuntu 24.04 / ROS 2 Jazzy / Gazebo Harmonic 에서 실증 필요.
#
# 사용:
#   # 터미널 A (P4 로컬 costmap 은 UGV lidar 필요 → ugv_lidar:=true)
#   ros2 launch san_sim_gazebo convoy_demo.launch.py ugv_lidar:=true
#   # 터미널 B — P2(정적 goal 항법):
#   ros2 launch san_sim_gazebo convoy_nav2.launch.py
#   # P3(참조경로 추종; convoy_ugv 비활성 후):
#   ros2 launch san_sim_gazebo convoy_nav2.launch.py follow_ref_path:=true
#   # P4(돌발 recovery + 참조 재획득):
#   ros2 launch san_sim_gazebo convoy_nav2.launch.py follow_ref_path:=true follower_mode:=navigate_poses
import os
import tempfile

import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    GroupAction,
    OpaqueFunction,
    TimerAction,
)
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node, PushRosNamespace
from nav2_common.launch import RewrittenYaml


def make_convoy_nav2_params(ns: str, base_params: str) -> str:
    """로봇별 Nav2 params: tf frame 네임스페이스 + pointcloud 토픽 네임스페이스.
    swarm_nav.make_robot_nav2_params 와 동일 패턴(단일 소스 convoy_nav2_params.yaml)."""
    with open(base_params) as f:
        cfg = yaml.safe_load(f)

    base = f"{ns}/base_footprint"
    odom = f"{ns}/odom"

    def setp(node, key, val):
        cfg.get(node, {}).get("ros__parameters", {})[key] = val

    setp("bt_navigator", "robot_base_frame", base)
    gc = cfg["global_costmap"]["global_costmap"]["ros__parameters"]
    gc["robot_base_frame"] = base  # global_frame 은 "map" 유지
    lc = cfg["local_costmap"]["local_costmap"]["ros__parameters"]
    lc["global_frame"] = odom
    lc["robot_base_frame"] = base
    setp("behavior_server", "global_frame", odom)
    setp("behavior_server", "robot_base_frame", base)

    for _node, p in cfg.items():
        rp = p.get("ros__parameters", {}) if isinstance(p, dict) else {}
        if "odom_topic" in rp:
            rp["odom_topic"] = "odom"  # 상대 → /<ns>/odom
    for layer in (gc, lc):
        pc = layer.get("obstacle_layer", {}).get("pointcloud")
        if isinstance(pc, dict):
            pc["topic"] = f"/{ns}/scan/points"

    out = os.path.join(tempfile.gettempdir(), f"{ns}_convoy_nav2_params.yaml")
    with open(out, "w") as f:
        yaml.safe_dump(cfg, f, default_flow_style=False)
    return out


def _nav(pkg, exe, name, params):
    return Node(
        package=pkg, executable=exe, name=name, output="screen", parameters=[params]
    )


def launch_setup(context, *args, **kwargs):
    robots_arg = LaunchConfiguration("robots").perform(context)
    use_sim = LaunchConfiguration("use_sim_time").perform(context) == "true"
    autostart = LaunchConfiguration("autostart").perform(context) == "true"
    follow = LaunchConfiguration("follow_ref_path").perform(context) == "true"
    follower_mode = LaunchConfiguration("follower_mode").perform(context)
    params_file = LaunchConfiguration("params_file").perform(context)
    if not params_file:
        params_file = os.path.join(
            get_package_share_directory("san_nav2"), "config", "convoy_nav2_params.yaml"
        )

    robots = [int(r) for r in robots_arg.split(",") if r.strip()]
    actions = []
    for i, rid in enumerate(robots):
        ns = f"robot_{rid}"
        params = RewrittenYaml(
            source_file=make_convoy_nav2_params(ns, params_file),
            root_key=ns,
            param_rewrites={"use_sim_time": "true" if use_sim else "false"},
            convert_types=True,
        )

        group = [
            PushRosNamespace(ns),
            # map → <ns>/odom: sim 지상진실 identity.
            Node(
                package="tf2_ros",
                executable="static_transform_publisher",
                name="sim_map_to_odom",
                output="screen",
                parameters=[{"use_sim_time": use_sim}],
                arguments=["0", "0", "0", "0", "0", "0", "map", f"{ns}/odom"],
            ),
            # <ns>/odom → <ns>/base_footprint: 지상진실 /robot_<id>/odom 중계(O-2 shim).
            Node(
                package="san_operator_tools",
                executable="convoy_odom_tf",
                name="convoy_odom_tf",
                output="screen",
                parameters=[
                    {
                        "use_sim_time": use_sim,
                        "odom_topic": "odom",
                        "parent_frame": f"{ns}/odom",
                        "child_frame": f"{ns}/base_footprint",
                    }
                ],
            ),
            _nav("nav2_controller", "controller_server", "controller_server", params),
            _nav("nav2_planner", "planner_server", "planner_server", params),
            _nav("nav2_behaviors", "behavior_server", "behavior_server", params),
            _nav("nav2_bt_navigator", "bt_navigator", "bt_navigator", params),
            _nav("nav2_smoother", "smoother_server", "smoother_server", params),
            _nav(
                "nav2_waypoint_follower",
                "waypoint_follower",
                "waypoint_follower",
                params,
            ),
            Node(
                package="nav2_lifecycle_manager",
                executable="lifecycle_manager",
                name="lifecycle_manager_navigation",
                output="screen",
                parameters=[
                    {
                        "use_sim_time": use_sim,
                        "autostart": autostart,
                        "node_names": [
                            "controller_server",
                            "planner_server",
                            "behavior_server",
                            "bt_navigator",
                            "smoother_server",
                            "waypoint_follower",
                        ],
                    }
                ],
            ),
        ]
        # P3(follow_ref_path:=true): /convoy/ref_path/r{id} → nav2 FollowPath 추종자.
        # ⚠ convoy_ugv(convoy_demo)와 동시에 켜면 /robot_<id>/cmd_vel 경합 → 택일해야 한다
        #   (convoy_demo 의 convoy_ugv 비활성 후 사용; convoy_demo 플래그는 후속).
        if follow:
            group.append(
                Node(
                    package="san_operator_tools",
                    executable="convoy_nav2_follower",
                    name="convoy_nav2_follower",
                    output="screen",
                    parameters=[
                        {
                            "use_sim_time": use_sim,
                            "robot_id": rid,
                            "path_frame": "map",
                            "mode": follower_mode,
                        }
                    ],
                )
            )
        # 로봇별 Nav2 bringup 시차(N 라이프사이클 동시 구성/활성 시 자원경합으로 후속 로봇
        # 정체 — swarm_nav 와 동일 staggering).
        actions.append(TimerAction(period=2.0 + i * 4.0, actions=[GroupAction(group)]))

    return actions


def generate_launch_description():
    return LaunchDescription(
        [
            # convoy_demo 사슬과 동일 순서 기본값.
            DeclareLaunchArgument("robots", default_value="3,4,5,2"),
            DeclareLaunchArgument("use_sim_time", default_value="true"),
            DeclareLaunchArgument("autostart", default_value="true"),
            DeclareLaunchArgument(
                "follow_ref_path",
                default_value="false",
                description="P3/P4: /convoy/ref_path/r{id} → nav2 추종자 기동 "
                "(convoy_ugv 비활성 필요)",
            ),
            # DeclareLaunchArgument(
            #     "follower_mode",
            #     default_value="follow_path",
            #     description="추종자 모드: follow_path(P3, 경량) | navigate_poses"
            #     "(P4, recovery+재획득)",
            # ),
            DeclareLaunchArgument(
                "follower_mode",
                default_value="navigate_to_pose",
                description="추종자 모드: navigate_to_pose(기본, planner 연결+갭유지+recovery) | "
                "follow_path(P3, 경량) | navigate_poses(P4)",
            ),
            DeclareLaunchArgument(
                "params_file",
                default_value="",
                description="convoy_nav2_params.yaml 오버라이드(공백=san_nav2 기본)",
            ),
            OpaqueFunction(function=launch_setup),
        ]
    )
