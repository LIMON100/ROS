#! /usr/bin/env python3
import time
from geometry_msgs.msg import PoseStamped
from rclpy.duration import Duration
import rclpy
from rclpy.node import Node
from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult

def main():
    rclpy.init()
    navigator = BasicNavigator()

    print("Setting Initial Pose...")
    initial_pose = PoseStamped()
    initial_pose.header.frame_id = 'map'
    initial_pose.header.stamp = navigator.get_clock().now().to_msg()
    initial_pose.pose.position.x = 0.0
    initial_pose.pose.position.y = 0.0
    initial_pose.pose.orientation.w = 1.0
    navigator.setInitialPose(initial_pose)

    time.sleep(3) # Wait for AMCL/SLAM to settle

    # --- SINGLE GOAL (20m Ahead) ---
    goal_pose = PoseStamped()
    goal_pose.header.frame_id = 'map'
    goal_pose.header.stamp = navigator.get_clock().now().to_msg()
    goal_pose.pose.position.x = 20.0
    goal_pose.pose.position.y = 0.0
    goal_pose.pose.orientation.w = 1.0

    print("Sending Goal: 20m Forward...")
    navigator.goToPose(goal_pose)  # <--- Changed from followWaypoints to goToPose

    while not navigator.isTaskComplete():
        feedback = navigator.getFeedback()
        # print('Feedback: ', feedback)
        # time.sleep(1.0)

    result = navigator.getResult()
    if result == TaskResult.SUCCEEDED:
        print('Goal Reached!')
    else:
        print('Goal Failed!')

    exit(0)

if __name__ == '__main__':
    main()