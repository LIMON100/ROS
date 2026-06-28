# SAN v1.5 — Communication link status (san_comm_link 발행).
#
# active_link 값 (uint8):
#   0 = WIFI6 (primary, mesh)
#   1 = LTE   (failover)
#   2 = LORA  (last resort, low-bandwidth)
#
# Publisher: san_comm_link/comm_link_node (1 Hz)
# Subscribers: hub_orchestrator, role_management, mission_node

std_msgs/Header header

uint8   active_link                   # 현재 활성 통신 링크
bool    wifi6_ok                      # WiFi6 mesh 정상 여부
bool    lte_ok                        # LTE 정상 여부

uint32  wifi6_consec_ok_count         # WiFi6 연속 정상 카운트
uint32  wifi6_consec_fail_count       # WiFi6 연속 실패 카운트
uint32  switch_count                  # 전환 횟수 (누적)
uint64  last_switch_timestamp_ms      # 마지막 전환 시점 epoch ms
string  last_switch_reason            # 마지막 전환 사유 (debug)
