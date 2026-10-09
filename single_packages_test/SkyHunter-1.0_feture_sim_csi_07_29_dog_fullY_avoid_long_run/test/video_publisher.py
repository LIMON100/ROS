#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2

class VideoPublisher(Node):
    """
    This node reads a video file and publishes it as a ROS2 topic.
    """
    def __init__(self):
        """
        Initializes the node, publisher, timer, and video capture.
        """
        super().__init__('video_publisher')
        
        # Create a publisher for the /camera/image_raw topic
        self.publisher_ = self.create_publisher(Image, '/camera/image_raw', 10)
        
        # Set the timer period (e.g., 30 Hz)
        self.timer_period = 1/30.0  # seconds
        self.timer = self.create_timer(self.timer_period, self.timer_callback)
        
        # Create a CvBridge to convert between OpenCV and ROS images
        self.bridge = CvBridge()
        
        # Declare and get the video file path parameter
        self.declare_parameter('video_path', 'path/to/your/video.mp4')
        video_path = self.get_parameter('video_path').get_parameter_value().string_value
        
        # Open the video file
        self.cap = cv2.VideoCapture(video_path)
        if not self.cap.isOpened():
            self.get_logger().error(f"Could not open video file: {video_path}")
            self.destroy_node()
            rclpy.shutdown()

    def timer_callback(self):
        """
        This callback is called by the timer to read and publish a frame.
        """
        ret, frame = self.cap.read()
        
        if ret:
            # Convert the OpenCV image to a ROS Image message and publish it
            self.publisher_.publish(self.bridge.cv2_to_imgmsg(frame, "bgr8"))
        else:
            # Restart the video when it ends
            self.get_logger().info('End of video, restarting.')
            self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)

def main(args=None):
    """
    Main function to initialize and run the node.
    """
    rclpy.init(args=args)
    
    video_publisher = VideoPublisher()
    
    if rclpy.ok():
        try:
            rclpy.spin(video_publisher)
        except KeyboardInterrupt:
            pass
    
    # Cleanup
    video_publisher.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
