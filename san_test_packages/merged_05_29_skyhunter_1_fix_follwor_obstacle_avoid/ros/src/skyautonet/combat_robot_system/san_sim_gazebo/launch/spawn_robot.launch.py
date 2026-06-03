# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

import os
import xacro
from ament_index_python.packages import get_package_share_directory
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.launch_context import LaunchContext
from launch.launch_description import LaunchDescription
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

pkg_tin3_description = get_package_share_directory("san_description")
pkg_tin3_gz_simulation = get_package_share_directory("san_sim_gazebo")

def spawn_robot(
    context: LaunchContext,
    namespace: LaunchConfiguration,
    robot_name: LaunchConfiguration,
    x: LaunchConfiguration,
    y: LaunchConfiguration,
    z: LaunchConfiguration,
    lidar_mode: LaunchConfiguration,
    use_sim_time: LaunchConfiguration,
):
    # Callers (sim.launch.py / swarm_sim.launch.py) pass the namespace
    # with a leading "/" (e.g. "/robot_1"); strip it so robot_ns is the
    # bare token used for frame/topic prefixes and the node namespace
    # (the xacro builds topic_prefix as "/" + ns, so a leading "/" here
    # would yield a double slash).
    robot_ns = context.perform_substitution(namespace).lstrip("/")
    name_value = context.perform_substitution(robot_name)
    x_value = context.perform_substitution(x)
    y_value = context.perform_substitution(y)
    z_value = context.perform_substitution(z)
    roll_value = "0.0"
    pitch_value = "0.0"
    yaw_value = "0.0"
    lidar_mode_value = context.perform_substitution(lidar_mode)
    use_sim_time_value = context.perform_substitution(use_sim_time).lower() == 'true'

    robot_desc = xacro.process(
        os.path.join(pkg_tin3_description, "urdf", "san_robot.urdf.xacro"),
        mappings={"robot_ns": robot_ns, "lidar_mode": lidar_mode_value},
    )

    # Gazebo model name: prefer the explicit robot_name arg, fall back to
    # the namespace then a default — so spawning never depends on an
    # external params file (config/robot_params.yaml is not in the repo).
    robot_gazebo_name = name_value or robot_ns or "san_combat_robot"
    node_name_prefix = (robot_ns + "_") if robot_ns else ""

    robot_state_publisher = Node(
        namespace=robot_ns,
        package="robot_state_publisher",
        executable="robot_state_publisher",
        name="robot_state_publisher",
        output="both",
        parameters=[
            {"use_sim_time": use_sim_time_value},
            {"robot_description": robot_desc},
            {"frame_prefix": robot_ns + "/" if robot_ns else ""},
        ],
        remappings=[("/tf", "/tf"), ("/tf_static", "/tf_static")],
    )

    spawn_entity = Node(
        namespace=robot_ns,
        package="ros_gz_sim",
        executable="create",
        name="ros_gz_sim_create",
        output="both",
        arguments=[
            "-topic", "robot_description", "-name", robot_gazebo_name,
            "-x", str(x_value), "-y", str(y_value), "-z", str(z_value),
            "-R", str(roll_value), "-P", str(pitch_value), "-Y", str(yaw_value),
        ],
    )

    if robot_ns == "":
        topic_bridge = Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="parameter_bridge",
            parameters=[
                {"config_file": os.path.join(pkg_tin3_gz_simulation, "config", "ros_gz_bridge.yaml")},
                {"use_sim_time": use_sim_time_value},
                {"qos_overrides./tf_static.publisher.durability": "transient_local"},
            ],
            output="screen",
        )
    else:
        bridge_prefix = "/" + robot_ns
        topic_bridge = Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name=node_name_prefix + "parameter_bridge",
            arguments=[
                bridge_prefix + "/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist",
                bridge_prefix + "/gimbal/pan_cmd@std_msgs/msg/Float64]gz.msgs.Double",
                bridge_prefix + "/gimbal/tilt_cmd@std_msgs/msg/Float64]gz.msgs.Double",
                bridge_prefix + "/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry",
                bridge_prefix + "/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V",
                bridge_prefix + "/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model",
                bridge_prefix + "/gps/fix@sensor_msgs/msg/NavSatFix[gz.msgs.NavSat", 
                bridge_prefix + "/imu/data@sensor_msgs/msg/Imu[gz.msgs.IMU",
                bridge_prefix + "/scan/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked",
                bridge_prefix + "/rgb_camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
                bridge_prefix + "/ir_camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
                bridge_prefix + "/front_camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
                bridge_prefix + "/rear_camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
            ],
            remappings=[(bridge_prefix + "/tf", "/tf")],
            parameters=[
                {"use_sim_time": use_sim_time_value},
                {"qos_overrides./tf.publisher.reliability": "reliable"},
                {"qos_overrides./tf_static.publisher.durability": "transient_local"},
            ],
            output="screen",
        )

    return [robot_state_publisher, spawn_entity, topic_bridge]


def generate_launch_description():
    return LaunchDescription([
        # Arg names match what sim.launch.py / swarm_sim.launch.py pass.
        DeclareLaunchArgument("namespace", default_value=""),
        DeclareLaunchArgument("robot_name", default_value=""),
        DeclareLaunchArgument("x", default_value="0.0"),
        DeclareLaunchArgument("y", default_value="0.0"),
        DeclareLaunchArgument("z", default_value="0.5"),
        DeclareLaunchArgument("lidar_mode", default_value="full"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        OpaqueFunction(function=spawn_robot, args=[
            LaunchConfiguration("namespace"),
            LaunchConfiguration("robot_name"),
            LaunchConfiguration("x"),
            LaunchConfiguration("y"),
            LaunchConfiguration("z"),
            LaunchConfiguration("lidar_mode"),
            LaunchConfiguration("use_sim_time"),
        ]),
    ])