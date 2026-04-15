#!/usr/bin/env python3
"""
jammer_service - Standalone RF jamming management service

Provides services to jam/unjam mesh links and publishes current jam state.
Can be called by any node (test scripts, operator console, etc.)

Config: loaded via ROS 2 parameter system (networking.launch.py).
  - skyhunter_comm.yaml (skyhunter_comm): publish_rate_hz, default_attenuation_db
"""

import rclpy
from rclpy.node import Node

from skyhunter_msgs.msg import JammedLink, JammedLinks
from skyhunter_msgs.srv import JamLink, UnjamLink
from std_srvs.srv import Trigger


class JammerService(Node):
    """Standalone jamming management service."""

    def __init__(self):
        super().__init__('jammer_service')

        # ── Parameters ───────────────────────────────────────────────────────
        self.declare_parameter('publish_rate_hz',       10.0)
        self.declare_parameter('default_attenuation_db', 50.0)

        publish_rate             = self.get_parameter('publish_rate_hz').value
        self.default_attenuation = self.get_parameter('default_attenuation_db').value

        # Jams storage: {(min_id, max_id): {'attenuation': float, 'expire_time': float or None}}
        self.jams = {}

        # Publisher for current jam state
        self.jammed_links_pub = self.create_publisher(
            JammedLinks,
            '/jammed_links',
            10
        )

        # Services
        self.jam_srv = self.create_service(
            JamLink,
            '/jam_link',
            self.jam_link_callback
        )
        self.unjam_srv = self.create_service(
            UnjamLink,
            '/unjam_link',
            self.unjam_link_callback
        )
        self.clear_srv = self.create_service(
            Trigger,
            '/clear_all_jams',
            self.clear_all_jams_callback
        )

        # Timer to publish jam state and check expirations
        self.publish_timer = self.create_timer(
            1.0 / publish_rate,
            self.timer_callback
        )

        self.get_logger().info(
            f'jammer_service started: publish_rate={publish_rate}Hz'
        )


    def _make_key(self, robot_a: str, robot_b: str) -> tuple:
        """Create canonical key for link (smaller ID first)."""
        a = int(robot_a)
        b = int(robot_b)
        return (min(a, b), max(a, b))

    def _check_expirations(self):
        """Remove expired jams."""
        now = self.get_clock().now().nanoseconds / 1e9
        expired = []
        for key, data in self.jams.items():
            if data['expire_time'] is not None and now >= data['expire_time']:
                expired.append(key)
        
        for key in expired:
            del self.jams[key]
            self.get_logger().info(f"Jam {key[0]}-{key[1]} expired")

    def timer_callback(self):
        """Check expirations and publish jammed links state."""
        self._check_expirations()
        self.publish_jammed_links()

    def publish_jammed_links(self):
        """Publish current jammed links state."""
        msg = JammedLinks()
        msg.header.stamp = self.get_clock().now().to_msg()

        for (a, b), data in self.jams.items():
            link = JammedLink()
            link.robot_a = str(a)
            link.robot_b = str(b)
            link.attenuation_db = data['attenuation']
            msg.jammed_links.append(link)

        self.jammed_links_pub.publish(msg)

    def jam_link_callback(self, request, response):
        """Service: Apply jam attenuation to a link."""
        key = self._make_key(request.robot_a, request.robot_b)
        
        # Use default attenuation if not specified or zero
        attenuation = request.attenuation_db
        if attenuation <= 0:
            attenuation = self.default_attenuation

        # Calculate expiration time
        expire_time = None
        if request.duration_s > 0:
            now = self.get_clock().now().nanoseconds / 1e9
            expire_time = now + request.duration_s

        self.jams[key] = {
            'attenuation': attenuation,
            'expire_time': expire_time
        }
        
        duration_str = f" for {request.duration_s}s" if request.duration_s > 0 else " (permanent)"
        response.success = True
        response.message = f"Jammed link {key[0]}-{key[1]} with {attenuation} dB{duration_str}"
        self.get_logger().info(response.message)
        return response

    def unjam_link_callback(self, request, response):
        """Service: Remove jam from a link."""
        key = self._make_key(request.robot_a, request.robot_b)
        
        if key in self.jams:
            del self.jams[key]
            response.success = True
            response.message = f"Unjammed link {key[0]}-{key[1]}"
        else:
            response.success = False
            response.message = f"Link {key[0]}-{key[1]} was not jammed"
        
        self.get_logger().info(response.message)
        return response

    def clear_all_jams_callback(self, request, response):
        """Service: Clear all active jams."""
        count = len(self.jams)
        self.jams.clear()
        
        response.success = True
        response.message = f"Cleared {count} jammed links"
        self.get_logger().info(response.message)
        return response


def main(args=None):
    rclpy.init(args=args)
    node = JammerService()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()


if __name__ == '__main__':
    main()