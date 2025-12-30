import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32 # Import the same message type

class NumberSubscriberNode(Node):
    def __init__(self):
        super().__init__('number_subscriber')
        
        # Create a subscriber.
        # Arguments: Message Type, Topic Name, Callback Function, Queue Size
        self.subscription = self.create_subscription(
            Int32,
            'my_random_number',
            self.listener_callback,
            10)
        
        self.get_logger().info('Number Subscriber Node has been started and is listening.')

    def listener_callback(self, msg):
        # This function is called every time a message is received
        self.get_logger().info(f'I heard: "{msg.data}"')

def main(args=None):
    rclpy.init(args=args)
    number_subscriber = NumberSubscriberNode()
    rclpy.spin(number_subscriber)
    number_subscriber.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()