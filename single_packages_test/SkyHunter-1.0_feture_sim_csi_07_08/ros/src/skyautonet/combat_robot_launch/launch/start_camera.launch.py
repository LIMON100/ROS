import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():

    return LaunchDescription([
        Node(
            package='camera_ros',
            executable='camera_node',
            name='camera_node',
            output='screen',
            parameters=[{
                'format': 'RGB888',
                'width': 640,
                'height': 640,
                'sensor_mode': '640:480',
                'FrameDurationLimits': '[10000,10000]',
            }]
        )
    ])
