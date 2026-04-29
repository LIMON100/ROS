from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    use_sim_time = LaunchConfiguration('use_sim_time')

    return LaunchDescription([
        DeclareLaunchArgument('use_sim_time', default_value='true'),

        # Leader Logic
        Node(
            package='skyhunter_control', executable='leader_node',
            output='screen', parameters=[{'use_sim_time': use_sim_time}],
            remappings=[('odom', '/odom'), ('leader_state', '/leader_state')]
        ),
        
        # Leader Perception
        Node(
            package='skyhunter_perception', executable='yolo_detector_node',
            name='leader_yolo_node', output='screen',
            parameters=[{'robot_ns': '', 'use_sim_time': use_sim_time}]
        ),

        # Leader Management
        Node(
            package='skyhunter_control', executable='leadership_manager',
            name='leadership_manager_R1', parameters=[{'robot_int_id': 1, 'use_sim_time': use_sim_time}],
            remappings=[('leader_state', '/leader_state'), ('/swarm/virtual_leader/state', '/swarm/virtual_leader/state')]
        ),

        # Waypoint Sender (Mission Control)
        Node(
            package='skyhunter_nav_tools', executable='waypoint_sender',
            output='screen', parameters=[{'use_sim_time': use_sim_time}]
        )
    ])