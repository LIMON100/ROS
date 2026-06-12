from setuptools import setup
from glob import glob

package_name = "san_operator_tools"

setup(
    name=package_name,
    version="1.5.0",
    packages=[package_name],
    data_files=[
        ("share/ament_index/resource_index/packages",
         ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        ("share/" + package_name + "/config", glob("config/*.yaml")),
        ("share/" + package_name + "/launch", glob("launch/*.launch.py")),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Kim Taegeun",
    maintainer_email="taegeun.kim@skyautonet.com",
    description="SAN v1.5 — Operator utility nodes adapted from Limon code",
    license="Proprietary - SkyAutoNet",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "waypoint_sender    = san_operator_tools.waypoint_sender:main",
            "waypoint_to_nav2   = san_operator_tools.waypoint_to_nav2:main",
            "leader_path_nav    = san_operator_tools.leader_path_nav:main",
            "formation_switcher = san_operator_tools.formation_switcher:main",
            "swarm_dashboard    = san_operator_tools.swarm_dashboard:main",
        ],
    },
)
