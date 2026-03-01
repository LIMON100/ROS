# import rclpy
# from rclpy.action import ActionClient
# from rclpy.node import Node
# from nav2_msgs.action import NavigateToPose
# from geometry_msgs.msg import PoseStamped
# from action_msgs.msg import GoalStatus

# class WaypointFollower(Node):
#     def __init__(self):
#         super().__init__('waypoint_follower')
        
#         self._action_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')
        
#         # --- CONFIGURE YOUR WAYPOINTS HERE --
#         self.waypoints = [
#             (10.0, 0.0),   # 1. Go forward 10m
#             (20.0, 0.0),  # 2. Turn left and go 10m
#             (30.0, 0.0),   # 3. Go back to X=0
#             (40.0, 0.0),     # 4. Return to start
#             (50.0, 0.0)  
#         ]
        
#         self.current_wp_index = 0
        
#         # Start the timer (5 seconds delay)
#         self.timer = self.create_timer(5.0, self.start_mission)

#     def start_mission(self):
#         self.timer.cancel()
#         self.get_logger().info('Mission Start! Sending first waypoint...')
#         self.send_next_goal()

#     def send_next_goal(self):
#         # Check if we have finished all waypoints
#         if self.current_wp_index >= len(self.waypoints):
#             self.get_logger().info('All waypoints completed! Mission finished.')
#             return

#         # Get current target coordinates
#         target_x, target_y = self.waypoints[self.current_wp_index]
        
#         self.get_logger().info(f'Sending Waypoint {self.current_wp_index + 1}/{len(self.waypoints)}: x={target_x}, y={target_y}')

#         # Wait for Nav2 server
#         self._action_client.wait_for_server()

#         # Build Goal Message
#         goal_msg = NavigateToPose.Goal()
#         goal_msg.pose.header.frame_id = 'map'
#         goal_msg.pose.header.stamp = self.get_clock().now().to_msg()
        
#         goal_msg.pose.pose.position.x = float(target_x)
#         goal_msg.pose.pose.position.y = float(target_y)
#         goal_msg.pose.pose.position.z = 0.0
        
#         # Simple orientation (facing forward/X-axis usually, Nav2 planner handles rotation)
#         goal_msg.pose.pose.orientation.w = 1.0 
        
#         # Send goal
#         self._send_goal_future = self._action_client.send_goal_async(goal_msg)
#         self._send_goal_future.add_done_callback(self.goal_response_callback)

#     def goal_response_callback(self, future):
#         goal_handle = future.result()
#         if not goal_handle.accepted:
#             self.get_logger().error('Goal rejected by Nav2!')
#             # Optional: Retry logic or stop
#             return

#         self.get_logger().info('Goal accepted. Robot is moving...')
        
#         # Request the result (this blocks until the robot finishes or fails)
#         self._get_result_future = goal_handle.get_result_async()
#         self._get_result_future.add_done_callback(self.get_result_callback)

#     def get_result_callback(self, future):
#         result = future.result().result
#         status = future.result().status
        
#         # Check if the robot succeeded (Status 2 is SUCCEEDED in action_msgs)
#         if status == GoalStatus.STATUS_SUCCEEDED:
#             self.get_logger().info(f'Waypoint {self.current_wp_index + 1} reached!')
            
#             # Move to the next waypoint
#             self.current_wp_index += 1
            
#             # Small delay before sending next command to let nav2 settle
#             self.timer = self.create_timer(2.0, self.send_next_goal)
#         else:
#             self.get_logger().error(f'Failed with status code: {status}')
#             self.get_logger().error('Mission aborted.')

# def main(args=None):
#     rclpy.init(args=args)
#     node = WaypointFollower()
#     rclpy.spin(node)
#     rclpy.shutdown()

# if __name__ == '__main__':
#     main()




# import rclpy
# from rclpy.action import ActionClient
# from rclpy.node import Node
# from nav2_msgs.action import NavigateToPose
# from action_msgs.msg import GoalStatus
# import time
# import std_srvs.srv
# from nav2_msgs.srv import ClearEntireCostmap

# class WaypointFollower(Node):
#     def __init__(self):
#         super().__init__('waypoint_follower')
        
#         self._action_client = ActionClient(self, NavigateToPose, 'navigate_to_pose')
        
#         # --- WAYPOINTS (X, Y) ---
#         # self.waypoints = [
#         #     (20.0, 0.0), (40.0, 0.0), (50.0, 0.0),
#         # ]
        
#         self.waypoints = [
#             (30.0, 0.0),   # 1. Straight 30m
#             (20.0, 15.0),  # 2. Left 10m
#             (30.0, 10.0),  # 3. Straight 10m
#             (30.0, -20.0)  # 4. Right 30m
#         ]

#         # self.waypoints = [
#         #     (2425.0, 293.5), # 1. Straight 20m
#         #     (2425.0, 283.5), # 2. Left 10m
#         #     (2415.0, 283.5), # 3. Straight 10m
#         #     (2415.0, 313.5)  # 4. Right 30m
#         # ]

#         self.current_wp_index = 0
#         self.waiting_timer = None
        
#         # 1. Start a startup timer (assigning it to self.timer to avoid the error)
#         self.get_logger().info('Initializing... Waiting 5s for Gazebo/Nav2 to settle.')
#         self.timer = self.create_timer(5.0, self.start_mission)

#         # self.clear_costmap_client = self.create_client(std_srvs.srv.Empty, '/global_costmap/clear_entirely_global_costmap')
#         self.clear_client = self.create_client(ClearEntireCostmap, '/global_costmap/clear_entirely_global_costmap')

#         self.is_busy = False


#     def start_mission(self):
#         # 2. Stop the startup timer so it doesn't run again
#         if self.timer is not None:
#             self.timer.cancel()
#             self.destroy_timer(self.timer)
#             self.timer = None
            
#         self.get_logger().info('MISSION STARTING...')
#         self.send_next_goal()

#     def send_next_goal(self):
#         # if self.current_wp_index >= len(self.waypoints):
#         #     self.get_logger().info('SUCCESS: All waypoints completed!')
#         #     return
        
#         if self.is_busy or self.current_wp_index >= len(self.waypoints):
#             return
        
#         self.is_busy = True 

#         target_x, target_y = self.waypoints[self.current_wp_index]
#         self.get_logger().info(f'MOVING TO: WP {self.current_wp_index + 1} (x={target_x}, y={target_y})')

#         self._action_client.wait_for_server()

#         goal_msg = NavigateToPose.Goal()
#         goal_msg.pose.header.frame_id = 'map'
#         goal_msg.pose.header.stamp = self.get_clock().now().to_msg()
#         goal_msg.pose.pose.position.x = float(target_x)
#         goal_msg.pose.pose.position.y = float(target_y)
#         goal_msg.pose.pose.orientation.w = 1.0 
        
#         self._send_goal_future = self._action_client.send_goal_async(goal_msg)
#         self._send_goal_future.add_done_callback(self.goal_response_callback)

#     def goal_response_callback(self, future):
#         goal_handle = future.result()
#         if not goal_handle.accepted:
#             self.get_logger().error('Nav2 rejected the goal!')
#             return

#         self.get_logger().info('Goal accepted. Navigating...')
#         goal_handle.get_result_async().add_done_callback(self.get_result_callback)

#     def get_result_callback(self, future):
#         result = future.result().result
#         status = future.result().status
        
#         # CLIENT REQUIREMENT: Detailed debug logging of goal and status code
#         self.get_logger().info(f'[DEBUG] Processing Goal Index: {self.current_wp_index}')
#         self.get_logger().info(f'[DEBUG] Nav2 Action Status Code Received: {status}')
        
#         # Status 4 = SUCCEEDED
#         if status == GoalStatus.STATUS_SUCCEEDED:
#             self.get_logger().info(f'Waypoint {self.current_wp_index + 1} reached successfully!')
            
#             # Move to the next waypoint
#             self.current_wp_index += 1
#             self.is_busy = False
            
#             # CLIENT REQUIREMENT: Add 500ms delay before processing next goal
#             time.sleep(0.5)
            
#             # System Settling delay (your 5s requirement)
#             self.get_logger().info('Waiting 5s for system settling...')
#             self.timer = self.create_timer(5.0, self.send_next_goal)
            
#         elif status == GoalStatus.STATUS_ABORTED:
#             self.get_logger().warn('Status 6 detected. Clearing and cooling down...')
#             # Wipe map
#             req = ClearEntireCostmap.Request()
#             self.clear_client.call_async(req)
#             self.create_timer(5.0, self.retry_unlock_callback)
#         else:
#             self.get_logger().error(f'Mission failed with unexpected status: {status}')

#     def retry_unlock_callback(self):
#         self.is_busy = False 
#         self.send_next_goal()

#     def wait_timer_callback(self):
#         if self.waiting_timer is not None:
#             self.waiting_timer.cancel()
#             self.destroy_timer(self.waiting_timer)
#             self.waiting_timer = None
            
#         # --- SURGICAL FIX: WIPE THE GHOSTS ---
#         self.get_logger().info('Wiping sensor ghosts from the map...')
#         while not self.clear_client.wait_for_service(timeout_sec=1.0):
#             self.get_logger().info('Service not available, waiting...')
        
#         req = ClearEntireCostmap.Request()
#         self.clear_client.call_async(req)
        
#         # Send the next goal immediately after clearing
#         self.send_next_goal()

# def main(args=None):
#     rclpy.init(args=args)
#     node = WaypointFollower()
#     try:
#         rclpy.spin(node)
#     except KeyboardInterrupt:
#         pass
#     finally:
#         node.destroy_node()
#         rclpy.shutdown()

# if __name__ == '__main__':
#     main()








import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from nav2_msgs.action import NavigateToPose
from std_msgs.msg import Int8
from action_msgs.msg import GoalStatus
import time

class WaypointFollower(Node):
    def __init__(self):
        super().__init__('waypoint_follower')
        
        # Start targeting Global R1
        self.target_ns = "" 
        self.connect_to_nav2()
        
        self.role_sub = self.create_subscription(Int8, '/SH_02/local_role', self.succession_cb, 10)
        
        # Define Mission
        self.waypoints = [
                (10.0, 0.0),   # 1. Straight 30m
                (20.0, 15.0),  # 2. Left 10m
                (30.0, 10.0),  # 3. Straight 10m
                (30.0, -20.0)  # 4. Right 30m
        ]
        # self.waypoints = [(10.0, 0.0), (20.0, 0.0), (30.0, 0.0), (40.0, 0.0)]
        self.current_wp_index = 0
        self.is_busy = False
        
        # Start after delay
        self.timer = self.create_timer(5.0, self.start_mission)

    def connect_to_nav2(self):
        # Helper to connect to the correct action server
        action_topic = f'{self.target_ns}/navigate_to_pose' if self.target_ns else '/navigate_to_pose'
        self.get_logger().info(f"Connecting to Action Server: {action_topic}...")
        self._action_client = ActionClient(self, NavigateToPose, action_topic)

    def succession_cb(self, msg):
        # Role 2 means LEADER
        if msg.data == 2 and self.target_ns != "/SH_02":
            self.get_logger().warn("!!! LEADER DIED. SWITCHING TARGET TO SH_02 !!!")
            
            # 1. Cancel/Reset current state
            self.is_busy = False
            
            # 2. Switch Target
            self.target_ns = "/SH_02"
            self.connect_to_nav2()
            
            # 3. Wait for SH_02 Nav2 to be ready (It might need a second after takeover)
            # We use a non-blocking check in a timer loop or just simple wait here for simplicity
            timeout_counter = 0
            while not self._action_client.wait_for_server(timeout_sec=1.0):
                self.get_logger().info("Waiting for SH_02 Nav2 stack...")
                timeout_counter += 1
                if timeout_counter > 5:
                    break
            
            # 4. RESEND THE CURRENT GOAL to the new leader
            self.get_logger().info(f"Resending current WP {self.current_wp_index+1} to New Leader SH_02")
            self.send_next_goal()

    def start_mission(self):
        self.timer.cancel()
        self.send_next_goal()

    def send_next_goal(self):
        if self.current_wp_index >= len(self.waypoints):
            self.get_logger().info('All waypoints completed! Mission finished.')
            return
        
        if self.is_busy or self.current_wp_index >= len(self.waypoints):
            return
        
        self.is_busy = True
        target_x, target_y = self.waypoints[self.current_wp_index]
        
        goal_msg = NavigateToPose.Goal()
        goal_msg.pose.header.frame_id = 'map'
        goal_msg.pose.header.stamp = self.get_clock().now().to_msg()
        goal_msg.pose.pose.position.x = float(target_x)
        goal_msg.pose.pose.position.y = float(target_y)
        goal_msg.pose.pose.orientation.w = 1.0
        
        # Ensure client is ready before sending
        if not self._action_client.server_is_ready():
             self.get_logger().warn("Nav2 server not ready yet...")
             self.is_busy = False
             return

        self.get_logger().info(f'Sending Goal: x={target_x} y={target_y}')
        self._send_goal_future = self._action_client.send_goal_async(goal_msg)
        self._send_goal_future.add_done_callback(self.goal_response_callback)

    def goal_response_callback(self, future):
        goal_handle = future.result()
        if not goal_handle.accepted:
            self.get_logger().error("Goal Rejected!")
            self.is_busy = False
            return
        
        self.get_logger().info("Goal Accepted. Leader Moving...")
        goal_handle.get_result_async().add_done_callback(self.get_result_callback)

    def get_result_callback(self, future):
        status = future.result().status
        if status == GoalStatus.STATUS_SUCCEEDED:
            self.get_logger().info("Waypoint Reached!")
            self.current_wp_index += 1
            self.is_busy = False
            time.sleep(0.5)
            self.send_next_goal()
        else:
            self.get_logger().warn(f"Goal failed with status: {status}")
            self.is_busy = False # Allow retry

def main():
    rclpy.init()
    node = WaypointFollower()
    rclpy.spin(node)
    rclpy.shutdown()