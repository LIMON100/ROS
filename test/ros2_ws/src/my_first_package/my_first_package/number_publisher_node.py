import rclpy
from rclpy.node import Node
from std_msgs.msg import Int32  # We need to import the message type we want to publish
import random

class NumberPublisherNode(Node):
    def __init__(self):
        super().__init__('number_publisher')
        
        # Create a publisher.
        # Arguments: Message Type, Topic Name, Queue Size
        self.publisher_ = self.create_publisher(Int32, 'my_random_number', 10)
        
        # Create a timer to call the publish function every 1 second
        self.timer = self.create_timer(1.0, self.publish_number)
        self.get_logger().info('Number Publisher Node has been started and is publishing.')

    def publish_number(self):
        # Create a message object
        msg = Int32()
        
        # Fill the message with data
        msg.data = random.randint(0, 100)
        
        # Publish the message
        self.publisher_.publish(msg)
        
        # Log what we sent
        self.get_logger().info(f'Publishing: "{msg.data}"')

def main(args=None):
    rclpy.init(args=args)
    number_publisher = NumberPublisherNode()
    rclpy.spin(number_publisher)
    number_publisher.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()