#!/usr/bin/env python3
"""GNSS status bridge.

Republishes the real GNSS sensor topics produced by gnss_heading.py
  - sensor_msgs/NavSatFix      on /fix          (position + fix quality)
  - std_msgs/Float64           on /edge_heading (true-north heading, 0~360 deg)
  - geometry_msgs/TwistStamped on /vel          (ENU ground-velocity components)
into a single combat_robot_msgs/GnssStatus on /gnss/status, which robot_server's
command_server subscribes to for the app's leader latitude/longitude/heading/speed
(replacing the DEFAULT_GPS placeholder once a real fix is available).

Validity contract (see GnssStatus.msg): until a fresh, valid fix exists the
position is published as NaN and heading/speed/accuracy as -1.0 with
fix_status=FIX_NONE, so command_server's isfinite/range guards reject placeholders.
"""
import math

import rclpy
from rclpy.node import Node
from rclpy.qos import qos_profile_sensor_data

from std_msgs.msg import Float64
from sensor_msgs.msg import NavSatFix, NavSatStatus
from geometry_msgs.msg import TwistStamped
from combat_robot_msgs.msg import GnssStatus


class GnssStatusBridge(Node):
    def __init__(self):
        super().__init__('gnss_status_bridge')

        self.declare_parameter('publish_rate_hz', 5.0)
        self.declare_parameter('fix_timeout_sec', 2.0)
        self.declare_parameter('heading_timeout_sec', 2.0)
        self.declare_parameter('vel_timeout_sec', 2.0)
        # NavSatFix.status collapses RTK float/fixed into GBAS_FIX, so the RTK grade
        # is refined from the reported horizontal accuracy.
        self.declare_parameter('rtk_fixed_max_acc_m', 0.15)
        self.declare_parameter('rtk_float_max_acc_m', 0.50)

        self._publish_rate = float(self.get_parameter('publish_rate_hz').value)
        self._fix_timeout = float(self.get_parameter('fix_timeout_sec').value)
        self._heading_timeout = float(self.get_parameter('heading_timeout_sec').value)
        self._vel_timeout = float(self.get_parameter('vel_timeout_sec').value)
        self._rtk_fixed_acc = float(self.get_parameter('rtk_fixed_max_acc_m').value)
        self._rtk_float_acc = float(self.get_parameter('rtk_float_max_acc_m').value)

        self._fix = None
        self._fix_stamp = None
        self._heading = None
        self._heading_stamp = None
        self._speed = None
        self._speed_stamp = None

        # gnss_heading.py publishes /fix, /edge_heading, /vel with default (reliable,
        # depth 10) QoS; match it on the subscriber side.
        self.create_subscription(NavSatFix, '/fix', self._on_fix, 10)
        self.create_subscription(Float64, '/edge_heading', self._on_heading, 10)
        self.create_subscription(TwistStamped, '/vel', self._on_vel, 10)

        # command_server subscribes /gnss/status with SensorDataQoS (best-effort).
        self._pub = self.create_publisher(GnssStatus, '/gnss/status', qos_profile_sensor_data)

        period = 1.0 / self._publish_rate if self._publish_rate > 0.0 else 0.2
        self.create_timer(period, self._publish)

        self.get_logger().info(
            'gnss_status_bridge started: /fix + /edge_heading + /vel -> /gnss/status '
            f'@ {self._publish_rate:.1f} Hz'
        )

    def _on_fix(self, msg):
        self._fix = msg
        self._fix_stamp = self.get_clock().now()

    def _on_heading(self, msg):
        self._heading = float(msg.data)
        self._heading_stamp = self.get_clock().now()

    def _on_vel(self, msg):
        self._speed = math.hypot(msg.twist.linear.x, msg.twist.linear.y)
        self._speed_stamp = self.get_clock().now()

    def _age_sec(self, stamp):
        if stamp is None:
            return None
        return (self.get_clock().now() - stamp).nanoseconds / 1e9

    def _grade_fix(self, navsat_status, h_acc):
        if navsat_status == NavSatStatus.STATUS_GBAS_FIX:
            if h_acc < 0.0:
                # Accuracy unavailable: keep an RTK-class grade rather than demoting
                # an actual GBAS/RTK solution to DGPS just because sigma is unknown.
                return GnssStatus.FIX_RTK_FLOAT
            if h_acc <= self._rtk_fixed_acc:
                return GnssStatus.FIX_RTK_FIXED
            if h_acc <= self._rtk_float_acc:
                return GnssStatus.FIX_RTK_FLOAT
            return GnssStatus.FIX_DGPS
        if navsat_status == NavSatStatus.STATUS_SBAS_FIX:
            return GnssStatus.FIX_DGPS
        if navsat_status == NavSatStatus.STATUS_FIX:
            return GnssStatus.FIX_3D
        return GnssStatus.FIX_NONE

    def _publish(self):
        out = GnssStatus()
        out.header.stamp = self.get_clock().now().to_msg()
        out.header.frame_id = 'gps'

        fix_age = self._age_sec(self._fix_stamp)
        fix_fresh = (
            self._fix is not None
            and fix_age is not None
            and fix_age <= self._fix_timeout
        )

        # Horizontal/vertical 1-sigma accuracy from the ENU diagonal covariance.
        h_acc = -1.0
        v_acc = -1.0
        if fix_fresh and self._fix.position_covariance_type != NavSatFix.COVARIANCE_TYPE_UNKNOWN:
            cov = self._fix.position_covariance
            var_e, var_n, var_u = cov[0], cov[4], cov[8]
            if var_e >= 0.0 and var_n >= 0.0:
                h_acc = math.sqrt(max((var_e + var_n) / 2.0, 0.0))
            if var_u >= 0.0:
                v_acc = math.sqrt(max(var_u, 0.0))

        if (fix_fresh
                and self._fix.status.status != NavSatStatus.STATUS_NO_FIX
                and math.isfinite(self._fix.latitude)
                and math.isfinite(self._fix.longitude)):
            out.latitude = float(self._fix.latitude)
            out.longitude = float(self._fix.longitude)
            out.altitude_m = float(self._fix.altitude)
            out.fix_status = self._grade_fix(self._fix.status.status, h_acc)
            out.horizontal_accuracy_m = float(h_acc)
            out.vertical_accuracy_m = float(v_acc)
        else:
            out.latitude = float('nan')
            out.longitude = float('nan')
            out.altitude_m = float('nan')
            out.fix_status = GnssStatus.FIX_NONE
            out.horizontal_accuracy_m = -1.0
            out.vertical_accuracy_m = -1.0

        # Satellite count is not exposed by the upstream NMEA bridge.
        out.num_satellites = 0

        heading_age = self._age_sec(self._heading_stamp)
        if (self._heading is not None
                and heading_age is not None
                and heading_age <= self._heading_timeout
                and math.isfinite(self._heading)):
            out.heading_deg = float(self._heading % 360.0)
        else:
            out.heading_deg = -1.0

        speed_age = self._age_sec(self._speed_stamp)
        if (self._speed is not None
                and speed_age is not None
                and speed_age <= self._vel_timeout
                and math.isfinite(self._speed)
                and self._speed >= 0.0):
            out.ground_speed_mps = float(self._speed)
        else:
            out.ground_speed_mps = -1.0

        self._pub.publish(out)


def main():
    rclpy.init()
    node = GnssStatusBridge()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        try:
            if rclpy.ok():
                rclpy.shutdown()
        except Exception:
            pass


if __name__ == '__main__':
    main()
