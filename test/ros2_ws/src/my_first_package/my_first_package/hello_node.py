# Import the ROS 2 Python client library
import rclpy
from rclpy.node import Node

class HelloNode(Node):
    def __init__(self):
        # Call the Node constructor and give it a name
        super().__init__('hello_node')
        
        # Create a timer that fires every 1 second
        self.timer = self.create_timer(1.0, self.timer_callback)
        self.get_logger().info('Hello Node has been started!')

    def timer_callback(self):
        # This function is called by the timer
        self.get_logger().info('Hello, Limon!')

def main(args=None):
    # Initialize the ROS 2 client library
    rclpy.init(args=args)
    
    # Create an instance of our node
    hello_node = HelloNode()
    
    # "Spin" the node, which keeps it running and allows it to process callbacks
    rclpy.spin(hello_node)
    
    # Clean up and shutdown
    hello_node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
