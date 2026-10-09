#!/usr/bin/env python3
"""Teammate-masking lidar filter for swarm nav2 costmaps.

Each robot's lidar sees its formation teammates, which the nav2 obstacle layer
would otherwise mark as lethal obstacles — so robots block each other (planning
aborts) whenever the formation packs them close (column, tight slots, corners).

This node, run per robot inside its /sN namespace:
  - publishes the robot's own map-frame position to the shared /swarm/robot_world_pos
    (all robots share the same GNSS datum, so sN/map coordinates are comparable),
  - subscribes to that topic to learn teammates' world positions,
  - removes lidar points within mask_radius of any teammate from /sN/rslidar_points
    and republishes the cleaned cloud on /sN/rslidar_points_filtered, which the
    costmaps + collision_monitor consume instead of the raw cloud.

Static obstacles (boxes, walls) are untouched, so real obstacle avoidance is intact;
only the teammates are masked out.
"""
import math

import numpy as np
import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data

import tf2_ros
from geometry_msgs.msg import PointStamped
from sensor_msgs.msg import PointCloud2
from sensor_msgs_py import point_cloud2 as pc2
from std_msgs.msg import Bool


class SwarmLidarFilter(Node):
    def __init__(self):
        super().__init__('swarm_lidar_filter')
        self.robot_id = int(self.declare_parameter('robot_id', 1).value)
        self.map_frame = self.declare_parameter('map_frame', 'map').value
        self.base_frame = self.declare_parameter('base_frame', 'base_footprint').value
        self.mask_radius = float(self.declare_parameter('mask_radius_m', 1.4).value)

        self.tf_buffer = tf2_ros.Buffer()
        self.tf_listener = tf2_ros.TransformListener(self.tf_buffer, self)

        self.teammates = {}   # id -> (map_x, map_y)
        self.own_map = None
        self._last_proc = 0.0
        # Formation-aware: only mask teammates once formed up (steady formation), so they
        # don't perturb each other. During form-up/maneuvers we DON'T mask, so robots see
        # + route around each other (no ramming). Executor drives /sN/mask_teammates.
        self.mask_enabled = False
        self.create_subscription(Bool, 'mask_teammates',
                                 lambda m: setattr(self, 'mask_enabled', m.data), 10)

        self.pose_pub = self.create_publisher(PointStamped, '/swarm/robot_world_pos', 10)
        self.create_subscription(PointStamped, '/swarm/robot_world_pos', self.on_pose, 10)
        self.create_timer(0.1, self.publish_own_pose)

        self.cloud_pub = self.create_publisher(PointCloud2, 'rslidar_points_filtered',
                                               qos_profile_sensor_data)
        self.create_subscription(PointCloud2, 'rslidar_points', self.on_cloud,
                                 qos_profile_sensor_data)
        self.get_logger().info(
            f'swarm_lidar_filter up: id={self.robot_id} map={self.map_frame} '
            f'mask={self.mask_radius}m')

    def publish_own_pose(self):
        try:
            t = self.tf_buffer.lookup_transform(
                self.map_frame, self.base_frame, rclpy.time.Time())
        except Exception:
            return
        self.own_map = (t.transform.translation.x, t.transform.translation.y)
        m = PointStamped()
        m.header.stamp = self.get_clock().now().to_msg()
        m.header.frame_id = str(self.robot_id)
        m.point.x, m.point.y = self.own_map
        self.pose_pub.publish(m)

    def on_pose(self, m):
        try:
            rid = int(m.header.frame_id)
        except ValueError:
            return
        if rid != self.robot_id:
            self.teammates[rid] = (m.point.x, m.point.y)

    def on_cloud(self, msg):
        # Throttle: filtering full clouds in Python is costly; the costmap doesn't need
        # full lidar rate. Process at most ~4 Hz to keep CPU (and the ekf) breathing.
        now = self.get_clock().now().nanoseconds * 1e-9
        if now - self._last_proc < 0.24:
            return
        self._last_proc = now
        mates = list(self.teammates.values())
        if not self.mask_enabled or not mates:
            self.cloud_pub.publish(msg)   # form-up/maneuver: see teammates (avoid), or none
            return
        # teammate world positions -> this cloud's frame
        try:
            tf = self.tf_buffer.lookup_transform(
                msg.header.frame_id, self.map_frame, rclpy.time.Time())
        except Exception:
            self.cloud_pub.publish(msg)   # no TF yet -> pass through (don't drop data)
            return
        tx = tf.transform.translation.x
        ty = tf.transform.translation.y
        q = tf.transform.rotation
        yaw = math.atan2(2.0 * (q.w * q.z + q.x * q.y),
                         1.0 - 2.0 * (q.y * q.y + q.z * q.z))
        c, s = math.cos(yaw), math.sin(yaw)
        mates_local = []
        for (mx, my) in mates:
            lx = c * mx - s * my + tx
            ly = s * mx + c * my + ty
            mates_local.append((lx, ly))

        try:
            pts = pc2.read_points(msg, skip_nans=False)
        except Exception:
            self.cloud_pub.publish(msg)
            return
        if len(pts) == 0:
            self.cloud_pub.publish(msg)
            return
        x = np.asarray(pts['x'], dtype=np.float64)
        y = np.asarray(pts['y'], dtype=np.float64)
        keep = np.ones(x.shape, dtype=bool)
        r2 = self.mask_radius * self.mask_radius
        for (lx, ly) in mates_local:
            keep &= ((x - lx) ** 2 + (y - ly) ** 2) > r2
        out = pc2.create_cloud(msg.header, msg.fields, pts[keep].tolist())
        self.cloud_pub.publish(out)


def main():
    rclpy.init()
    node = SwarmLidarFilter()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
