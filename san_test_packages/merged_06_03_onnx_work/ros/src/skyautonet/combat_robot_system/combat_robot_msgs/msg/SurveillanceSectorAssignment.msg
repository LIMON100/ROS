# SAN v1.5 IDS §5.10 — Surveillance sector assignment.
#
# Leader 가 각 follower 에게 감시 sector 를 할당. 10초 주기 (default) 또는
# 이벤트 시 (대형 전환, 위협 식별, follower 탈락) 즉시 발행.
#
# 권원:
#   * SDD-SWARM v1.5 §8.1 (분배 원칙)
#   * SDD-SWARM v1.5 §8.5 (주기적 갱신)
#   * SDD-SUR v1.5 §3 (감시 영역 분배 정책)
#   * IDS-CMD v1.5 §5.10

std_msgs/Header header

uint32  sequence             # monotonic — for deduplication
uint32  robot_id             # 대상 robot

float32 sector_start_deg     # -180.0 ~ 180.0 (heading 기준)
float32 sector_end_deg
uint32  valid_period_sec     # default 10

# Priority — 다중 sector 할당 시 가중치
uint8   PRIORITY_PRIMARY      = 0
uint8   PRIORITY_THREAT_FOCUS = 1   # 위협 방향 집중 (§8.6.1)
uint8   PRIORITY_OVERLAP      = 2   # 인접 sector overlap 보강
uint8   priority

# 권장 Pan-Tilt 모드 (실제 모드는 PanTiltCommand 가 설정)
uint8   MODE_SWEEP  = 0
uint8   MODE_TRACK  = 1
uint8   MODE_FIXED  = 2
uint8   mode_hint

uint64  timestamp_ms
