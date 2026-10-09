from setuptools import setup
package_name = 'imu_publisher'
setup(
    name=package_name,
    version='0.0.1',
    packages=[package_name],
    data_files=[
        ('share/ament_index/resource_index/packages', [f'resource/{package_name}']),
        ('share/' + package_name, ['package.xml']),
        ('share/' + package_name + '/launch', ['launch/imu_launch.py']),
        ('share/' + package_name + '/config', ['config/imu_mac.yaml']),
    ],
    install_requires=['setuptools', 'bluepy'],
    zip_safe=True,
    maintainer='Your Name',
    maintainer_email='you@example.com',
    description='WT901 IMU BLE → /imu/data_raw publisher',
    license='Apache-2.0',
    entry_points={
        'console_scripts': [
            'imu_node = imu_publisher.imu_node:main'
        ],
    },
)
