import rclpy
from rclpy.node import Node
from std_msgs.msg import String

class CommandHubNode(Node):
    def __init__(self):
        super().__init__('command_hub_node')
        
        # Publisher for Alerts
        self.alert_publisher = self.create_publisher(String, 'system/alert', 10)
        
        # Publisher for Commands (NEW)
        self.command_publisher = self.create_publisher(String, 'robot/command', 10)
        
        # Subscriber to sensor status
        self.status_subscription = self.create_subscription(
            String, 'sensor/status', self.status_callback, 10)
            
        # Timers
        self.alert_timer = self.create_timer(5.0, self.publish_alert) # Every 5 seconds
        self.command_timer = self.create_timer(3.0, self.publish_command) # Every 3 seconds
        
        self.get_logger().info('Command Hub Node is running.')

    def publish_alert(self):
        msg = String()
        msg.data = 'System Alert: Battery low!'
        self.alert_publisher.publish(msg)
        self.get_logger().info(f'Publishing Alert: "{msg.data}"')
        
    def publish_command(self):
        msg = String()
        msg.data = 'Execute self-test procedure.'
        self.command_publisher.publish(msg)
        self.get_logger().info(f'Publishing Command: "{msg.data}"')
        
    def status_callback(self, msg):
        self.get_logger().info(f'Monitoring Sensor Status: "{msg.data}"')

def main(args=None):
    rclpy.init(args=args)
    node = CommandHubNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()