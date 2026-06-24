#!/usr/bin/env python3
# SkyHunter Go2 SITL 콘보이 데모 — 로봇개(Go2) 리더 1 + UGV 4대 단일종대 (한 명령 기동).
#
# 기본(sitl:=true)으로 vendored Go2 SITL(unitree_go2_ros2, ros/src/third_party)을 먼저
# 기동(gz world="default" + 리더 /odom_gt·/cmd_vel)하고, warmup 후 그 world 에 UGV 4대를
# spawn → 리더 코디네이터(convoy_coordinator) + UGV 체인(convoy_ugv ×4) + RViz 시각화.
# sitl:=false 면 외부에서 이미 띄운 SITL 에 붙는다(warmup:=3 권장).
#
# UGV 는 san_description 실모델(lidar_mode:=none + 카메라 strip; 런치 내 순수 파이썬,
# shell-out 없음)로 5로봇 RTF 확보. 데이터 통신 정책: UGV→리더 보고 @2Hz / 리더→UGV
# 선행타겟+장애물맵 @2Hz. RViz: 계획경로/리더궤적/로봇마커/체인/장애물.
#
# 검증: Ubuntu 24.04 / ROS 2 Jazzy / Gazebo Harmonic 8.14 — 리더 waypoint 8/8 완주,
#       UGV 4대 gap≈3.5 m 종대 유지, 장애물 회피(≥2.25 m), 전복 0.
import os
import re
import tempfile

import xacro
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    AppendEnvironmentVariable,
    DeclareLaunchArgument,
    EmitEvent,
    IncludeLaunchDescription,
    OpaqueFunction,
    TimerAction,
)
from launch.conditions import IfCondition
from launch.events import Shutdown
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

# 편대 매핑: 리더(Go2) -> robot_3(deputy) -> robot_4 -> robot_5 -> robot_2(hub)
CHAIN = [3, 4, 5, 2]
SPAWN_X = {3: -3.0, 4: -6.0, 5: -9.0, 2: -12.0}
# 장애물 (x, y, radius): (12,2.5)=4번째 waypoint(apex) 상 → 충돌 → 회피,
# (20,1)=6번째 waypoint 상 → 충돌 → 회피, (28,-3)=경로(x=28,y=-1)서 2m 비껴 안전
# → 회피X waypoint 추종(회피 회랑 밖, "충돌예상 시에만 회피" 시연).
OBSTACLES = [(12.0, 2.5, 0.5), (20.0, 1.0, 0.5), (28.0, -3.0, 0.5)]
# 계획 경로(긴 ~40m 완만 S-curve) — 코디네이터 waypoints + Gazebo 점 마커 공유.
# 급회전 없어 저RTF gait 안정. costmap lidar 로 장애물 인지, 충돌 예상 시에만 회피.
WAYPOINTS = [
    (0.0, 0.0),
    (4.0, 1.0),
    (8.0, 2.0),
    (12.0, 2.5),
    (16.0, 2.0),
    (20.0, 1.0),
    (24.0, 0.0),
    (28.0, -1.0),
    (32.0, -1.5),
    (36.0, -1.0),
    (40.0, 0.0),
]


def _strip_cameras(urdf_xml):
    # UGV urdf 에서 카메라 센서 블록 제거(렌더 부하 제거). diff-drive/odom/imu/joint 보존.
    out, _ = re.subn(
        r'<sensor\b[^>]*type="camera"[^>]*>.*?</sensor>', "", urdf_xml, flags=re.S
    )
    return out


def _spawn_nodes(world):
    # UGV 4대 + 장애물 실린더를 gz world 에 spawn 하는 ros_gz_sim create Node 들.
    share = get_package_share_directory("san_description")
    xacro_file = os.path.join(share, "urdf", "san_robot.urdf.xacro")
    # UGV URDF 임시파일 — 사용자별 디렉터리(고정 /tmp/robot_N.urdf 공유 금지). 고정 공유경로는
    # 다중 사용자/재실행(특히 한 번 root 로 실행 후)에서 소유권 충돌→Permission denied 유발.
    tmpdir = os.path.join(tempfile.gettempdir(), f"san_convoy_{os.getuid()}")
    os.makedirs(tmpdir, exist_ok=True)
    nodes = []
    for n in CHAIN:
        ns = f"robot_{n}"
        urdf = xacro.process(
            xacro_file,
            mappings={"robot_ns": ns, "robot_name": ns, "lidar_mode": "low"},
        )
        urdf = _strip_cameras(urdf)
        path = os.path.join(tmpdir, f"{ns}.urdf")
        with open(path, "w") as f:
            f.write(urdf)
        nodes.append(
            Node(
                package="ros_gz_sim",
                executable="create",
                output="screen",
                arguments=[
                    "-world",
                    world,
                    "-file",
                    path,
                    "-name",
                    ns,
                    "-x",
                    str(SPAWN_X[n]),
                    "-y",
                    "0",
                    "-z",
                    "0.35",
                ],
            )
        )
        nodes.append(
            Node(
                package="robot_state_publisher",
                executable="robot_state_publisher",
                name=f"rsp_{ns}",
                output="screen",
                parameters=[{
                    "robot_description": urdf,
                    "frame_prefix": f"{ns}/",
                    "use_sim_time": True,
                }], 
                remappings=[("/joint_states", f"/{ns}/joint_states")],
            )   
        ) 
    for oi, (ox, oy, r) in enumerate(OBSTACLES):
        obs_sdf = (
            f'<sdf version="1.8"><model name="obs_c{oi}"><static>true</static>'
            f'<link name="l"><collision name="c"><geometry><cylinder>'
            f"<radius>{r}</radius><length>2</length></cylinder></geometry></collision>"
            f'<visual name="v"><geometry><cylinder><radius>{r}</radius><length>2</length>'
            f"</cylinder></geometry><material><ambient>0.8 0.2 0.2 1</ambient>"
            f"<diffuse>0.8 0.2 0.2 1</diffuse></material></visual></link></model></sdf>"
        )
        nodes.append(
            Node(
                package="ros_gz_sim",
                executable="create",
                output="screen",
                arguments=[
                    "-world",
                    world,
                    "-string",
                    obs_sdf,
                    "-name",
                    f"obs_c{oi}",
                    "-x",
                    str(ox),
                    "-y",
                    str(oy),
                    "-z",
                    "1.0",
                ],
            )
        )
    # 계획 경로 waypoint 작은 점(녹색 구) 마커 — Gazebo map 에 경로 표시
    for k, (wx, wy) in enumerate(WAYPOINTS):
        wp_sdf = (
            f'<sdf version="1.8"><model name="wp_{k}"><static>true</static>'
            f'<link name="l"><visual name="v"><geometry><sphere><radius>0.05</radius>'
            f"</sphere></geometry><material><ambient>0.1 0.9 0.1 1</ambient>"
            f"<diffuse>0.1 0.9 0.1 1</diffuse></material></visual></link></model></sdf>"
        )
        nodes.append(
            Node(
                package="ros_gz_sim",
                executable="create",
                output="screen",
                arguments=[
                    "-world",
                    world,
                    "-string",
                    wp_sdf,
                    "-name",
                    f"wp_{k}",
                    "-x",
                    str(wx),
                    "-y",
                    str(wy),
                    "-z",
                    "0.05",
                ],
            )
        )
    return nodes


def _convoy_nodes(gap, ugv_vmax, leader_vmax, leader_wmax, lidar_only):
    bridge_args = []
    for n in CHAIN:
        bridge_args += [
            f"/robot_{n}/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist",
            f"/robot_{n}/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry",
            f"/robot_{n}/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V",
            f"/robot_{n}/scan/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked",
        ] 
    nodes = [
        Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="convoy_ugv_bridge",
            arguments=bridge_args,
            remappings=[(f"/robot_{n}/tf", "/tf") for n in CHAIN],
            output="screen",
        ),
        # 리더(Go2) ground-truth pose 를 /odom_gt 로 공급. 코디네이터/costmap/viz 는 /odom_gt
        # 로 리더 pose 를 받음(tf2 미사용).
        # ★gz 토픽 /odom 은 오염되어 있음: champ bringup 의 gazebo_bridge 가 /odom 을 양방향(@)
        #   브리지하므로 champ EKF(footprint_to_odom_ekf, odometry/filtered:=odom)의 ROS /odom
        #   (초기 0,0)이 gz /odom 으로 역주입 → gz /odom 퍼블리셔 2개(Go2 진실 + EKF 0,0)로
        #   플리커. 따라서 공유 /odom 대신 Go2 전용 gz 토픽
        #   /model/go2/odometry_with_covariance(퍼블리셔 1개, 경합 없음)을 ROS /odom_gt 로 브리지.
        Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="leader_odom_gt_bridge",
            arguments=[
                "/model/go2/odometry_with_covariance"
                "@nav_msgs/msg/Odometry[gz.msgs.OdometryWithCovariance"
            ],
            remappings=[("/model/go2/odometry_with_covariance", "/odom_gt")],
            output="screen",
        ),
        Node(
            package="san_operator_tools",
            executable="convoy_coordinator",
            name="convoy_coordinator",
            output="screen",
            parameters=[
                {
                    "leader_vmax": leader_vmax,
                    "leader_wmax": leader_wmax,
                    "waypoints": [float(c) for wp in WAYPOINTS for c in wp],
                    # lidar_only=True → 정적 장애물 prior 제거(빈 배열) → 회피가 전적으로
                    # Go2 lidar 검출(/convoy/detected_obstacles)에 의존. lidar 회피 검증용.
                    "obstacles": (
                        [] if lidar_only else [float(c) for ob in OBSTACLES for c in ob]
                    ),
                }
            ],
        ),
        # Go2 4D lidar → 로컬 costmap + 검출 장애물(/convoy/detected_obstacles) — 회피 인지 입력
        Node(
            package="san_operator_tools",
            executable="convoy_costmap",
            name="convoy_costmap",
            output="screen",
        ),
    ]
    NAV2_FOLLOWERS = []
    for n in CHAIN:
        nodes.append(Node(
            package="tf2_ros", executable="static_transform_publisher",
            name=f"map_to_odom_{n}",
            arguments=[str(SPAWN_X[n]), "0", "0", "0", "0", "0",
                        "map", f"robot_{n}/odom"],
            output="screen")) 
        if n in NAV2_FOLLOWERS:        # skip breadcrumb for nav2 robots
            continue
        nodes.append(
            Node(
                package="san_operator_tools",
                executable="convoy_ugv",
                name="convoy_ugv",
                namespace=f"robot_{n}",
                output="screen",
                parameters=[
                    {
                        "robot_id": n,
                        "spawn_x": SPAWN_X[n],
                        "spawn_y": 0.0,
                        "gap_m": gap,
                        "max_linear_mps": ugv_vmax,
                        "min_gap_m": 1.4,
                        "slow_gap_m": 2.2,
                    }
                ],
            )
        )
    return nodes


def _bringup(context, *args, **kwargs):
    # gz 기동 후 warmup 지연을 두고 UGV spawn + 브리지 + 콘보이 노드를 띄운다.
    world = LaunchConfiguration("world").perform(context)
    warmup = float(LaunchConfiguration("warmup_s").perform(context))
    lidar_only = LaunchConfiguration("lidar_only").perform(context).lower() == "true"
    gap = LaunchConfiguration("gap_m")
    ugv_vmax = LaunchConfiguration("ugv_vmax")
    leader_vmax = LaunchConfiguration("leader_vmax")
    leader_wmax = LaunchConfiguration("leader_wmax")
    actions = _spawn_nodes(world) + _convoy_nodes(
        gap, ugv_vmax, leader_vmax, leader_wmax, lidar_only
    )
    return [TimerAction(period=warmup, actions=actions)]


def _shutdown_timer(context, *args, **kwargs):
    # sim_timeout 초 후 전체 launch(gz 포함)를 정상 종료. 0 이하면 무한 실행.
    t = float(LaunchConfiguration("sim_timeout").perform(context))
    if t <= 0.0:
        return []
    return [
        TimerAction(
            period=t,
            actions=[EmitEvent(event=Shutdown(reason=f"sim_timeout {t:g}s reached"))],
        )
    ]


def generate_launch_description():
    # gz 가 UGV 의 package://san_description/meshes 비주얼을 해석하도록 리소스 경로 추가
    # (없으면 UGV 가 물리/odom 은 정상이나 Gazebo 에서 안 보임). share 의 부모 = .../share.
    san_desc_share_parent = os.path.dirname(
        get_package_share_directory("san_description")
    )
    gz_resource = AppendEnvironmentVariable(
        "GZ_SIM_RESOURCE_PATH", san_desc_share_parent
    )

    sitl_launch = os.path.join(
        get_package_share_directory("unitree_go2_sim"),
        "launch",
        "unitree_go2_launch.py",
    )
    sitl = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(sitl_launch),
        launch_arguments={"rviz": "false"}.items(),
        condition=IfCondition(LaunchConfiguration("sitl")),
    )

    viz = Node(
        package="san_operator_tools",
        executable="convoy_viz",
        name="convoy_viz",
        output="screen",
    )
    rviz_cfg = os.path.join(
        get_package_share_directory("san_sim_gazebo"), "rviz", "convoy.rviz"
    )
    rviz = Node(
        package="rviz2",
        executable="rviz2",
        name="rviz2",
        arguments=["-d", rviz_cfg],
        output="screen",
        condition=IfCondition(LaunchConfiguration("rviz")),
    )

    return LaunchDescription(
        [
            DeclareLaunchArgument(
                "sitl",
                default_value="true",
                description="launch the vendored Go2 SITL (leader)",
            ),
            DeclareLaunchArgument(
                "world",
                default_value="default",
                description="gz world name the Go2 SITL runs in",
            ),
            DeclareLaunchArgument(
                "warmup_s",
                default_value="45.0",
                description="delay before UGV spawn (SITL gz warmup)",
            ),
            DeclareLaunchArgument("gap_m", default_value="3.0"),
            DeclareLaunchArgument("ugv_vmax", default_value="0.6"),
            # 로봇개 순항속도 = UGV 최대속도(0.6). 뒤 UGV 와 간격이 match_gap(3.5 m) 초과로
            # 벌어지면 코디네이터 throttle(floor 0.5)이 자동 감속 → UGV 가 따라잡도록.
            DeclareLaunchArgument("leader_vmax", default_value="0.6"),
            DeclareLaunchArgument("leader_wmax", default_value="0.25"),
            DeclareLaunchArgument(
                "rviz",
                default_value="true",
                description="launch RViz convoy path visualization",
            ),
            DeclareLaunchArgument(
                "lidar_only",
                default_value="false",
                description="drop static obstacle prior → avoidance relies solely on "
                "Go2 lidar detections (/convoy/detected_obstacles)",
            ),
            DeclareLaunchArgument(
                "sim_timeout",
                default_value="120.0",
                description="auto-shutdown the whole sim after N seconds (<=0 = run forever)",
            ),
            gz_resource,
            sitl,
            viz,
            rviz,
            OpaqueFunction(function=_bringup),
            OpaqueFunction(function=_shutdown_timer),
        ]
    )
