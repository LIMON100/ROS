import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue
from launch.substitutions import Command

def generate_launch_description():
    pkg_bird_nav = get_package_share_directory('combat_robot_nav2')
    pkg_vehicle_desc = get_package_share_directory('combat_robot_description')
    
    # 🔥 핵심 추가: 가제보(Gazebo)가 3D 모델(Mesh)을 찾을 수 있도록 ROS 2 패키지 경로를 알려줍니다.
    # pkg_vehicle_desc는 '.../share/combat_robot_description' 이므로, 상위 폴더인 '.../share'를 경로로 등록합니다.
    gz_resource_path = SetEnvironmentVariable(
        name='GZ_SIM_RESOURCE_PATH',
        value=os.path.join(pkg_vehicle_desc, '..')
    )

    world_file = os.path.join(pkg_bird_nav, 'world', 'sejong.world')

    gazebo_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(get_package_share_directory('ros_gz_sim'), 'launch', 'gz_sim.launch.py')
        ),
        launch_arguments={'gz_args': f'-r {world_file}'}.items()
    )

    xacro_file = os.path.join(pkg_vehicle_desc, 'urdf', 'robot.urdf.xacro')
    # robot_description 는 URDF(XML) 문자열. ParameterValue(value_type=str) 로 감싸지 않으면
    # launch_ros 가 YAML 로 파싱하려다 "Unable to parse ... as yaml" 로 launch 가 죽을 수 있다.
    robot_description_config = ParameterValue(
        Command(['xacro ', xacro_file]), value_type=str)

    node_robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_description_config, 'use_sim_time': True}]
    )

    spawn_entity = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=['-topic', 'robot_description', '-name', 'combat_robot', '-z', '0.3'],
        output='screen'
    )

    bridge_node = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=[
            '/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock',
            '/cmd_vel@geometry_msgs/msg/Twist@gz.msgs.Twist',
            '/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            # gpu_lidar 의 <topic> 이 /front_lidar/scan/points 라서 gz 는
            #   /front_lidar/scan/points        -> LaserScan
            #   /front_lidar/scan/points/points -> PointCloudPacked (실제 3D 클라우드)
            # 를 발행한다. PointCloud2 를 받으려면 .../points/points 를 브리지해야 한다.
            '/front_lidar/scan/points/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked',
            '/sensing/imu/imu_data@sensor_msgs/msg/Imu[gz.msgs.IMU',
            '/sensing/gnss/nav_sat_fix@sensor_msgs/msg/NavSatFix[gz.msgs.NavSat',
            '/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model',
        ],
        remappings=[
            # nav2 costmap/collision_monitor 가 기대하는 토픽으로 직접 발행
            # (실로봇 rslidar 드라이버와 동일한 /rslidar_points → sim/실로봇 일치).
            ('/front_lidar/scan/points/points', '/rslidar_points'),
            # sim 의 gz 센서 토픽을 localization 스택(navsat/ekf)이 기대하는 실로봇
            # 토픽명과 일치시킨다. (실로봇은 gnss_heading.py 가 /fix·/gps/heading_imu 발행)
            #   gz /sensing/gnss/nav_sat_fix → /fix              (navsat gps/fix)
            #   gz /sensing/imu/imu_data     → /gps/heading_imu  (navsat imu + ekf imu0)
            ('/sensing/gnss/nav_sat_fix', '/fix'),
            ('/sensing/imu/imu_data', '/gps/heading_imu'),
        ],
        parameters=[{'use_sim_time': True}],
        output='screen'
    )

    tf_gnss_fix = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        arguments=['0', '0', '0', '0', '0', '0', 'gnss_base_link', 'gnss_sensor'],
        output='screen'
    )

    return LaunchDescription([
        gz_resource_path,  # 방금 추가한 환경 변수를 실행 목록에 넣습니다.
        gazebo_launch,
        node_robot_state_publisher,
        spawn_entity,
        bridge_node,
        tf_gnss_fix
    ])