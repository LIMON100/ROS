from launch import LaunchDescription
from launch.substitutions import PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_hub_orchestrator", executable="hub_orchestrator_node",
            name="hub_orchestrator_node", output="screen",
            parameters=[PathJoinSubstitution([
                FindPackageShare("san_hub_orchestrator"),
                "config", "hub_orchestrator.yaml",
            ])],
        ),
    ])
