#!/usr/bin/env python3
# Copyright 2026 SkyAutoNet Inc.
#
# Proprietary and confidential. Unauthorized copying, distribution, or use
# of this file, via any medium, is strictly prohibited.

"""
wifi6_mesh_sim - WiFi6 802.11s mesh RF simulation node

Calculates inter-robot distances via TF2, applies path loss model,
outputs per-link metrics. Auto-discovers robots from TF tree.

Subscribes to /jammed_links was REMOVED (Soft Kill 제외 — 본 시스템은
RF 재머 미통합).

Config: loaded via ROS 2 parameter system (networking.launch.py).
  - robot_params.yaml (skyhunter_bringup): robot/tf params
  - san_comm_sim.yaml (san_comm_sim):  RF model, delay, packet loss, rates
"""

import math

import rclpy
from rclpy.node import Node
from tf2_ros import (
    Buffer,
    TransformListener,
    LookupException,
    ConnectivityException,
    ExtrapolationException,
)

from san_comm_msgs.msg import MeshLink, MeshMetrics, MeshLinkStates
# Soft Kill 제외: JammedLinks 제거 (본 시스템은 RF 재머 미통합)

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

        # ── Parameters ───────────────────────────────────────────────────────
        # Robot / TF params — from robot_params.yaml (skyhunter_bringup)
        self.declare_parameter('robot.prefix','SH_')
        self.declare_parameter('robot.base_link','base_link')
        self.declare_parameter('robot.odom_frame','odom')
        self.declare_parameter('tf.common_frame','world')

        # Discovery — from san_comm_sim.yaml
        self.declare_parameter('auto_discover', True)
        self.declare_parameter('discovery_interval_sec', 5.0)

        # RF path loss model
        self.declare_parameter('path_loss_exponent', 2.8)
        self.declare_parameter('reference_rssi_dbm', -30.0)
        self.declare_parameter('reference_distance_m', 1.0)
        self.declare_parameter('shadow_fading_std', 4.0)
        self.declare_parameter('disconnect_threshold_dbm', -85.0)

        # Delay model
        self.declare_parameter('base_delay_ms', 2.0)
        self.declare_parameter('contention_delay_per_robot_ms', 0.5)

        # Packet loss model
        self.declare_parameter('loss_thresholds_dbm', [-50.0, -60.0, -70.0, -80.0, -85.0])
        self.declare_parameter('loss_percentages', [0.0, 1.0, 5.0, 15.0, 30.0, 100.0])

        # Publish rates
        self.declare_parameter('metrics_hz', 10.0)
        self.declare_parameter('states_hz', 1.0)

        p = self.get_parameter

        # ── Build config objects ──────────────────────────────────────────────
        robot_config = RobotConfig(
            prefix       = p('robot.prefix').value,
            base_link    = p('robot.base_link').value,
            odom_frame   = p('robot.odom_frame').value,
            common_frame = p('tf.common_frame').value,
        )
        rf_config = RFModelConfig(
            path_loss_exponent       = p('path_loss_exponent').value,
            reference_rssi_dbm       = p('reference_rssi_dbm').value,
            reference_distance_m     = p('reference_distance_m').value,
            shadow_fading_std        = p('shadow_fading_std').value,
            disconnect_threshold_dbm = p('disconnect_threshold_dbm').value,
        )
        delay_config = DelayModelConfig(
            base_delay_ms                 = p('base_delay_ms').value,
            contention_delay_per_robot_ms = p('contention_delay_per_robot_ms').value,
        )
        loss_config = PacketLossConfig(
            thresholds_dbm   = p('loss_thresholds_dbm').value,
            loss_percentages = p('loss_percentages').value,
        )

        auto_discover      = p('auto_discover').value
        discovery_interval = p('discovery_interval_sec').value
        metrics_rate       = p('metrics_hz').value
        states_rate        = p('states_hz').value

        # ── Models ────────────────────────────────────────────────────────────
        self.discovery    = RobotDiscovery(robot_config, self.get_logger())
        self.path_loss    = PathLossModel(rf_config, delay_config, loss_config)
        self.common_frame = robot_config.common_frame

        # ── TF2 ───────────────────────────────────────────────────────────────
        self.tf_buffer   = Buffer()
        self.tf_listener = TransformListener(self.tf_buffer, self)

        # ── State ─────────────────────────────────────────────────────────────
        self.jams       = {}   # {(min_id, max_id): attenuation_db}
        self.link_cache = []

        # ── Publishers ────────────────────────────────────────────────────────
        self.metrics_pub = self.create_publisher(MeshMetrics,    '/mesh_metrics',     10)
        self.states_pub  = self.create_publisher(MeshLinkStates, '/mesh_link_states', 10)

        # ── Subscriber ────────────────────────────────────────────────────────
        # Soft Kill 제외: /jammed_links subscription 제거.
        # self.jams 는 영구히 빈 dict — RF 감쇠 시뮬레이션 비활성.

        # ── Timers ────────────────────────────────────────────────────────────
        self.create_timer(1.0 / metrics_rate, self.metrics_callback)
        self.create_timer(1.0 / states_rate,  self.states_callback)

        if auto_discover:
            self.create_timer(discovery_interval, self.discovery_callback)
            self.create_timer(2.0, self._initial_discovery)   # wait for TF to be available

        self.get_logger().info(
            f'wifi6_mesh_sim started: auto_discover={auto_discover}, '
            f'metrics={metrics_rate}Hz, states={states_rate}Hz'
        )

    # ── Discovery ─────────────────────────────────────────────────────────────

    def _initial_discovery(self):
        self.discovery_callback()
        self.get_logger().info(
            f'Initial discovery: {self.discovery.robot_count} robots, '
            f'{self.discovery.link_count} links'
        )

    def discovery_callback(self):
        old_count = self.discovery.robot_count
        self.discovery.discover_from_tf(self.tf_buffer)
        new_count = self.discovery.robot_count

        if new_count != old_count:
            self.get_logger().info(
                f'Robot count changed: {old_count} -> {new_count} '
                f'(robots: {self.discovery.robots})'
            )

    # ── Link computation ──────────────────────────────────────────────────────

    def get_robot_distance(self, robot_a: int, robot_b: int) -> float:
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
        link          = MeshLink()
        link.robot_a  = str(robot_a)
        link.robot_b  = str(robot_b)

        distance = self.get_robot_distance(robot_a, robot_b)
        if distance < 0:
            link.connected = False
            return link

        link.distance_m = distance
        key             = (min(robot_a, robot_b), max(robot_a, robot_b))
        jam_attenuation = self.jams.get(key, 0.0)
        active_count    = sum(1 for l in self.link_cache if l.connected)
        metrics         = self.path_loss.compute_link_metrics(distance, active_count, jam_attenuation)

        link.rssi_dbm        = metrics['rssi_dbm']
        link.connected       = metrics['connected']
        link.packet_loss_pct = metrics['packet_loss_pct']
        link.delay_ms        = metrics['delay_ms']
        link.jammed          = metrics['jammed']

        return link

    # ── Timer callbacks ───────────────────────────────────────────────────────

    def metrics_callback(self):
        links            = [self.compute_link(a, b) for a, b in self.discovery.get_robot_pairs()]
        self.link_cache  = links

        msg              = MeshMetrics()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.links        = links
        self.metrics_pub.publish(msg)

    def states_callback(self):
        connected    = []
        disconnected = []
        rssi_sum     = 0.0
        rssi_count   = 0

        for link in self.link_cache:
            pair = f"{link.robot_a}-{link.robot_b}"
            if link.connected:
                connected.append(pair)
                rssi_sum   += link.rssi_dbm
                rssi_count += 1
            else:
                disconnected.append(pair)

        msg                    = MeshLinkStates()
        msg.header.stamp       = self.get_clock().now().to_msg()
        msg.connected_pairs    = connected
        msg.disconnected_pairs = disconnected
        msg.total_links        = len(self.link_cache)
        msg.active_links       = len(connected)
        msg.avg_rssi_dbm       = rssi_sum / rssi_count if rssi_count > 0 else -100.0

        self.states_pub.publish(msg)

    # jammed_links_callback 제거 (Soft Kill 제외)
    # self.jams 는 ctor 에서 빈 dict 로 초기화된 상태 유지 — 모든 link metric
    # 계산은 attenuation_db=0 으로 진행


def main(args=None):
    rclpy.init(args=args)
    node = WiFi6MeshSim()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()