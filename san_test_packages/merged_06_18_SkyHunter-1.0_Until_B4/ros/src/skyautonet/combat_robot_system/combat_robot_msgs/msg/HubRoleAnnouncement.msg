# SAN v1.5 IDS §5.16 — Hub-Deputy 이중화 broadcast.
#
# Hub UGV (S2) heartbeat 5초 timeout 시 Deputy UGV (S3)가 HUB_PROMOTED
# 를 broadcast 하면서 LTE + SLAM aggregation + GStreamer SRT relay 를
# 모두 인수. Hub 복구 시 HUB_DEMOTED 로 Deputy 가 강등.
#
# v1.5 changes (DCN-2026-001 D-005):
#   - Body formally added to IDS (v1.4 promised only in Revision History).
#   - "Full takeover" semantics: HUB_PROMOTED requires all three capability
#     flags (lte_active, slam_aggregation_active, video_relay_active) be
#     simultaneously true; partial takeovers are advertised via
#     LTERoleAnnouncement (§5.14) only.

uint8 HUB_NORMAL   = 0    # 현재 Hub (정상 운용)
uint8 HUB_FAILING  = 1    # Hub heartbeat 지연 감지 (Deputy 승계 대기)
uint8 HUB_PROMOTED = 2    # Deputy 가 Hub 역할 인수 (LTE + SLAM + 영상)
uint8 HUB_DEMOTED  = 3    # 원 Hub 복구로 강등

std_msgs/Header header
uint32 sequence
uint32 robot_id
uint32 hub_term                   # 단조 증가
uint8  role                       # HUB_*
bool   lte_active                 # LTE 게이트웨이 운용 중
bool   slam_aggregation_active    # SLAM 통합 중
bool   video_relay_active         # GStreamer SRT relay 운용 중
string reason                     # "hub_heartbeat_timeout" | "hub_recovered" | "init"
uint64 timestamp_ms
