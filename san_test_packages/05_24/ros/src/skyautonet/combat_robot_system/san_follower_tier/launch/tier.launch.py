from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    robot_id = LaunchConfiguration("robot_id", default="3")
    return LaunchDescription([
        DeclareLaunchArgument("robot_id", default_value="3"),
        Node(
            package="san_follower_tier",
            executable="tier_node",
            name="tier_node",
            output="screen",
            parameters=[
                PathJoinSubstitution([
                    FindPackageShare("san_follower_tier"),
                    "config", "tier.yaml",
                ]),
                {"robot_id": robot_id},
            ],
        ),
    ])
