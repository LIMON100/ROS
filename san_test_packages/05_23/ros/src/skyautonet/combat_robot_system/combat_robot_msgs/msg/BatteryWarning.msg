# SAN v1.5 IDS §4.7 — robot → swarm battery warning (event-driven).
#
# RobotStatus.battery_percent 의 임계값 횡단 (cross) 시 event 로 발행.
# 정기 broadcast 가 아닌 transition event — 30% / 20% / 10% / 5% 임계
# 횡단 시점에만 publish.
#
# 수신 측 행동:
#   - mission_node BT: BatteryCritical (P3) 우선순위 트리거
#   - 운용병사 UI: 시각/청각 알림
#   - Hub UGV: D-005 Hub 승계 가능성 사전 계산
#
# 발신: 각 robot 의 robot_status_publisher (event-driven)
# 권원: IDS-CMD v1.5 §4.7 (PDR-7 신설)
#       SDD-SWARM v1.5 §6.1 P3 BatteryCritical subtree
#
# QoS: RELIABLE, KEEP_LAST(10) — 운용자에게 모든 transition 전달 보장

# ─── 경고 레벨 enum ─────────────────────────────────────────
uint8 LEVEL_INFO         = 0    # 정상 (≥30%) — broadcast 안함
uint8 LEVEL_WARN         = 1    # 20-30% — RTH 권고 시작
uint8 LEVEL_CRITICAL     = 2    # 10-20% — 즉시 RTH (BT P3)
uint8 LEVEL_EMERGENCY    = 3    # <10% — 안전 정지 (Stand)

# ─── 권고 행동 enum ─────────────────────────────────────────
uint8 ACTION_CONTINUE    = 0    # 임무 지속
uint8 ACTION_RTH_SOON    = 1    # 30 분 내 RTH 권고
uint8 ACTION_RTH_NOW     = 2    # 즉시 RTH (현재 임무 중단)
uint8 ACTION_STAND_DOWN  = 3    # 안전 정지 (이동 불가)

std_msgs/Header header

uint32 robot_id
uint32 sequence

uint8  level                          # LEVEL_*
uint8  prior_level                    # 직전 level (transition 추적)
uint8  recommended_action             # ACTION_*

float32 battery_percent               # 현재 (0.0-100.0)
float32 voltage_v                     # 셀 전압 (정합 검증용)
float32 current_draw_a                # 평균 소모 전류
float32 estimated_minutes_remaining   # 현재 운용 모드 기준 추정

string reason                         # operator-facing 텍스트 (예: "Crossed 20% threshold")

uint64 timestamp_ms                   # epoch ms
