import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    pkg_tin3_navigation = get_package_share_directory("tin3_navigation")
    nav2_params_path = os.path.join(pkg_tin3_navigation, "config", "nav2_params.yaml")
    namespace = "SH_02"

    param_substitutions = {
        'robot_base_frame': 'SH_02/base_footprint', # CRITICAL: Changed from base_link to base_footprint
        'odom_frame': 'SH_02/odom',
        'global_frame': 'map',
        'use_sim_time': 'true'
    }
    configured_params = RewrittenYaml(
        source_file=nav2_params_path,
        root_key='SH_02', 
        param_rewrites=param_substitutions,
        convert_types=True
    )

    # CRITICAL FIX: Isolate SH_02 from Robot-1's sensors!
    remappings = [
        ('/tf', '/tf'), 
        ('/tf_static', '/tf_static'),
        ('/scan/points', f'/{namespace}/scan/points'),
        ('/odom_filtered', f'/{namespace}/odom_filtered'),
        ('/cmd_vel', f'/{namespace}/cmd_vel'),
        ('/plan', f'/{namespace}/plan')
    ]

    nodes = [
        Node(package='nav2_controller', executable='controller_server', namespace=namespace, 
             output='screen', parameters=[configured_params], remappings=remappings),
        Node(package='nav2_planner', executable='planner_server', namespace=namespace, 
             output='screen', parameters=[configured_params], remappings=remappings),
        Node(package='nav2_behaviors', executable='behavior_server', namespace=namespace, 
             output='screen', parameters=[configured_params], remappings=remappings),
        Node(package='nav2_bt_navigator', executable='bt_navigator', namespace=namespace, 
             output='screen', parameters=[configured_params], remappings=remappings),
        Node(package='nav2_lifecycle_manager', executable='lifecycle_manager', name='lifecycle_manager_sh02', 
             namespace=namespace, output='screen', parameters=[{'autostart': False, 'node_names': [
                'controller_server', 'planner_server', 'behavior_server', 'bt_navigator'
            ]}])
    ]
    return LaunchDescription(nodes)