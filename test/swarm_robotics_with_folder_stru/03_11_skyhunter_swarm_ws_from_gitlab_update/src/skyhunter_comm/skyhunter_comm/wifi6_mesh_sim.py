#!/usr/bin/env python3
"""
wifi6_mesh_sim - WiFi6 802.11s mesh RF simulation node

Calculates inter-robot distances via TF2, applies path loss model,
outputs per-link metrics. Auto-discovers robots from TF tree.

Subscribes to /jammed_links from jammer_service for RF jamming.

Config files (loaded directly):
  - skyhunter_gazebo/config/robot_params.yaml: robot/tf params
  - skyhunter_comm/config/wifi6_mesh_sim.yaml: discovery/rf_model/delay_model/packet_loss/rates
"""

import os
import math
import yaml
import rclpy
from rclpy.node import Node
from ament_index_python.packages import get_package_share_directory
from tf2_ros import Buffer, TransformListener, LookupException, ConnectivityException, ExtrapolationException

from skyhunter_msgs.msg import MeshLink, MeshMetrics, MeshLinkStates, JammedLinks

from .models import (
    RobotConfig,
    RobotDiscovery,
    RFModelConfig,
    DelayModelConfig,
    PacketLossConfig,
    PathLossModel,
)


class WiFi6MeshSim(Node):
    """WiFi6 mesh simulation node with auto-discovery."""

    def __init__(self):
        super().__init__('wifi6_mesh_sim')

        # Load config from YAML files directly
        robot_params, mesh_params = self._load_yaml_configs()
        
        # Build config objects
        robot_config = self._build_robot_config(robot_params)
        rf_config, delay_config, loss_config = self._build_mesh_config(mesh_params)

        # Initialize models
        self.discovery = RobotDiscovery(robot_config, self.get_logger())
        self.path_loss = PathLossModel(rf_config, delay_config, loss_config)
        self.common_frame = robot_config.common_frame

        # TF2 setup
        self.tf_buffer = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)

        # Jams storage: {(min_id, max_id): attenuation_db}
        # Updated by subscription to /jammed_links
        self.jams = {}

        # Link cache for states summary
        self.link_cache = []

        # Publishers
        self.metrics_pub = self.create_publisher(MeshMetrics, '/mesh_metrics', 10)
        self.states_pub = self.create_publisher(MeshLinkStates, '/mesh_link_states', 10)

        # Subscriber for jammed links (from jammer_service)
        self.jammed_links_sub = self.create_subscription(
            JammedLinks,
            '/jammed_links',
            self.jammed_links_callback,
            10
        )

        # Get rates and discovery settings from loaded params
        metrics_rate = mesh_params['rates']['metrics_hz']
        states_rate = mesh_params['rates']['states_hz']
        discovery_interval = mesh_params['discovery']['interval_sec']
        auto_discover = mesh_params['discovery']['auto_discover']

        # Timers
        self.metrics_timer = self.create_timer(1.0 / metrics_rate, self.metrics_callback)
        self.states_timer = self.create_timer(1.0 / states_rate, self.states_callback)

        if auto_discover:
            self.discovery_timer = self.create_timer(discovery_interval, self.discovery_callback)
            # Initial discovery after short delay (wait for TF)
            self.create_timer(2.0, self._initial_discovery)

        self.get_logger().info(
            f'wifi6_mesh_sim started: auto_discover={auto_discover}, '
            f'metrics={metrics_rate}Hz, states={states_rate}Hz'
        )

    def _load_yaml_configs(self) -> tuple:
        """Load config from YAML files directly."""
        pkg_gazebo = get_package_share_directory('skyhunter_gazebo')
        pkg_networking = get_package_share_directory('skyhunter_comm')

        # Load robot params
        robot_yaml_path = os.path.join(pkg_gazebo, 'config', 'robot_params.yaml')
        with open(robot_yaml_path) as f:
            robot_params = yaml.safe_load(f)['/**']['ros__parameters']

        # Load mesh params
        mesh_yaml_path = os.path.join(pkg_networking, 'config', 'wifi6_mesh_sim.yaml')
        with open(mesh_yaml_path) as f:
            mesh_params = yaml.safe_load(f)['wifi6_mesh_sim']['ros__parameters']

        self.get_logger().info(f'Loaded config from: {robot_yaml_path}, {mesh_yaml_path}')
        return robot_params, mesh_params

    def _build_robot_config(self, params: dict) -> RobotConfig:
        """Build robot configuration from loaded params."""
        return RobotConfig(
            prefix=params['robot']['prefix'],
            base_link=params['robot']['base_link'],
            odom_frame=params['robot']['odom_frame'],
            common_frame=params['tf']['common_frame'],
        )

    def _build_mesh_config(self, params: dict) -> tuple:
        """Build mesh-specific configuration from loaded params."""
        rf_config = RFModelConfig(
            path_loss_exponent=params['rf_model']['path_loss_exponent'],
            reference_rssi_dbm=params['rf_model']['reference_rssi_dbm'],
            reference_distance_m=params['rf_model']['reference_distance_m'],
            shadow_fading_std=params['rf_model']['shadow_fading_std'],
            disconnect_threshold_dbm=params['rf_model']['disconnect_threshold_dbm'],
        )

        delay_config = DelayModelConfig(
            base_delay_ms=params['delay_model']['base_delay_ms'],
            contention_delay_per_robot_ms=params['delay_model']['contention_delay_per_robot_ms'],
        )

        loss_config = PacketLossConfig(
            thresholds_dbm=params['packet_loss']['thresholds_dbm'],
            loss_percentages=params['packet_loss']['loss_percentages'],
        )

        return rf_config, delay_config, loss_config

    def _initial_discovery(self):
        """Run initial robot discovery."""
        self.discovery_callback()
        self.get_logger().info(
            f'Initial discovery: {self.discovery.robot_count} robots, '
            f'{self.discovery.link_count} links'
        )

    def discovery_callback(self):
        """Periodic robot discovery from TF tree."""
        old_count = self.discovery.robot_count
        self.discovery.discover_from_tf(self.tf_buffer)
        new_count = self.discovery.robot_count

        if new_count != old_count:
            self.get_logger().info(
                f'Robot count changed: {old_count} -> {new_count} '
                f'(robots: {self.discovery.robots})'
            )

    def get_robot_distance(self, robot_a: int, robot_b: int) -> float:
        """Get distance between two robots via TF2 lookup."""
        try:
            frame_a = self.discovery.get_robot_frame(robot_a)
            frame_b = self.discovery.get_robot_frame(robot_b)

            t1 = self.tf_buffer.lookup_transform(self.common_frame, frame_a, rclpy.time.Time())
            t2 = self.tf_buffer.lookup_transform(self.common_frame, frame_b, rclpy.time.Time())

            dx = t1.transform.translation.x - t2.transform.translation.x
            dy = t1.transform.translation.y - t2.transform.translation.y
            dz = t1.transform.translation.z - t2.transform.translation.z

            return math.sqrt(dx*dx + dy*dy + dz*dz)

        except (LookupException, ConnectivityException, ExtrapolationException):
            return -1.0

    def compute_link(self, robot_a: int, robot_b: int) -> MeshLink:
        """Compute metrics for a single link."""
        link = MeshLink()
        link.robot_a = str(robot_a)
        link.robot_b = str(robot_b)

        distance = self.get_robot_distance(robot_a, robot_b)
        if distance < 0:
            link.connected = False
            return link

        link.distance_m = distance

        # Get jam attenuation if any
        key = (min(robot_a, robot_b), max(robot_a, robot_b))
        jam_attenuation = self.jams.get(key, 0.0)

        # Compute all metrics using path loss model
        active_count = sum(1 for l in self.link_cache if l.connected)
        metrics = self.path_loss.compute_link_metrics(distance, active_count, jam_attenuation)

        link.rssi_dbm = metrics['rssi_dbm']
        link.connected = metrics['connected']
        link.packet_loss_pct = metrics['packet_loss_pct']
        link.delay_ms = metrics['delay_ms']
        link.jammed = metrics['jammed']

        return link

    def metrics_callback(self):
        """Publish mesh metrics."""
        links = []
        for robot_a, robot_b in self.discovery.get_robot_pairs():
            link = self.compute_link(robot_a, robot_b)
            links.append(link)

        self.link_cache = links

        msg = MeshMetrics()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.links = links
        self.metrics_pub.publish(msg)

    def states_callback(self):
        """Publish link states summary."""
        msg = MeshLinkStates()
        msg.header.stamp = self.get_clock().now().to_msg()

        connected = []
        disconnected = []
        rssi_sum = 0.0
        rssi_count = 0

        for link in self.link_cache:
            pair = f"{link.robot_a}-{link.robot_b}"
            if link.connected:
                connected.append(pair)
                rssi_sum += link.rssi_dbm
                rssi_count += 1
            else:
                disconnected.append(pair)

        msg.connected_pairs = connected
        msg.disconnected_pairs = disconnected
        msg.total_links = len(self.link_cache)
        msg.active_links = len(connected)
        msg.avg_rssi_dbm = rssi_sum / rssi_count if rssi_count > 0 else -100.0

        self.states_pub.publish(msg)

    def jammed_links_callback(self, msg: JammedLinks):
        """Update local jams dict from jammer_service."""
        self.jams.clear()
        for link in msg.jammed_links:
            a = int(link.robot_a)
            b = int(link.robot_b)
            key = (min(a, b), max(a, b))
            self.jams[key] = link.attenuation_db


def main(args=None):
    rclpy.init(args=args)
    node = WiFi6MeshSim()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()