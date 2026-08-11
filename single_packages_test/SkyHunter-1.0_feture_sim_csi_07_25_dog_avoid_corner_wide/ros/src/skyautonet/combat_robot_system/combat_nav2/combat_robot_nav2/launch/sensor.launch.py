import os
import re
from launch import LaunchDescription
from launch.actions import (SetEnvironmentVariable, DeclareLaunchArgument,
                            OpaqueFunction)
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory


# 센서(IMU/GPS/LiDAR)를 robot_id 하나로 자동 namespace 매핑한다.
#   ros2 launch combat_robot_nav2 sensor.launch.py robot_id:=2
#     → /s2/imu/data, /s2/fix, /s2/rslidar_points, frame s2/imu_link·s2/front_lidar_link
#   robot_id 생략 → legacy(namespace 없음): /imu/data, /fix, /rslidar_points
#
# 배경: nav2/ekf/swarm 스택(bringup_realtime)은 robot_ns 로 자동 매핑되지만
#   센서 드라이버는 절대토픽(/imu/data,/fix)·config토픽(/rslidar_points)이라
#   자동 매핑에서 빠져 있었다. 여기서 그 갭을 메운다.
#   - Xsens/GNSS: 절대토픽 → launch remap + frame 파라미터
#   - RoboSense : 토픽·frame 을 config.yaml 에서 읽으므로, base config 를 읽어
#                 namespace 판을 /tmp 에 생성하고 config_path 로 넘긴다.


def _launch_setup(context, *args, **kwargs):
    robot_id = LaunchConfiguration('robot_id').perform(context).strip()
    ns = 's{}'.format(robot_id) if robot_id else ''
    tprefix = '/{}'.format(ns) if ns else ''       # topic prefix
    fprefix = '{}/'.format(ns) if ns else ''       # tf frame prefix

    actions = []

    # --- 1. Xsens MTi IMU (절대토픽 /imu/data → /<ns>/imu/data, frame <ns>/imu_link) ---
    xsens_yaml = os.path.join(
        get_package_share_directory('bluespace_ai_xsens_mti_driver'),
        'param', 'xsens_mti_node.yaml')
    actions.append(Node(
        package='bluespace_ai_xsens_mti_driver', executable='xsens_mti_node',
        output='screen',
        parameters=[xsens_yaml, {'frame_id': '{}imu_link'.format(fprefix)}],
        remappings=[('/imu/data', '{}/imu/data'.format(tprefix))] if ns else [],
    ))

    # --- 2. GNSS heading provider (절대토픽 → /<ns>/...) ---
    gnss_remaps = []
    if ns:
        gnss_remaps = [
            ('/fix', '{}/fix'.format(tprefix)),
            ('/gps/heading_imu', '{}/gps/heading_imu'.format(tprefix)),
            ('/vel', '{}/vel'.format(tprefix)),
            ('/edge_heading', '{}/edge_heading'.format(tprefix)),
        ]
    # ★ C++ 노드(gnss_heading_node) 사용 — python 판(gnss_heading.py)이 CPU ~25%
    #   먹어 저사양 보드에서 ekf rate miss/과부하 유발. C++ 는 ~5%, drop-in(동일 파라미터/토픽).
    actions.append(Node(
        package='combat_robot_nav2', executable='gnss_heading_node', output='screen',
        parameters=[{
            'port': '/dev/gps',
            'baud': 921600,
            # heading_frame_id 는 TF 트리에 존재해야 navsat 이 heading 을 변환한다.
            # (기본 'gps' 는 트리에 없어 /odometry/gps 가 안 뜨는 버그가 있었음)
            'heading_frame_id': '{}base_footprint'.format(fprefix) if ns else 'gps',
            'gps_frame_id': '{}gps'.format(fprefix) if ns else 'gps',
            'antenna_yaw_offset_deg': 70.5,
        }],
        remappings=gnss_remaps,
    ))

    # --- 3. RoboSense LiDAR (config.yaml 을 namespace 판으로 templating) ---
    # rslidar config.yaml 은 install/share 에 설치되지 않고 src 에만 있다
    # (rslidar_sdk_node 는 config_path='' 이면 컴파일된 PROJECT_PATH=src 를 읽음).
    # 이 launch 파일의 실제 경로(심링크 해제)에서 상대경로로 src config 를 찾는다.
    _here = os.path.dirname(os.path.realpath(__file__))
    _candidates = [
        os.path.join(_here, '..', '..', 'rslidar_sdk', 'config', 'config.yaml'),
        os.path.join(get_package_share_directory('rslidar_sdk'), 'config', 'config.yaml'),
    ]
    base_cfg = next((p for p in _candidates if os.path.exists(p)), None)
    if base_cfg is None:
        raise RuntimeError('rslidar config.yaml 을 찾을 수 없음: {}'.format(_candidates))
    with open(base_cfg) as f:
        txt = f.read()
    topic = '{}/rslidar_points'.format(tprefix) if ns else '/rslidar_points'
    frame = '{}front_lidar_link'.format(fprefix) if ns else 'rslidar'
    txt = re.sub(r'ros_send_point_cloud_topic:\s*\S+',
                 'ros_send_point_cloud_topic: {}'.format(topic), txt)
    txt = re.sub(r'ros_frame_id:\s*\S+',
                 'ros_frame_id: {}'.format(frame), txt)
    out_cfg = '/tmp/rslidar_{}.yaml'.format(ns if ns else 'default')
    with open(out_cfg, 'w') as f:
        f.write(txt)
    actions.append(Node(
        package='rslidar_sdk', executable='rslidar_sdk_node', output='screen',
        parameters=[{'config_path': out_cfg}],
    ))

    return actions


def generate_launch_description():
    return LaunchDescription([
        SetEnvironmentVariable(name='ROS_DISABLE_LO_ONLY', value='1'),
        DeclareLaunchArgument(
            'robot_id', default_value='',
            description='Robot number N → namespace /sN. Empty = legacy (no namespace). '
                        'eth0 라이다 IP 는 nmcli 로 영구 설정돼 있다고 가정(이 launch 는 네트워크 안 건드림).'),
        OpaqueFunction(function=_launch_setup),
    ])
