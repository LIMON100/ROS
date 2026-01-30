
# Nav2 Compatibility Test Launch File

import os
import tempfile

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, OpaqueFunction
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def launch_nav2_with_namespace(context, *args, **kwargs):
    """Generate Nav2 nodes with namespace substitution at runtime."""
    
    # Get the actual namespace value
    robot_ns = LaunchConfiguration('robot_ns').perform(context)
    
    # Read the template file
    pkg_share = get_package_share_directory('tin3_bot')
    template_path = os.path.join(pkg_share, 'config', 'nav2_params.yaml')
    
    with open(template_path, 'r') as f:
        params_content = f.read()
    
    # Handle namespace substitution
    if robot_ns:
        # Multi-robot: robot_ns/frame
        params_content = params_content.replace('{robot_ns}/', f'{robot_ns}/')
        params_content = params_content.replace('{robot_ns}', robot_ns)
        odom_frame = f'{robot_ns}/odom'
        print(f"[nav2_test] Robot namespace: {robot_ns}")
    else:
        # Single robot: no prefix
        params_content = params_content.replace('{robot_ns}/', '')
        params_content = params_content.replace('{robot_ns}', '')
        odom_frame = 'odom'
        print("[nav2_test] Robot namespace: (none - single robot)")
    
    # Write to temporary file
    ns_label = robot_ns if robot_ns else 'single'
    temp_params_file = tempfile.NamedTemporaryFile(
        mode='w',
        suffix='.yaml',
        prefix=f'nav2_params_{ns_label}_',
        delete=False
    )
    temp_params_file.write(params_content)
    temp_params_file.close()
    
    print(f"[nav2_test] Generated params file: {temp_params_file.name}")
    
    # Static TF: map -> odom (or map -> {robot_ns}/odom)
    # This is needed because we don't have a map server / AMCL running
    static_tf_map_to_odom = Node(
        package='tf2_ros',
        executable='static_transform_publisher',
        name='map_to_odom_tf',
        arguments=[
            '--x', '0', '--y', '0', '--z', '0',
            '--roll', '0', '--pitch', '0', '--yaw', '0',
            '--frame-id', 'map',
            '--child-frame-id', odom_frame
        ],
        parameters=[{'use_sim_time': True}],
    )
    
    # Common node kwargs
    node_kwargs = {
        'output': 'screen',
        'parameters': [temp_params_file.name],
    }
    if robot_ns:
        node_kwargs['namespace'] = robot_ns
    
    # Controller Server
    controller_server = Node(
        package='nav2_controller',
        executable='controller_server',
        name='controller_server',
        remappings=[
            ('cmd_vel', 'cmd_vel'),
            ('odom', 'odom'),
        ],
        **node_kwargs,
    )
    
    # Planner Server
    planner_server = Node(
        package='nav2_planner',
        executable='planner_server',
        name='planner_server',
        **node_kwargs,
    )
    
    # Behavior Server
    behavior_server = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        name='behavior_server',
        **node_kwargs,
    )
    
    # BT Navigator
    bt_navigator = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        name='bt_navigator',
        **node_kwargs,
    )
    
    # Local Costmap (standalone for testing)
    local_costmap = Node(
        package='nav2_costmap_2d',
        executable='nav2_costmap_2d',
        name='local_costmap',
        remappings=[
            ('scan', 'scan/points'),
        ],
        **node_kwargs,
    )
    
    # Lifecycle Manager kwargs
    lifecycle_kwargs = {
        'output': 'screen',
        'parameters': [
            {'use_sim_time': True},
            {'autostart': True},
            {'node_names': [
                'controller_server',
                'planner_server',
                'behavior_server',
                'bt_navigator',
                'local_costmap',
            ]},
        ],
    }
    if robot_ns:
        lifecycle_kwargs['namespace'] = robot_ns
    
    # Lifecycle Manager
    lifecycle_manager = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_navigation',
        **lifecycle_kwargs,
    )
    
    return [
        static_tf_map_to_odom,
        local_costmap,
        controller_server,
        planner_server,
        behavior_server,
        bt_navigator,
        lifecycle_manager,
    ]


def generate_launch_description():
    return LaunchDescription([
        # Declare robot namespace argument (empty string = single robot)
        DeclareLaunchArgument(
            'robot_ns',
            default_value='',
            description='Robot namespace (empty for single robot, e.g., robot_01 for multi-robot)'
        ),
        
        # Use OpaqueFunction to perform runtime substitution
        OpaqueFunction(function=launch_nav2_with_namespace),
    ])