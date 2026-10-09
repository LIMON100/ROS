import rclpy
from rclpy.node import Node
from sensor_msgs.msg import Image
from cv_bridge import CvBridge
import cv2

class VideoPublisher(Node):
    def __init__(self):
        super().__init__('video_publisher')
        self.publisher_ = self.create_publisher(Image, '/camera/image_raw', 10)
        self.timer_period = 1/60.0  # seconds
        self.timer = self.create_timer(self.timer_period, self.timer_callback)
        self.bridge = CvBridge()
        self.declare_parameter('video_path', '/home/ssr/Downloads/seoulwalk_60fps_640.mp4')
        video_path = self.get_parameter('video_path').get_parameter_value().string_value
        self.cap = cv2.VideoCapture(video_path)
        if not self.cap.isOpened():
            self.get_logger().error(f"Could not open video file: {video_path}")
            self.destroy_node()
            rclpy.shutdown()


    def timer_callback(self):
        ret, frame = self.cap.read()
        if ret:
            self.publisher_.publish(self.bridge.cv2_to_imgmsg(frame, "rgb8"))
        else:
            self.get_logger().info('End of video, restarting.')
            self.cap.set(cv2.CAP_PROP_POS_FRAMES, 0)


def main(args=None):
    rclpy.init(args=args)
    video_publisher = VideoPublisher()
    if rclpy.ok():
        rclpy.spin(video_publisher)
    video_publisher.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
