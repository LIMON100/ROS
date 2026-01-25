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
    llicense='Apache-2.0',
    extras_require={
        'test': [
            'pytest',
        ],
    },
    entry_points={
        'console_scripts': [
            'send_goal = skyhunter_nav_tools.send_goal:main',
        ],
    },
)
