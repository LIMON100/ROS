# SAN v1.5 IDS §5.11 — Pan-Tilt control command.
#
# Leader 또는 운용자가 특정 robot 의 pan-tilt 를 제어. SectorAssignment 를
# 받은 follower 가 자체 발행 (sweep 시작용) 도 가능.
#
# 권원:
#   * SDD-SWARM v1.5 §8.3 (Pan-Tilt 운용 모드)
#   * SDD-SWARM v1.5 §8.4 (Sweep 속도 공식)
#   * IDS-CMD v1.5 §5.11
#   * KPP 팬틸트 추적 ≤ 0.05° (Track 모드 정확도)

std_msgs/Header header

uint32  sequence
uint32  robot_id

float32 target_pan_deg       # heading 기준 (-180 ~ 180)
float32 target_tilt_deg      # 지평선 기준 (-30 ~ +60)
float32 speed_dps            # degrees / second

# Operating modes per SDD §8.3
uint8   MODE_SWEEP   = 0     # 할당 sector 내 좌우 왕복
uint8   MODE_FIXED   = 1     # 지정 방향 정지
uint8   MODE_TRACK   = 2     # 표적 위치 자동 추종 (≤ 0.05° KPP)
uint8   MODE_ENGAGE  = 3     # 「Confirm Fire」 직전 lock-on
uint8   mode

float32 sweep_range_deg      # sweep 시 좌우 폭 (sector_width 와 동일하게 설정)

uint64  timestamp_ms
