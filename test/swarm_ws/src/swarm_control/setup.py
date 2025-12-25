from setuptools import find_packages, setup

package_name = 'swarm_control'

setup(
    name=package_name,
    version='0.0.0',
    packages=find_packages(exclude=['test']),
    data_files=[
        ('share/ament_index/resource_index/packages',
            ['resource/' + package_name]),
        ('share/' + package_name, ['package.xml']),
    ],
    install_requires=['setuptools'],
    zip_safe=True,
    maintainer='limonubuntu',
    maintainer_email='mahmudurlimon41@gmail.com',
    description='TODO: Package description',
    license='TODO: License declaration',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    # Inside setup.py

    entry_points={
        'console_scripts': [
            'sim_engine = swarm_control.sim_engine_node:main',
            'formation_brain = swarm_control.formation_node:main',
            'robot_driver_0 = swarm_control.robot_driver_node:main_robot0',
            'robot_driver_1 = swarm_control.robot_driver_node:main_robot1',
            'robot_driver_2 = swarm_control.robot_driver_node:main_robot2',
            'robot_driver_3 = swarm_control.robot_driver_node:main_robot3',
            'robot_driver_4 = swarm_control.robot_driver_node:main_robot4',
            'gui_controller = swarm_control.gui_node:main',
        ],
    },
)
