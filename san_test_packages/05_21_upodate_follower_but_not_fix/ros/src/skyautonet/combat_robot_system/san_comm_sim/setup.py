from setuptools import setup, find_packages
from glob import glob

package_name = "san_comm_sim"

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
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="Kim Taegeun",
    maintainer_email="taegeun.kim@skyautonet.com",
    description="SAN v1.5 — Comm simulators (Soft Kill 제외)",
    license="Proprietary - SkyAutoNet",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "wifi6_mesh_simulator = san_comm_sim.wifi6_mesh_simulator:main",
            "lte_simulator        = san_comm_sim.lte_simulator:main",
            "lora_simulator       = san_comm_sim.lora_simulator:main",
            "swarm_comm_manager   = san_comm_sim.swarm_comm_manager:main",
            "mesh_visualizer      = san_comm_sim.mesh_visualizer:main",
            "rssi_plotter         = san_comm_sim.rssi_plotter:main",
            "comm_traffic_filter  = san_comm_sim.comm_traffic_filter:main",
        ],
    },
)
