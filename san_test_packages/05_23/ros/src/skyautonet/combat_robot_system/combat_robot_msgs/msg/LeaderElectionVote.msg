# SAN v1.5 IDS §5.3 — Modified Raft Leader Election (vote request).
#
# Candidate broadcasts when initiating an election (e.g., on Leader
# heartbeat timeout). Voters respond with LeaderElectionResponse.
#
# Modified Raft variant (D-005): the standard Raft "log freshness"
# voting rule is replaced with a 4-tier priority comparison:
#   Tier 1 — previous_leader_id_continuity   (was candidate the prior Leader?)
#   Tier 2 — lowest robot_id                  (deterministic tie-break)
#   Tier 3 — highest slam_health_score        (best aware of swarm geometry)
#   Tier 4 — highest battery_percent          (longest endurance)
#
# Voters apply the priority in order; first tier that distinguishes
# candidates determines the vote. This guarantees a unique Leader
# without log freshness comparison (which is not meaningful in our
# stateless command protocol).
#
# v1.5 신설 (PDR-7) — DCN-2026-001 D-005
# 권원: SDD-SWARM v1.5 §11 (Modified Raft Leader Election)
#       IDS-CMD v1.5 §5.3
#
# QoS: RELIABLE, KEEP_LAST(20) — election broadcasts MUST be delivered

std_msgs/Header header

uint32 sequence
uint32 term                            # Raft term (단조 증가, election round 식별)
uint32 candidate_robot_id              # 본인을 Leader 로 제안하는 robot

# 4-Tier 우선순위 evidence — voters 가 평가
uint32 previous_leader_id              # 직전 Leader 의 robot_id (Tier 1)
                                       # candidate 가 이 ID 와 동일하면 우선
float32 slam_health_score              # 0.0 ~ 1.0 (Tier 3, SDD §9)
float32 battery_percent                # 현재 (Tier 4)

uint64 timestamp_ms                    # candidate 측 epoch ms
