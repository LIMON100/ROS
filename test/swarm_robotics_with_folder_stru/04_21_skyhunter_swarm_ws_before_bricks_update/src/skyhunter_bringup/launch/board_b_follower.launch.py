from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def launch_setup(context, *args, **kwargs):
    num_robots = int(context.perform_substitution(LaunchConfiguration('num_robots')))
    use_sim_time = LaunchConfiguration('use_sim_time')
    
    nodes_to_start = []

    for i in range(2, num_robots + 1):
        follower_ns = f'SH_{i:02d}'
        row = i // 2
        side = 1 if i % 2 == 0 else -1
        drive_off_back = -3.0 * row
        drive_off_side = 1.5 * side

        # Leadership Manager
        nodes_to_start.append(Node(
            package='skyhunter_control', executable='leadership_manager', namespace=follower_ns,
            parameters=[{'robot_int_id': i, 'use_sim_time': use_sim_time}],
            remappings=[('leader_state', f'/{follower_ns}/leader_state'), ('/swarm/virtual_leader/state', '/swarm/virtual_leader/state')]
        ))

        # Local Dormant Leader Node
        nodes_to_start.append(Node(
            package='skyhunter_control', executable='leader_node', namespace=follower_ns,
            parameters=[{'use_sim_time': use_sim_time}],
            remappings=[('odom', f'/{follower_ns}/odom_filtered'), ('leader_state', f'/{follower_ns}/leader_state'), ('plan', f'/{follower_ns}/plan')]
        ))

        # Follower Logic
        nodes_to_start.append(Node(
            package='skyhunter_control', executable='follower_node', namespace=follower_ns,
            parameters=[{'use_sim_time': use_sim_time, 'offset_dist': float(drive_off_back), 'offset_lateral': float(drive_off_side)}],
            remappings=[('leader_state', '/swarm/virtual_leader/state'), ('odom', f'/{follower_ns}/odom_filtered'),
                        ('cmd_vel', f'/{follower_ns}/cmd_vel'), ('scan/points', f'/{follower_ns}/scan/points'),
                        ('/tf', '/tf'), ('/tf_static', '/tf_static')]
        ))

        # Dormant YOLO Node
        nodes_to_start.append(Node(
            package='skyhunter_perception', executable='yolo_detector_node', namespace=follower_ns,
            parameters=[{'robot_ns': follower_ns, 'use_sim_time': use_sim_time}]
        ))

    return nodes_to_start

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('num_robots', default_value='3'),
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        OpaqueFunction(function=launch_setup)
    ])