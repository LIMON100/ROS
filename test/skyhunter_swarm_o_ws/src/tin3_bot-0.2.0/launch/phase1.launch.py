import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():
    pkg_ros = get_package_share_directory("ros_gz_sim")
    pkg_tin = get_package_share_directory("tin3_bot")
    
    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH", 
        value=[os.environ.get("GZ_SIM_RESOURCE_PATH", ""), ":", os.path.dirname(pkg_tin)]
    )

    world = os.path.join(pkg_tin, "worlds", "obstacle_world.sdf")
    
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_ros, "launch", "gz_sim.launch.py")), 
        launch_arguments={"gz_args": f"-r {world}"}.items()
    )
    
    spawn_robot = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_tin, "launch", "spawn_robot.launch.py")),
        launch_arguments={
            "robot_ns": "robot_01", 
            "x": "0.0", "y": "0.0", "z": "0.5", 
            "use_ekf": "true", "lidar_mode": "full"
        }.items()
    )

    pc_to_laser = Node(
        package='pointcloud_to_laserscan', 
        executable='pointcloud_to_laserscan_node', 
        name='pc_to_laser',
        remappings=[('cloud_in', '/robot_01/scan/points'), ('scan', '/robot_01/scan')],
        parameters=[{
            'target_frame': 'robot_01/lidar_link', 
            'range_max': 20.0, 'use_inf': True, 'use_sim_time': True,
            'min_height': 0.15, 'max_height': 1.0,
        }]
    )

    slam = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        namespace='robot_01', 
        parameters=[{
            'use_sim_time': True,
            'base_frame': 'robot_01/base_footprint',
            'odom_frame': 'robot_01/odom',
            'map_frame': 'map',  # Ensure this is exactly 'map'
            'mode': 'mapping',
            'scan_topic': '/robot_01/scan' # Use absolute topic path
        }]
    )

    return LaunchDescription([
        gz_resource_path, 
        gz_sim, 
        spawn_robot, 
        Node(package="ros_gz_bridge", executable="parameter_bridge", arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"]),
        pc_to_laser, 
        slam
    ])