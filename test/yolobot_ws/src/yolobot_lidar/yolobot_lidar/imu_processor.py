#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Imu
import math
from rclpy.qos import qos_profile_sensor_data

class ImuProcessorNode(Node):
    def __init__(self):
        super().__init__('imu_processor')

        # Subscribe to the topic defined in your URDF plugin
        self.subscription = self.create_subscription(
            Imu,
            '/gazebo_ros_imu_sensor/out',           # Added leading slash '/' just in case
            self.imu_callback,
            qos_profile_sensor_data # <--- THIS FIXES THE SILENCE
        )
        
        self.get_logger().info('IMU Processor Started. Waiting for data...')

    def euler_from_quaternion(self, x, y, z, w):
        """
        Converts Quaternion (x, y, z, w) to Euler Angles (Roll, Pitch, Yaw).
        Returns angles in RADIANS.
        """
        # Roll (x-axis rotation)
        t0 = +2.0 * (w * x + y * z)
        t1 = +1.0 - 2.0 * (x * x + y * y)
        roll_x = math.atan2(t0, t1)

        # Pitch (y-axis rotation)
        t2 = +2.0 * (w * y - z * x)
        t2 = +1.0 if t2 > +1.0 else t2
        t2 = -1.0 if t2 < -1.0 else t2
        pitch_y = math.asin(t2)

        # Yaw (z-axis rotation) - THIS IS THE DIRECTION
        t3 = +2.0 * (w * z + x * y)
        t4 = +1.0 - 2.0 * (y * y + z * z)
        yaw_z = math.atan2(t3, t4)

        return roll_x, pitch_y, yaw_z

    def imu_callback(self, msg):
        # 1. Get Orientation (Quaternion)
        q = msg.orientation
        
        # 2. Convert to Euler Angles (Radians)
        roll, pitch, yaw = self.euler_from_quaternion(q.x, q.y, q.z, q.w)

        # 3. Convert Radians to Degrees for readability
        roll_deg = math.degrees(roll)
        pitch_deg = math.degrees(pitch)
        yaw_deg = math.degrees(yaw)

        # 4. Get Angular Velocity (How fast are we turning?)
        turn_rate = msg.angular_velocity.z # radians per second

        # 5. Get Linear Acceleration (Are we speeding up?)
        accel_x = msg.linear_acceleration.x

        # Logic to determine direction string
        direction = "Unknown"
        if -5 < yaw_deg < 5: direction = "North (Front)"
        elif 85 < yaw_deg < 95: direction = "West (Left)"
        elif -95 < yaw_deg < -85: direction = "East (Right)"
        elif 175 < abs(yaw_deg) <= 180: direction = "South (Back)"

        # Print Output
        print(f"--- IMU STATUS ---")
        print(f"Heading (Yaw): {yaw_deg:.2f}° [{direction}]")
        print(f"Pitch (Tilt):  {pitch_deg:.2f}°")
        print(f"Turning Speed: {turn_rate:.3f} rad/s")
        print(f"Acceleration:  {accel_x:.3f} m/s^2")
        print("------------------")

def main(args=None):
    rclpy.init(args=args)
    node = ImuProcessorNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()