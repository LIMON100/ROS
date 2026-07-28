import os
import sys

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (IncludeLaunchDescription, DeclareLaunchArgument,
                            TimerAction, OpaqueFunction, GroupAction)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node, PushRosNamespace
from launch_ros.parameter_descriptions import ParameterValue

sys.path.insert(0, os.path.dirname(__file__))
from swarm_launch_utils import rewrite_yaml_for_namespace  # noqa: E402


# nav2 규약: 모든 TF 발행 노드는 절대 /tf 를 네임스페이스 상대 tf 로 리맵해야
# PushRosNamespace 안에서 /sN/tf 로 나간다.
TF_REMAP = [('/tf', 'tf'), ('/tf_static', 'tf_static')]


# ============================================================================
# 군집(swarm) 역할 파라미터 — ★ s1/s2/s3 리더/팔로워 결정 지점 ★
# ----------------------------------------------------------------------------
# swarm_path_executor 의 리더 여부는  is_leader_ = (robot_id == leader_robot_id).
#   · 리더(s1):    robot_id:=1  leader_robot_id:=1  role:=leader
#                  formation_followers:=2,3   (이 팔로워 form-up 완료까지 출발 대기)
#   · 팔로워(s2):  robot_id:=2  leader_robot_id:=1  role:=follower
#   · 팔로워(s3):  robot_id:=3  leader_robot_id:=1  role:=follower
# 즉 각 차량을 자기 robot_id 로 띄우고 leader_robot_id 는 전원 동일(리더 id)로 준다.
# (role 인자는 명령서버/문서용. executor 의 실제 리더판정은 robot_id==leader_robot_id.)
#
# ★ 네임스페이스(robot_ns) — 다중 차량 동일 ROS_DOMAIN 공존
# ----------------------------------------------------------------------------
# robot_ns:='' (기본)  → 단일로봇. 현행 동작 그대로(프레임 map/odom/base_footprint,
#                        절대토픽 /odom /fix …, 글로벌 /tf). 실차 배포 무변경.
# robot_ns:='s2' 등   → 전 노드를 PushRosNamespace(/s2) 로 감싸고 ekf/nav2 의
#                        프레임(s2/map …)·절대토픽(/s2/…)을 sim 과 동일한
#                        rewrite_yaml_for_namespace 로 prefix. 같은 도메인의 다른
#                        차량과 토픽/TF 가 충돌하지 않는다.
# 주의: 센서(sensor.launch.py: gnss/imu/lidar)와 can_reader 가 발행하는 /fix,
#   /gps/heading_imu, /imu/data, /rslidar_points 도 같은 /sN 아래여야 ekf/nav2 가
#   소비한다. can_reader 는 이 launch 가 /sN 으로 리맵하지만, sensor.launch 는
#   별도 기동이므로 robot_ns 를 동일하게 줘서 띄워야 한다(후속 sensor 네임스페이스화).
# ============================================================================


def _executor_node(context, ns):
    """robot_id/대형 파라미터를 적용한 swarm_path_executor 노드. ns 가 있으면
    map_frame 을 sN/map 으로 자동 설정(인자 map_frame 기본 'map' 을 override)."""
    use_sim_time = LaunchConfiguration('use_sim_time').perform(context).lower() == 'true'
    robot_id = int(LaunchConfiguration('robot_id').perform(context))
    leader_id = int(LaunchConfiguration('leader_robot_id').perform(context))
    formation_enable = LaunchConfiguration('formation_enable').perform(context).lower() == 'true'
    formation_mode = LaunchConfiguration('formation_mode').perform(context)
    spacing = float(LaunchConfiguration('formation_lateral_spacing_m').perform(context))
    cruise = float(LaunchConfiguration('formation_cruise_speed_mps').perform(context))
    control_mode = LaunchConfiguration('control_mode').perform(context)
    path_cmd_topic = LaunchConfiguration('path_command_topic').perform(context)
    ctrl_cmd_topic = LaunchConfiguration('control_command_topic').perform(context)
    followers_str = LaunchConfiguration('formation_followers').perform(context).strip()

    # ns 가 있으면 프레임은 sN/map, 없으면 인자값(기본 map).
    map_frame = f'{ns}/map' if ns else LaunchConfiguration('map_frame').perform(context)

    params = {
        'use_sim_time': use_sim_time,
        'robot_id': robot_id,
        'leader_robot_id': leader_id,
        'map_frame': map_frame,
        'formation.enable': formation_enable,
        'formation.mode': formation_mode,
        'formation.lateral_spacing_m': spacing,
        'formation.cruise_speed_mps': cruise,
        'control_mode': control_mode,
        'path_command_topic': path_cmd_topic,
        'control_command_topic': ctrl_cmd_topic,
    }
    # 리더만 form-up 게이트: "2,3" 또는 "2 3" -> [2, 3]. 비우면 게이트 없음(즉시 출발).
    if followers_str:
        params['formation.followers'] = [
            int(x) for x in followers_str.replace(' ', ',').split(',') if x.strip()
        ]

    return Node(
        package='combat_robot_nav2', executable='swarm_path_executor',
        name='swarm_path_executor', output='screen',
        parameters=[params])


def _legacy_stack(context, use_sim_time, with_gnss):
    """robot_ns='' — 단일로봇. 기존 동작(비-네임스페이스) 그대로."""
    pkg_combat_nav = get_package_share_directory('combat_robot_nav2')
    pkg_vehicle_desc = get_package_share_directory('combat_robot_description')
    xacro_file = os.path.join(pkg_vehicle_desc, 'urdf', 'robot.urdf.xacro')

    rsp_node = Node(
        package='robot_state_publisher', executable='robot_state_publisher',
        parameters=[{'robot_description': Command(['xacro ', xacro_file]),
                     'use_sim_time': use_sim_time}])

    localization_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_combat_nav, 'launch', 'localization.launch.py')),
        launch_arguments={'use_sim_time': str(use_sim_time),
                          'with_gnss': str(with_gnss)}.items())

    can_reader_node = Node(
        package='combat_robot_nav2', executable='can_reader.py', name='can_reader',
        output='screen', parameters=[{'use_sim_time': use_sim_time}])

    map_yaml_file = os.path.join(pkg_combat_nav, 'map', 'incheon', 'incheon.yaml')
    map_server_node = Node(
        package='nav2_map_server', executable='map_server', name='map_server',
        output='screen',
        parameters=[{'yaml_filename': map_yaml_file}, {'use_sim_time': use_sim_time}])
    lifecycle_manager_map = Node(
        package='nav2_lifecycle_manager', executable='lifecycle_manager',
        name='lifecycle_manager_map', output='screen',
        parameters=[{'use_sim_time': use_sim_time}, {'autostart': True},
                    {'node_names': ['map_server']}])
    map_timer = TimerAction(period=2.0, actions=[map_server_node, lifecycle_manager_map])

    navigation_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_combat_nav, 'launch', 'navigation.launch.py')),
        launch_arguments={'use_sim_time': str(use_sim_time)}.items())
    nav_timer = TimerAction(period=5.0, actions=[navigation_launch])

    mission_timer = TimerAction(
        period=8.0, actions=[_executor_node(context, ns='')])

    return [rsp_node, localization_launch, can_reader_node,
            map_timer, nav_timer, mission_timer]


def _namespaced_stack(context, ns, use_sim_time, with_gnss):
    """robot_ns='sN' — 다중로봇. 전 스택을 PushRosNamespace(/sN) 로 감싸고
    ekf/nav2 프레임·절대토픽을 sN 으로 prefix(sim robot_bringup_sim 과 동일 패턴)."""
    pkg_nav = get_package_share_directory('combat_robot_nav2')
    pkg_desc = get_package_share_directory('combat_robot_description')
    pkg_nav2_bringup = get_package_share_directory('nav2_bringup')
    xacro_file = os.path.join(pkg_desc, 'urdf', 'robot.urdf.xacro')
    bt_dir = os.path.join(pkg_nav, 'include')

    # --- per-robot 재작성 config -------------------------------------------------
    ekf_src = os.path.join(pkg_nav, 'config', 'ekf.yaml')
    nav2_src = os.path.join(pkg_nav, 'config', 'nav2_params.yaml')
    # ekf 는 여기서 직접 띄우므로 파라미터를 ns 아래로 nest.
    ekf_params = rewrite_yaml_for_namespace(
        ekf_src, ns, extra_overrides={'use_sim_time': use_sim_time}, nest_under_ns=True)
    # nav2 는 navigation_launch 가 root_key=ns 로 nest 하므로 pre-nest 하지 않음.
    nav2_params = rewrite_yaml_for_namespace(
        nav2_src, ns,
        extra_overrides={
            'use_sim_time': use_sim_time,
            'default_nav_to_pose_bt_xml': os.path.join(bt_dir, 'single_plan_bt.xml'),
            'default_nav_through_poses_bt_xml': os.path.join(bt_dir, 'way_plan_bt.xml'),
        })

    # frame_prefix(sN/) 로 URDF 프레임을 prefix. robot_ns 는 실차 xacro 에서 미사용
    # (gz 전용)이라 무해하지만 sim 과 동일하게 전달.
    robot_description = ParameterValue(
        Command(['xacro ', xacro_file, ' robot_ns:=', ns]), value_type=str)

    # --- description -------------------------------------------------------------
    rsp = Node(
        package='robot_state_publisher', executable='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description,
                     'use_sim_time': use_sim_time, 'frame_prefix': ns + '/'}],
        remappings=TF_REMAP)

    # --- can_reader: 절대 /odom, /cmd_vel 을 /sN 으로 리맵 ------------------------
    can_reader = Node(
        package='combat_robot_nav2', executable='can_reader.py', name='can_reader',
        output='screen', parameters=[{'use_sim_time': use_sim_time}],
        remappings=[('/odom', f'/{ns}/odom'), ('/cmd_vel', f'/{ns}/cmd_vel')])

    # --- localization (GNSS) — localization.launch.py 를 인라인 + 프레임/토픽 prefix
    loc_nodes = _localization_nodes(ns, ekf_params, use_sim_time, with_gnss)

    # --- map server (incheon, sN/map 프레임) -------------------------------------
    map_file = os.path.join(pkg_nav, 'map', 'incheon', 'incheon.yaml')
    map_server = Node(
        package='nav2_map_server', executable='map_server', name='map_server',
        output='screen',
        parameters=[{'yaml_filename': map_file, 'use_sim_time': use_sim_time,
                     'frame_id': f'{ns}/map'}])
    map_lifecycle = Node(
        package='nav2_lifecycle_manager', executable='lifecycle_manager',
        name='lifecycle_manager_map', output='screen',
        parameters=[{'use_sim_time': use_sim_time, 'autostart': True,
                     'node_names': ['map_server']}])
    map_timer = TimerAction(period=2.0, actions=[map_server, map_lifecycle])

    # --- nav2 (full navigation_launch, namespace=ns) -----------------------------
    # navigation_launch 는 root_key=ns 로 params 를 nest 하지만 네임스페이스 자체는
    # push 하지 않으므로 바깥 PushRosNamespace(ns) 그룹 안에서 돈다.
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_nav2_bringup, 'launch', 'navigation_launch.py')),
        launch_arguments={'namespace': ns, 'use_sim_time': str(use_sim_time),
                          'autostart': 'true', 'params_file': nav2_params}.items())
    nav_timer = TimerAction(period=5.0, actions=[nav2])

    # --- swarm executor ----------------------------------------------------------
    mission_timer = TimerAction(period=8.0, actions=[_executor_node(context, ns)])

    group = [PushRosNamespace(ns), rsp, can_reader] + loc_nodes + \
            [map_timer, nav_timer, mission_timer]
    return [GroupAction(group)]


def _localization_nodes(ns, ekf_params, use_sim_time, with_gnss):
    """localization.launch.py 의 노드들을 ns prefix + TF_REMAP 으로 인라인.
    프레임은 sN/ (ekf_params 가 rewrite 됨), navsat/frame_fixer 토픽은 /sN/."""
    nodes = []

    if with_gnss:
        # gnss_base_link → gps static TF (둘 다 sN/ prefix).
        nodes.append(Node(
            package='tf2_ros', executable='static_transform_publisher',
            name='tf_gnss_fix',
            arguments=['0', '0', '0', '0', '0', '0', f'{ns}/gnss_base_link', f'{ns}/gps'],
            output='screen', parameters=[{'use_sim_time': use_sim_time}],
            remappings=TF_REMAP))
    else:
        # GNSS 미사용: sN/map → sN/odom 고정.
        nodes.append(Node(
            package='tf2_ros', executable='static_transform_publisher',
            name='static_map_to_odom',
            arguments=['0', '0', '0', '0', '0', '0', f'{ns}/map', f'{ns}/odom'],
            output='screen', parameters=[{'use_sim_time': use_sim_time}],
            remappings=TF_REMAP))

    # EKF odom (frames sN/odom, sN/base_footprint — ekf_params 에서 rewrite 됨).
    nodes.append(Node(
        package='robot_localization', executable='ekf_node',
        name='ekf_filter_node_odom', output='screen', parameters=[ekf_params],
        remappings=[('odometry/filtered', 'odometry/local')] + TF_REMAP))

    if with_gnss:
        nodes.append(Node(
            package='robot_localization', executable='ekf_node',
            name='ekf_filter_node_map', output='screen', parameters=[ekf_params],
            remappings=[('odometry/filtered', 'odometry/global')] + TF_REMAP))
        nodes.append(Node(
            package='robot_localization', executable='navsat_transform_node',
            name='navsat_transform', output='screen', parameters=[ekf_params],
            remappings=[('imu', f'/{ns}/gps/heading_imu'),
                        ('gps/fix', f'/{ns}/fix'),
                        ('odometry/filtered', 'odometry/local')] + TF_REMAP))
        nodes.append(Node(
            package='combat_robot_nav2', executable='frame_fixer.py',
            name='frame_fixer', output='screen',
            parameters=[{'use_sim_time': use_sim_time,
                         'input_topic': f'/{ns}/odometry/gps',
                         'output_topic': f'/{ns}/odometry/gps_map',
                         'target_frame_id': f'{ns}/map'}]))
    return nodes


def launch_setup(context, *args, **kwargs):
    ns = LaunchConfiguration('robot_ns').perform(context).strip().lstrip('/')
    use_sim_time = LaunchConfiguration('use_sim_time').perform(context).lower() == 'true'
    with_gnss = LaunchConfiguration('with_gnss').perform(context).lower() == 'true'
    if ns:
        return _namespaced_stack(context, ns, use_sim_time, with_gnss)
    return _legacy_stack(context, use_sim_time, with_gnss)


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        DeclareLaunchArgument('with_gnss', default_value='true'),
        # --- 네임스페이스 (다중로봇) ---
        DeclareLaunchArgument('robot_ns', default_value='',
                              description="이 차량 네임스페이스(''=단일로봇/현행, 's2' 등=네임스페이스 스택)"),
        # --- 군집 역할/대형 인자 (기본값 = 단일로봇 리더, 현행 동작 보존) ---
        DeclareLaunchArgument('robot_id', default_value='1',
                              description='이 차량 번호(s1=1,s2=2,s3=3)'),
        DeclareLaunchArgument('leader_robot_id', default_value='1',
                              description='리더 번호(전 차량 동일). robot_id==이 값이면 리더'),
        DeclareLaunchArgument('role', default_value='leader',
                              description='leader|follower (명령서버/문서용)'),
        DeclareLaunchArgument('formation_followers', default_value='',
                              description='리더가 출발 전 기다릴 팔로워 목록 예:"2,3" (리더만 설정, 팔로워는 빈값)'),
        DeclareLaunchArgument('formation_enable', default_value='false',
                              description='편대 오프셋/마스킹 활성. 단일로봇=false, 군집=true'),
        DeclareLaunchArgument('formation_mode', default_value='static',
                              description='static|dynamic'),
        DeclareLaunchArgument('formation_lateral_spacing_m', default_value='2.0'),
        DeclareLaunchArgument('formation_cruise_speed_mps', default_value='0.8'),
        DeclareLaunchArgument('control_mode', default_value='follow_path'),
        DeclareLaunchArgument('map_frame', default_value='map',
                              description="단일로봇 전용. robot_ns 설정 시 sN/map 자동"),
        DeclareLaunchArgument('path_command_topic', default_value='mission/path_command',
                              description='경로명령 입력(기본=FSM 게이트 출력). 글로벌버스 직결 시 swarm/path_command'),
        DeclareLaunchArgument('control_command_topic', default_value='mission/control_command'),
        OpaqueFunction(function=launch_setup),
    ])
