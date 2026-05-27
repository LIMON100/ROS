import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    pkg_share = get_package_share_directory('san_sim_gazebo')
    
    # This launch file wraps the client's swarm_sim 
    # and configures it for the 4-robot demo required by the plan.
    return LaunchDescription([
        DeclareLaunchArgument('world', default_value='empty_world.sdf'),
        
        IncludeLaunchDescription(
            PythonLaunchDescriptionSource(
                os.path.join(pkg_share, 'launch', 'swarm_sim.launch.py')
            ),
            launch_arguments={
                'num_robots': '4', # Demo Day requirement
                'world': LaunchConfiguration('world'),
                'use_sim_time': 'true'
            }.items()
        )
    ])