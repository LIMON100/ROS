"""
the waypoint_sender node is responsible for sending a predefined sequence of waypoints to the current Leader's Nav2 stack.
It also listens for role changes to detect if the current Leader (SH-01) has been terminated, and if so, 
it automatically switches to sending waypoints to the new Leader (SH-02) without any user intervention.
"""

import rclpy
from rclpy.node import Node
from rclpy.action import ActionClient
from nav2_msgs.action import NavigateToPose
import std_msgs
from std_msgs.msg import Int8
from action_msgs.msg import GoalStatus
import time
from std_msgs.msg import Int8, Bool
from skyhunter_msgs.msg import LeaderState

class WaypointFollower(Node):
    def __init__(self):
        super().__init__('waypoint_follower')
        
        # Start targeting Global R1
        self.target_ns = "" 
        self.connect_to_nav2()
        
        self.role_sub = self.create_subscription(Int8, '/SH_02/local_role', self.succession_cb, 10)
        self.halt_pub = self.create_publisher(Bool, '/swarm/halt_active', 10)
        self.cmd_formation_pub = self.create_publisher(std_msgs.msg.Int8, '/swarm/formation_command', 10)
        
        self.current_goal_handle = None      # <-- ADD
        self.mission_ended = False           # <-- ADD

        # NEW: listen to Fire Net state
        self.state_sub = self.create_subscription(
            LeaderState,
            '/swarm/virtual_leader/state',   # this carries the YOLO combat state
            self.fire_net_callback,
            10)
        
        # Define Mission
        # self.waypoints = [
        #         (10.0, 0.0),   # 1. Straight 30m
        #         (20.0, 15.0),  # 2. Left 10m
        #         (30.0, 10.0),  # 3. Straight 10m
        #         (30.0, -20.0)  # 4. Right 30m
        # ]

        self.waypoints = [
            (-100.5, 90.0),  # 1. Straight ~15m down the highway
            (-90.0,  64.0),  # 2. Straight ~30m down the highway
            (-80.5,  51.0),  # 3. Straight ~45m down the highway
            (-70.0,  38.0)   # 4. Straight ~60m down the highway
        ]

        # self.waypoints = [(10.0, 0.0), (20.0, 0.0), ] #(30.0, 0.0), (40.0, 0.0)]

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
            
            # 3. Wait for SH_02 Nav2 to be ready 
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
        # PROMINENT MISSION COMPLETE LOG ---
        if self.current_wp_index >= len(self.waypoints):
            self.get_logger().info('=============================================')
            self.get_logger().info('MISSION COMPLETE: All Waypoints Reached!     ')
            self.get_logger().info('=============================================')
            return
        
        if self.is_busy:
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
        self.current_goal_handle = goal_handle          # <-- ADD THIS LINE
        goal_handle.get_result_async().add_done_callback(self.get_result_callback)

    # def get_result_callback(self, future):
    #     status = future.result().status
    #     if status == GoalStatus.STATUS_SUCCEEDED:
    #         self.get_logger().info("Waypoint Reached!")
    #         self.get_logger().info("=============================================")
    #         self.current_wp_index += 1
    #         self.is_busy = False

    #         # time.sleep(0.5)
    #         time.sleep(20.0)

    #         self.send_next_goal()
    #     else:
    #         self.get_logger().warn(f"Goal failed with status: {status}")
    #         self.is_busy = False

    def get_result_callback(self, future):
        status = future.result().status
        if status == GoalStatus.STATUS_SUCCEEDED:
            self.get_logger().info("Waypoint Reached!")
            self.get_logger().info("=============================================")
            self.current_wp_index += 1
            self.is_busy = False

            # ================== HALT MODE START ==================
            halt_msg = Bool()
            halt_msg.data = True
            self.halt_pub.publish(halt_msg)
            self.get_logger().info(">>> 20s PERIMETER SWEEP HALT STARTED <<<")

            time.sleep(3.0)   # ← This is the exact 20s the client wants

            # ================== HALT MODE END ==================
            halt_msg.data = False
            self.halt_pub.publish(halt_msg)
            self.get_logger().info(">>> Halt finished - resuming mission <<<")
            
            self.send_next_goal()
        else:
            self.get_logger().warn(f"Goal failed with status: {status}")
            self.is_busy = False

    def fire_net_callback(self, msg: LeaderState):
        if msg.target_locked and msg.swarm_state >= 3 and self.is_busy and not self.mission_ended:
            self.get_logger().info("🔥 FIRE NET TRIGGERED - Leader advancing to 5m combat line!")
            self.is_busy = False
            self.mission_ended = True
            self.current_wp_index = len(self.waypoints)  # prevent any more waypoints

            # --- SMART LEADER COMBAT POSITION ---
            import math
            # Calculate angle from target TO leader
            dx = msg.pose.position.x - msg.target_pos.x
            dy = msg.pose.position.y - msg.target_pos.y
            angle_from_target = math.atan2(dy, dx)
            
            # Leader takes a spot exactly 5m away on that line
            combat_x = msg.target_pos.x + 5.0 * math.cos(angle_from_target)
            combat_y = msg.target_pos.y + 5.0 * math.sin(angle_from_target)
            
            goal_msg = NavigateToPose.Goal()
            goal_msg.pose.header.frame_id = 'map'
            goal_msg.pose.header.stamp = self.get_clock().now().to_msg()
            goal_msg.pose.pose.position.x = combat_x
            goal_msg.pose.pose.position.y = combat_y
            
            # Face the target (Yaw = angle from target + 180 degrees)
            target_yaw = angle_from_target + math.pi
            goal_msg.pose.pose.orientation.z = math.sin(target_yaw / 2.0)
            goal_msg.pose.pose.orientation.w = math.cos(target_yaw / 2.0)
            
            # Send the Combat Goal to Nav2
            self._action_client.send_goal_async(goal_msg)
            
            # Tell followers to wake up cameras
            halt_msg = Bool(data=True)
            self.halt_pub.publish(halt_msg)
    
    def cancel_done_callback(self, future):
        self.get_logger().info("Nav2 goal cancelled (Fire Net encirclement active)")

def main():
    rclpy.init()
    node = WaypointFollower()
    rclpy.spin(node)
    rclpy.shutdown()

if __name__ == '__main__':
    main()
