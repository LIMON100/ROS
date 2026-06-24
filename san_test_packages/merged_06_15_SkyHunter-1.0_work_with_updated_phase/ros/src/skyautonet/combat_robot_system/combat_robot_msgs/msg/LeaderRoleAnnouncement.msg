# SAN v1.5 IDS §5.15 — 4-tier Leader 승계 broadcast.
#
# Drives the Deputy → Hub → battery-max follower → Limp Mode chain.
# Every candidate self-declares with a CANDIDATE announcement after a
# priority-tier grace period; the highest-priority candidate wins by
# emitting PROMOTED. Split-brain prevention via monotonically
# increasing leader_term.
#
# v1.5 changes (DCN-2026-001 D-005):
#   - Body formally added to IDS (v1.4 promised only in Revision History).
#   - PRIORITY_UNSPECIFIED = 0 enum value added (matches v1.5 IDS proto IDL).

uint8 LEADER_NORMAL    = 0    # 정상 Leader 운용 (or 일반 robot)
uint8 LEADER_CANDIDATE = 1    # 자기 선언 후보
uint8 LEADER_PROMOTED  = 2    # 승계 완료, 현재 Leader 역할 수행
uint8 LEADER_DEMOTED   = 3    # 원 Leader 복구로 강등

# Succession priority — also broadcast so receivers can sanity-check
# the candidate self-tier matches their own view of the swarm health.
uint8 PRIORITY_UNSPECIFIED  = 0    # v1.5: explicit 0-default (matches IDS §5.15 IDL)
uint8 PRIORITY_DEPUTY       = 1
uint8 PRIORITY_HUB          = 2
uint8 PRIORITY_BATTERY_MAX  = 3
uint8 PRIORITY_LIMP_MODE    = 4

std_msgs/Header header
uint32 sequence
uint32 robot_id
uint32 leader_term                # split-brain 방지 (단조 증가)
uint8  role                       # LEADER_*
uint8  succession_priority        # PRIORITY_*
float32 battery_percent           # 3-순위 (battery max) 비교에 사용
string reason                     # "leader_heartbeat_timeout" | "leader_recovered" | "battery_lowest" | ...
uint64 timestamp_ms
