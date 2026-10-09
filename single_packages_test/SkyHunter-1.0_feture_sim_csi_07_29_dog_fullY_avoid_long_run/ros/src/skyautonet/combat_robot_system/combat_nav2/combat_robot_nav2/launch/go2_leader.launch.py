"""Go2 leader BODY for the CSI swarm — spawns the Go2 into an already-running gz
world and exposes it on the leader's /s1/* topics (/s1/cmd_vel in, /s1/odom out)."""
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, TimerAction
from launch.substitutions import Command, LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    use_sim_time = True
    ns = 's1'
    
    go2_sim = get_package_share_directory('unitree_go2_sim')
    go2_desc = get_package_share_directory('unitree_go2_description')
    joints_config = os.path.join(go2_sim, 'config/joints/joints.yaml')
    links_config = os.path.join(go2_sim, 'config/links/links.yaml')
    gait_config = os.path.join(go2_sim, 'config/gait/gait.yaml')
    ros_control_config = os.path.join(go2_sim, 'config/ros_control/ros_control.yaml')
    model_path = os.path.join(go2_desc, 'urdf/unitree_go2_robot.xacro')

    x = LaunchConfiguration('x')
    y = LaunchConfiguration('y')
    z = LaunchConfiguration('z')
    yaw = LaunchConfiguration('yaw')
    
    robot_description = {'robot_description': Command(
        ['xacro ', model_path, ' robot_controllers:=', ros_control_config])}
    
    rsp = Node(package='robot_state_publisher', executable='robot_state_publisher',
                output='screen',
                parameters=[robot_description, {'use_sim_time': use_sim_time}])
    
    spawn = Node(package='ros_gz_sim', executable='create', output='screen',
                arguments=['-name', 'go2', '-topic', 'robot_description',
                            '-x', x, '-y', y, '-z', z, '-Y', yaw])
    
    quadruped_controller = Node(
        package='champ_base', executable='quadruped_controller_node', output='screen',
        parameters=[{'use_sim_time': use_sim_time}, {'gazebo': True},
                    {'publish_joint_states': True}, {'publish_joint_control': True},
                    {'publish_foot_contacts': False},
                    {'joint_controller_topic': 'joint_group_effort_controller/joint_trajectory'},
                    {'urdf': Command(['xacro ', model_path])},
                    joints_config, links_config, gait_config,
                    {'hardware_connected': False}, {'close_loop_odom': True}],
        remappings=[('/cmd_vel/smooth', f'/{ns}/cmd_vel_leader')])
        
    state_estimator = Node(
        package='champ_base', executable='state_estimation_node', output='screen',
        parameters=[{'use_sim_time': use_sim_time}, {'orientation_from_imu': True},
                    {'urdf': Command(['xacro ', model_path])},
                    joints_config, links_config, gait_config])
    
    bridge = Node(  
        package='ros_gz_bridge', executable='parameter_bridge', name='go2_gz_bridge',
        output='screen', parameters=[{'use_sim_time': use_sim_time}],
        arguments=[
            '/imu/data@sensor_msgs/msg/Imu[gz.msgs.IMU',
            '/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model',
            '/unitree_lidar/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked',
            '/odom_gt@nav_msgs/msg/Odometry[gz.msgs.Odometry',
            '/gps/fix@sensor_msgs/msg/NavSatFix[gz.msgs.NavSat',
            '/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist',
            '/joint_group_effort_controller/joint_trajectory@trajectory_msgs/msg/JointTrajectory]gz.msgs.JointTrajectory',
        ],  
        
        remappings=[
            ('/odom_gt', f'/{ns}/odom'),
            ('/gps/fix', f'/{ns}/fix'),
            ('/imu/data', f'/{ns}/imu/data'),
            ('/unitree_lidar/points', f'/{ns}/rslidar_points_raw'),
        ])
    
    spawner_js = TimerAction(period=2.0, actions=[Node(  # MUST BE 2.0
          package='controller_manager', executable='spawner', output='screen',
          arguments=['--controller-manager-timeout', '120', 'joint_states_controller'],
          parameters=[{'use_sim_time': use_sim_time}])])
    
    spawner_effort = TimerAction(period=4.0, actions=[Node(  # MUST BE 4.0
        package='controller_manager', executable='spawner', output='screen',
        arguments=['--controller-manager-timeout', '120', 'joint_group_effort_controller'],
        parameters=[{'use_sim_time': use_sim_time}])])
        
    return LaunchDescription([
        DeclareLaunchArgument('x', default_value='0.0'),
        DeclareLaunchArgument('y', default_value='0.0'),
        DeclareLaunchArgument('z', default_value='0.375'),
        DeclareLaunchArgument('yaw', default_value='0.0'),
        rsp, spawn, quadruped_controller, state_estimator, bridge,
        Node(package='tf2_ros', executable='static_transform_publisher', name='go2_lidar_tf',
            arguments=['0.2','0','0.4','0','0','0','s1/base_footprint','lidar_l1_link'],
            remappings=[('/tf','/s1/tf'),('/tf_static','/s1/tf_static')],
            parameters=[{'use_sim_time': True}]),
        Node(package='tf2_ros', executable='static_transform_publisher', name='go2_gps_tf',
            arguments=['0.1','0','0.4','0','0','0','s1/base_footprint','gps_link'],
            remappings=[('/tf','/s1/tf'),('/tf_static','/s1/tf_static')],
            parameters=[{'use_sim_time': True}]),
        spawner_js, spawner_effort,
    ])