from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_comm_link", executable="comm_link_node",
            name="comm_link_node", output="screen",
            parameters=[PathJoinSubstitution([
                FindPackageShare("san_comm_link"),
                "config", "comm_link.yaml",
            ])],
        ),
    ])
