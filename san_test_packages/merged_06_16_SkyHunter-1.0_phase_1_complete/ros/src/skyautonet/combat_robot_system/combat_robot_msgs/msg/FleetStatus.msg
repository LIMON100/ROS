# SAN v1.5 — Fleet/swarm-wide status aggregation.
#
# Hub UGV 가 1 Hz 로 발행하는 군집 통합 상태. Operator UI + audit log
# + 임무 분석에서 사용.
#
# Publisher: san_hub_orchestrator/hub_orchestrator_node (1 Hz)
# Subscribers: operator UI, audit logger

std_msgs/Header header

uint32  total_robots                     # 등록된 전체 로봇 수
uint32  healthy_robots                   # 정상 운용 중 (HEALTHY)
uint32  limp_mode_robots                 # limp 모드 (DEGRADED)
uint32  disconnected_robots              # 통신 단절 (timeout 기준)

float32 min_battery_percent              # 최저 배터리 % (전체 fleet)
float32 mean_battery_percent             # 평균 배터리 %

float32 mission_progress_percent         # 임무 진행률 (0.0 ~ 100.0)

uint32  robots_with_rtk_fix              # RTK FIX 보유 로봇 수
uint32  robots_with_rtk_float            # RTK FLOAT 로봇 수
uint32  robots_with_no_fix               # GNSS fix 없는 로봇 수

uint32  active_threats_count             # 검출된 위협 (드론/적) 수
uint32  follower_map_tiles_received      # Hub 가 수신한 follower SLAM tile 수

uint64  last_aggregation_timestamp_ms    # 마지막 집계 시점 epoch ms
