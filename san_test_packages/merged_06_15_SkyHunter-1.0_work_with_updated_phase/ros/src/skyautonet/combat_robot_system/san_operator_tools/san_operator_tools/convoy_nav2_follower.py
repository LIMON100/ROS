#!/usr/bin/env python3
# Convoy follower (nav2): feeds the leader's reference path into THIS robot's
# nav2 FollowPath action so it auto-follows the leader — no manual goals.
import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node
from nav_msgs.msg import Path
from nav2_msgs.action import FollowPath


class ConvoyNav2Follower(Node):
    def __init__(self):
        super().__init__("convoy_nav2_follower")
        self.rid = self.declare_parameter("robot_id", 3).value
        self.ac = ActionClient(self, FollowPath, f"/robot_{self.rid}/follow_path")
        self.create_subscription(
            Path, f"/convoy/ref_path/r{self.rid}", self.on_path, 1)
        self.active = False
        self.get_logger().info(f"ConvoyNav2Follower r{self.rid} up")

    def on_path(self, msg):
        if self.active or not msg.poses:
            return
        if not self.ac.wait_for_server(timeout_sec=1.0):
            return                      # nav2 not active yet — retry on next path
        goal = FollowPath.Goal()
        goal.path = msg  
        goal.controller_id = "FollowPath"
        self.active = True
        fut = self.ac.send_goal_async(goal)
        fut.add_done_callback(self._accepted)
        self.get_logger().info(
            f"r{self.rid}: following {len(msg.poses)}-pose leader path")
            
    def _accepted(self, fut):
        gh = fut.result()
        if not gh.accepted:
            self.active = False
            return
        gh.get_result_async().add_done_callback(lambda _f: self._done())
        
    def _done(self):
        # path finished/aborted → allow re-send (e.g. after an obstacle recovery)
        self.active = False
        
        
def main():
    rclpy.init()
    rclpy.spin(ConvoyNav2Follower())
    rclpy.shutdown()