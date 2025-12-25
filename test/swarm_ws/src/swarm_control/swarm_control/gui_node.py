# In swarm_control/gui_node.py
# Your swarm_gui.py logic, but modified
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Point
from nav_msgs.msg import Odometry
import pygame

class GuiNode(Node):
    def __init__(self):
        super().__init__('gui_node')
        # ... (Pygame setup) ...
        
        self.target_pub = self.create_publisher(Point, '/set_target', 10)
        self.robot_poses = {} # Store poses from ROS
        self.odom_subs = []
        for i in range(5):
            self.odom_subs.append(self.create_subscription(
                Odometry, f'/robot_{i}/odom',
                lambda msg, robot_id=i: self.odom_callback(msg, robot_id), 10))

    def odom_callback(self, msg, robot_id):
        self.robot_poses[robot_id] = msg.pose.pose

    def run_gui_loop(self):
        # ... (Your Pygame while loop) ...
        # If user clicks START:
        #    target_msg = Point()
        #    target_msg.x = target_obj.rect.centerx
        #    self.target_pub.publish(target_msg)
        
        # In the drawing part of the loop:
        # for robot_id, pose in self.robot_poses.items():
        #     draw_robot_at(pose.position.x, pose.position.y)