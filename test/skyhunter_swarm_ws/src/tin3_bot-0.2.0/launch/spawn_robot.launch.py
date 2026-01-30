# Spawn robot launch file for tin3_bot
# Can be used standalone or called from main launch with namespace for multi-robot

import os

from ament_index_python.packages import get_package_share_directory
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.conditions import IfCondition
from launch.launch_context import LaunchContext
from launch.launch_description import LaunchDescription
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
import xacro


def spawn_robot(
    context: LaunchContext,
    namespace: LaunchConfiguration,
    x_pos: LaunchConfiguration,
    y_pos: LaunchConfiguration,
    z_pos: LaunchConfiguration,
    use_ekf: LaunchConfiguration,
    lidar_mode: LaunchConfiguration,
):
    pkg_project_description = get_package_share_directory("tin3_bot")
    robot_ns = context.perform_substitution(namespace)
    x_value = context.perform_substitution(x_pos)
    y_value = context.perform_substitution(y_pos)
    z_value = context.perform_substitution(z_pos)
    ekf_enabled = context.perform_substitution(use_ekf).lower() == "true"
    lidar_mode_value = context.perform_substitution(lidar_mode)

    robot_desc = xacro.process(
        os.path.join(
            pkg_project_description,
            "urdf",
            "robot.urdf.xacro",
        ),
        mappings={"robot_ns": robot_ns,"lidar_mode": lidar_mode_value, },
    )

    if robot_ns == "":
        robot_gazebo_name = "tin3_bot"
        node_name_prefix = ""
    else:
        robot_gazebo_name = "tin3_bot_" + robot_ns
        node_name_prefix = robot_ns + "_"

    # Launch robot state publisher node
    robot_state_publisher = Node(
        namespace=robot_ns,
        package="robot_state_publisher",
        executable="robot_state_publisher",
        name="robot_state_publisher",
        output="both",
        parameters=[
            {"use_sim_time": True},
            {"robot_description": robot_desc},
            {"frame_prefix": robot_ns + "/" if robot_ns else ""},
        ],
        remappings=[
        ("/tf", "/tf"),
        ("/tf_static", "/tf_static"),
             ]
    )

    # Spawn robot inside simulation
    spawn_entity = Node(
        namespace=robot_ns,
        package="ros_gz_sim",
        executable="create",
        name="ros_gz_sim_create",
        output="both",
        arguments=[
            "-topic",
            "robot_description",
            "-name",
            robot_gazebo_name,
            "-x",
            x_value,
            "-y",
            y_value,
            "-z",
            z_value,
        ],
    )
    if robot_ns == "":
        topic_bridge = Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name="parameter_bridge",
            parameters=[
                {"config_file": os.path.join(pkg_project_description, "config", "ros_gz_bridge.yaml")},
                {"qos_overrides./tf_static.publisher.durability": "transient_local"},
            ],
            output="screen",
        )
    else:
        # Multi-robot - use arguments with namespace
        bridge_prefix = "/" + robot_ns
        topic_bridge = Node(
            package="ros_gz_bridge",
            executable="parameter_bridge",
            name=node_name_prefix + "parameter_bridge",
            arguments=[
                # Commands (ROS → Gazebo)
                bridge_prefix + "/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist",
                bridge_prefix + "/gimbal/pan_cmd@std_msgs/msg/Float64]gz.msgs.Double",
                bridge_prefix + "/gimbal/tilt_cmd@std_msgs/msg/Float64]gz.msgs.Double",
                # Sensor data (Gazebo → ROS)
                bridge_prefix + "/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry",
                bridge_prefix + "/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V",
                bridge_prefix + "/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model",
                bridge_prefix + "/gps/fix@gps_msgs/msg/GPSFix[gz.msgs.NavSat",
                bridge_prefix + "/imu/data@sensor_msgs/msg/Imu[gz.msgs.IMU",
                bridge_prefix + "/scan/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked",
                bridge_prefix + "/camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
                bridge_prefix + "/ir/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
                
            ],
            remappings=[
                (bridge_prefix + "/tf", "/tf"),  
            ],
            parameters=[
                {"qos_overrides./tf_static.publisher.durability": "transient_local"},
            ],
            output="screen",
        )
    
    # Base nodes (always launched)
    nodes = [
        robot_state_publisher,
        spawn_entity,
        topic_bridge,
    ]
    # EKF Node (robot_localization) - only if enabled
    if ekf_enabled:
        ekf_node = Node(
            package="robot_localization",
            executable="ekf_node",
            name="ekf_filter_node",
            namespace=robot_ns,
            output="screen",
            parameters=[
                os.path.join(pkg_project_description, "config", "ekf.yaml"),
                {"use_sim_time": True},
                {"odom_frame": robot_ns + "/odom" if robot_ns else "odom"},
                {"base_link_frame": robot_ns + "/base_footprint" if robot_ns else "base_footprint"},
                {"world_frame": robot_ns + "/odom" if robot_ns else "odom"},
            ],
        )
        nodes.append(ekf_node)
    return nodes


def generate_launch_description():
    name_argument = DeclareLaunchArgument(
        "robot_ns",
        default_value="",
        description="Robot namespace",
    )
    x_argument = DeclareLaunchArgument(
        "x",
        default_value="0.0",
        description="Spawn position X",
    )
    y_argument = DeclareLaunchArgument(
        "y",
        default_value="0.0",
        description="Spawn position Y",
    )
    z_argument = DeclareLaunchArgument(
        "z",
        default_value="0.5",
        description="Spawn position Z",
    )
    ekf_argument = DeclareLaunchArgument(
        "use_ekf",
        default_value="false",
        description="Enable EKF sensor fusion (robot_localization)",
    )
    lidar_mode_argument = DeclareLaunchArgument(
        "lidar_mode",
        default_value="full",
        description="LiDAR resolution: full, half, low",
)
    namespace = LaunchConfiguration("robot_ns")
    x_pos = LaunchConfiguration("x")
    y_pos = LaunchConfiguration("y")
    z_pos = LaunchConfiguration("z")
    use_ekf = LaunchConfiguration("use_ekf")
    lidar_mode = LaunchConfiguration("lidar_mode")
 

    return LaunchDescription(
        [
            name_argument,
            x_argument,
            y_argument,
            z_argument,
            ekf_argument,
            lidar_mode_argument,
            OpaqueFunction(
                function=spawn_robot, args=[namespace, x_pos, y_pos, z_pos,use_ekf,lidar_mode]
            ),
        ]
    )
