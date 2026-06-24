# SAN v1.5 IDS §5.3 — Modified Raft Leader Election (vote response).
#
# Voter robot 가 LeaderElectionVote 수신 후 평가 결과를 broadcast.
# Candidate 는 quorum (≥ N/2 + 1) 의 grant 를 모으면 Leader 로 선출.
# 선출 후 LeaderRoleAnnouncement 를 broadcast.
#
# vote_reason 은 디버깅 / audit 용 — 어느 tier 에서 결정됐는지 추적.
#
# v1.5 신설 (PDR-7) — DCN-2026-001 D-005
# 권원: SDD-SWARM v1.5 §11
#       IDS-CMD v1.5 §5.3
#
# QoS: RELIABLE, KEEP_LAST(20)

# ─── Vote decision 이유 enum (audit / 디버깅용) ────────────────────
uint8 REASON_TIER1_LEADER_CONTINUITY = 0   # 직전 Leader ID 와 일치
uint8 REASON_TIER2_LOWEST_ID         = 1   # 최저 robot_id
uint8 REASON_TIER3_HIGHEST_SLAM      = 2   # 최고 SLAM health
uint8 REASON_TIER4_HIGHEST_BATTERY   = 3   # 최고 battery %
uint8 REASON_TERM_OUTDATED           = 4   # 본인의 term 이 더 높음 → reject
uint8 REASON_ALREADY_VOTED           = 5   # 동일 term 에서 이미 다른 candidate 에 vote → reject
uint8 REASON_TIE_BROKEN_ARBITRARY    = 6   # 4 tier 모두 동일 → 임의 (랜덤 / 작은 ID)

std_msgs/Header header

uint32 sequence
uint32 term                            # 응답 대상 election term
uint32 candidate_robot_id              # vote 대상 candidate
uint32 voter_robot_id                  # 응답자

bool   vote_granted                    # true = candidate 지지, false = reject
uint8  vote_reason                     # REASON_*

# Voter 측 evidence (audit trail)
float32 voter_slam_health_score
float32 voter_battery_percent

uint64 timestamp_ms
