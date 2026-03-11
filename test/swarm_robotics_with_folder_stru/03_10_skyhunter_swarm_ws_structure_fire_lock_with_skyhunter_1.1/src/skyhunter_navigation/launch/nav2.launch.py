# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import DeclareLaunchArgument
# from launch.conditions import IfCondition
# from launch.substitutions import LaunchConfiguration, PythonExpression
# from launch_ros.actions import Node
# from nav2_common.launch import RewrittenYaml  # <--- CRITICAL IMPORT

# def generate_launch_description():
#     pkg_tin3_navigation = get_package_share_directory("skyhunter_navigation")
    
#     # 1. Configuration Variables
#     nav2_params_file = LaunchConfiguration("params_file")
#     use_rviz = LaunchConfiguration("use_rviz")
#     namespace = LaunchConfiguration('namespace')
#     use_sim_time = LaunchConfiguration('use_sim_time')
#     autostart = LaunchConfiguration('autostart')

#     declare_params = DeclareLaunchArgument(
#         "params_file", default_value=os.path.join(pkg_tin3_navigation, "config", "nav2_params.yaml"))
#     declare_use_rviz = DeclareLaunchArgument("use_rviz", default_value="true")
#     declare_namespace = DeclareLaunchArgument("namespace", default_value="")
#     declare_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
#     declare_autostart = DeclareLaunchArgument("autostart", default_value="true")

#     # 2. Dynamic Frame Generation
#     # If namespace is 'SH_02', frames become 'SH_02/odom', 'SH_02/base_footprint'
#     # If namespace is empty, they stay 'odom', 'base_footprint'
    
#     # We use PythonExpression to create the correct string
#     odom_frame = PythonExpression(["'", namespace, "/odom' if '", namespace, "' != '' else 'odom'"])
#     base_frame = PythonExpression(["'", namespace, "/base_footprint' if '", namespace, "' != '' else 'base_footprint'"])
#     scan_topic = PythonExpression(["'/", namespace, "/scan/points' if '", namespace, "' != '' else '/scan/points'"])

#     # 3. Create the RewrittenYaml
#     # This magically replaces the keys in your YAML file with the variables above
#     configured_params = RewrittenYaml(
#         source_file=nav2_params_file,
#         root_key=namespace,
#         param_rewrites={
#             'use_sim_time': use_sim_time,
#             'autostart': autostart,
#             'global_frame': 'map', # Global costmap always uses map
#             'robot_base_frame': base_frame,
#             'odom_frame': odom_frame,
#             'topic': scan_topic,
#             'scan_topic': scan_topic
#         },
#         convert_types=True
#     )

#     # 4. Nodes (Using configured_params instead of the raw file)
    
#     # TF Publisher (Needed for the empty/namespaced logic)
#     static_tf_map_odom = Node(
#         package="tf2_ros",
#         executable="static_transform_publisher",
#         name="static_tf_map_odom",
#         namespace=namespace,
#         arguments=["0", "0", "0", "0", "0", "0", "map", odom_frame],
#         parameters=[{"use_sim_time": use_sim_time}],
#     )

#     controller_server = Node(
#         package="nav2_controller", executable="controller_server",
#         name="controller_server", namespace=namespace, output="screen",
#         parameters=[configured_params], # <--- USING REWRITTEN YAML
#         remappings=[("cmd_vel", "cmd_vel_nav")],
#     )

#     planner_server = Node(
#         package="nav2_planner", executable="planner_server",
#         name="planner_server", namespace=namespace, output="screen",
#         parameters=[configured_params],
#     )

#     smoother_server = Node(
#         package="nav2_smoother", executable="smoother_server",
#         name="smoother_server", namespace=namespace, output="screen",
#         parameters=[configured_params],
#     )

#     behavior_server = Node(
#         package="nav2_behaviors", executable="behavior_server",
#         name="behavior_server", namespace=namespace, output="screen",
#         parameters=[configured_params],
#     )

#     bt_navigator = Node(
#         package="nav2_bt_navigator", executable="bt_navigator",
#         name="bt_navigator", namespace=namespace, output="screen",
#         parameters=[configured_params],
#     )

#     waypoint_follower = Node(
#         package="nav2_waypoint_follower", executable="waypoint_follower",
#         name="waypoint_follower", namespace=namespace, output="screen",
#         parameters=[configured_params],
#     )

#     velocity_smoother = Node(
#         package="nav2_velocity_smoother", executable="velocity_smoother",
#         name="velocity_smoother", namespace=namespace, output="screen",
#         parameters=[configured_params],
#         remappings=[("cmd_vel", "cmd_vel_nav"), ("cmd_vel_smoothed", "cmd_vel")],
#     )

#     # Lifecycle Manager
#     lifecycle_manager_navigation = Node(
#         package="nav2_lifecycle_manager", executable="lifecycle_manager",
#         name="lifecycle_manager",
#         namespace=namespace,
#         output="screen",
#         parameters=[
#             {"use_sim_time": use_sim_time},
#             {"autostart": autostart},
#             {"bond_timeout": 60.0}, # High timeout for safety
#             {"node_names": ["controller_server", "smoother_server", "planner_server", 
#                             "behavior_server", "bt_navigator", "waypoint_follower", "velocity_smoother"]},
#         ],
#     )

#     rviz_node = Node(
#         package="rviz2", executable="rviz2", name="rviz2",
#         arguments=["-d", os.path.join(pkg_tin3_navigation, "rviz", "nav2_config.rviz")],
#         parameters=[{"use_sim_time": use_sim_time}], output="screen", condition=IfCondition(use_rviz),
#     )

#     return LaunchDescription([
#         declare_params, declare_use_rviz, declare_namespace, declare_sim_time, declare_autostart,
#         static_tf_map_odom, controller_server, planner_server, smoother_server,
#         behavior_server, bt_navigator, waypoint_follower, velocity_smoother,
#         lifecycle_manager_navigation, rviz_node,
#     ])



import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PythonExpression
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    pkg_tin3_navigation = get_package_share_directory("skyhunter_navigation")
    
    # 1. Configuration Variables
    nav2_params_file = LaunchConfiguration("params_file")
    use_rviz = LaunchConfiguration("use_rviz")
    namespace = LaunchConfiguration('namespace')
    use_sim_time = LaunchConfiguration('use_sim_time')
    autostart = LaunchConfiguration('autostart')

    declare_params = DeclareLaunchArgument(
        "params_file", default_value=os.path.join(pkg_tin3_navigation, "config", "nav2_params.yaml"))
    declare_use_rviz = DeclareLaunchArgument("use_rviz", default_value="true")
    declare_namespace = DeclareLaunchArgument("namespace", default_value="")
    declare_sim_time = DeclareLaunchArgument("use_sim_time", default_value="true")
    declare_autostart = DeclareLaunchArgument("autostart", default_value="true")

    odom_frame = PythonExpression(["'", namespace, "/odom' if '", namespace, "' != '' else 'odom'"])
    base_frame = PythonExpression(["'", namespace, "/base_footprint' if '", namespace, "' != '' else 'base_footprint'"])
    scan_topic = PythonExpression(["'/", namespace, "/scan/points' if '", namespace, "' != '' else '/scan/points'"])

    configured_params = RewrittenYaml(
        source_file=nav2_params_file,
        root_key=namespace,
        param_rewrites={
            'use_sim_time': use_sim_time,
            'autostart': autostart,
            'global_frame': 'map', 
            'robot_base_frame': base_frame,
            'odom_frame': odom_frame,
            'topic': scan_topic,
            'scan_topic': scan_topic
        },
        convert_types=True
    )

    # Note: static_tf_map_odom has been REMOVED from here to prevent TF conflicts.
    # It is now exclusively handled by spawn_robot.launch.py.

    controller_server = Node(
        package="nav2_controller", executable="controller_server",
        name="controller_server", namespace=namespace, output="screen",
        parameters=[configured_params], remappings=[("cmd_vel", "cmd_vel_nav")],
    )

    planner_server = Node(
        package="nav2_planner", executable="planner_server",
        name="planner_server", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    smoother_server = Node(
        package="nav2_smoother", executable="smoother_server",
        name="smoother_server", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    behavior_server = Node(
        package="nav2_behaviors", executable="behavior_server",
        name="behavior_server", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    bt_navigator = Node(
        package="nav2_bt_navigator", executable="bt_navigator",
        name="bt_navigator", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    waypoint_follower = Node(
        package="nav2_waypoint_follower", executable="waypoint_follower",
        name="waypoint_follower", namespace=namespace, output="screen",
        parameters=[configured_params],
    )

    velocity_smoother = Node(
        package="nav2_velocity_smoother", executable="velocity_smoother",
        name="velocity_smoother", namespace=namespace, output="screen",
        parameters=[configured_params],
        remappings=[("cmd_vel", "cmd_vel_nav"), ("cmd_vel_smoothed", "cmd_vel")],
    )

    lifecycle_manager_navigation = Node(
        package="nav2_lifecycle_manager", executable="lifecycle_manager",
        name="lifecycle_manager", namespace=namespace, output="screen",
        parameters=[
            {"use_sim_time": use_sim_time},
            {"autostart": autostart},
            {"bond_timeout": 60.0},
            {"node_names":["controller_server", "smoother_server", "planner_server", 
                            "behavior_server", "bt_navigator", "waypoint_follower", "velocity_smoother"]},
        ],
    )

    rviz_node = Node(
        package="rviz2", executable="rviz2", name="rviz2",
        arguments=["-d", os.path.join(pkg_tin3_navigation, "rviz", "nav2_config.rviz")],
        parameters=[{"use_sim_time": use_sim_time}], output="screen", condition=IfCondition(use_rviz),
    )

    return LaunchDescription([
        declare_params, declare_use_rviz, declare_namespace, declare_sim_time, declare_autostart,
        controller_server, planner_server, smoother_server,
        behavior_server, bt_navigator, waypoint_follower, velocity_smoother,
        lifecycle_manager_navigation, rviz_node,
    ])