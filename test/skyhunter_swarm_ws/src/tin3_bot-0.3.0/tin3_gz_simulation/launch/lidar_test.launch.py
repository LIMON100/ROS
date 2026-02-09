import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import PathJoinSubstitution, LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")
    pkg_tin3_bot = get_package_share_directory("tin3_bot")

    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[os.environ.get("GZ_SIM_RESOURCE_PATH", ""), ":", os.path.dirname(pkg_tin3_bot)]
    )

    # 1. Start Gazebo (Rough Terrain)
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")),
        launch_arguments={"gz_args": "-r " + os.path.join(pkg_tin3_bot, "worlds", "obstacle_world.sdf")}.items(),
    )

    # 2. Spawn ONE Robot (Robot 01)
    spawn_robot = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin3_bot, "launch", "spawn_robot.launch.py")),
        launch_arguments={
            "robot_ns": "robot_01",
            "x": "0.0", "y": "0.0", "z": "0.5",
            "use_ekf": "true",
            "lidar_mode": "full"
        }.items(),
    )

    # 3. Clock Bridge
    clock_bridge = Node(package="ros_gz_bridge", executable="parameter_bridge",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"], output="screen")

    # 4. YOUR DEBUG NODE (Runs C++ logic)
    debug_node = Node(
        package='skyhunter_perception',
        executable='lidar_debug_node',
        name='lidar_debugger',
        parameters=[{'cloud_topic': '/robot_01/scan/points'}], # Match the bridge topic
        output='screen'
    )

    return LaunchDescription([
        gz_resource_path,
        gz_sim,
        clock_bridge,
        spawn_robot,
        debug_node
    ])