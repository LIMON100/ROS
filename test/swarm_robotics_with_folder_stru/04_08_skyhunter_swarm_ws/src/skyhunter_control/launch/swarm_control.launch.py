"""
this launch file is designed to start the swarm control nodes for all 8 robots in the V-Shape formation. 
It reads the formation configuration from a YAML file and launches the leader and follower nodes with the appropriate parameters.
"""
import os
import yaml
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node

def generate_launch_description():
    
    pkg_formation = get_package_share_directory('skyhunter_formation')
    config_file = os.path.join(pkg_formation, 'config', 'formation_v_shape.yaml')

    # Load the YAML file
    with open(config_file, 'r') as f:
        config = yaml.safe_load(f)
    
    nodes = []

    # 1. Start Leader Node (Robot 1)
    leader_node = Node(
        package='skyhunter_formation',
        executable='leader_node',
        namespace='robot1',
        output='screen',
        parameters=[{'use_sim_time': True}]
    )
    nodes.append(leader_node)

    # 2. Start Follower Nodes (Robot 2 to 8)
    for i in range(2, 9):
        robot_name = f'robot{i}'
        
        # Get offsets from YAML
        robot_config = config['swarm_config'][robot_name]
        off_x = robot_config['offset_x']
        off_y = robot_config['offset_y']

        follower = Node(
            package='skyhunter_formation',
            executable='follower_node',
            namespace=robot_name,
            output='screen',
            parameters=[{
                'use_sim_time': True,
                'offset_x': off_x,
                'offset_y': off_y,
                'leader_topic': '/robot1/leader_state',
                # Gains can be tuned here globally if needed
                'k_x': 1.5,
                'k_y': 2.0,
                'k_theta': 4.0
            }]
        )

        nodes.append(follower)

        

    return LaunchDescription(nodes)