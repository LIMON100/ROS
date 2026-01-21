import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    
    pkg_skyhunter_localization = get_package_share_directory('skyhunter_localization')

    # Start the EKF node from the robot_localization package
    start_robot_localization_cmd = Node(
        package='robot_localization',
        executable='ekf_node',
        name='ekf_filter_node',
        output='screen',
        parameters=[os.path.join(pkg_skyhunter_localization, 'config', 'ekf.yaml'),
                    {'use_sim_time': True}]
    )

    return LaunchDescription([
        start_robot_localization_cmd
    ])