# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
SAN v1.5 — Drone target simulator launch.

Spawns the drone target simulator with default 3-scenario set
(loiter + attack_run + swarm_evasion). The actual drone entities
are spawned by gz sim from simple sphere SDF models — see
worlds/drone_target.sdf (uses cmd_pose teleport via ros_gz_bridge).
"""
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    return LaunchDescription([
        Node(
            package="san_sim_gazebo_helpers",
            executable="drone_target_simulator",
            name="drone_target_simulator",
            output="screen",
            parameters=[{
                "tick_rate_hz": 50.0,
                "scenarios": [
                    # Surveillance loiter at 30m altitude, 50m radius
                    "loiter:0,0,30,50,8.0",
                    # FPV attack from (80,30,40) → (0,0,1.5) at 15 m/s
                    "attack_run:80,30,40,0,0,1.5,15.0",
                    # Pseudo-random evasion centered at (0,0,25)
                    "swarm_evasion:0,0,25,60,60,15,12.0,1.5,42",
                ],
            }],
        ),
    ])
