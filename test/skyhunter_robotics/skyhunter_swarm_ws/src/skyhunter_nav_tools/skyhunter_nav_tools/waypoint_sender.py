import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node
from nav2_msgs.action import NavigateToPose
from geometry_msgs.msg import PoseStamped
from action_msgs.msg import GoalStatus

class WaypointFollower(Node):
    def __init__(self):
        super().__init__('waypoint_follower')
        
        self._action_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')
        
        # --- CONFIGURE YOUR WAYPOINTS HERE --
        self.waypoints = [
            (50.0, 0.0),   # 1. Go forward 10m
            (60.0, 0.0),  # 2. Turn left and go 10m
            (70.0, 0.0),   # 3. Go back to X=0
            (80.0, 0.0),     # 4. Return to start
            (90.0, 0.0)  
        ]
        
        self.current_wp_index = 0
        
        # Start the timer (5 seconds delay)
        self.timer = self.create_timer(5.0, self.start_mission)

    def start_mission(self):
        self.timer.cancel()
        self.get_logger().info('Mission Start! Sending first waypoint...')
        self.send_next_goal()

    def send_next_goal(self):
        # Check if we have finished all waypoints
        if self.current_wp_index >= len(self.waypoints):
            self.get_logger().info('All waypoints completed! Mission finished.')
            return

        # Get current target coordinates
        target_x, target_y = self.waypoints[self.current_wp_index]
        
        self.get_logger().info(f'Sending Waypoint {self.current_wp_index + 1}/{len(self.waypoints)}: x={target_x}, y={target_y}')

        # Wait for Nav2 server
        self._action_client.wait_for_server()

        # Build Goal Message
        goal_msg = NavigateToPose.Goal()
        goal_msg.pose.header.frame_id = 'map'
        goal_msg.pose.header.stamp = self.get_clock().now().to_msg()
        
        goal_msg.pose.pose.position.x = float(target_x)
        goal_msg.pose.pose.position.y = float(target_y)
        goal_msg.pose.pose.position.z = 0.0
        
        # Simple orientation (facing forward/X-axis usually, Nav2 planner handles rotation)
        goal_msg.pose.pose.orientation.w = 1.0 
        
        # Send goal
        self._send_goal_future = self._action_client.send_goal_async(goal_msg)
        self._send_goal_future.add_done_callback(self.goal_response_callback)

    def goal_response_callback(self, future):
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().error('Goal rejected by Nav2!')
            # Optional: Retry logic or stop
            return

        self.get_logger().info('Goal accepted. Robot is moving...')
        
        # Request the result (this blocks until the robot finishes or fails)
        self._get_result_future = goal_handle.get_result_async()
        self._get_result_future.add_done_callback(self.get_result_callback)

    def get_result_callback(self, future):
        result = future.result().result
        status = future.result().status
        
        # Check if the robot succeeded (Status 2 is SUCCEEDED in action_msgs)
        if status == GoalStatus.STATUS_SUCCEEDED:
            self.get_logger().info(f'Waypoint {self.current_wp_index + 1} reached!')
            
            # Move to the next waypoint
            self.current_wp_index += 1
            
            # Small delay before sending next command to let nav2 settle
            self.timer = self.create_timer(2.0, self.send_next_goal)
        else:
            self.get_logger().error(f'Failed with status code: {status}')
            self.get_logger().error('Mission aborted.')

def main(args=None):
    rclpy.init(args=args)
    node = WaypointFollower()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()