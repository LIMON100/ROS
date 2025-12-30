import rclpy
from rclpy.node import Node
from std_msgs.msg import String, Int32

class LogicHubNode(Node):
    def __init__(self):
        super().__init__('logic_hub_node')
        
        # Subscriber 1
        self.status_subscription = self.create_subscription(
            String, 'sensor/status', self.status_callback, 10)
        
        # Subscriber 2
        self.data_subscription = self.create_subscription(
            Int32, 'sensor/data', self.data_callback, 10)
            
        # Subscriber 3
        self.alert_subscription = self.create_subscription(
            String, 'system/alert', self.alert_callback, 10)
            
        self.get_logger().info('Logic Hub Node is listening to all topics.')

    def status_callback(self, msg):
        self.get_logger().info(f'Received Status: "{msg.data}"')

    def data_callback(self, msg):
        self.get_logger().info(f'Received Data: {msg.data}')
        
    def alert_callback(self, msg):
        self.get_logger().error(f'!!! ALERT RECEIVED: "{msg.data}" !!!')

def main(args=None):
    rclpy.init(args=args)
    node = LogicHubNode()
    rclpy.spin(node)
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()