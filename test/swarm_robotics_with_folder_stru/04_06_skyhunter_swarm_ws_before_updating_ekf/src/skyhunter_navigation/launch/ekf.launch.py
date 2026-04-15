# # =============================================================================
# # Standalone robot_localization EKF node.
# # Fuses: odom (wheel odometry) + IMU
# # Publishes: /odom_filtered + TF (odom → base_link)
# # =============================================================================

# import os

# from ament_index_python.packages import get_package_share_directory

# from launch import LaunchDescription
# from launch.actions import DeclareLaunchArgument
# from launch.substitutions import LaunchConfiguration
# from launch_ros.actions import Node


# def generate_launch_description():
#     pkg_tin3_navigation = get_package_share_directory("skyhunter_navigation")
    

#     ekf_config = os.path.join(pkg_tin3_navigation, "config", "ekf.yaml")

#     ekf_node = Node(
#         package="robot_localization",
#         executable="ekf_node",
#         name="ekf_filter_node",
#         output="screen",
#         parameters=[ekf_config],
#         remappings=[
#             ("odometry/filtered", "odom_filtered"),
#         ],
#     )

#     return LaunchDescription(
#         [

#             ekf_node,
           
#         ]
#     )


import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PythonExpression
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    pkg_nav = get_package_share_directory("skyhunter_navigation")
    ekf_config = os.path.join(pkg_nav, "config", "ekf.yaml")

    namespace_arg = DeclareLaunchArgument('namespace', default_value='')
    namespace = LaunchConfiguration('namespace')

    # Dynamically inject the namespace into the frames
    odom_frame = PythonExpression(["'", namespace, "/odom' if '", namespace, "' != '' else 'odom'"])
    base_frame = PythonExpression(["'", namespace, "/base_footprint' if '", namespace, "' != '' else 'base_footprint'"])

    configured_params = RewrittenYaml(
        source_file=ekf_config,
        root_key=namespace,
        param_rewrites={
            'odom_frame': odom_frame,
            'base_link_frame': base_frame,
            'world_frame': odom_frame,
        },
        convert_types=True
    )

    return LaunchDescription([
        namespace_arg,
        
        # 1. The EKF Node
        Node(
            package="robot_localization",
            executable="ekf_node", 
            name="ekf_filter_node",
            namespace=namespace,
            output="screen",
            parameters=[configured_params, {'use_sim_time': True}],
            remappings=[
                ("odometry/filtered", "odom_filtered"),
            ],
        ),
        

        # 2. The NavSat Transform Node
        Node(
            package="robot_localization",
            executable="navsat_transform_node",
            name="navsat_transform",
            namespace=namespace,
            output="screen",
            parameters=[configured_params, {'use_sim_time': True}],
            remappings=[
                ('gps/fix', 'gps/fix'),
                ('imu', 'imu/data'),
                ('odometry/gps', 'odometry/gps'),
                ('odometry/filtered', 'odom'), 
            ],
        )
    ])