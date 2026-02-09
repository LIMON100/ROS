# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch_ros.actions import Node
# from nav2_common.launch import RewrittenYaml

# def generate_launch_description():
#     pkg_ros = get_package_share_directory("ros_gz_sim")
#     pkg_tin = get_package_share_directory("tin3_bot")
    
#     gz_resource_path = SetEnvironmentVariable(name="GZ_SIM_RESOURCE_PATH", value=[os.environ.get("GZ_SIM_RESOURCE_PATH", ""), ":", os.path.dirname(pkg_tin)])

#     # 1. World & Robot & SLAM (Known Good)
#     world = os.path.join(pkg_tin, "worlds", "obstacle_world.sdf")
#     gz_sim = IncludeLaunchDescription(PythonLaunchDescriptionSource(os.path.join(pkg_ros, "launch", "gz_sim.launch.py")), launch_arguments={"gz_args": f"-r {world}"}.items())
    
#     spawn_robot = IncludeLaunchDescription(PythonLaunchDescriptionSource(os.path.join(pkg_tin, "launch", "spawn_robot.launch.py")),
#         launch_arguments={"robot_ns": "robot_01", "x": "0.0", "y": "0.0", "z": "0.5", "use_ekf": "true", "lidar_mode": "full"}.items())

    
#     pc_to_laser = Node(
#     package='pointcloud_to_laserscan', 
#     executable='pointcloud_to_laserscan_node', 
#     name='pc_to_laser',
#     remappings=[
#         ('cloud_in', '/robot_01/scan/points'), 
#         ('scan', '/robot_01/scan')
#     ],
#     parameters=[{
#         'target_frame': 'robot_01/lidar_link', 
#         'range_max': 20.0, 
#         'use_inf': True, 
#         'use_sim_time': True,
#         'min_height': 0.15,  
#         'max_height': 1.0,  
#     }]
#     )

#     slam = Node(package='slam_toolbox', executable='async_slam_toolbox_node', name='slam_toolbox', output='screen',
#         remappings=[('/scan', '/robot_01/scan')],
#         parameters=[{'use_sim_time': True, 'base_frame': 'robot_01/base_footprint', 'odom_frame': 'robot_01/odom', 'map_frame': 'map', 'mode': 'mapping'}])
    
#     nav2_config = os.path.join(pkg_tin, 'config', 'nav2_params.yaml')

#     configured_params = RewrittenYaml(
#         source_file=nav2_config,
#         root_key='robot_01',
#         param_rewrites={},
#         convert_types=True
#     )

#     controller = Node(
#         package='nav2_controller',
#         executable='controller_server',
#         name='controller_server',
#         output='screen',
#         namespace='robot_01',
#         parameters=[configured_params], 
#         remappings=[
#             ('/cmd_vel', '/robot_01/cmd_vel'), 
#             ('/odom', '/robot_01/odom')
#         ]
#     )

#     planner = Node(
#         package='nav2_planner',
#         executable='planner_server',
#         name='planner_server',
#         output='screen',
#         namespace='robot_01',
#         parameters=[configured_params]
#     )

#     recoveries = Node(
#         package='nav2_behaviors',
#         executable='behavior_server',
#         name='behavior_server',
#         output='screen',
#         namespace='robot_01',
#         parameters=[configured_params]
#     )

#     bt_nav = Node(
#         package='nav2_bt_navigator',
#         executable='bt_navigator',
#         name='bt_navigator',
#         output='screen',
#         namespace='robot_01',
#         parameters=[configured_params]
#     )

#     lifecycle = Node(
#         package='nav2_lifecycle_manager',
#         executable='lifecycle_manager',
#         name='lifecycle_manager_nav',
#         output='screen',
#         namespace='robot_01',
#         parameters=[{'use_sim_time': True, 'autostart': True, 
#                      'node_names': ['controller_server', 'planner_server', 'behavior_server', 'bt_navigator']}]
#     )

#     return LaunchDescription([
#         gz_resource_path, gz_sim, spawn_robot, 
#         Node(package="ros_gz_bridge", executable="parameter_bridge", arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"]),
#         pc_to_laser, slam,
#         controller, planner, recoveries, bt_nav, lifecycle
#     ])

# File: tin3_bot/launch/debug_nav2.launch.py (Corrected)
import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, SetEnvironmentVariable
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from nav2_common.launch import RewrittenYaml

def generate_launch_description():
    pkg_ros = get_package_share_directory("ros_gz_sim")
    pkg_tin = get_package_share_directory("tin3_bot")
    
    gz_resource_path = SetEnvironmentVariable(name="GZ_SIM_RESOURCE_PATH", value=[os.environ.get("GZ_SIM_RESOURCE_PATH", ""), ":", os.path.dirname(pkg_tin)])

    # 1. World & Robot & SLAM (Known Good)
    world = os.path.join(pkg_tin, "worlds", "obstacle_world.sdf")
    gz_sim = IncludeLaunchDescription(PythonLaunchDescriptionSource(os.path.join(pkg_ros, "launch", "gz_sim.launch.py")), launch_arguments={"gz_args": f"-r {world}"}.items())
    
    spawn_robot = IncludeLaunchDescription(PythonLaunchDescriptionSource(os.path.join(pkg_tin, "launch", "spawn_robot.launch.py")),
        launch_arguments={"robot_ns": "robot_01", "x": "0.0", "y": "0.0", "z": "0.5", "use_ekf": "true", "lidar_mode": "full"}.items())

    
    pc_to_laser = Node(
        package='pointcloud_to_laserscan', 
        executable='pointcloud_to_laserscan_node', 
        name='pc_to_laser',
        remappings=[
            ('cloud_in', '/robot_01/scan/points'), 
            ('scan', '/robot_01/scan')
        ],
        parameters=[{
            'target_frame': 'robot_01/lidar_link', 
            'range_max': 20.0, 
            'use_inf': True, 
            'use_sim_time': True,
            'min_height': 0.15,  
            'max_height': 1.0,  
        }]
    )

    slam = Node(package='slam_toolbox', executable='async_slam_toolbox_node', name='slam_toolbox', output='screen',
        remappings=[('/scan', '/robot_01/scan')],
        parameters=[{'use_sim_time': True, 'base_frame': 'robot_01/base_footprint', 'odom_frame': 'robot_01/odom', 'map_frame': 'map', 'mode': 'mapping'}])
    
    nav2_config = os.path.join(pkg_tin, 'config', 'nav2_params.yaml')

    configured_params = RewrittenYaml(
        source_file=nav2_config,
        root_key='robot_01',
        param_rewrites={},
        convert_types=True
    )

    # --- THIS IS THE CRITICAL FIX ---
    # Define the remapping to connect namespaced Nav2 nodes to the global /map topic
    nav2_map_remapping = [('/map', '/map')]

    controller = Node(
        package='nav2_controller',
        executable='controller_server',
        name='controller_server',
        output='screen',
        namespace='robot_01',
        parameters=[configured_params], 
        remappings=[
            ('/cmd_vel', '/robot_01/cmd_vel'), 
            ('/odom', '/robot_01/odom')
        ]
    )

    planner = Node(
        package='nav2_planner',
        executable='planner_server',
        name='planner_server',
        output='screen',
        namespace='robot_01',
        parameters=[configured_params],
        remappings=nav2_map_remapping  # <-- ADDED FIX
    )

    recoveries = Node(
        package='nav2_behaviors',
        executable='behavior_server',
        name='behavior_server',
        output='screen',
        namespace='robot_01',
        parameters=[configured_params],
        remappings=nav2_map_remapping  # <-- ADDED FIX
    )

    bt_nav = Node(
        package='nav2_bt_navigator',
        executable='bt_navigator',
        name='bt_navigator',
        output='screen',
        namespace='robot_01',
        parameters=[configured_params],
        remappings=nav2_map_remapping  # <-- ADDED FIX
    )

    lifecycle = Node(
        package='nav2_lifecycle_manager',
        executable='lifecycle_manager',
        name='lifecycle_manager_nav',
        output='screen',
        namespace='robot_01',
        parameters=[{'use_sim_time': True, 'autostart': True, 
                     'node_names': ['controller_server', 'planner_server', 'behavior_server', 'bt_navigator']}]
    )

    return LaunchDescription([
        gz_resource_path, gz_sim, spawn_robot, 
        Node(package="ros_gz_bridge", executable="parameter_bridge", arguments=["/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock"]),
        pc_to_laser, slam,
        controller, planner, recoveries, bt_nav, lifecycle
    ])