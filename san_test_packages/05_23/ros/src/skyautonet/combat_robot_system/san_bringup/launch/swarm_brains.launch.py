import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument, OpaqueFunction
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

def _launch_brains(context):
    num_robots = int(LaunchConfiguration('num_robots').perform(context))
    pkg_bringup = get_package_share_directory('san_bringup')
    
    actions = []
    for i in range(1, num_robots + 1):
        role = "leader" if i == 1 else "follower"
        
        actions.append(
            IncludeLaunchDescription(
                PythonLaunchDescriptionSource(
                    os.path.join(pkg_bringup, 'launch', 'squadron.launch.py')
                ),
                launch_arguments={
                    'robot_id': str(i),
                    'robot_role': role,
                    'deployment_mode': 'production'
                }.items()
            )
        )
    return actions

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument('num_robots', default_value='3'),
        OpaqueFunction(function=_launch_brains)
    ])