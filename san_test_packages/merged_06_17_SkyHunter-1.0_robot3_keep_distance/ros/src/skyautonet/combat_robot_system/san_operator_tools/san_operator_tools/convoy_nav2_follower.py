#!/usr/bin/env python3
# Convoy follower (nav2): feeds the leader's reference path into THIS robot's
# nav2 FollowPath action so it auto-follows the leader — no manual goals.
import rclpy
from rclpy.action import ActionClient
from rclpy.node import Node
from nav_msgs.msg import Odometry, Path
from rclpy.qos import qos_profile_sensor_data
from nav2_msgs.action import FollowPath


class ConvoyNav2Follower(Node):
    def __init__(self):
        super().__init__("convoy_nav2_follower")
        self.rid = self.declare_parameter("robot_id", 3).value
        self.ac = ActionClient(self, FollowPath, f"/robot_{self.rid}/follow_path")
        self.create_subscription(
            Path, f"/convoy/ref_path/r{self.rid}", self.on_path, 1)
        self.report_pub = self.create_publisher(
            Odometry, f"/convoy/report/r{self.rid}", 10)
        self.create_subscription(
            Odometry, f"/robot_{self.rid}/odom", self._on_odom,
            qos_profile_sensor_data)
        self.active = False
        self.get_logger().info(f"ConvoyNav2Follower r{self.rid} up")

    def on_path(self, msg):
        if self.active or not msg.poses:
            return
        if not self.ac.wait_for_server(timeout_sec=1.0):
            return                      # nav2 not active yet — retry on next path
        goal = FollowPath.Goal()
        msg.header.frame_id = "map"
        for ps in msg.poses:
            ps.header.frame_id = "map"
        goal.path = msg
        goal.controller_id = "FollowPath"
        self.active = True
        fut = self.ac.send_goal_async(goal)
        fut.add_done_callback(self._accepted)
        self.get_logger().info(
            f"r{self.rid}: following {len(msg.poses)}-pose leader path")
        
    def _on_odom(self, msg):
        self.report_pub.publish(msg)
            
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