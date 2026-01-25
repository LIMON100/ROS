import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node
from nav2_msgs.action import NavigateToPose
from geometry_msgs.msg import PoseStamped
import math

class GoalSender(Node):
    def __init__(self):
        super().__init__('goal_sender_node')
        self._action_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')

    def send_goal(self, x, y, yaw):
        goal_pose = PoseStamped()
        goal_pose.header.frame_id = 'map'
        goal_pose.header.stamp = self.get_clock().now().to_msg()
        goal_pose.pose.position.x = x
        goal_pose.pose.position.y = y
        
        # Simple conversion from Yaw (degrees) to Quaternion
        q_x, q_y, q_z, q_w = 0.0, 0.0, 0.0, 1.0 # Default: 0 degrees
        angle = float(yaw * 3.14159 / 180.0) # Convert to radians
        q_z = float(math.sin(angle / 2.0))
        q_w = float(math.cos(angle / 2.0))
        
        goal_pose.pose.orientation.x = q_x
        goal_pose.pose.orientation.y = q_y
        goal_pose.pose.orientation.z = q_z
        goal_pose.pose.orientation.w = q_w

        goal_msg = NavigateToPose.Goal()
        goal_msg.pose = goal_pose

        self.get_logger().info('Waiting for action server...')
        self._action_client.wait_for_server()

        self.get_logger().info(f'Sending goal request: Go to ({x}, {y})')
        self._send_goal_future = self._action_client.send_goal_async(goal_msg, feedback_callback=self.feedback_callback)
        
        self._send_goal_future.add_done_callback(self.goal_response_callback)

    def goal_response_callback(self, future):
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().error('Goal rejected by the server')
            return

        self.get_logger().info('Goal accepted! Waiting for result...')
        self._get_result_future = goal_handle.get_result_async()
        self._get_result_future.add_done_callback(self.get_result_callback)

    def get_result_callback(self, future):
        result = future.result().result
        self.get_logger().info(f'Result: {result}')
        rclpy.shutdown()

    def feedback_callback(self, feedback_msg):
        feedback = feedback_msg.feedback
        # You can print feedback here if you want, e.g., distance remaining
        # self.get_logger().info(f'Distance remaining: {feedback.distance_remaining}')
        pass

def main(args=None):
    rclpy.init(args=args)
    
    # --- DEFINE YOUR GOAL HERE ---
    # Let's send the robot to a point 5 meters in front and 2 meters to the left
    goal_x = 5.0
    goal_y = 2.0
    goal_yaw_degrees = 90.0 # Face left when it arrives
    # -----------------------------
    
    action_client = GoalSender()
    action_client.send_goal(goal_x, goal_y, goal_yaw_degrees)

    rclpy.spin(action_client)

if __name__ == '__main__':
    # Need this to get math.sin/cos
    import math
    main()