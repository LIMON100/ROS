import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    IncludeLaunchDescription,
    SetEnvironmentVariable,
)
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    # ==================== Package Paths ====================
    # Define paths to the necessary packages
    pkg_tin3_gz_simulation = get_package_share_directory("tin3_gz_simulation")
    pkg_tin3_gz_worlds = get_package_share_directory("tin3_gz_worlds")
    pkg_tin3_description = get_package_share_directory("tin3_description")
    pkg_ros_gz_sim = get_package_share_directory("ros_gz_sim")

    # ==================== Environment ====================
    # Set the Gazebo resource path to find worlds and robot meshes
    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[
            os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
            ":",
            pkg_tin3_description,  # For robot meshes (.dae, .stl)
            ":",
            pkg_tin3_gz_worlds,    # For world files (.sdf)
        ],
    )

    # ==================== Launch Arguments ====================
    # Allows you to specify the world file from the command line
    world_arg = DeclareLaunchArgument(
        "world",
        default_value=os.path.join(pkg_tin3_gz_worlds, "worlds", "obstacle_world.sdf"),
        description="Full path to the world file to load",
    )

    # Allows you to change the LiDAR's point density
    lidar_mode_arg = DeclareLaunchArgument(
        "lidar_mode",
        default_value="full",
        description="LiDAR resolution: full, half, low, none",
    )

    # Allows you to set the robot's starting position and orientation
    pose_arg = DeclareLaunchArgument(
        "pose",
        default_value="0.0 0.0 0.5 0.0 0.0 0.0",
        description="Initial pose of the robot as 'x y z roll pitch yaw'",
    )

    # ==================== Gazebo Simulation ====================
    # Start the Gazebo server and client
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, "launch", "gz_sim.launch.py")
        ),
        launch_arguments={"gz_args": LaunchConfiguration("world")}.items(),
    )

    # ==================== Clock Bridge ====================
    # Bridge the Gazebo clock to ROS 2 so `use_sim_time` works
    clock_bridge = Node(
        package="ros_gz_bridge",
        executable="parameter_bridge",
        arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock]"],
        output="screen",
    )

    # ==================== Spawn Robot ====================
    # Include the spawn_robot launch file for our single robot
    spawn_robot = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_tin3_gz_simulation, "launch", "spawn_robot.launch.py")
        ),
        launch_arguments={
            "robot_ns": "robot_01",
            "lidar_mode": LaunchConfiguration("lidar_mode"),
            "pose": LaunchConfiguration("pose"),
            # EKF is enabled by default in spawn_robot, so no need to pass it
        }.items(),
    )

    # ==================== PointCloud to LaserScan ====================
    # Converts the 3D point cloud to a 2D laser scan for SLAM
    pc_to_laser = Node(
        package="pointcloud_to_laserscan",
        executable="pointcloud_to_laserscan_node",
        name="pc_to_laser",
        remappings=[
            ("cloud_in", "/robot_01/scan/points"),
            ("scan", "/robot_01/scan"),
        ],
        parameters=[
            {
                "target_frame": "robot_01/lidar_link",
                "range_max": 20.0,
                "use_inf": True,
                "use_sim_time": True,
                "min_height": 0.15,
                "max_height": 1.0,
            }
        ],
    )

    # ==================== SLAM ====================
    # Run the SLAM node in the global namespace to produce a global /map
    slam = Node(
        package="slam_toolbox",
        executable="async_slam_toolbox_node",
        name="slam_toolbox",
        output="screen",
        parameters=[
            {
                "use_sim_time": True,
                "base_frame": "robot_01/base_footprint",
                "odom_frame": "robot_01/odom",
                "map_frame": "map",
                "mode": "mapping",
            }
        ],
        # Remap the global SLAM node to listen to the robot's namespaced scan topic
        remappings=[("/scan", "/robot_01/scan")],
    )

    # ==================== Launch Description Assembly ====================
    return LaunchDescription(
        [
            gz_resource_path,
            world_arg,
            lidar_mode_arg,
            pose_arg,
            gz_sim,
            clock_bridge,
            spawn_robot,
            pc_to_laser,
            slam,
        ]
    )