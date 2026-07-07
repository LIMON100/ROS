# SAN v1.5 IDS §3.8 — operator → swarm deadman heartbeat.
#
# 운용병사 (Galaxy Tab S9) 가 군집에게 보내는 주기적 alive 신호.
# 수신 측 (mission_node, hub_orchestrator) 은 마지막 수신 후 timeout
# (기본 3 초) 동안 새 heartbeat 가 없으면 OPERATOR_LOST 상태로 전환:
#   - mission_node → priority.health_critical = true → P2 RTH
#   - 모든 fire authorization 자동 차단
#
# v1.5 신설 (PDR-7) — DCN-2026-001 D-004 의 보조 신호:
#   * D-004 의 Audit log 무결성 향상 (operator session 추적)
#   * D-005 의 Leader 승계 시 operator 측 session 연속성 확인
#
# 발신 주기: 1 Hz (default), max 5 Hz
# QoS: BEST_EFFORT, KEEP_LAST(5)

std_msgs/Header header

uint32 sequence                       # 단조 증가
string operator_id                    # 운용병사 식별 (예: "OP-12")
string session_id                     # session UUID — Leader 승계 후에도 유지
uint32 last_command_id                # operator 가 마지막 발행한 command_id (D-004 echo)

# 운용병사 측 상태
float32 tablet_battery_percent        # Galaxy Tab S9 배터리 (0-100)
int16   uplink_rssi_dbm               # LTE/WiFi 신호 강도 (-120 ~ 0)
bool    operator_alive                # 항상 true; absence (= 메시지 단절) 이 dead

uint64  timestamp_ms                  # epoch ms — replay 보호용
