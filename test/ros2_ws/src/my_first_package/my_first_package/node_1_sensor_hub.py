import rclpy
from rclpy.node import Node
from std_msgs.msg import String, Int32
import random

class SensorHubNode(Node):
    def __init__(self):
        super().__init__('sensor_hub_node')
        
        # Publishers
        self.status_publisher = self.create_publisher(String, 'sensor/status', 10)
        self.data_publisher = self.create_publisher(Int32, 'sensor/data', 10)
        
        # Subscriber
        self.command_subscriber = self.create_subscription(
            String,
            'robot/command',
            self.command_callback,
            10)
            
        # Timers to publish data periodically
        self.status_timer = self.create_timer(2.0, self.publish_status) # Every 2 seconds
        self.data_timer = self.create_timer(0.5, self.publish_data)   # Every 0.5 seconds
        
        self.get_logger().info('Sensor Hub Node is running.')

    def publish_status(self):
        msg = String()
        msg.data = 'STATUS: OK'
        self.status_publisher.publish(msg)
        self.get_logger().info(f'Publishing Status: "{msg.data}"')

    def publish_data(self):
        msg = Int32()
        msg.data = random.randint(0, 1000)
        self.data_publisher.publish(msg)
        self.get_logger().info(f'Publishing Data: {msg.data}')
        
    def command_callback(self, msg):
        self.get_logger().warn(f'>>> Received Command: "{msg.data}" <<<')

def main(args=None):
    rclpy.init(args=args)
    node = SensorHubNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()