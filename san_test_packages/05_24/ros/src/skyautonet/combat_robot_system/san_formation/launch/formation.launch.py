from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_formation",
            executable="formation_node",
            name="formation_node",
            output="screen",
            parameters=[PathJoinSubstitution([
                FindPackageShare("san_formation"),
                "config", "formation.yaml",
            ])],
        ),
    ])
