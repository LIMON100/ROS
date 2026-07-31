 #!/usr/bin/env python3
"""Stable, smoothed heading for the Go2 leader (s1) -> /s1/gps/heading_imu.
Low-pass filters the wobbling gait heading so nav2 gets a steady yaw."""
import math
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from sensor_msgs.msg import Imu

class HeadingRelay(Node):
    def __init__(self, ns='s1'):
        super().__init__('go2_heading_relay')
        self.yaw_f = None
        self.ALPHA = 0.02                     # ~0.4 s smoothing at 140 Hz
        self.pub = self.create_publisher(Imu, f'/{ns}/gps/heading_imu', 10)
        self.create_subscription(Odometry, f'/{ns}/odom', self._cb, 10) 
        self.get_logger().info(f'smoothed heading relay: /{ns}/odom -> /{ns}/gps/heading_imu')
    def _cb(self, m):
        q = m.pose.pose.orientation
        yaw = math.atan2(2*(q.w*q.z + q.x*q.y), 1 - 2*(q.y*q.y + q.z*q.z))
        if self.yaw_f is None:
            self.yaw_f = yaw
        else:
            d = math.atan2(math.sin(yaw - self.yaw_f), math.cos(yaw - self.yaw_f))
            self.yaw_f += self.ALPHA * d
        out = Imu()
        out.header.stamp = m.header.stamp
        out.header.frame_id = 'gps'
        out.orientation.z = math.sin(self.yaw_f / 2.0)
        out.orientation.w = math.cos(self.yaw_f / 2.0)
        out.orientation_covariance[0] = 999.0
        out.orientation_covariance[4] = 999.0
        out.orientation_covariance[8] = 0.001
        out.angular_velocity_covariance[0] = -1.0
        out.linear_acceleration_covariance[0] = -1.0
        self.pub.publish(out)
        
def main():
    rclpy.init(); n = HeadingRelay()
    try: rclpy.spin(n)
    except KeyboardInterrupt: pass
    finally: n.destroy_node(); rclpy.shutdown()
    
if __name__ == '__main__':
    main()
    