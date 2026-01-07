from setuptools import setup
import os
from glob import glob

package_name = 'swarm_robot_ros2'

setup(
    name=package_name,
    version='0.0.0',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
        # Include all launch files
        (os.path.join('share', package_name, 'launch'), glob('launch/*.launch.py')),
        # Include all urdf files
        (os.path.join('share', package_name, 'urdf'), glob('urdf/*.urdf')),
        # Include all world files
        (os.path.join('share', package_name, 'worlds'), glob('worlds/*.world')),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='limonubuntu',
    maintainer_email='mahmudurlimon41@gmail.com',
    description='Swarm Robotics Foundation',
    license='TODO',
    tests_require=['pytest'],
    entry_points={
        'console_scripts': [
        ],
    },
)