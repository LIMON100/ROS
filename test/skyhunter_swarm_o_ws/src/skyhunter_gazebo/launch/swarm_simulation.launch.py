# import os
# from ament_index_python.packages import get_package_share_directory
# from launch import LaunchDescription
# from launch.actions import IncludeLaunchDescription, GroupAction
# from launch.launch_description_sources import PythonLaunchDescriptionSource
# from launch_ros.actions import Node, PushRosNamespace

# def generate_launch_description():
    
#     pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
#     pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
#     pkg_yolobot_description = get_package_share_directory('yolobot_description')
    
#     world_path = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'rough_terrain.world')
    
#     # Read the URDF file once
#     urdf_file = os.path.join(pkg_yolobot_description, 'robot', 'skyhunter_pro_v3.urdf')
#     with open(urdf_file, 'r') as infp:
#         robot_desc_original = infp.read()

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

#     for i in range(8):
#         robot_id = i + 1
#         robot_name = f'robot{robot_id}'
#         x_pos = spawn_poses[i][0]
#         y_pos = spawn_poses[i][1]

#         # --- THE MAGIC TRICK ---
#         # If it is Robot 1, replace the color in memory (RAM only)
#         if robot_id == 1:
#             # Change chassis to Red for Leader
#             # Note: Ensure your URDF has <material>Gazebo/DarkGrey</material> for chassis
#             current_robot_desc = robot_desc_original.replace(
#                 '<material>Gazebo/DarkGrey</material>', 
#                 '<material>Gazebo/Red</material>'
#             )
#         else:
#             # Everyone else gets the original color
#             current_robot_desc = robot_desc_original

#         robot_group = GroupAction([
#             PushRosNamespace(robot_name),
            
#             Node(
#                 package='robot_state_publisher',
#                 executable='robot_state_publisher',
#                 name='robot_state_publisher',
#                 output='screen',
#                 parameters=[{
#                     'use_sim_time': True,
#                     'robot_description': current_robot_desc, # Use the modified description
#                     'frame_prefix': f'{robot_name}/'
#                 }]
#             ),

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
import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, GroupAction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node, PushRosNamespace

def generate_launch_description():
    
    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
    pkg_yolobot_description = get_package_share_directory('yolobot_description')
    pkg_formation = get_package_share_directory('skyhunter_formation')
    
    # --- Config ---
    world_path = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'rough_terrain.world')
    config_file = os.path.join(pkg_formation, 'config', 'formation_v_shape.yaml')
    
    with open(config_file, 'r') as f:
        config = yaml.safe_load(f)

    # --- Gazebo ---
    gz_server = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_gazebo_ros, 'launch', 'gzserver.launch.py')),
        launch_arguments={'world': world_path}.items()
    )
    gz_client = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(os.path.join(pkg_gazebo_ros, 'launch', 'gzclient.launch.py'))
    )

    # --- Read URDF ---
    urdf_file = os.path.join(pkg_yolobot_description, 'robot', 'skyhunter_pro_v3.urdf')
    with open(urdf_file, 'r') as infp:
        robot_desc_original = infp.read()

    spawn_actions = []

    # --- ROBOT 1: LEADER (Autonomous) ---
    leader_group = GroupAction([
        PushRosNamespace('robot1'),
        Node(
            package='robot_state_publisher',
            executable='robot_state_publisher',
            parameters=[{'robot_description': robot_desc_original, 'frame_prefix': 'robot1/', 'use_sim_time': True}]
        ),
        Node(
            package='gazebo_ros',
            executable='spawn_entity.py',
            arguments=['-topic', 'robot_description', '-entity', 'skyhunter_1', '-x', '0.0', '-y', '0.0', '-z', '0.3'],
            output='screen'
        ),
        # Leader Logic
        Node(
            package='skyhunter_formation',
            executable='leader_node',
            parameters=[{'use_sim_time': True}]
        ),
        # Perception for Leader (To avoid obstacles itself)
        Node(
            package='skyhunter_perception',
            executable='elevation_mapper_node',
            parameters=[{'base_frame': 'robot1/base_link', 'map_frame': 'robot1/odom', 'use_sim_time': True}]
        ),
        # Note: Nav2 for leader would be launched here in a real deployment
    ])
    spawn_actions.append(leader_group)


    # --- ROBOTS 2-8: FOLLOWERS (Formation + Safety) ---
    spawn_poses = [
        (-2.0, 2.0), (-2.0, -2.0), (-4.0, 4.0), (-4.0, -4.0), 
        (-6.0, 6.0), (-6.0, -6.0), (-8.0, 0.0)
    ]

    for i in range(7):
        robot_id = i + 2
        robot_name = f'robot{robot_id}'
        x_pos, y_pos = spawn_poses[i]
        
        # Get offsets
        robot_config = config['swarm_config'][robot_name]
        
        follower_group = GroupAction([
            PushRosNamespace(robot_name),
            Node(
                package='robot_state_publisher',
                executable='robot_state_publisher',
                parameters=[{'robot_description': robot_desc_original, 'frame_prefix': f'{robot_name}/', 'use_sim_time': True}]
            ),
            Node(
                package='gazebo_ros',
                executable='spawn_entity.py',
                arguments=['-topic', 'robot_description', '-entity', f'skyhunter_{robot_id}', 
                           '-x', str(x_pos), '-y', str(y_pos), '-z', '0.3'],
                output='screen'
            ),
            # --- CRITICAL: PERCEPTION FOR FOLLOWER ---
            Node(
                package='skyhunter_perception',
                executable='elevation_mapper_node',
                name='elevation_mapper',
                parameters=[{
                    'base_frame': f'{robot_name}/base_link',
                    'map_frame': f'{robot_name}/odom', # Local odom frame
                    'use_sim_time': True
                }]
            ),
            # --- FOLLOWER LOGIC ---
            Node(
                package='skyhunter_formation',
                executable='follower_node',
                parameters=[{
                    'use_sim_time': True,
                    'offset_x': robot_config['offset_x'],
                    'offset_y': robot_config['offset_y'],
                    'leader_topic': '/robot1/leader_state',
                    'map_topic': 'elevation_map' # Resolves to /robotX/elevation_map
                }]
            )
        ])
        spawn_actions.append(follower_group)

    return LaunchDescription([gz_server, gz_client] + spawn_actions)