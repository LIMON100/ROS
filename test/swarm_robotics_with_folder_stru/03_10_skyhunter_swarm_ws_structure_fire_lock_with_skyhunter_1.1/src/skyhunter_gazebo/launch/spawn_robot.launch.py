# import os

# from ament_index_python.packages import get_package_share_directory
# from launch.actions import DeclareLaunchArgument, OpaqueFunction
# from launch.launch_context import LaunchContext
# from launch.launch_description import LaunchDescription
# from launch.substitutions import LaunchConfiguration
# from launch_ros.actions import Node
# import xacro

# # ==================== Package Paths ====================
# pkg_tin3_description = get_package_share_directory("skyhunter_description")
# pkg_tin3_gz_simulation = get_package_share_directory("skyhunter_gazebo")


# def spawn_robot(
#     context: LaunchContext,
#     namespace: LaunchConfiguration,
#     pose: LaunchConfiguration,
#     lidar_mode: LaunchConfiguration,
#     use_sim_time: LaunchConfiguration,
# ):
#     robot_ns = context.perform_substitution(namespace)
#     pose_str = context.perform_substitution(pose)
#     lidar_mode_value = context.perform_substitution(lidar_mode)
#     use_sim_time_value = context.perform_substitution(use_sim_time).lower() == 'true'
#     # Parse "x y z" string
#     pose_parts = pose_str.split()
#     x_value = pose_parts[0] if len(pose_parts) > 0 else "0.0"
#     y_value = pose_parts[1] if len(pose_parts) > 1 else "0.0"
#     z_value = pose_parts[2] if len(pose_parts) > 2 else "0.5"
#     roll_value = pose_parts[3] if len(pose_parts) > 3 else "0.0"
#     pitch_value = pose_parts[4] if len(pose_parts) > 4 else "0.0"
#     yaw_value = pose_parts[5] if len(pose_parts) > 5 else "0.0"

#     robot_desc = xacro.process(
#         os.path.join(pkg_tin3_description, "urdf", "robot.urdf.xacro"),
#         mappings={"robot_ns": robot_ns, "lidar_mode": lidar_mode_value},
#     )

#     if robot_ns == "":
#         robot_gazebo_name = "skyhunter_bot"
#         node_name_prefix = ""
#     else:
#         robot_gazebo_name = "skyhunter_bot" + robot_ns
#         node_name_prefix = robot_ns + "_"

#     # Robot state publisher
#     robot_state_publisher = Node(
#         namespace=robot_ns,
#         package="robot_state_publisher",
#         executable="robot_state_publisher",
#         name="robot_state_publisher",
#         output="both",
#         parameters=[
#             {"use_sim_time": use_sim_time_value},
#             {"robot_description": robot_desc},
#             {"frame_prefix": robot_ns + "/" if robot_ns else ""},
#         ],
#         remappings=[
#             ("/tf", "/tf"),
#             ("/tf_static", "/tf_static"),
#         ],
#     )

#     # Spawn robot in Gazebo
#     spawn_entity = Node(
#         namespace=robot_ns,
#         package="ros_gz_sim",
#         executable="create",
#         name="ros_gz_sim_create",
#         output="both",
#         arguments=[
#             "-topic", "robot_description",
#             "-name", robot_gazebo_name,
#             "-x", x_value,
#             "-y", y_value,
#             "-z", z_value,
#             "-R", roll_value,
#             "-P", pitch_value,
#             "-Y", yaw_value,
#         ],
#     )

#     # ==================== ROS-Gazebo Bridge ====================
#     if robot_ns == "":
#         # Single robot - use YAML config
#         topic_bridge = Node(
#             package="ros_gz_bridge",
#             executable="parameter_bridge",
#             name="parameter_bridge",
#             parameters=[
#                 {"config_file": os.path.join(pkg_tin3_gz_simulation, "config", "ros_gz_bridge.yaml")},
#                 {"use_sim_time": use_sim_time_value},
#                 {"qos_overrides./tf_static.publisher.durability": "transient_local"},
#             ],
#             output="screen",
#         )
#     else:
#         # Multi-robot - use arguments with namespace
#         bridge_prefix = "/" + robot_ns
#         topic_bridge = Node(
#             package="ros_gz_bridge",
#             executable="parameter_bridge",
#             name=node_name_prefix + "parameter_bridge",
#             arguments=[
#                 bridge_prefix + "/cmd_vel@geometry_msgs/msg/Twist]gz.msgs.Twist",
#                 bridge_prefix + "/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V",
#                 bridge_prefix + "/odom@nav_msgs/msg/Odometry[gz.msgs.Odometry",
#                 bridge_prefix + "/scan/points@sensor_msgs/msg/PointCloud2[gz.msgs.PointCloudPacked",
#                 bridge_prefix + "/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model",

#                 # --- ADD THESE LINES TO FIX THE CAMERA ---
#                 bridge_prefix + "/rgb_camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
#                 bridge_prefix + "/ir_camera/image_raw@sensor_msgs/msg/Image[gz.msgs.Image",
#                 # -
#             ],
#             remappings=[
#                 (bridge_prefix + "/tf", "/tf"),
#             ],
#             parameters=[
#                 {"use_sim_time": use_sim_time_value},
#                 # --- CRITICAL FIX: FORCE RELIABLE TRANSFORMS ---
#                 {"qos_overrides./tf.publisher.reliability": "reliable"},
#                 {"qos_overrides./tf_static.publisher.durability": "transient_local"},
#             ],
#             output="screen",
#         )

#     return [robot_state_publisher, spawn_entity, topic_bridge]


# def generate_launch_description():
#     return LaunchDescription([
#         DeclareLaunchArgument("robot_ns", default_value="", description="Robot namespace"),
#         DeclareLaunchArgument("pose", default_value="0 0 0.5", description="Spawn pose 'x y z' or 'x y z R P Y'"),
#         DeclareLaunchArgument("lidar_mode", default_value="full", description="LiDAR: full, half, low"),
#         DeclareLaunchArgument("use_sim_time", default_value="true"),
#         OpaqueFunction(
#             function=spawn_robot,
#             args=[
#                 LaunchConfiguration("robot_ns"),
#                 LaunchConfiguration("pose"),
#                 LaunchConfiguration("lidar_mode"),
#                 LaunchConfiguration("use_sim_time"),
#             ],
#         ),
#     ])


import os
import xacro
import yaml
from ament_index_python.packages import get_package_share_directory
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.launch_context import LaunchContext
from launch.launch_description import LaunchDescription
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

pkg_tin3_description = get_package_share_directory("skyhunter_description")
pkg_tin3_gz_simulation = get_package_share_directory("skyhunter_gazebo")

def spawn_robot(
    context: LaunchContext,
    namespace: LaunchConfiguration,
    pose: LaunchConfiguration,
    lidar_mode: LaunchConfiguration,
    use_sim_time: LaunchConfiguration,
):
    robot_ns = context.perform_substitution(namespace)
    pose_str = context.perform_substitution(pose)
    lidar_mode_value = context.perform_substitution(lidar_mode)
    use_sim_time_value = context.perform_substitution(use_sim_time).lower() == 'true'
    
    with open(os.path.join(pkg_tin3_gz_simulation, "config", "robot_params.yaml")) as f:
        robot_params = yaml.safe_load(f)
        ROBOT_PREFIX = robot_params["/**"]["ros__parameters"]["robot"]["prefix"]

    pose_parts = pose_str.split()
    x_value = pose_parts[0] if len(pose_parts) > 0 else "0.0"
    y_value = pose_parts[1] if len(pose_parts) > 1 else "0.0"
    z_value = pose_parts[2] if len(pose_parts) > 2 else "0.5"
    roll_value = pose_parts[3] if len(pose_parts) > 3 else "0.0"
    pitch_value = pose_parts[4] if len(pose_parts) > 4 else "0.0"
    yaw_value = pose_parts[5] if len(pose_parts) > 5 else "0.0"

    robot_desc = xacro.process(
        os.path.join(pkg_tin3_description, "urdf", "robot.urdf.xacro"),
        mappings={"robot_ns": robot_ns, "lidar_mode": lidar_mode_value},
    )

    single_robot_name = ROBOT_PREFIX.rstrip('_')

    if robot_ns == "":
        robot_gazebo_name = single_robot_name
        node_name_prefix = ""
    else:
        robot_gazebo_name = robot_ns
        node_name_prefix = robot_ns + "_"

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
                bridge_prefix + "/gps/fix@gps_msgs/msg/GPSFix[gz.msgs.NavSat",
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

    # CRITICAL FIX: Make sure BOTH Leader and Followers publish map -> odom
    odom_frame = robot_ns + '/odom' if robot_ns else 'odom'
    odom_to_world = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name=node_name_prefix + 'odom_to_world',
        arguments=[
            str(x_value), str(y_value), '0', 
            # str(x_value), str(y_value), str(z_value), 
            '0', '0', str(yaw_value),
            'map', odom_frame
        ],
        parameters=[{"use_sim_time": use_sim_time_value}],
    )

    return[robot_state_publisher, spawn_entity, topic_bridge, odom_to_world]

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument("robot_ns", default_value=""),
        DeclareLaunchArgument("pose", default_value="0 0 0.5"),
        DeclareLaunchArgument("lidar_mode", default_value="full"),
        DeclareLaunchArgument("use_sim_time", default_value="true"),
        OpaqueFunction(function=spawn_robot, args=[
            LaunchConfiguration("robot_ns"), LaunchConfiguration("pose"),
            LaunchConfiguration("lidar_mode"), LaunchConfiguration("use_sim_time"),
        ]),
    ])