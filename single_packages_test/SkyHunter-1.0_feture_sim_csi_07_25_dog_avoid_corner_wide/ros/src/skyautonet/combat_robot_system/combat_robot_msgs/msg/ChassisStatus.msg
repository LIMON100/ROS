# 차량(chassis) 컨트롤러가 publish 하는 status.
# robot_server 가 /chassis/status 토픽으로 subscribe 하여 BMS / 구동 상태를
# 앱 상태 패킷의 battery_pct, velocity 등에 반영함.
#
# 가능한 한 모든 필드를 매 주기 채워 보내주세요. 값을 모르면 0 또는 NaN 사용 가능.

std_msgs/Header header

# Drive state
uint8 DRIVE_OK = 0          # 정상 운행 가능
uint8 DRIVE_FAULT = 1       # 모터/통신 장애 — 운행 불가
uint8 DRIVE_ESTOP = 2       # E-Stop 인가됨
uint8 drive_state

# 배터리 — BMS 값
uint8 battery_pct           # 0~100 (255 = 알 수 없음)
float32 battery_voltage_v   # V
float32 battery_current_a   # A  ( + = 방전 / - = 충전 )

# Velocity feedback — 차량이 실제로 움직이는 속도 (encoder/odometry 기반)
# 알 수 없으면 NaN.
float32 linear_velocity_mps     # 전진 양 = +, 후진 = -
float32 angular_velocity_rps    # rad/s, 반시계 양 = +

# Fault flags — bitmask. 여러 결함 동시 표현 가능.
uint32 FAULT_NONE = 0
uint32 FAULT_LEFT_WHEEL = 1
uint32 FAULT_RIGHT_WHEEL = 2
uint32 FAULT_LOW_BATTERY = 4
uint32 FAULT_OVERTEMP = 8
uint32 FAULT_COMM = 16          # chassis ↔ robot_server 통신 (Modbus 등) 장애
uint32 fault_flags

# 모터 최고 온도 (좌/우 휠 중 큰 값). 알 수 없으면 NaN.
float32 motor_temp_c
