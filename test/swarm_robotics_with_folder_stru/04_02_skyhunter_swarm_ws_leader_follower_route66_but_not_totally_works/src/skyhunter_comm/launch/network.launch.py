#!/usr/bin/env python3
"""
networking.launch.py — SkyHunter Comm full networking stack

Starts all 5 communication nodes with their YAML configs loaded.
Includes common_config.launch.py first (sets ROS_DOMAIN_ID + FASTDDS env vars).

Usage:
    ros2 launch skyhunter_comm networking.launch.py

Config files:
    skyhunter_gazebo/config/robot_params.yaml   — shared robot/TF params
    skyhunter_comm/config/skyhunter_comm.yaml   — all comm node params
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description() -> LaunchDescription:

    comm_share   = get_package_share_directory('skyhunter_comm')
    gazebo_share = get_package_share_directory('skyhunter_gazebo')

    # ── Config files ──────────────────────────────────────────────────────────
    # robot_params.yaml is owned by skyhunter_gazebo (single source of truth).
    # skyhunter_comm references it at runtime — no local copy needed.
    robot_params = os.path.join(gazebo_share, 'config', 'robot_params.yaml')
    comm_config  = os.path.join(comm_share,   'config', 'skyhunter_comm.yaml')


    # ── Node 1: WiFi6 mesh simulator ──────────────────────────────────────────
    wifi6_mesh_sim = Node(
        package    = 'skyhunter_comm',
        executable = 'wifi6_mesh_simulator',
        name       = 'wifi6_mesh_simulator',
        output     = 'screen',
        parameters = [robot_params, comm_config],
    )

    # ── Node 2: Swarm comm manager (FSM) ──────────────────────────────────────
    swarm_comm_manager = Node(
        package    = 'skyhunter_comm',
        executable = 'swarm_comm_manager',
        name       = 'swarm_comm_manager',
        output     = 'screen',
        parameters = [robot_params, comm_config],
    )

    # ── Node 3: LTE simulator ─────────────────────────────────────────────────
    lte_simulator = Node(
        package    = 'skyhunter_comm',
        executable = 'lte_simulator',
        name       = 'lte_simulator',
        output     = 'screen',
        parameters = [comm_config],
    )

    # ── Node 4: Comm traffic filter ───────────────────────────────────────────
    comm_traffic_filter = Node(
        package    = 'skyhunter_comm',
        executable = 'comm_traffic_filter',
        name       = 'comm_traffic_filter',
        output     = 'screen',
        parameters = [robot_params, comm_config],
    )

    # ── Node 5: LoRa simulator ────────────────────────────────────────────────
    lora_simulator = Node(
        package    = 'skyhunter_comm',
        executable = 'lora_simulator',
        name       = 'lora_simulator',
        output     = 'screen',
        parameters = [robot_params, comm_config],
    )
    # ── Node 6: Jammer service ────────────────────────────────────────────────
    jammer_service = Node(
        package    = 'skyhunter_comm',
        executable = 'jammer_service',
        name       = 'jammer_service',
        output     = 'screen',
        parameters = [comm_config],
    )
    return LaunchDescription([
        wifi6_mesh_sim,
        swarm_comm_manager,
        lte_simulator,
        comm_traffic_filter,
        lora_simulator,
        jammer_service
    ])