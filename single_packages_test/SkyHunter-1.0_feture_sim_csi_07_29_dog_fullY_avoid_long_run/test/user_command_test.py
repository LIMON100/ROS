import rclpy
from rclpy.node import Node
from combat_robot_msgs.msg import UserCommand
import sys
import select
import termios
import tty

class UserCommandPublisher(Node):
    def __init__(self):
        super().__init__('test_user_command_publisher')
        self.publisher_ = self.create_publisher(UserCommand, '/user_command', 10)
        
        # State for boolean fields
        self.gun_trigger = False
        self.gun_trigger_permission = False
        self.last_command_id = UserCommand.STOP

        self.key_to_command = {
            'q': UserCommand.STOP,
            'w': UserCommand.MOVE_MODE,
            'e': UserCommand.SURVEILLANCE_MODE,
            'r': UserCommand.DRONE_SURVEILLANCE_MODE,
            't': UserCommand.ATTACK_MODE,
            'a': UserCommand.TRACKING_MODE,
            's': UserCommand.ASSAULT_MODE,
            'd': UserCommand.EMERGENCY_STOP,
        }
        self.get_logger().info("Press a key to send a command:")
        self.get_logger().info("q: STOP, w: MOVE_MODE, e: SURVEILLANCE_MODE, r: DRONE_SURVEILLANCE_MODE")
        self.get_logger().info("t: ATTACK_MODE, a: TRACKING_MODE, s: ASSAULT_MODE, d: EMERGENCY_STOP")
        self.get_logger().info("f: Toggle Gun Trigger Permission, g: Toggle Gun Trigger")
        self.get_logger().info("Press Ctrl+C to quit.")

    def publish_command(self, command_id=None):
        if command_id is not None:
            self.last_command_id = command_id
            
        msg = UserCommand()
        msg.header.stamp = self.get_clock().now().to_msg()
        msg.command_from = UserCommand.TABLET
        msg.command_id = self.last_command_id
        msg.gun_trigger = self.gun_trigger
        msg.gun_trigger_permission = self.gun_trigger_permission
        
        self.publisher_.publish(msg)
        self.get_logger().info(f'Published UserCommand: ID={msg.command_id}, Trigger={msg.gun_trigger}, Permission={msg.gun_trigger_permission}')

def main(args=None):
    # Store original terminal settings
    settings = termios.tcgetattr(sys.stdin)

    rclpy.init(args=args)
    node = UserCommandPublisher()

    try:
        tty.setraw(sys.stdin.fileno())
        while rclpy.ok():
            # Use select to check for input without blocking
            if select.select([sys.stdin], [], [], 0) == ([sys.stdin], [], []):
                key = sys.stdin.read(1)
                if key == '\x03': # Ctrl+C
                    break
                
                if key in node.key_to_command:
                    node.publish_command(node.key_to_command[key])
                elif key == 'f':
                    node.gun_trigger_permission = not node.gun_trigger_permission
                    node.publish_command()
                elif key == 'g':
                    node.gun_trigger = not node.gun_trigger
                    node.publish_command()
            
            # Allow ROS2 to process events
            rclpy.spin_once(node, timeout_sec=0.01)
    except KeyboardInterrupt:
        pass
    finally:
        # Restore terminal settings
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()