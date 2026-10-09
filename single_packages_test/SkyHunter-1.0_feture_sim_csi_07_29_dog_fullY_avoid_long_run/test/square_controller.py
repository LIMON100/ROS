import rclpy
from rclpy.node import Node
from combat_robot_msgs.msg import PanTiltState, PanTiltControlCommand
import time
import threading
import math

class SquareControllerNode(Node):
    """
    Pan/Tilt를 제어하여 사각형 패턴을 그리는 ROS2 노드.
    CONTROL_DIR 모드를 사용하여 목표 지점까지의 이동을 pan_tilt_controller에 위임합니다.
    """
    def __init__(self):
        super().__init__('square_controller_node')

        # ROS 파라미터 선언 (실행 시 변경 가능)
        self.declare_parameter('square_center_pan', 0.0)
        self.declare_parameter('square_center_tilt', 0.0)
        self.declare_parameter('square_size', 20.0)  # 사각형 한 변의 길이 (도)
        self.declare_parameter('move_speed', 30)     # 이동 속도 (0-255)
        self.declare_parameter('tolerance', 1.0)     # 목표 도달 허용 오차 (도)

        # Pan/Tilt 제어 명령을 보내는 퍼블리셔
        self.publisher_ = self.create_publisher(
            PanTiltControlCommand,
            '/pan_tilt_control_command',
            10)
        
        # Pan/Tilt 현재 상태를 받는 서브스크라이버
        self.subscription_ = self.create_subscription(
            PanTiltState,
            '/current_actuator_state_info',
            self.state_callback,
            10)

        # Pan/Tilt 상태 저장을 위한 변수
        self.current_pan = 0.0
        self.current_tilt = 0.0
        self.state_received = False
        self.lock = threading.Lock() # 스레드 간 데이터 동기화를 위한 잠금

        # 메인 제어 로직을 별도의 스레드에서 실행
        self.control_thread = threading.Thread(target=self.run_square_pattern)
        self.control_thread.daemon = True
        self.control_thread.start()

    def state_callback(self, msg):
        """PanTiltState 메시지를 수신할 때마다 호출되는 콜백 함수"""
        with self.lock:
            self.current_pan = msg.horizontal_angle
            self.current_tilt = msg.vertical_angle
            if not self.state_received:
                self.state_received = True
                self.get_logger().info(f"초기 상태 수신 완료: Pan={self.current_pan:.2f}, Tilt={self.current_tilt:.2f}")

    def move_to_target(self, target_pan, target_tilt):
        """지정된 목표 지점으로 이동 명령을 보내고 도달할 때까지 대기하는 함수"""
        speed = self.get_parameter('move_speed').get_parameter_value().integer_value
        tolerance = self.get_parameter('tolerance').get_parameter_value().double_value

        self.get_logger().info(f"목표 지점으로 이동 시작: Pan={target_pan:.2f}, Tilt={target_tilt:.2f}")

        # CONTROL_DIR 모드로 제어 메시지 생성
        msg = PanTiltControlCommand()
        msg.control_mode = PanTiltControlCommand.CONTROL_DIR
        msg.horizontal_angle = float(target_pan)
        msg.vertical_angle = float(target_tilt)
        msg.pan_speed = speed
        msg.tilt_speed = speed
        self.publisher_.publish(msg)

        # 목표 지점에 도달할 때까지 대기
        while rclpy.ok():
            with self.lock:
                # Pan 각도 오차 계산 (0~360도 순환 고려)
                pan_error = abs(self.current_pan - target_pan)
                if pan_error > 180.0:
                    pan_error = 360.0 - pan_error
                
                tilt_error = abs(self.current_tilt - target_tilt)

            # 오차가 허용 범위 이내로 들어오면 루프 종료
            if pan_error < tolerance and tilt_error < tolerance:
                self.get_logger().info(f"목표 지점 도달: Pan={self.current_pan:.2f}, Tilt={self.current_tilt:.2f}")
                break
            
            time.sleep(0.1) # CPU 사용량을 줄이기 위한 짧은 대기

    def run_square_pattern(self):
        """사각형 패턴을 그리는 메인 제어 로직"""
        # 첫 상태 메시지를 받을 때까지 대기
        while not self.state_received and rclpy.ok():
            self.get_logger().info("Pan/Tilt 상태 메시지를 기다리는 중...")
            time.sleep(1.0)
        
        if not rclpy.ok():
            return

        # 파라미터에서 사각형 정보 가져오기
        center_pan = self.get_parameter('square_center_pan').get_parameter_value().double_value
        center_tilt = self.get_parameter('square_center_tilt').get_parameter_value().double_value
        half_size = self.get_parameter('square_size').get_parameter_value().double_value / 2.0

        # 사각형의 4개 꼭짓점 좌표 계산
        corners = [
            (center_pan - half_size, center_tilt - half_size), # 1. 좌하단
            (center_pan + half_size, center_tilt - half_size), # 2. 우하단
            (center_pan + half_size, center_tilt + half_size), # 3. 우상단
            (center_pan - half_size, center_tilt + half_size)  # 4. 좌상단
        ]

        self.get_logger().info("사각형 그리기 패턴을 시작합니다...")
        
        # 먼저 시작점(첫 번째 꼭짓점)으로 이동
        self.move_to_target(corners[0][0], corners[0][1])
        time.sleep(1.0) # 각 꼭짓점에서 잠시 대기

        corner_index = 1
        while rclpy.ok():
            target_pan, target_tilt = corners[corner_index]
            self.move_to_target(target_pan, target_tilt)
            time.sleep(1.0) # 각 꼭짓점에서 잠시 대기

            corner_index = (corner_index + 1) % 4 # 다음 꼭짓점으로 인덱스 순환

    def send_stop_command(self):
        """BRAKE 명령을 전송하여 Pan/Tilt 유닛을 정지시킵니다."""
        msg = PanTiltControlCommand()
        msg.control_mode = PanTiltControlCommand.CONTROL_BRAKE
        self.publisher_.publish(msg)
        self.get_logger().info("정지 명령을 전송했습니다.")

def main(args=None):
    rclpy.init(args=args)
    node = SquareControllerNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        node.get_logger().info("키보드 인터럽트 수신. 정지 명령을 보냅니다...")
        node.send_stop_command()
        # 명령이 전송될 시간을 잠시 줍니다.
        time.sleep(0.5)
    finally:
        # 노드 소멸 및 rclpy 종료
        node.get_logger().info("노드를 종료합니다.")
        node.destroy_node()
        rclpy.shutdown()

if __name__ == '__main__':
    main()
