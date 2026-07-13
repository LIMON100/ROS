"""Launch 5-robot Gazebo simulation (1 leader + 1 hub + 3 followers).

Each robot runs the full patrol stack via SH_Unitree_Patrol/main.py with
per-robot config (robot_id, role).
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution
from launch_ros.actions import Node

ROBOTS = [
    {"id": 1, "role": "leader",   "x":  0.0, "y":  0.0, "color": "red"},
    {"id": 2, "role": "hub",      "x": -2.0, "y":  0.0, "color": "blue"},
    {"id": 3, "role": "follower", "x": -3.5, "y":  3.5, "color": "green"},
    {"id": 4, "role": "follower", "x": -3.5, "y": -3.5, "color": "green"},
    {"id": 5, "role": "follower", "x": -7.0, "y":  0.0, "color": "green"},
]


def generate_launch_description():
    actions = []

    actions.append(DeclareLaunchArgument(
        "world", default_value="empty_field",
        description="World file name (without .world extension)"))

    world = LaunchConfiguration("world")

    # Gazebo server + client. ROS 2 launch evaluates LaunchConfiguration
    # at runtime — `$(arg ...)` is ROS 1 XML-launch syntax and ends up in
    # argv as a literal string, which Gazebo can't resolve. Use a
    # PathJoinSubstitution list with the LaunchConfiguration in place.
    actions.append(Node(
        package="gazebo_ros", executable="gzserver",
        arguments=[
            PathJoinSubstitution([
                "/opt/ros2_ws/src/gazebo/worlds",
                [world, ".world"],
            ]),
            "-s", "libgazebo_ros_factory.so",
            "-s", "libgazebo_ros_init.so",
        ],
        output="screen",
    ))
    actions.append(Node(
        package="gazebo_ros", executable="gzclient",
        output="screen",
    ))

    for r in ROBOTS:
        actions.append(Node(
            package="gazebo_ros", executable="spawn_entity.py",
            namespace=f"robot{r['id']}",
            arguments=[
                "-entity", f"robot{r['id']}",
                "-x", str(r["x"]), "-y", str(r["y"]), "-z", "0.3",
                "-file",
                "/opt/ros2_ws/src/gazebo/models/unitree_go2/model.sdf",
            ],
        ))

        actions.append(Node(
            package="patrol_stack", executable="main",
            namespace=f"robot{r['id']}",
            output="screen",
            parameters=[{
                "robot_id": r["id"],
                "robot_role": r["role"],
                "ros_domain_id": 42,
                "config_file": f"/opt/patrol/config/robot{r['id']}.yaml",
            }],
        ))

    return LaunchDescription(actions)
