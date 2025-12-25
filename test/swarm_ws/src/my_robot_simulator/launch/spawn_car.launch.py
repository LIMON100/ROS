import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch_ros.actions import Node

def generate_launch_description():
    # Get the path to the Gazebo ROS package
    pkg_gazebo_ros = get_package_share_directory('gazebo_ros')
    
    # Get the path to your package
    pkg_my_robot_simulator = get_package_share_directory('my_robot_simulator')

    # --- Gazebo Launch ---
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_gazebo_ros, 'launch', 'gazebo.launch.py')
        ),
    )

    # --- Spawn Entity Node ---
    # This node will spawn your car model into the Gazebo simulation
    spawn_entity = Node(
        package='gazebo_ros', 
        executable='spawn_entity.py',
        arguments=['-topic', 'robot_description', '-entity', 'simple_car'],
        output='screen'
    )
    
    # --- Robot State Publisher ---
    # This node publishes the robot's state (TF tree)
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        name='robot_state_publisher',
        output='screen',
        parameters=[{'use_sim_time': True, 'robot_description': open(os.path.join(pkg_my_robot_simulator, 'models', 'simple_car', 'model.sdf')).read()}]
    )

    return LaunchDescription([
        gazebo,
        robot_state_publisher,
        spawn_entity,
    ])