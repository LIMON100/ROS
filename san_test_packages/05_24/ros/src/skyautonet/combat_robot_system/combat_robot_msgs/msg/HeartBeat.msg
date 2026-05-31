# SAN v1.5 IDS §5.8 — inter-robot heartbeat (watchdog liveliness).
#
# 각 robot 이 주기적으로 broadcast 하는 liveliness 신호. DDS liveliness
# QoS 의 application-level 보완으로, 다음을 추가 제공:
#   - 단순 alive 가 아닌 role / health / battery 의 atomic snapshot
#   - 운용 모드 (RECON/COMBAT/RTB/DEV_TEST) 동기화
#   - inter-robot tier transition (T4 BREADCRUMB_RECOVERY trigger)
#
# 수신 측 행동 (60 초 timeout 기본):
#   - Hub UGV: D-005 Hub 승계 가능성 모니터링
#   - Leader: Follower 군집 무결성 추적
#   - Follower: T4 BREADCRUMB_RECOVERY 트리거 평가 (SDD §6.2)
#
# v1.5 신설 (PDR-7)
# 권원: SDD-SWARM v1.5 §5 (통신 stack), §6.2 (T4 trigger)
#       IDS-CMD v1.5 §5.8
#
# 발신 주기: 1 Hz (default), max 5 Hz
# QoS: BEST_EFFORT, KEEP_LAST(3) — recent only (heartbeat 의 본질)

# ─── 역할 enum (LeaderRoleAnnouncement / HubRoleAnnouncement 와 정합) ─
uint8 ROLE_LEADER   = 0
uint8 ROLE_HUB      = 1
uint8 ROLE_FOLLOWER = 2

# ─── 건강 상태 enum ─────────────────────────────────────────
uint8 HEALTH_HEALTHY  = 0    # 정상
uint8 HEALTH_DEGRADED = 1    # 일부 센서/통신 저하, 운용 가능
uint8 HEALTH_CRITICAL = 2    # 즉시 안전 조치 필요

std_msgs/Header header

uint32 robot_id
uint32 sequence                        # 단조 증가 — packet loss 추적

uint8  role                            # ROLE_*
uint8  health_status                   # HEALTH_*
float32 battery_percent

# 운용 모드 echo (SDD §12 OperationState)
uint8  operation_mode                  # 0=RECON, 1=COMBAT, 2=RTB, 3=DEV_TEST

# Follower tier (SDD §6.2 5-Tier FSM)
uint8  current_tier                    # 0-5 (T0/T1/T1.5/T2/T3/T4)

uint64 timestamp_ms                    # 발신 시점 epoch ms
