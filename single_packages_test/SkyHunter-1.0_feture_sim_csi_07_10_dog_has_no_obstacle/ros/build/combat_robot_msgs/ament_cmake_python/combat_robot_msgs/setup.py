from setuptools import find_packages
from setuptools import setup

setup(
    name='combat_robot_msgs',
    version='0.0.0',
    packages=find_packages(
        include=('combat_robot_msgs', 'combat_robot_msgs.*')),
)
