import os
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory

def generate_launch_description():
    
    pkg_skyhunter_slam = get_package_share_directory('skyhunter_slam')
    
    # --- Declare Launch Arguments ---
    use_sim_time_arg = DeclareLaunchArgument(
        'use_sim_time',
        default_value='true',
        description='Use simulation (Gazebo) clock if true'
    )
    
    slam_params_file_arg = DeclareLaunchArgument(
        'slam_params_file',
        default_value=os.path.join(pkg_skyhunter_slam, 'config', 'mapper_params_online_async.yaml'),
        description='Full path to the ROS2 parameters file to use for the slam_toolbox node'
    )

    # --- PointCloud to LaserScan Node ---
    # Converts the 3D PointCloud2 from the LiDAR into a 2D LaserScan message
    pointcloud_to_laserscan_node = Node(
        package='pointcloud_to_laserscan',
        executable='pointcloud_to_laserscan_node',
        name='pointcloud_to_laserscan',
        remappings=[('cloud_in', '/lidar/points'), # Subscribe to your universal 3D topic
                    ('scan_out', '/scan')],         # Publish to the standard 2D /scan topic
        parameters=[{
            'target_frame': 'base_link', # The frame to project the scan into
            'transform_tolerance': 0.01,
            'min_height': -0.3,          # Points below this Z value (relative to target_frame) will be ignored
            'max_height': 1.5,           # Points above this Z value will be ignored
            'angle_min': -3.1415,        # Start angle of the scan [rad]
            'angle_max': 3.1415,         # End angle of the scan [rad]
            'angle_increment': 0.0087,   # Angular resolution [rad]
            'scan_time': 0.1,
            'range_min': 0.2,
            'range_max': 30.0,
            'use_inf': True,
            'inf_epsilon': 1.0,
            'use_sim_time': LaunchConfiguration('use_sim_time'),
        }]
    )

    # --- SLAM Toolbox Node ---
    # The main SLAM node
    slam_toolbox_node = Node(
        package='slam_toolbox',
        executable='async_slam_toolbox_node',
        name='slam_toolbox',
        output='screen',
        parameters=[
            LaunchConfiguration('slam_params_file'),
            {'use_sim_time': LaunchConfiguration('use_sim_time')}
        ]
    )

    return LaunchDescription([
        use_sim_time_arg,
        slam_params_file_arg,
        pointcloud_to_laserscan_node,
        slam_toolbox_node
    ])