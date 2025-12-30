from setuptools import find_packages, setup

package_name = 'my_first_package'

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
            'sensor_hub = my_first_package.node_1_sensor_hub:main',
            'logic_hub = my_first_package.node_2_logic_hub:main',
            'command_hub = my_first_package.node_3_command_hub:main',
        ],
    },
)
