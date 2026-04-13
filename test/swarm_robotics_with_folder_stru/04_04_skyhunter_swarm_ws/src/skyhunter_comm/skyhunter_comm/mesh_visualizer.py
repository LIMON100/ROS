#!/usr/bin/env python3
"""Mesh network visualizer - subscribes to metrics, publishes RViz markers."""

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy

from skyhunter_msgs.msg import MeshMetrics, JammedLinks
from visualization_msgs.msg import Marker, MarkerArray
from geometry_msgs.msg import Point
from std_msgs.msg import ColorRGBA

import tf2_ros


class MeshVisualizer(Node):
    def __init__(self):
        super().__init__('mesh_visualizer')
        
        # TF for robot positions
        self.tf_buffer = tf2_ros.Buffer()
        self.tf_listener = tf2_ros.TransformListener(self.tf_buffer, self)
        
        # Subscribers
        self.create_subscription(MeshMetrics, '/mesh_metrics', self.metrics_cb, 10)
        self.create_subscription(JammedLinks, '/jammed_links', self.jammed_cb, 10)
        
        # Publisher
        self.viz_pub = self.create_publisher(MarkerArray, '/mesh_viz', 10)
        
        # State
        self.jammed_pairs = set()
        
        self.get_logger().info('Mesh visualizer started')

    def jammed_cb(self, msg):
        self.jammed_pairs = {
            (j.robot_a, j.robot_b) for j in msg.jammed_links
        }

    def metrics_cb(self, msg):
        markers = MarkerArray()
        
        for i, link in enumerate(msg.links):
            m = Marker()
            m.header.frame_id = "world"
            m.header.stamp = self.get_clock().now().to_msg()
            m.ns = "mesh_links"
            m.id = i
            m.type = Marker.LINE_STRIP
            m.action = Marker.ADD
            
            # Get positions from TF
            try:
                tf_a = self.tf_buffer.lookup_transform('world', f'{link.robot_a}/base_link', rclpy.time.Time())
                tf_b = self.tf_buffer.lookup_transform('world', f'{link.robot_b}/base_link', rclpy.time.Time())
                
                m.points = [
                    Point(x=tf_a.transform.translation.x, 
                          y=tf_a.transform.translation.y, 
                          z=tf_a.transform.translation.z + 0.5),
                    Point(x=tf_b.transform.translation.x, 
                          y=tf_b.transform.translation.y, 
                          z=tf_b.transform.translation.z + 0.5)
                ]
            except tf2_ros.LookupException:
                continue
            
            # Check if jammed
            is_jammed = (link.robot_a, link.robot_b) in self.jammed_pairs or \
                        (link.robot_b, link.robot_a) in self.jammed_pairs
            
            # Style
            m.scale.x = 0.08 if is_jammed else 0.05
            m.color = self._get_color(link.rssi, link.connected, is_jammed)
            m.lifetime.sec = 1
            
            markers.markers.append(m)
        
        self.viz_pub.publish(markers)

    def _get_color(self, rssi, connected, jammed):
        if jammed:
            return ColorRGBA(r=1.0, g=0.0, b=1.0, a=0.9)  # Magenta = jammed
        if not connected:
            return ColorRGBA(r=0.3, g=0.3, b=0.3, a=0.3)  # Grey = disconnected
        if rssi > -50:
            return ColorRGBA(r=0.0, g=1.0, b=0.0, a=0.8)  # Green
        elif rssi > -70:
            return ColorRGBA(r=1.0, g=1.0, b=0.0, a=0.8)  # Yellow
        elif rssi > -85:
            return ColorRGBA(r=1.0, g=0.5, b=0.0, a=0.8)  # Orange
        else:
            return ColorRGBA(r=1.0, g=0.0, b=0.0, a=0.5)  # Red


def main(args=None):
    rclpy.init(args=args)
    node = MeshVisualizer()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()