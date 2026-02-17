import rclpy
from rclpy.node import Node
from geometry_msgs.msg import Twist
import sys, select, termios, tty

msg = """
Control Your SkyHunter Robot!
---------------------------
Moving around:
        i
   j    k    l
        ,

i/, : forward/backward
j/l : turn left/right

q/z : increase/decrease max speeds by 10%
w/x : increase/decrease linear speed only
e/c : increase/decrease angular speed only

space key or k : force stop

CTRL-C to quit
"""

# Maps keys to movements (linear, angular)
moveBindings = {
    'i': (1, 0),
    'j': (0, 1),
    'l': (0, -1),
    ',': (-1, 0),
}

# Maps keys to speed adjustments (linear, angular)
speedBindings = {
    'q': (1.1, 1.1),
    'z': (0.9, 0.9),
    'w': (1.1, 1),
    'x': (0.9, 1),
    'e': (1, 1.1),
    'c': (1, 0.9),
}

def getKey(settings):
    tty.setraw(sys.stdin.fileno())
    rlist, _, _ = select.select([sys.stdin], [], [], 0.1)
    if rlist:
        key = sys.stdin.read(1)
    else:
        key = ''
    termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)
    return key

def print_status(speed, turn):
    print(f"Current Speeds:\tlinear {speed:.2f}\tangular {turn:.2f}")

def main(args=None):
    settings = termios.tcgetattr(sys.stdin)
    
    rclpy.init(args=args)
    node = rclpy.create_node('teleop_keyboard')
    pub = node.create_publisher(Twist, 'cmd_vel', 10)

    speed = 1.0  # Default linear speed
    turn = 1.0   # Default angular speed
    x, th = 0.0, 0.0

    try:
        print(msg)
        print_status(speed, turn)
        
        while True:
            key = getKey(settings)
            
            if key in moveBindings:
                x = moveBindings[key][0]
                th = moveBindings[key][1]
            elif key in speedBindings:
                speed = speed * speedBindings[key][0]
                turn = turn * speedBindings[key][1]
                print_status(speed, turn)
            elif key == ' ' or key == 'k':
                x, th = 0.0, 0.0
            elif key == '\x03': # CTRL-C
                break
            
            twist = Twist()
            twist.linear.x = x * speed
            twist.linear.y = 0.0
            twist.linear.z = 0.0
            twist.angular.x = 0.0
            twist.angular.y = 0.0
            twist.angular.z = th * turn
            pub.publish(twist)

    except Exception as e:
        print(e)

    finally:
        # Publish a final zero-velocity message
        twist = Twist()
        pub.publish(twist)
        termios.tcsetattr(sys.stdin, termios.TCSADRAIN, settings)

if __name__ == '__main__':
    main()