import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch.launch_description_sources import PythonLaunchDescriptionSource
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    pkg_dir = get_package_share_directory('combat_robot_nav2')
    nav2_bringup_dir = get_package_share_directory('nav2_bringup')

    use_sim_time = LaunchConfiguration('use_sim_time', default='false')
    param_file = os.path.join(pkg_dir, 'config', 'nav2_params_sim.yaml')

    # BT xml 절대경로 하드코딩 제거: 설치된 share/include 경로로 런타임 치환.
    # (워크스페이스 위치와 무관하게 동작)
    bt_dir = os.path.join(pkg_dir, 'include')
    param_substitutions = {
        'default_nav_to_pose_bt_xml': os.path.join(bt_dir, 'single_plan_bt.xml'),
        'default_nav_through_poses_bt_xml': os.path.join(bt_dir, 'way_plan_bt.xml'),
        # nav2_params.yaml 에는 실로봇 기준 use_sim_time:false 가 하드코딩돼 있다.
        # sim(use_sim_time:=true) 에서는 nav2 전체 노드가 /clock 을 쓰도록 여기서 덮어쓴다.
        # (안 하면 nav2 는 wall-clock, 센서/TF 는 sim-time → collision_monitor 가
        #  타임스탬프 불일치로 소스를 버리고 로봇을 정지시킨다.)
        'use_sim_time': use_sim_time,
    }
    configured_params = RewrittenYaml(
        source_file=param_file,
        root_key='',
        param_rewrites=param_substitutions,
        convert_types=True,
    )

    navigation_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(nav2_bringup_dir, 'launch', 'navigation_launch.py')
        ),
        launch_arguments={
            'params_file': configured_params,
            'use_sim_time': use_sim_time,  # 🚀 수정: 시뮬레이션 시간 동기화
            'autostart': 'true'
        }.items()
    )

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='false'),
        navigation_launch
    ])
