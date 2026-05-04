#!/usr/bin/env python3
import rclpy
from rclpy.node import Node
from geometry_msgs.msg import PoseArray
import tkinter as tk
from tkinter import ttk
import threading

class SwarmDashboard(Node):
    def __init__(self):
        super().__init__('swarm_dashboard')
        self.poses = []
        # Subscribe to the swarm monitor topic
        self.subscription = self.create_subscription(
            PoseArray,
            '/swarm/poses',
            self.pose_callback,
            10)
        
    def pose_callback(self, msg):
        # Convert PoseArray to a simple list of coordinates
        new_poses = []
        for i, pose in enumerate(msg.poses):
            name = "LEADER (SH_01)" if i == 0 else f"FOLLOWER (SH_{i+1:02d})"
            new_poses.append({
                'name': name,
                'x': round(pose.position.x, 3),
                'y': round(pose.position.y, 3),
                'z': round(pose.position.z, 3)
            })
        self.poses = new_poses

def run_ros(node):
    rclpy.spin(node)

def update_gui():
    # Clear current table
    for item in tree.get_children():
        tree.delete(item)
    
    # Insert new data
    for pose in ros_node.poses:
        tree.insert('', tk.END, values=(pose['name'], pose['x'], pose['y'], pose['z']))
    
    # Schedule next update in 100ms (10Hz)
    root.after(100, update_gui)

if __name__ == '__main__':
    rclpy.init()
    ros_node = SwarmDashboard()

    # Start ROS in a background thread so the GUI doesn't freeze
    thread = threading.Thread(target=run_ros, args=(ros_node,), daemon=True)
    thread.start()

    # --- Setup GUI Window ---
    root = tk.Tk()
    root.title("SKYHUNTER Swarm Tactical Dashboard")
    root.geometry("600x400")

    # Table Setup
    columns = ('name', 'x', 'y', 'z')
    tree = ttk.Treeview(root, columns=columns, show='headings')
    
    tree.heading('name', text='Robot Unit')
    tree.heading('x', text='Global X (meters)')
    tree.heading('y', text='Global Y (meters)')
    tree.heading('z', text='Altitude Z (meters)')
    
    tree.column('name', width=150)
    tree.column('x', width=100)
    tree.column('y', width=100)
    tree.column('z', width=100)
    
    tree.pack(fill=tk.BOTH, expand=True, padx=10, pady=10)

    # Status Bar
    status_label = tk.Label(root, text="System: ONLINE | Domain: 42", bd=1, relief=tk.SUNKEN, anchor=tk.W)
    status_label.pack(side=tk.BOTTOM, fill=tk.X)

    # Start GUI update loop
    update_gui()
    
    try:
        root.mainloop()
    except KeyboardInterrupt:
        pass
    finally:
        rclpy.shutdown()