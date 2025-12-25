# In swarm_control/formation_node.py
# (Your formation.py logic lives here)
import rclpy
from rclpy.node import Node
from nav_msgs.msg import Odometry
from geometry_msgs.msg import Point

class FormationNode(Node):
    def __init__(self):
        super().__init__('formation_node')
        self.num_robots = 5
        self.robot_odoms = {} # Dictionary to store latest odom

        self.goal_pubs = []
        self.odom_subs = []

        for i in range(self.num_robots):
            self.goal_pubs.append(self.create_publisher(Point, f'/robot_{i}/goal', 10))
            self.odom_subs.append(self.create_subscription(
                Odometry, f'/robot_{i}/odom', 
                lambda msg, robot_id=i: self.odom_callback(msg, robot_id), 10))
                
        # Subscribe to target from GUI
        self.target_sub = self.create_subscription(Point, '/set_target', self.target_callback, 10)
        self.target = Point()
        
        self.timer = self.create_timer(0.1, self.calculate_formation)

    def odom_callback(self, msg, robot_id):
        self.robot_odoms[robot_id] = msg

    def target_callback(self, msg):
        self.target = msg

    def calculate_formation(self):
        if 0 not in self.robot_odoms: return # Wait for leader
        
        leader_pose = self.robot_odoms[0].pose.pose
        
        # 1. Leader Goal is the Target
        self.goal_pubs[0].publish(self.target)

        # 2. Follower Goals (Your existing logic)
        # ... (calculate slot_x, slot_y for each follower)
        # follower_goal = Point()
        # follower_goal.x = slot_x
        # self.goal_pubs[follower_id].publish(follower_goal)