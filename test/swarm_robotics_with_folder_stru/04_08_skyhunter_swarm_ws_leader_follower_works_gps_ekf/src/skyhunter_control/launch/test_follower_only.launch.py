"""
this launch file is designed to test the LIDAR processing and obstacle detection capabilities of a follower robot (SH_02) in Gazebo, 
without the formation control logic. It includes the necessary nodes to spawn the robot, run the Gazebo simulation, 
and process LIDAR data for obstacle detection.
"""

import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():
    pkg_gz = get_package_share_directory('tin3_gz_simulation')
    pkg_description = get_package_share_directory('tin3_description')
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')

    # Fix Gazebo Resource Path
    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[os.path.dirname(pkg_description), ":" + os.environ.get("GZ_SIM_RESOURCE_PATH", "")]
    )

    return LaunchDescription([
        gz_resource_path,
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')),
            launch_arguments={'gz_args': '-r obstacle_world.sdf'}.items(),
        ),
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(os.path.join(pkg_gz, 'launch', 'spawn_robot.launch.py')),
            launch_arguments={'robot_ns': 'robot_02', 'pose': '0 0 0.5 0 0 0', 'lidar_mode': 'half'}.items(),
        ),
        Node(package="ros_gz_bridge", executable="parameter_bridge",
             arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"]),
        # Launch the new processor
        Node(
            package='skyhunter_formation',
            executable='lidar_test_node', # Make sure this matches CMakeLists.txt
            output='screen'
        )
    ])