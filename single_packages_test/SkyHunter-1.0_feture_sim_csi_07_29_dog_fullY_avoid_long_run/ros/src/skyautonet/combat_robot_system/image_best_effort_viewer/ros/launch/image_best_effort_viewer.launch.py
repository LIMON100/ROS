from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        Node(
            package='image_best_effort_viewer',
            executable='image_best_effort_viewer',
            name='image_best_effort_viewer',
            output='screen'
        )
    ])
