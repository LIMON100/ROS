from setuptools import find_packages, setup

package_name = 'skyhunter_nav_tools'

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
    license='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'waypoint_sender = skyhunter_nav_tools.waypoint_sender:main',
            'formation_switcher = skyhunter_nav_tools.formation_switcher:main',
            'terminate_leader = skyhunter_nav_tools.terminate_leader:main',
        ],
    },
)
