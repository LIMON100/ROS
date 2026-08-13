"""Per-robot namespaced sim bringup (one robot, shares an external gz world).

Brings up, all under /<robot_ns>:
  robot_state_publisher (frame_prefix) + gz spawn + ros_gz bridge
  robot_localization (ekf odom/map + navsat + frame_fixer)  [GNSS path]
  nav2 (nav2_bringup navigation_launch, namespaced, frames rewritten)
  swarm_path_executor (robot_id / formation)
  command_server (role adapter: leader or follower)

TF is namespaced (/<ns>/tf) via the ('/tf','tf') remap on every tf-touching node,
matching nav2_bringup's convention; frames are additionally prefixed (<ns>/...).
"""
import os
import sys

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction, OpaqueFunction, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node, PushRosNamespace
from launch_ros.parameter_descriptions import ParameterValue

sys.path.insert(0, os.path.dirname(__file__))
from swarm_launch_utils import rewrite_yaml_for_namespace  # noqa: E402

TF_REMAP = [('/tf', 'tf'), ('/tf_static', 'tf_static')]


def launch_setup(context, *args, **kwargs):
    ns = LaunchConfiguration('robot_ns').perform(context)
    robot_id = LaunchConfiguration('robot_id').perform(context)
    leader_id = LaunchConfiguration('leader_robot_id').perform(context)
    role = LaunchConfiguration('role').perform(context)
    x = LaunchConfiguration('x').perform(context)
    y = LaunchConfiguration('y').perform(context)
    z = LaunchConfiguration('z').perform(context)
    yaw = LaunchConfiguration('yaw').perform(context)
    spacing = LaunchConfiguration('formation_lateral_spacing_m').perform(context)
    formation_mode = LaunchConfiguration('formation_mode').perform(context)
    ff_str = LaunchConfiguration('formation_followers').perform(context)
    formation_followers = [int(x) for x in ff_str.split(',') if x.strip()]
    use_sim_time = True

    pkg_nav = get_package_share_directory('combat_robot_nav2')
    pkg_desc = get_package_share_directory('combat_robot_description')
    xacro_file = os.path.join(pkg_desc, 'urdf', 'robot.urdf.xacro')
    bt_dir = os.path.join(pkg_nav, 'include')

    # --- per-robot rewritten configs -------------------------------------------
    ekf_src = os.path.join(pkg_nav, 'config', 'ekf.yaml')
    nav2_src = os.path.join(pkg_nav, 'config', 'nav2_params_sim.yaml')
    # ekf is launched directly here, so nest its params under the namespace.
    ekf_params = rewrite_yaml_for_namespace(
        ekf_src, ns, extra_overrides={'use_sim_time': use_sim_time}, nest_under_ns=True)
    # nav2 params are nested by navigation_launch (root_key=namespace) — do not pre-nest.
    # Force use_sim_time:true here: nav2_params_sim.yaml hardcodes false per node and
    # navigation_launch does not override it, which would desync nav2 (wall clock) from
    # the sim-stamped TF and break pose lookup.
    nav2_params = rewrite_yaml_for_namespace(
        nav2_src, ns,
        extra_overrides={
            'use_sim_time': True,
            'default_nav_to_pose_bt_xml': os.path.join(bt_dir, 'single_plan_bt.xml'),
            'default_nav_through_poses_bt_xml': os.path.join(bt_dir, 'way_plan_bt.xml'),
        })

    # use_cameras:=false — strip the 4 unused gz cameras per robot (huge CPU/GPU save in
    # multi-robot sim; the cameras would otherwise saturate the host and freeze the ekf).
    robot_description = ParameterValue(
        Command(['xacro ', xacro_file, ' robot_ns:=', ns, ' use_cameras:=false']),
        value_type=str)

    # NOTE: every node below is wrapped in PushRosNamespace(ns) at the bottom, so
    # nodes must NOT set namespace= themselves (that would double to /ns/ns).
    # --- description + sim model -----------------------------------------------
    rsp = Node(
        package='robot_state_publisher', executable='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description,
                     'use_sim_time': use_sim_time,
                     'frame_prefix': ns + '/'}],
        remappings=TF_REMAP)

    spawn = Node(
        package='ros_gz_sim', executable='create', output='screen',
        arguments=['-topic', f'/{ns}/robot_description', '-name', ns,
                   '-x', x, '-y', y, '-z', z, '-Y', yaw])

    # bridge_lidar:=false — 라이다 PointCloud 브리지 제외. HIL 에서 두뇌가 보드(wifi 너머)에
    # 있을 때 라이다 PointCloud2 를 wifi 로 보내면 보드 ekf 가 처리부하로 update rate 를 못 맞춰
    # TF(base_footprint->odom) 가 늦어지고 costmap activate 가 timeout(nav2 bringup abort)됨.
    # sim 빈맵은 라이다가 불필요하므로 보드 두뇌가 받는 s2 몸체에서만 끈다.
    bridge_lidar = LaunchConfiguration('bridge_lidar').perform(context).lower() == 'true'
    bridge_args = [
        f'/{ns}/cmd_vel@geometry_msgs/msg/Twist@gz.msgs.Twist',
        f'/{ns}/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry',
        f'/{ns}/sensing/imu/imu_data@sensor_msgs/msg/Imu[gz.msgs.IMU',
        f'/{ns}/sensing/gnss/nav_sat_fix@sensor_msgs/msg/NavSatFix[gz.msgs.NavSat',
        f'/{ns}/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model',
    ]
    bridge_remaps = [
        (f'/{ns}/sensing/gnss/nav_sat_fix', f'/{ns}/fix'),
        (f'/{ns}/sensing/imu/imu_data', f'/{ns}/gps/heading_imu'),
    ]
    if bridge_lidar:
        bridge_args.insert(2, f'/{ns}/front_lidar/scan/points/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked')
        bridge_remaps.insert(0, (f'/{ns}/front_lidar/scan/points/points', f'/{ns}/rslidar_points'))
    bridge = Node(
        package='ros_gz_bridge', executable='parameter_bridge',
        name='gz_bridge', output='screen',
        parameters=[{'use_sim_time': use_sim_time}],
        arguments=bridge_args,
        remappings=bridge_remaps)

    # --- localization (GNSS path) ----------------------------------------------
    ekf_odom = Node(
        package='robot_localization', executable='ekf_node',
        name='ekf_filter_node_odom', output='screen',
        parameters=[ekf_params],
        remappings=[('odometry/filtered', 'odometry/local')] + TF_REMAP)
    ekf_map = Node(
        package='robot_localization', executable='ekf_node',
        name='ekf_filter_node_map', output='screen',
        parameters=[ekf_params],
        remappings=[('odometry/filtered', 'odometry/global')] + TF_REMAP)
    # Shared datum across all robots (the sejong.world GNSS origin) so the same
    # lat/lon path maps to a consistent sN/map for every robot — required for the
    # follower's lateral formation offset to hold. Without it each robot would take
    # its own first fix as datum (origin at its spawn) and they would converge.
    datum_lat = float(LaunchConfiguration('datum_lat').perform(context))
    datum_lon = float(LaunchConfiguration('datum_lon').perform(context))
    navsat = Node(
        package='robot_localization', executable='navsat_transform_node',
        name='navsat_transform', output='screen',
        parameters=[ekf_params, {'use_sim_time': use_sim_time,
                                 'wait_for_datum': True,
                                 'datum': [datum_lat, datum_lon, 0.0]}],
        remappings=[('imu', f'/{ns}/gps/heading_imu'),
                    ('gps/fix', f'/{ns}/fix'),
                    ('odometry/filtered', 'odometry/local')] + TF_REMAP)
    frame_fixer = Node(
        package='combat_robot_nav2', executable='frame_fixer_node',
        name='frame_fixer', output='screen',
        parameters=[{'use_sim_time': use_sim_time,
                     'input_topic': f'/{ns}/odometry/gps',
                     'output_topic': f'/{ns}/odometry/gps_map',
                     'target_frame_id': f'{ns}/map'}])

    # --- nav2 -------------------------------------------------------------------
    # navigation_launch nests params under root_key=namespace but does NOT push the
    # namespace itself, so it must run inside our PushRosNamespace(ns) group.
    # Trimmed navigation (drops route_server/waypoint_follower/smoother_server/docking)
    # to cut node count + CPU when running N full nav2 stacks on one host. cmd_vel chain
    # (controller->velocity_smoother->collision_monitor) preserved.
    nav2 = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_nav, 'launch',
                                                   'navigation_lite.launch.py')),
        launch_arguments={'namespace': ns, 'use_sim_time': 'true',
                          'autostart': 'true', 'params_file': nav2_params}.items())

    # --- map server (static empty sim map; global_costmap static_layer needs it) -
    # The grid must be published in this robot's prefixed frame (sN/map). A dedicated
    # lifecycle manager activates it (nav2's navigation_launch manager does not own it).
    map_file = os.path.join(pkg_nav, 'map', 'sim_empty', 'sim_empty.yaml')
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

    # --- mission / command ------------------------------------------------------
    exec_params = {'use_sim_time': use_sim_time,
                   'robot_id': int(robot_id),
                   'leader_robot_id': int(leader_id),
                   'map_frame': f'{ns}/map',
                   'spawn_x': float(x),
                   'spawn_y': float(y),
                   'formation.lateral_spacing_m': float(spacing),
                   'formation.enable': True,
                   'formation.densify_m': 0.0,
                   # [SIM] 주행속도 상향(기본 0.8 은 sim 관찰엔 너무 느림). 실차 launch 는 미override.
                   'formation.cruise_speed_mps': float(LaunchConfiguration('formation_cruise_speed_mps').perform(context)),
                   'formation.obstacle_detect_range_m': 12.0, #25.0,
                   'formation.lidar_self_range_m': 4.0,
                   'formation.obstacle_z_min_m': 0.50, #0.15,
                   'formation.min_cluster_pts': 3,
                   'formation.obstacle_persist_ticks': 1,
                   'formation.mode': formation_mode}
    # Only pass formation.followers when non-empty: an empty list becomes an invalid
    # () tuple param in launch. Empty = no form-up gate (executor default).
    if formation_followers:
        exec_params['formation.followers'] = formation_followers
    executor = Node(
        package='combat_robot_nav2', executable='swarm_path_executor',
        name='swarm_path_executor', output='screen',
        parameters=[exec_params])

    command_server = Node(
        package='robot_server', executable='command_server_node',
        name='command_server', output='screen',
        parameters=[{'use_sim_time': use_sim_time,
                     'role': role,
                     'robot_id': int(robot_id),
                     'robot_namespace': ns,
                     'deployment_mode': 'office_test'}])

    # [per-robot 보드 모델] 각 로봇이 자기 FSM 을 돌려 자기 path_executor 를 게이트한다.
    # command_server → /sN/swarm/{path,control}_command → FSM → /sN/mission/{path,control}_command
    # → executor. sim 은 detection/gun/pantilt HW 가 없으므로 status 체크 비활성(게이트는 투명전달).
    fsm = Node(
        package='combat_robot_operation_system',
        executable='combat_robot_operation_system_node',
        name='combat_robot_operation_system', output='screen',
        parameters=[{'use_sim_time': use_sim_time,
                     'deployment_mode': 'office_test',
                     'checks.detector_status': False,
                     'checks.gun_status': False,
                     'checks.pantilt_status': False}])

    # Teammate-masking lidar filter: removes formation teammates from this robot's
    # lidar so the nav2 costmap/collision_monitor don't treat them as obstacles
    # (otherwise robots block each other in tight formations). Static obstacles intact.
    swarm_lidar_filter = Node(
        package='combat_robot_nav2', executable='swarm_lidar_filter.py',
        name='swarm_lidar_filter', output='screen',
        parameters=[{'use_sim_time': use_sim_time,
                     'robot_id': int(robot_id),
                     'map_frame': f'{ns}/map',
                     'base_frame': f'{ns}/base_footprint',
                     'mask_radius_m': 1.2}],
        remappings=TF_REMAP)

    # A dynamic follower drives /cmd_vel directly from the formation controller, so it
    # does NOT run nav2 (which would contend for cmd_vel via collision_monitor). It
    # still needs localization (ekf) for its own pose. The leader always runs nav2.
    dynamic_follower = (formation_mode == 'dynamic' and role == 'follower')

    # --- HIL 분할: 몸체(gz)/두뇌(nav·loc·executor)/명령(server·fsm) 을 따로 켤 수 있다.
    # 기본은 셋 다 true → 기존 단일호스트 sim 동작과 동일.
    #   · 호스트(몸체 제공):  launch_brain:=false launch_command:=false  → rsp+gz spawn+bridge
    #   · 보드(두뇌만, HIL):  launch_body:=false  launch_command:=false  → rsp+ekf+nav2+executor
    #     (보드엔 command_server/fsm 패키지가 없으므로 launch_command 는 항상 false)
    launch_body = LaunchConfiguration('launch_body').perform(context).lower() == 'true'
    launch_brain = LaunchConfiguration('launch_brain').perform(context).lower() == 'true'
    launch_command = LaunchConfiguration('launch_command').perform(context).lower() == 'true'

    # rsp 는 양쪽 다 필요(gz spawn 은 /robot_description, nav2/ekf 는 TF 트리). 항상 포함.
    nodes = [PushRosNamespace(ns), rsp]
    if launch_body:
        nodes += [spawn, bridge]
    if launch_brain:
        nodes += [ekf_odom, ekf_map, navsat, frame_fixer, executor]
        # swarm_lidar_filter: formation-aware teammate masking — masks teammates only once
        # formed up (no perturbation/push → symmetric formation), sees them during form-up
        # (route around, no ramming). Driven by /sN/mask_teammates from the executor.
        if not dynamic_follower:
            nodes += [map_server, map_lifecycle, nav2, swarm_lidar_filter]
    if launch_command:
        nodes += [command_server, fsm]

    return [GroupAction(nodes)]


def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('robot_ns', default_value='s1'),
        # HIL 분할 플래그(기본 셋 다 true = 단일호스트 sim). 설명은 launch_setup 참고.
        DeclareLaunchArgument('launch_body', default_value='true',
                              description='gz spawn+bridge (몸체). 호스트만 true'),
        DeclareLaunchArgument('launch_brain', default_value='true',
                              description='ekf+nav2+executor (두뇌). HIL 시 보드만 true'),
        DeclareLaunchArgument('launch_command', default_value='true',
                              description='command_server+fsm. 보드엔 패키지 없어 false'),
        DeclareLaunchArgument('bridge_lidar', default_value='true',
                              description='gz 라이다 PointCloud 브리지. HIL 보드두뇌 몸체는 false(wifi 부하 회피)'),
        DeclareLaunchArgument('robot_id', default_value='1'),
        DeclareLaunchArgument('leader_robot_id', default_value='1'),
        DeclareLaunchArgument('role', default_value='leader'),
        DeclareLaunchArgument('x', default_value='0.0'),
        DeclareLaunchArgument('y', default_value='0.0'),
        DeclareLaunchArgument('z', default_value='0.3'),
        DeclareLaunchArgument('yaw', default_value='0.0'),
        DeclareLaunchArgument('formation_lateral_spacing_m', default_value='2.0'),
        DeclareLaunchArgument('formation_mode', default_value='static',
                              description='static (offset-path) | dynamic (track leader)'),
        DeclareLaunchArgument('formation_followers', default_value='',
                              description='leader-only: comma-sep follower ids to form up before start, e.g. "2,3"'),
        # sejong.world <spherical_coordinates> origin — shared datum for all robots.
        DeclareLaunchArgument('datum_lat', default_value='36.61002559225'),
        DeclareLaunchArgument('datum_lon', default_value='127.28772570583'),
        DeclareLaunchArgument('formation_cruise_speed_mps', default_value='1.5'),
        OpaqueFunction(function=launch_setup),
    ])
