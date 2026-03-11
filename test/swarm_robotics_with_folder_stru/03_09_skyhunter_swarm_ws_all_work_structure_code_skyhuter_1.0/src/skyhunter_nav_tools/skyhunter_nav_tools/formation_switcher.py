#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from std_msgs.msg import Int8 # We use Int8 to match formation_type
import sys
import time

class FormationSwitcher(Node):
    def __init__(self, mode):
        super().__init__('formation_switcher')
        # We publish to a command topic that the Leader listens to
        self.publisher_ = self.create_publisher(Int8, '/swarm/formation_command', 10)
        
        msg = Int8()
        if mode.lower() == "column":
            msg.data = 1
        elif mode.lower() == "diamond":
            msg.data = 2
        else:
            msg.data = 0 # V-Shape
            
        # Broadcast the command
        for _ in range(5):
            self.publisher_.publish(msg)
            self.get_logger().info(f'Sending Command: {mode} ({msg.data})')
            import time
            time.sleep(0.1)
        
        raise SystemExit # Exit after sending

def main():
    import sys
    if len(sys.argv) < 2:
        print("Usage: ros2 run skyhunter_nav_tools formation_switcher [V-Shape|Column|Diamond]")
        return

    rclpy.init()
    try:
        node = FormationSwitcher(sys.argv[1])
        rclpy.spin(node)
    except SystemExit:
        pass
    rclpy.shutdown()

if __name__ == '__main__':
    main()