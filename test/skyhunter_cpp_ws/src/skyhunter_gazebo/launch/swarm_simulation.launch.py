# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import IncludeLaunchDescription, GroupAction, RegisterEventHandler
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch_ros.actions import Node, PushRosNamespace
# from launch.event_handlers import OnProcessExit

# def generate_launch_description():
    
#     # --- Paths ---
#     pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
#     pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
#     pkg_yolobot_description = get_package_share_directory('yolobot_description')
    
#     # --- World & Model ---
#     world_path = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'nav_test.world')
#     urdf_file = os.path.join(pkg_yolobot_description, 'robot', 'skyhunter_pro_v3.urdf')
#     with open(urdf_file, 'r') as infp:
#         robot_desc = infp.read()

#     # --- Gazebo Start ---
#     gz_server = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_gazebo_ros, 'launch', 'gzserver.launch.py')
#         ),
#         launch_arguments={'world': world_path}.items()
#     )
#     gz_client = IncludeLaunchDescription(
#         PythonLaunchDescriptionSource(
#             os.path.join(pkg_gazebo_ros, 'launch', 'gzclient.launch.py')
#         )
#     )

#     # --- Spawn Configuration ---
#     # (X, Y) start positions matching the V-Shape to avoid collision on startup
#     spawn_poses = [
#         (0.0, 0.0),    # Robot 1 (Leader)
#         (-2.0, 2.0),   # Robot 2
#         (-2.0, -2.0),  # Robot 3
#         (-4.0, 4.0),   # Robot 4
#         (-4.0, -4.0),  # Robot 5
#         (-6.0, 6.0),   # Robot 6
#         (-6.0, -6.0),  # Robot 7
#         (-8.0, 0.0)    # Robot 8
#     ]

#     spawn_actions = []

#     # Loop to generate 8 robots
#     for i in range(8):
#         robot_id = i + 1
#         robot_name = f'robot{robot_id}'
#         x_pos = spawn_poses[i][0]
#         y_pos = spawn_poses[i][1]

#         # Group for Namespacing
#         robot_group = GroupAction([
#             PushRosNamespace(robot_name),
            
#             # Robot State Publisher
#             Node(
#                 package='robot_state_publisher',
#                 executable='robot_state_publisher',
#                 name='robot_state_publisher',
#                 output='screen',
#                 parameters=[{
#                     'use_sim_time': True,
#                     'robot_description': robot_desc,
#                     'frame_prefix': f'{robot_name}/'
#                 }]
#             ),

#             # Spawner
#             Node(
#                 package='gazebo_ros',
#                 executable='spawn_entity.py',
#                 name=f'spawn_{robot_name}',
#                 arguments=[
#                     '-topic', 'robot_description',
#                     '-entity', f'skyhunter_{robot_id}',
#                     '-x', str(x_pos), '-y', str(y_pos), '-z', '0.3',
#                     '-robot_namespace', robot_name
#                 ],
#                 output='screen'
#             )
#         ])
#         spawn_actions.append(robot_group)

#     return LaunchDescription([
#         gz_server,
#         gz_client,
#     ] + spawn_actions)



import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, GroupAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node, PushRosNamespace

def generate_launch_description():
    
    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
    pkg_yolobot_description = get_package_share_directory('yolobot_description')
    
    world_path = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'rough_terrain.world')
    
    # Read the URDF file once
    urdf_file = os.path.join(pkg_yolobot_description, 'robot', 'skyhunter_pro_v3.urdf')
    with open(urdf_file, 'r') as infp:
        robot_desc_original = infp.read()

    gz_server = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gzserver.launch.py')
        ),
        launch_arguments={'world': world_path}.items()
    )
    gz_client = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gzclient.launch.py')
        )
    )

    spawn_poses = [
        (0.0, 0.0),    # Robot 1 (Leader)
        (-2.0, 2.0),   # Robot 2
        (-2.0, -2.0),  # Robot 3
        (-4.0, 4.0),   # Robot 4
        (-4.0, -4.0),  # Robot 5
        (-6.0, 6.0),   # Robot 6
        (-6.0, -6.0),  # Robot 7
        (-8.0, 0.0)    # Robot 8
    ]

    spawn_actions = []

    for i in range(8):
        robot_id = i + 1
        robot_name = f'robot{robot_id}'
        x_pos = spawn_poses[i][0]
        y_pos = spawn_poses[i][1]

        # --- THE MAGIC TRICK ---
        # If it is Robot 1, replace the color in memory (RAM only)
        if robot_id == 1:
            # Change chassis to Red for Leader
            # Note: Ensure your URDF has <material>Gazebo/DarkGrey</material> for chassis
            current_robot_desc = robot_desc_original.replace(
                '<material>Gazebo/DarkGrey</material>', 
                '<material>Gazebo/Red</material>'
            )
        else:
            # Everyone else gets the original color
            current_robot_desc = robot_desc_original

        robot_group = GroupAction([
            PushRosNamespace(robot_name),
            
            Node(
                package='robot_state_publisher',
                executable='robot_state_publisher',
                name='robot_state_publisher',
                output='screen',
                parameters=[{
                    'use_sim_time': True,
                    'robot_description': current_robot_desc, # Use the modified description
                    'frame_prefix': f'{robot_name}/'
                }]
            ),

            Node(
                package='gazebo_ros',
                executable='spawn_entity.py',
                name=f'spawn_{robot_name}',
                arguments=[
                    '-topic', 'robot_description',
                    '-entity', f'skyhunter_{robot_id}',
                    '-x', str(x_pos), '-y', str(y_pos), '-z', '0.3',
                    '-robot_namespace', robot_name
                ],
                output='screen'
            )
        ])
        spawn_actions.append(robot_group)

    return LaunchDescription([
        gz_server,
        gz_client,
    ] + spawn_actions)