import os
from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, IncludeLaunchDescription
from launch.launch_description_sources import PythonLaunchDescriptionSource
from launch.substitutions import LaunchConfiguration

def generate_launch_description():
    
    pkg_skyhunter_navigation = get_package_share_directory('skyhunter_navigation')
    pkg_nav2_bringup = get_package_share_directory('nav2_bringup')

    # --- Declare Launch Arguments ---
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true'
    )
    
    nav2_params_file_arg = DeclareLaunchArgument(
        'params_file',
        default_value=os.path.join(pkg_skyhunter_navigation, 'config', 'nav2_params.yaml'),
        description='Full path to the ROS2 parameters file for Nav2'
    )

    # --- Use the Official Nav2 Bringup Launch File ---
    nav2_bringup_launch = IncludeLaunchDescription(
        PythonLaunchDescriptionSource(
            os.path.join(pkg_nav2_bringup, 'launch', 'bringup_launch.py')
        ),
        # Pass our custom parameters to the official launch file
        launch_arguments={
            'slam': 'True',  # <-- THIS IS THE KEY FIX. Tells Nav2 we are using SLAM.
            'map': '',       # This is now ignored because slam:=True
            'use_sim_time': LaunchConfiguration('use_sim_time'),
            'params_file': LaunchConfiguration('params_file'),
            'autostart': 'True',
            'use_composition': 'True'
        }.items()
    )

    return LaunchDescription([
        use_sim_time_arg,
        nav2_params_file_arg,
        nav2_bringup_launch
    ])