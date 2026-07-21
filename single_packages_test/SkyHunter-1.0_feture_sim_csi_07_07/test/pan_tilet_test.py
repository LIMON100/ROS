#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PTZ 카메라 시리얼 제어 테스트 코드
J-PT-2315HP 모델용
"""

import serial
import time
import struct



class PTZController:
    # 공통 딜레이 설정
    delay = 0.1
    def __init__(self, port='COM3', baudrate=9600, timeout=1):
        """
        PTZ 컨트롤러 초기화
        
        Args:
            port: 시리얼 포트 (Windows: COM1, COM2... / Linux: /dev/ttyUSB0, /dev/ttyS0...)
            baudrate: 통신 속도 (기본값: 9600)
            timeout: 타임아웃 (초)
        """
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self.ser = None
        
    def connect(self):
        """시리얼 연결"""
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=self.timeout
            )
            print(f"시리얼 연결 성공: {self.port}")
            return True
        except Exception as e:
            print(f"시리얼 연결 실패: {e}")
            return False
    
    def disconnect(self):
        """시리얼 연결 해제"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("시리얼 연결 해제됨")
    
    def calculate_checksum(self, data):
        """체크섬 계산"""
        return sum(data) & 0xFF
    
    def send_command(self, address, speed, command_type, angle):
        """
        PTZ 명령 전송
        
        Args:
            address: 카메라 주소 (0x01-0xFF)
            speed: 속도 (0x00-0xFF)
            command_type: 명령 타입 (Pan: 0x4B, Tilt: 0x4D, Pan inquiry: 0x51, Tilt inquiry: 0x53)
            angle: 각도 (0x0000-0xFFFF)
        """
        if not self.ser or not self.ser.is_open:
            print("시리얼 연결이 되어있지 않습니다.")
            return False
        
        # 프로토콜에 따른 패킷 구성
        # Byte0: 0xFF (고정)
        # Byte1: address
        # Byte2: speed
        # Byte3: command_type (Pan: 0x4B, Tilt: 0x4D, Pan inquiry: 0x51, Tilt inquiry: 0x53)
        # Byte4: angle high byte
        # Byte5: angle low byte
        # Byte6: checksum
        
        packet = bytearray(7)
        packet[0] = 0xFF  # 시작 바이트
        packet[1] = address & 0xFF
        packet[2] = speed & 0xFF
        packet[3] = command_type & 0xFF  # 명령 타입
        packet[4] = (angle >> 8) & 0xFF  # 각도 상위 바이트
        packet[5] = angle & 0xFF         # 각도 하위 바이트
        packet[6] = self.calculate_checksum(packet[1:6])
        
        try:
            self.ser.write(packet)
            print(f"명령 전송: {' '.join([f'0x{b:02X}' for b in packet])}")
            time.sleep(self.delay)  # 명령 전송 후 대기
            return True
        except Exception as e:
            print(f"명령 전송 실패: {e}")
            return False
    
    def send_pan_position_inquiry(self, address):
        """Pan Position inquiry 명령"""
        return self.send_command(address, 0x00, 0x51, 0x0000)
    
    def send_tilt_position_inquiry(self, address):
        """Tilt Position inquiry 명령"""
        return self.send_command(address, 0x00, 0x53, 0x0000)
    
    def send_pan_control(self, address, speed, angle):
        """Pan 제어 명령"""
        return self.send_command(address, speed, 0x4B, angle)
    
    def send_tilt_control(self, address, speed, angle):
        """Tilt 제어 명령"""
        return self.send_command(address, speed, 0x4D, angle)
    
    def send_goto_origin(self, address, speed=0x10, preset_id=223):
        """
        Origin으로 돌아가는 명령
        0xFF 0x01 0x10(speed) 0x07 0x00 223(preset ID) sum
        """
        if not self.ser or not self.ser.is_open:
            print("시리얼 연결이 되어있지 않습니다.")
            return False
        
        packet = bytearray(7)
        packet[0] = 0xFF  # 시작 바이트
        packet[1] = address & 0xFF
        packet[2] = speed & 0xFF
        packet[3] = 0x07  # Origin 명령
        packet[4] = 0x00  # 고정값
        packet[5] = preset_id & 0xFF  # Preset ID (기본 223)
        packet[6] = self.calculate_checksum(packet[1:6])
        
        try:
            self.ser.write(packet)
            print(f"Origin 명령 전송: {' '.join([f'0x{b:02X}' for b in packet])}")
            time.sleep(self.delay)  # 명령 전송 후 대기
            return True
        except Exception as e:
            print(f"Origin 명령 전송 실패: {e}")
            return False

    def send_direction_control(self, address, direction, pan_speed, tilt_speed):
        """
        방향 제어 명령 전송

        Args:
            address: 카메라 주소 (0x01-0xFF)
            direction: 방향 (e.g., UP: 0x08, DOWN: 0x10, LEFT: 0x04, RIGHT: 0x02, STOP: 0x00)
            pan_speed: 팬 속도 (0x00-0x3F, 0x40 is turbo)
            tilt_speed: 틸트 속도 (0x00-0x3F)
        """
        if not self.ser or not self.ser.is_open:
            print("시리얼 연결이 되어있지 않습니다.")
            return False

        # 프로토콜에 따른 패킷 구성
        # FF adr 0x00 dir 0xpp(pan_speed) 0xpp(pan_speed) 0xqq(tilt_speed) sum
        # Stop: FF adr 0x00 0x00 0x00 0x00 sum

        packet = bytearray(7)
        packet[0] = 0xFF
        packet[1] = address & 0xFF
        packet[2] = 0x00
        packet[3] = direction & 0xFF
        packet[4] = pan_speed & 0xFF
        packet[5] = tilt_speed & 0xFF
        packet[6] = self.calculate_checksum(packet[1:6])

        try:
            self.ser.write(packet)
            print(f"방향 제어 명령 전송: {' '.join([f'0x{b:02X}' for b in packet])}")
            time.sleep(self.delay)  # 명령 전송 후 대기
            return True
        except Exception as e:
            print(f"방향 제어 명령 전송 실패: {e}")
            return False
    def read_response(self):
        """응답 읽기"""
        if not self.ser or not self.ser.is_open:
            return None
        
        try:
            if self.ser.in_waiting > 0:
                response = self.ser.read(self.ser.in_waiting)
                return response
        except Exception as e:
            print(f"응답 읽기 실패: {e}")
        return None


def test_ptz_control():
    """PTZ 제어 테스트"""
    print("=== PTZ 카메라 시리얼 제어 테스트 ===")
    

    
    # 시리얼 포트 설정 (환경에 맞게 수정)
    # Windows: 'COM3', 'COM4' 등
    # Linux: '/dev/ttyUSB0', '/dev/ttyS0' 등
    port = input("시리얼 포트를 입력하세요 (예: COM3 또는 /dev/ttyUSB0): ").strip()
    if not port:
        port = 'COM3'  # 기본값
    
    ptz = PTZController(port=port)
    
    if not ptz.connect():
        return
    
    try:
        while True:
            print("\n=== 테스트 메뉴 ===")
            print("1. 기본 명령 테스트")
            print("2. 사용자 정의 명령")
            print("3. 방향 제어 테스트")
            print("5. Pan Position inquiry")
            print("6. Tilt Position inquiry")
            print("7. Pan/Tilt 제어 테스트")
            print("8. Origin으로 돌아가기")
            print("0. 종료")
            
            choice = input("선택하세요: ").strip()
            
            if choice == '0':
                break
            elif choice == '1':
                # 기본 명령 테스트
                print("\n--- 기본 명령 테스트 ---")
                test_commands = [
                    (0x01, 0x10, 0x4B, 0x0000),  # Pan 제어
                    (0x01, 0x20, 0x4D, 0x1000),  # Tilt 제어
                    (0x01, 0x30, 0x4B, 0x2000),  # Pan 제어
                    (0x01, 0x00, 0x4D, 0x0000),  # Tilt 정지
                ]
                
                for addr, speed, cmd_type, angle in test_commands:
                    cmd_name = "Pan" if cmd_type == 0x4B else "Tilt"
                    print(f"전송: {cmd_name} 주소=0x{addr:02X}, 속도=0x{speed:02X}, 각도=0x{angle:04X}")
                    ptz.send_command(addr, speed, cmd_type, angle)
                    time.sleep(ptz.delay)  # 명령 간 대기 시간
            
            elif choice == '2':
                # 사용자 정의 명령
                print("\n--- 사용자 정의 명령 ---")
                print("6바이트 패킷을 16진수 형태로 스페이스로 구분하여 입력하세요 (예: FF 01 10 4B 00 00)")
                print("마지막 바이트(체크섬)는 자동으로 계산됩니다.")
                try:
                    user_input = input("패킷 입력 (6바이트, 16진수): ").strip()
                    if not user_input:
                        print("입력이 비어있습니다.")
                        continue
                    
                    # 스페이스로 구분된 16진수 문자열을 파싱
                    values = user_input.split()
                    if len(values) != 6:
                        print("정확히 6개의 값을 입력해야 합니다.")
                        continue
                    
                    # 각 값을 16진수 정수로 변환하고 범위 확인
                    packet = bytearray(7)
                    for i, value in enumerate(values):
                        num = int(value, 16) # 16진수로 변환
                        if 0 <= num <= 255:
                            packet[i] = num
                        else:
                            print(f"값 {num}이 범위(0-255)를 벗어났습니다.")
                            break
                    else:
                        # 패킷 전송
                        if ptz.ser and ptz.ser.is_open:
                            # 체크섬 계산
                            packet[6] = ptz.calculate_checksum(packet[1:6])
                            ptz.ser.write(packet)
                            print(f"사용자 정의 명령 전송: {' '.join([f'0x{b:02X}' for b in packet])}")
                            time.sleep(ptz.delay)  # 명령 전송 후 대기
                        else:
                            print("시리얼 연결이 되어있지 않습니다.")
                        
                except ValueError:
                    print("잘못된 입력입니다. 16진수 숫자만 입력하세요.")
                except Exception as e:
                    print(f"명령 전송 실패: {e}")
            
            elif choice == '3':
                # 방향 제어 테스트
                print("\n--- 방향 제어 테스트 ---")
                addr = 0x01
                pan_speed = 0x20
                tilt_speed = 0x20
                
                try:
                    ps_input = input(f"Pan Speed (0~63, 64 is turbo, default: {pan_speed}): ").strip()
                    if ps_input:
                        pan_speed = int(ps_input)
                    
                    ts_input = input(f"Tilt Speed (0~63, default: {tilt_speed}): ").strip()
                    if ts_input:
                        tilt_speed = int(ts_input)

                    if not (0 <= pan_speed <= 64 and 0 <= tilt_speed <= 63):
                        print("속도 값이 범위를 벗어났습니다. 기본값으로 실행합니다.")
                        pan_speed = 0x20
                        tilt_speed = 0x20

                except ValueError:
                    print("잘못된 입력입니다. 기본값으로 실행합니다.")
                    pan_speed = 0x20
                    tilt_speed = 0x20
                
                # 각도 모니터링 설정
                monitor_angle = False
                monitor_interval = 1.0  # 기본 1초 간격
                
                try:
                    monitor_input = input("각도 모니터링을 활성화하시겠습니까? (y/n, 기본: n): ").strip().lower()
                    if monitor_input == 'y':
                        monitor_angle = True
                        interval_input = input("모니터링 간격(초, 기본: 1.0): ").strip()
                        if interval_input:
                            monitor_interval = float(interval_input)
                        print(f"각도 모니터링이 활성화되었습니다. 간격: {monitor_interval}초")
                except ValueError:
                    print("잘못된 간격 값입니다. 기본값 1.0초를 사용합니다.")
                
                dir_map = {
                    '1': ("Up", 0x08),
                    '2': ("Down", 0x10),
                    '3': ("Left", 0x04),
                    '4': ("Right", 0x02),
                    '5': ("Up-Left", 0x0C),   # 0x08 + 0x04
                    '6': ("Up-Right", 0x0A),  # 0x08 + 0x02
                    '7': ("Down-Left", 0x14), # 0x10 + 0x04
                    '8': ("Down-Right", 0x12),# 0x10 + 0x02
                    '9': ("Stop", 0x00),
                }
                
                last_monitor_time = time.time()
                
                while True:
                    print("\n-- 방향 선택 --")
                    for k, v in dir_map.items():
                        print(f"{k}. {v[0]}")
                    print("0. 이전 메뉴로")
                    # 각도 모니터링이 활성화된 경우 주기적으로 각도 확인
                    if monitor_angle and (time.time() - last_monitor_time) >= monitor_interval:
                        print("\n--- 현재 각도 확인 ---")
                        
                        # Pan Position inquiry (재시도 포함)
                        pan_response = None
                        for retry in range(3):  # 최대 3번 재시도
                            time.sleep(ptz.delay)
                            ptz.send_pan_position_inquiry(addr)
                            time.sleep(ptz.delay)  # 응답 대기 시간
                            pan_response = ptz.read_response()
                            if pan_response and len(pan_response) >= 6:
                                break
                        
                        # Tilt Position inquiry (재시도 포함)
                        tilt_response = None
                        for retry in range(3):  # 최대 3번 재시도
                            time.sleep(ptz.delay)  # Pan과 Tilt 명령 사이 대기
                            ptz.send_tilt_position_inquiry(addr)
                            time.sleep(ptz.delay)  # 응답 대기 시간
                            tilt_response = ptz.read_response()
                            if tilt_response and len(tilt_response) >= 6:
                                break
                        
                        pan_position = "N/A"
                        tilt_position = "N/A"
                        
                        if pan_response and len(pan_response) >= 6:
                            pan_pos = (pan_response[4] << 8) | pan_response[5]
                            pan_position = f"{pan_pos} (0x{pan_pos:04X})"
                        else:
                            print("Pan 응답 없음 (3번 재시도 후)")
                        
                        if tilt_response and len(tilt_response) >= 6:
                            tilt_pos = (tilt_response[4] << 8) | tilt_response[5]
                            tilt_position = f"{tilt_pos} (0x{tilt_pos:04X})"
                        else:
                            print("Tilt 응답 없음 (3번 재시도 후)")
                        
                        print(f"Pan Position: {pan_position}")
                        print(f"Tilt Position: {tilt_position}")
                        print("-" * 30)
                        
                        last_monitor_time = time.time()
                    
                    dir_choice = input("선택: ").strip()
                    if dir_choice == '0':
                        print("정지 명령 후 이전 메뉴로 돌아갑니다.")
                        ptz.send_direction_control(addr, 0x00, 0x00, 0x00)
                        break
                    
                    if dir_choice in dir_map:
                        dir_name, dir_val = dir_map[dir_choice]
                        if dir_name == "Stop":
                            ptz.send_direction_control(addr, 0x00, 0x00, 0x00)
                        else:
                            ptz.send_direction_control(addr, dir_val, pan_speed, tilt_speed)
                    else:
                        print("잘못된 선택입니다.")
            elif choice == '5':
                # Pan Position inquiry
                print("\n--- Pan Position inquiry ---")
                addr = 0x01
                print(f"Pan Position inquiry 전송 (주소: 0x{addr:02X})")
                ptz.send_pan_position_inquiry(addr)
                time.sleep(ptz.delay)  # 응답 대기 시간
                
                response = ptz.read_response()
                if response:
                    print(f"Pan Position 응답: {' '.join([f'0x{b:02X}' for b in response])}")
                    if len(response) >= 6:
                        pan_position = (response[4] << 8) | response[5]
                        print(f"Pan Position 값: {pan_position} (0x{pan_position:04X})")
                else:
                    print("응답이 없습니다.")
            elif choice == '6':
                # Tilt Position inquiry
                print("\n--- Tilt Position inquiry ---")
                addr = 0x01
                print(f"Tilt Position inquiry 전송 (주소: 0x{addr:02X})")
                ptz.send_tilt_position_inquiry(addr)
                time.sleep(ptz.delay)  # 응답 대기 시간
                
                response = ptz.read_response()
                if response:
                    print(f"Tilt Position 응답: {' '.join([f'0x{b:02X}' for b in response])}")
                    if len(response) >= 6:
                        tilt_position = (response[4] << 8) | response[5]
                        print(f"Tilt Position 값: {tilt_position} (0x{tilt_position:04X})")
                else:
                    print("응답이 없습니다.")
            
            elif choice == '7':
                # Pan/Tilt 제어 테스트
                print("\n--- Pan/Tilt 제어 테스트 ---")
                addr = 0x01
                
                # Pan 제어
                print("Pan 제어 테스트...")
                ptz.send_pan_control(addr, 0x20, 0x1000)
                time.sleep(ptz.delay)  # 명령 간 대기 시간
                
                # Tilt 제어
                print("Tilt 제어 테스트...")
                ptz.send_tilt_control(addr, 0x20, 0x800)
                time.sleep(ptz.delay)  # 명령 간 대기 시간
                
                # 원점 복귀
                print("원점 복귀...")
                ptz.send_pan_control(addr, 0x10, 0x0000)
                time.sleep(ptz.delay)  # 명령 간 대기 시간
                ptz.send_tilt_control(addr, 0x10, 0x0000)
                
                print("Pan/Tilt 제어 테스트 완료")
            
            elif choice == '8':
                # Origin으로 돌아가기
                print("\n--- Origin으로 돌아가기 ---")
                addr = 0x01
                speed = 0x10
                preset_id = 223
                
                try:
                    speed_input = input(f"속도 (기본값 {speed}): ").strip()
                    if speed_input:
                        speed = int(speed_input)
                    
                    preset_input = input(f"Preset ID (기본값 {preset_id}): ").strip()
                    if preset_input:
                        preset_id = int(preset_input)
                    
                    if 0 <= speed <= 255 and 0 <= preset_id <= 255:
                        print(f"Origin 명령 전송: 주소=0x{addr:02X}, 속도=0x{speed:02X}, Preset ID={preset_id}")
                        ptz.send_goto_origin(addr, speed, preset_id)
                    else:
                        print("입력값이 범위를 벗어났습니다.")
                except ValueError:
                    print("잘못된 입력입니다. 기본값으로 실행합니다.")
                    ptz.send_goto_origin(addr, speed, preset_id)
            
            else:
                print("잘못된 선택입니다.")
    
    except KeyboardInterrupt:
        print("\n사용자에 의해 중단되었습니다.")
    
    finally:
        ptz.disconnect()

def main():
    """메인 함수"""
    print("PTZ 카메라 시리얼 제어 테스트 프로그램")
    print("필요한 패키지: pyserial")
    print("설치 방법: pip install pyserial")
    print("-" * 50)
    
    try:
        import serial
        test_ptz_control()
    except ImportError:
        print("pyserial 패키지가 설치되지 않았습니다.")
        print("다음 명령으로 설치하세요: pip install pyserial")

if __name__ == "__main__":
    main()

