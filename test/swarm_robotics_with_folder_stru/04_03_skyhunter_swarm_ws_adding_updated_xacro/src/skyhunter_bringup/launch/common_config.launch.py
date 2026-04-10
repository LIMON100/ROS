# tin3_bringup/launch/common_config.launch.py
# Sets ROS_DOMAIN_ID=42 and points to FastDDS XML profile.
# Include this launch file FIRST in any other launch file.
import os
from launch import LaunchDescription
from launch.actions import SetEnvironmentVariable
from ament_index_python.packages import get_package_share_directory


def generate_launch_description():
    common_share = get_package_share_directory('skyhunter_bringup')

    return LaunchDescription([
        SetEnvironmentVariable('ROS_DOMAIN_ID', '42'),
        SetEnvironmentVariable(
            'FASTDDS_DEFAULT_PROFILES_FILE',
            os.path.join(common_share, 'config', 'fastrtps_profile.xml')
        ),
    ])
