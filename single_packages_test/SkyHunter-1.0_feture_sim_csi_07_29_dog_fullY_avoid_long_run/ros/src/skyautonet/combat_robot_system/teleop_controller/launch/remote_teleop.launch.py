from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='teleop_controller',
            executable='remote_teleop.py',
            name='remote_teleop',
            output='screen'
        )
    ])