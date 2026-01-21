import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource

def generate_launch_description():

    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    pkg_skyhunter_gazebo = get_package_share_directory('skyhunter_gazebo')
    # pkg_skyhunter_description = get_package_share_directory('skyhunter_description')
    pkg_skyhunter_description = get_package_share_directory('yolobot_description')
    pkg_skyhunter_localization = get_package_share_directory('skyhunter_localization')

    # Use our new, self-contained world file
    world = os.path.join(pkg_skyhunter_gazebo, 'worlds', 'agriculture.world')

    # Launch Gazebo with the world
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gazebo.launch.py'),
        ),
        launch_arguments={'world': world}.items()
    )

    # Get the robot's URDF file
    urdf_file_path = os.path.join(pkg_skyhunter_description, 'robot', 'skyhunter_pro_v3.urdf') 

    start_localization_cmd = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_skyhunter_localization, 'launch', 'localization.launch.py')
        )
    )

    # Start the Robot State Publisher
    start_robot_state_publisher_cmd = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        parameters=[{'robot_description': open(urdf_file_path).read(),
                     'use_sim_time': True}]
    )

    # Spawn the robot into the world at a safe starting position
    spawn_entity = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=['-topic', 'robot_description', '-entity', 'skyhunter',
                   '-x', '0.0', 
                   '-y', '0.0', 
                   '-z', '0.5'
                   ],
        output='screen'
    )

    return LaunchDescription([
        gazebo,
        start_robot_state_publisher_cmd,
        spawn_entity,
        start_localization_cmd
    ])