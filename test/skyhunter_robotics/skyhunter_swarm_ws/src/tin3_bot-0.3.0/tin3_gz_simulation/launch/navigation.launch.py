import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    pkg_tin = get_package_share_directory("tin3_gz_simulation")
    nav2_config = os.path.join(pkg_tin, 'config', 'nav2_params.yaml')

    configured_params = RewrittenYaml(
        source_file=nav2_config,
        root_key='robot_01', 
        param_rewrites={},
        convert_types=True
    )

    # Common parameters for all Nav2 nodes
    nav2_params = [configured_params]

    controller = Node(
        package='nav2_controller',
        executable='controller_server',
        name='controller_server',
        namespace='robot_01',
        output='screen',
        parameters=nav2_params,
        remappings=[('/cmd_vel', '/robot_01/cmd_vel'), ('/odom', '/robot_01/odom')]
    )

    planner = Node(
        package='nav2_planner',
        executable='planner_server',
        name='planner_server',
        namespace='robot_01',
        output='screen',
        parameters=nav2_params,
        remappings=[('/map', '/map')] # <--- ADD THIS
    )

    recoveries = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        name='behavior_server',
        namespace='robot_01',
        output='screen',
        parameters=nav2_params
    )

    bt_nav = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        name='bt_navigator',
        namespace='robot_01',
        output='screen',
        parameters=nav2_params
    )

    lifecycle = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_nav',
        namespace='robot_01',
        output='screen',
        parameters=[{'use_sim_time': True, 'autostart': True, 
                     'node_names': ['controller_server', 'planner_server', 'behavior_server', 'bt_navigator']}]
    )

    return LaunchDescription([
        controller, 
        planner, 
        recoveries, 
        bt_nav, 
        lifecycle
    ])