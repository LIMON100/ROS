# LiDAR 드라이버가 publish 하는 status.
# robot_server 가 /lidar/status 토픽으로 subscribe 하여 LiDAR 헬스 / 장애물 정보를
# 추후 status 패킷에 반영하거나 안전 분기 입력으로 활용함.
#
# 가능한 한 모든 필드를 매 주기 채워 보내주세요.

std_msgs/Header header

# Driver / hardware 상태
uint8 LIDAR_OK = 0          # 정상 스캔 중
uint8 LIDAR_DEGRADED = 1    # 스캔되지만 품질 저하 (rate↓, noise↑)
uint8 LIDAR_FAULT = 2       # 통신 / hardware 장애
uint8 status

# 마지막 스캔 통계
uint32 last_scan_point_count
float32 scan_rate_hz        # 실측 Hz (목표값과 차이 나면 LIDAR_DEGRADED 권장)

# 장애물 감지 — 안전 정지 입력으로 활용 가능.
# 가까이 있는 장애물 없으면 obstacle_detected=false, min_obstacle_distance_m=NaN.
bool obstacle_detected
float32 min_obstacle_distance_m
