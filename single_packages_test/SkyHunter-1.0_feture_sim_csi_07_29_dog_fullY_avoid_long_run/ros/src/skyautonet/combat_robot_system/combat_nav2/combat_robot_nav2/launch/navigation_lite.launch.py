"""Trimmed nav2 navigation bringup for the multi-robot sim.

Same as nav2_bringup/navigation_launch.py but drops the servers unused by this
stack (route_server, waypoint_follower, smoother_server, docking) to cut node count
and CPU when running N full nav2 stacks on one host. The cmd_vel chain is kept intact:
  controller_server (cmd_vel_nav) -> velocity_smoother (cmd_vel_smoothed)
  -> collision_monitor (cmd_vel, Twist for the gz bridge).
Kept lifecycle nodes: controller, planner, behavior, bt_navigator, velocity_smoother,
collision_monitor. Interface (namespace/params_file/use_sim_time/autostart) matches the
original so robot_bringup_sim can swap it in.
"""
import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, GroupAction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.descriptions import ParameterFile
from nav2_common.launch import RewrittenYaml


def generate_launch_description():
    namespace = LaunchConfiguration('namespace')
    use_sim_time = LaunchConfiguration('use_sim_time')
    autostart = LaunchConfiguration('autostart')
    params_file = LaunchConfiguration('params_file')

    remappings = [('/tf', 'tf'), ('/tf_static', 'tf_static')]

    lifecycle_nodes = [
        'controller_server',
        'planner_server',
        'behavior_server',
        'bt_navigator',
        'velocity_smoother',
        'collision_monitor',
    ]

    configured_params = ParameterFile(
        RewrittenYaml(
            source_file=params_file,
            root_key=namespace,
            param_rewrites={'autostart': autostart},
            convert_types=True,
        ),
        allow_substs=True,
    )

    # NOTE: nodes do NOT set namespace= — robot_bringup_sim runs this launch inside a
    # PushRosNamespace(ns) group already. namespace is used only for the param root_key.
    common = dict(output='screen', parameters=[configured_params])

    load_nodes = GroupAction(actions=[
        Node(package='nav2_controller', executable='controller_server',
             remappings=remappings + [('cmd_vel', 'cmd_vel_nav')], **common),
        Node(package='nav2_planner', executable='planner_server',
             remappings=remappings, **common),
        Node(package='nav2_behaviors', executable='behavior_server', name='behavior_server',
             remappings=remappings + [('cmd_vel', 'cmd_vel_nav')], **common),
        Node(package='nav2_bt_navigator', executable='bt_navigator', name='bt_navigator',
             remappings=remappings, **common),
        Node(package='nav2_velocity_smoother', executable='velocity_smoother',
             name='velocity_smoother',
             remappings=remappings + [('cmd_vel', 'cmd_vel_nav')], **common),
        Node(package='nav2_collision_monitor', executable='collision_monitor',
             name='collision_monitor', remappings=remappings, **common),
        Node(package='nav2_lifecycle_manager', executable='lifecycle_manager',
             name='lifecycle_manager_navigation', output='screen',
             parameters=[{'autostart': autostart}, {'node_names': lifecycle_nodes}]),
    ])

    return LaunchDescription([
        DeclareLaunchArgument('namespace', default_value=''),
        DeclareLaunchArgument('use_sim_time', default_value='true'),
        DeclareLaunchArgument('autostart', default_value='true'),
        DeclareLaunchArgument('params_file', default_value=''),
        load_nodes,
    ])
