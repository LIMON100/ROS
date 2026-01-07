import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription, DeclareLaunchArgument
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    pkg_swarm_robot = get_package_share_directory('swarm_robot_ros2')

    # 1. World Argument
    # world_file = os.path.join(pkg_swarm_robot, 'worlds', 'swarm_arena.world')
    world_file = os.path.join(pkg_swarm_robot, 'worlds', 'small_house.world')
    
    # 2. Gazebo Launch
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gazebo.launch.py')
        ),
        launch_arguments={'world': world_file}.items(),
    )

    # 3. Robot State Publisher (Reads URDF)
    urdf_file = os.path.join(pkg_swarm_robot, 'urdf', 'yolobot.urdf')
    with open(urdf_file, 'r') as infp:
        robot_desc = infp.read()

    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'robot_description': robot_desc, 'use_sim_time': True}]
    )

    # 4. Spawn Entity (Spawns the robot into Gazebo)
    spawn_entity = Node(
        package='gazebo_ros',
        executable='spawn_entity.py',
        arguments=['-topic', 'robot_description', '-entity', 'yolobot_1', '-x', '0.0', '-y', '0.0', '-z', '0.1'],
        output='screen'
    )

    return LaunchDescription([
        gazebo,
        robot_state_publisher,
        spawn_entity
    ])