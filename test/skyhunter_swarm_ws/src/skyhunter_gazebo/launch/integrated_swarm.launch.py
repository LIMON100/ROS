import os
import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, GroupAction, SetEnvironmentVariable, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node, PushRosNamespace
from launch.substitutions import LaunchConfiguration, PythonExpression, PathJoinSubstitution
from launch.conditions import IfCondition

def generate_launch_description():
    
    # --- Paths ---
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')
    pkg_tin3_bot = get_package_share_directory('tin3_bot')
    pkg_formation = get_package_share_directory('skyhunter_formation')
    
    # --- Environment Fix ---
    gz_resource_path = SetEnvironmentVariable(
        name="GZ_SIM_RESOURCE_PATH",
        value=[
            os.environ.get("GZ_SIM_RESOURCE_PATH", ""),
            ":",
            os.path.dirname(pkg_tin3_bot),
        ],
    )

    # --- Arguments ---
    num_robots_arg = DeclareLaunchArgument(
        'num_robots', default_value='2',
        description='Number of robots to spawn (1-8)'
    )
    
    lidar_mode_arg = DeclareLaunchArgument(
        'lidar_mode', default_value='low',
        description='LiDAR resolution (full, half, low)'
    )

    world_name_arg = DeclareLaunchArgument(
        'world_name', default_value='rough_terrain.sdf',
        description='World file name inside tin3_bot/worlds'
    )

    # --- Configs ---
    config_file = os.path.join(pkg_formation, 'config', 'formation_v_shape.yaml')
    with open(config_file, 'r') as f:
        formation_config = yaml.safe_load(f)

    # --- 1. Start Gazebo ---
    gz_sim = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')
        ),
        launch_arguments={
            'gz_args': ['-r ', PathJoinSubstitution([pkg_tin3_bot, 'worlds', LaunchConfiguration('world_name')])]
        }.items()
    )

    # --- 2. Clock Bridge ---
    clock_bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=['/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock'],
        output='screen'
    )

    # --- 3. Dynamic Spawn Loop ---
    spawn_actions = []
    
    spawn_poses = [
        (0.0, 0.0),    # Robot 1
        (-2.0, 2.0),   # Robot 2
        (-2.0, -2.0),  # Robot 3
        (-4.0, 4.0),   # Robot 4
        (-4.0, -4.0),  # Robot 5
        (-6.0, 6.0),   # Robot 6
        (-6.0, -6.0),  # Robot 7
        (-8.0, 0.0)    # Robot 8
    ]

    for i in range(8):
        robot_id = i + 1
        robot_name = f'robot{robot_id}'
        x_pos, y_pos = spawn_poses[i]

        # Logic: Only spawn if robot_id <= num_robots
        should_spawn = IfCondition(
            PythonExpression([str(robot_id), ' <= ', LaunchConfiguration('num_robots')])
        )
        
        # --- SAFE OFFSET LOOKUP ---
        # Only look up offsets if this is a follower (ID > 1)
        off_x = 0.0
        off_y = 0.0
        if robot_id > 1:
            # Check if config exists to prevent KeyError
            if robot_name in formation_config['swarm_config']:
                off_x = formation_config['swarm_config'][robot_name]['offset_x']
                off_y = formation_config['swarm_config'][robot_name]['offset_y']
        
        # A. Spawn Robot
        spawn_robot_cmd = IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_tin3_bot, 'launch', 'spawn_robot.launch.py')
            ),
            launch_arguments={
                'robot_ns': robot_name,
                'x': str(x_pos),
                'y': str(y_pos),
                'z': '0.5',
                'use_ekf': 'false',
                'lidar_mode': LaunchConfiguration('lidar_mode')
            }.items(),
            condition=should_spawn
        )
        spawn_actions.append(spawn_robot_cmd)

        # B. Logic Nodes
        logic_group = GroupAction([
            PushRosNamespace(robot_name),

            # 1. Perception
            Node(
                package='skyhunter_perception',
                executable='elevation_mapper_node',
                name='elevation_mapper',
                parameters=[{
                    'use_sim_time': True,
                    'base_frame': f'{robot_name}/base_footprint', # tin3_bot uses base_footprint
                    'map_frame': f'{robot_name}/odom',
                    # Force the topic to be absolute relative to the namespace
                    # Since we are in PushRosNamespace(robot_name), "scan/points" becomes "/robotX/scan/points"
                    'cloud_topic': 'scan/points', 
                    'map.length': 10.0
                }],
                condition=should_spawn
            ),

            # 2. Leader Logic (Only for Robot 1)
            Node(
                package='skyhunter_formation',
                executable='leader_node',
                name='leader_logic',
                parameters=[{'use_sim_time': True, 'odom_topic': 'odom'}],
                condition=IfCondition(PythonExpression([str(robot_id), ' == 1']))
            ),

            # 3. Follower Logic (Only for Robot > 1)
            # Notice we pass the safely extracted off_x / off_y variables
            Node(
                package='skyhunter_formation',
                executable='follower_node',
                name='follower_logic',
                parameters=[{
                    'use_sim_time': True,
                    'offset_x': off_x,
                    'offset_y': off_y,
                    'leader_topic': '/robot1/leader_state',
                    'map_topic': 'elevation_map',
                    'k_x': 1.5, 'k_y': 2.0, 'k_theta': 4.0
                }],
                condition=IfCondition(PythonExpression([str(robot_id), ' > 1 and ', str(robot_id), ' <= ', LaunchConfiguration('num_robots')]))
            )
        ])
        spawn_actions.append(logic_group)

    return LaunchDescription([
        gz_resource_path,
        num_robots_arg,
        lidar_mode_arg,
        world_name_arg,
        gz_sim,
        clock_bridge,
    ] + spawn_actions)