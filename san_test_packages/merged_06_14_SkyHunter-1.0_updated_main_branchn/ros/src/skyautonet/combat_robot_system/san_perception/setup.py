from glob import glob

from setuptools import find_packages, setup

package_name = "san_perception"

setup(
    name=package_name,
    version="1.5.0",
    packages=find_packages(exclude=["test"]),
    data_files=[
        ("share/ament_index/resource_index/packages",
         ["resource/" + package_name]),
        ("share/" + package_name, ["package.xml"]),
        ("share/" + package_name + "/config", glob("config/*.yaml")),
        ("share/" + package_name + "/launch", glob("launch/*.launch.py")),
    ],
    install_requires=["setuptools", "numpy"],
    zip_safe=True,
    maintainer="Kim Taegeun",
    maintainer_email="taegeun.kim@skyautonet.com",
    description="SAN v1.5 Phase 2-E — Perception rclpy node (Tier 3).",
    license="Proprietary - SkyAutoNet",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "perception_node = san_perception.perception_node:main",
        ],
    },
)
