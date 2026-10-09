#!/usr/bin/env python3

import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import time
import pygame
import math

class RemoteTeleop(Node):
    def __init__(self):
        super().__init__('remote_teleop')
        self.publisher_ = self.create_publisher(Twist, '/user_command', 1)

        # Pygame initialization
        pygame.init()
        pygame.display.set_mode((100, 100)) # A small window is required for event handling

        # Velocity parameters
        self.max_linear = 100.0 # 12.5 = actual speed 1 km/h
        self.max_angular = 100.0
        self.linear_increment = 1
        self.angular_increment = 1


        # Short press configuration
        self.LINEAR_SHORT_PRESS_VEL = 31.25
        self.ANGULAR_SHORT_PRESS_VEL = 78.54
        self.LINEAR_SHORT_PRESS_DURATION = 5.0
        self.ANGULAR_SHORT_PRESS_DURATION = 5.0
        self.LONG_PRESS_THRESHOLD = 0.2  # 200ms

        # State variables
        self.linear_velocity = 0.0
        self.angular_velocity = 0.0
        self.key_down_times = {}
        self.active_timers = {}

        self.decelerating_linear = False

        self.get_logger().info("==== Remote Teleop Control (Pygame) ====")
        self.get_logger().info("Click the Pygame window to give it focus.")
        self.get_logger().info("W/S for Linear | A/D for Angular | Q to Stop")
        self.get_logger().info("==========================================")

    def run(self):
        try:
            while rclpy.ok():
                now = time.time()
                keys_down = self.key_down_times.keys()

                # --- Pygame Event Handling ---
                for event in pygame.event.get():
                    if event.type == pygame.QUIT:
                        return # Exit the loop

                    if event.type == pygame.KEYDOWN:
                        if event.key in [pygame.K_w, pygame.K_s, pygame.K_a, pygame.K_d, pygame.K_q]:
                            self.key_down_times[event.key] = now
                        if event.key in [pygame.K_w, pygame.K_s]:
                            self.decelerating_linear = False

                    
                    #Key Release Action
                    if event.type == pygame.KEYUP:
                        if event.key in self.key_down_times:
                            down_time = self.key_down_times.pop(event.key)

                            #Short Press Release Action
                            if now - down_time < self.LONG_PRESS_THRESHOLD:
                               
                                if event.key == pygame.K_w:  # Linear : maintain velocity till q is pressed          
                                    self.linear_velocity = self.LINEAR_SHORT_PRESS_VEL
                                elif event.key == pygame.K_s:
                                    self.linear_velocity = -self.LINEAR_SHORT_PRESS_VEL
                                elif event.key == pygame.K_a: # Angular   
                                    self.active_timers['angular'] = now + self.ANGULAR_SHORT_PRESS_DURATION
                                    self.angular_velocity = self.ANGULAR_SHORT_PRESS_VEL
                                elif event.key == pygame.K_d:
                                    self.active_timers['angular'] = now + self.ANGULAR_SHORT_PRESS_DURATION
                                    self.angular_velocity = -self.ANGULAR_SHORT_PRESS_VEL
                            else:
                                # Long Press Release Action
                                if event.key in [pygame.K_w, pygame.K_s]:
                                    self.decelerating_linear = True
                                if event.key in [pygame.K_a, pygame.K_d]:
                                    self.angular_velocity = 0

                # --- Long Press Continuous Action ---
                for key, down_time in self.key_down_times.items():
                    if now - down_time > self.LONG_PRESS_THRESHOLD:
                        if key == pygame.K_w:
                            self.linear_velocity += self.linear_increment
                        elif key == pygame.K_s:
                            self.linear_velocity -= self.linear_increment
                        elif key == pygame.K_a:
                            self.angular_velocity += self.angular_increment
                        elif key == pygame.K_d:
                            self.angular_velocity -= self.angular_increment

                # Linear Long Press Release Action
                if not {pygame.K_w, pygame.K_s}.intersection(keys_down):
                    if self.decelerating_linear: 
                        if self.linear_velocity > 0:
                            self.linear_velocity = max(0, self.linear_velocity - self.linear_increment)
                        elif self.linear_velocity < 0:
                              self.linear_velocity = min(0, self.linear_velocity + self.linear_increment)
                        else:
                            self.decelerating_linear = False 

                # Angular Short Release Action
                if 'angular' in self.active_timers and now > self.active_timers['angular']:
                    self.angular_velocity = 0
                    del self.active_timers['angular']

                
                # --- Stop Key (Immediate) ---
                if pygame.K_q in self.key_down_times:
                    self.linear_velocity = 0.0
                    self.angular_velocity = 0.0
                    self.key_down_times = {}
                    self.active_timers = {}

                # --- Clamp Velocities ---
                self.linear_velocity = max(-self.max_linear, min(self.max_linear, self.linear_velocity))
                self.angular_velocity = max(-self.max_angular, min(self.max_angular, self.angular_velocity))

                # --- Publish Twist Message ---
                twist = Twist()
                twist.linear.x = self.linear_velocity * (2.22 / 100.0) # maximum linear velocity : 2.22 [m/s]
                twist.angular.z = self.angular_velocity * (6.0 / 100.0) # maximum angular velocity : 6 [rad/s]
                self.publisher_.publish(twist)
                
                # --- Logger ---
                #self.get_logger().info(
                #        f"Lin Vel: {self.linear_velocity:.2f} , Ang Vel: {self.angular_velocity:.2f}  | "
                #        f"Keys Down: {[pygame.key.name(k) for k in self.key_down_times]}"
                #        , throttle_duration_sec=0.1)
                self.get_logger().info(
                        f"Lin Vel: {twist.linear.x * 3.6 :.2f} [km/h] , Ang Vel: {twist.angular.z:.2f} [rad/s]")
            
               

                rclpy.spin_once(self, timeout_sec=0.05)

        except KeyboardInterrupt:
            pass
        finally:
            pygame.quit()

def main(args=None):
    rclpy.init(args=args)
    node = RemoteTeleop()
    node.run()
    node.destroy_node()
    rclpy.shutdown()

if __name__ == '__main__':
    main()
