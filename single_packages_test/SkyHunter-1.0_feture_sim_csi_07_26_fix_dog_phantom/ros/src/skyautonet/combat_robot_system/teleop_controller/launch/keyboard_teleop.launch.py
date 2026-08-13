from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='teleop_controller',
            executable='keyboard_teleop.py',
            name='keyboard_teleop',
            output='screen',
            prefix='xterm -e'
        )
    ])