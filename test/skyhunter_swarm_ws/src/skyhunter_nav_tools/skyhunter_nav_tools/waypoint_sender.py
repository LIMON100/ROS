#! /usr/bin/env python3
import time
from geometry_msgs.msg import PoseStamped
import rclpy
from nav2_simple_commander.robot_navigator import BasicNavigator, TaskResult

def main():
    rclpy.init()

    # CRITICAL: Match the namespace used in the Launch file GroupAction
    robot_ns = 'robot_01'
    navigator = BasicNavigator(namespace=robot_ns)

    print(f"Waiting for Nav2 to be active in namespace: /{robot_ns}...")
    # This checks /robot_01/amcl/get_state. If launch file is fixed, this passes.
    navigator.waitUntilNav2Active(localizer='amcl') 

    # --- 1. SET INITIAL POSE ---
    # We must tell AMCL where we are to start.
    print("Setting Initial Pose...")
    initial_pose = PoseStamped()
    initial_pose.header.frame_id = 'map'
    initial_pose.header.stamp = navigator.get_clock().now().to_msg()
    initial_pose.pose.position.x = 0.0
    initial_pose.pose.position.y = 0.0
    initial_pose.pose.orientation.z = 0.0
    initial_pose.pose.orientation.w = 1.0
    navigator.setInitialPose(initial_pose)

    # Wait a moment for AMCL particle cloud to initialize
    time.sleep(3.0) 

    # --- 2. SEND GOAL ---
    goal_pose = PoseStamped()
    goal_pose.header.frame_id = 'map'
    goal_pose.header.stamp = navigator.get_clock().now().to_msg()
    goal_pose.pose.position.x = 20.0
    goal_pose.pose.position.y = 0.0
    goal_pose.pose.orientation.w = 1.0

    print("Sending Goal: 20m Forward...")
    navigator.goToPose(goal_pose)

    # --- 3. MONITOR PROGRESS ---
    i = 0
    while not navigator.isTaskComplete():
        i += 1
        feedback = navigator.getFeedback()
        if feedback and i % 5 == 0:
            print(f'Distance remaining: {feedback.distance_remaining:.2f} meters')
        
        # Optional: Timeout if it takes too long (e.g., 60 seconds)
        # if Duration.from_msg(feedback.navigation_time) > Duration(seconds=600):
        #     navigator.cancelTask()

    # --- 4. RESULT ---
    result = navigator.getResult()
    if result == TaskResult.SUCCEEDED:
        print('Goal Reached!')
    elif result == TaskResult.CANCELED:
        print('Goal was canceled!')
    elif result == TaskResult.FAILED:
        print('Goal failed!')

    exit(0)

if __name__ == '__main__':
    main()