from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_mission",
            executable="mission_node",
            name="mission_node",
            output="screen",
            parameters=[PathJoinSubstitution([
                FindPackageShare("san_mission"),
                "config", "mission.yaml",
            ])],
        ),
    ])
