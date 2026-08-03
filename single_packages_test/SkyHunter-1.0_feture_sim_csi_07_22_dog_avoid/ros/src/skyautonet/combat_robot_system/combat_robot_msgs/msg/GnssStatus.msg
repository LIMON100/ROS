# GNSS / RTK 드라이버가 publish 하는 status.
# robot_server 가 /gnss/status 토픽으로 subscribe 하여 leader 위치/heading/speed 와
# 앱 상태 패킷의 fix 품질 표시에 사용함.
#
# 모든 필드를 매 주기 채워 보내주세요. 값을 알 수 없으면 아래 정의된 INVALID 값을 사용.

std_msgs/Header header

# Fix 품질 (NMEA / RTK 기준)
uint8 FIX_NONE = 0          # No fix / 수신기 끊김
uint8 FIX_2D = 1            # 2D 위성 fix
uint8 FIX_3D = 2            # 3D 위성 fix
uint8 FIX_DGPS = 3          # Differential GPS
uint8 FIX_RTK_FLOAT = 4     # RTK Float (cm~m 수준)
uint8 FIX_RTK_FIXED = 5     # RTK Fixed (cm 수준)
uint8 fix_status

# 추적 위성 수 (0~32 typical)
uint8 num_satellites

# 위치 — WGS-84
# 유효하지 않으면 latitude=longitude=NaN
float64 latitude            # 도
float64 longitude           # 도
float64 altitude_m          # MSL m

# 방위각 / 속도
# 유효하지 않으면 -1.0
float32 heading_deg         # 0~360, true north 기준
float32 ground_speed_mps    # >= 0

# 정확도 (1-sigma CEP)
# 알 수 없으면 -1.0
float32 horizontal_accuracy_m
float32 vertical_accuracy_m
