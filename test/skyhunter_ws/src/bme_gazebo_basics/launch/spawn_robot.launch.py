import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration, Command
from launch_ros.actions import Node

def generate_launch_description():
    pkg_bme_gazebo_basics = get_package_share_directory('bme_gazebo_basics')
    pkg_ros_gz_sim = get_package_share_directory('ros_gz_sim')

    # Declare the launch arguments
    model_arg = DeclareLaunchArgument(
        'model', default_value='mogi_bot_mecanum.urdf',
        description='Name of the robot URDF file'
    )
    world_arg = DeclareLaunchArgument(
        'world', default_value='world.sdf',
        description='Gazebo world file'
    )

    # --- Gazebo Launch ---
    # Starts the Gazebo server and GUI.
    gazebo = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_ros_gz_sim, 'launch', 'gz_sim.launch.py')
        ),
        # Pass the world file to Gazebo as an argument
        launch_arguments={'gz_args': ['-r ', os.path.join(pkg_bme_gazebo_basics, 'worlds', LaunchConfiguration('world'))]}.items()
    )

    # --- Robot State Publisher ---
    # Reads the URDF/XACRO file, processes it, and publishes the robot_description topic
    robot_description_content = Command([
        'xacro ', os.path.join(pkg_bme_gazebo_basics, 'urdf', LaunchConfiguration('model'))
    ])
    robot_state_publisher = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        output='screen',
        parameters=[{'use_sim_time': True, 'robot_description': robot_description_content}]
    )

    # --- Spawn Entity Node ---
    # Spawns the robot into Gazebo from the robot_description topic
    spawn_entity = Node(
        package='ros_gz_sim',
        executable='create',
        arguments=['-topic', 'robot_description',
                   '-name', 'mogi_bot',
                   '-x', '0',
                   '-y', '0',
                   '-z', '0.5'],
        output='screen'
    )
    
    # --- Bridge Node ---
    # Connects ROS 2 topics with Gazebo topics
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        arguments=['/clock@rosgraph_msgs/msg/Clock[gz.msgs.Clock',
                   '/cmd_vel@geometry_msgs/msg/Twist@gz.msgs.Twist',
                   '/odom@nav_msgs/msg/Odometry@gz.msgs.Odometry',
                   '/tf@tf2_msgs/msg/TFMessage[gz.msgs.Pose_V',
                   '/joint_states@sensor_msgs/msg/JointState[gz.msgs.Model'],
        output='screen'
    )

    return LaunchDescription([
        model_arg,
        world_arg,
        gazebo,
        robot_state_publisher,
        spawn_entity,
        bridge
    ])
