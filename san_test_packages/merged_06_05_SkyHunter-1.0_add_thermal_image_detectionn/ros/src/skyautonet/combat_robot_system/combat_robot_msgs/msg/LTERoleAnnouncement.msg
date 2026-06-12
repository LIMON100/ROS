# SAN v1.3 IDS §5.14 - LTE 게이트웨이 승격/강등 broadcast
#
# Hub UGV (S2)의 LTE 모뎀이 작동 불능 상태일 때 designated backup
# follower (S3, S5)가 LTE 게이트웨이 역할을 인수하는 프로토콜.
#
# split-brain 방지: lte_term 단조 증가, 낮은 term의 announcement는 무시.

uint8 LTE_NONE              = 0   # 일반 robot, LTE 사용 없음
uint8 LTE_BACKUP_DESIGNATED = 1   # 백업 지정만, 비활성 (대기)
uint8 LTE_PROMOTED          = 2   # 활성 LTE 게이트웨이
uint8 LTE_DEMOTED           = 3   # 강등 (Hub 복귀 시)

std_msgs/Header header
uint32 sequence
uint32 robot_id
uint32 lte_term                   # split-brain 방지 (단조 증가)
uint8  role                       # LTE_NONE | LTE_BACKUP_DESIGNATED | LTE_PROMOTED | LTE_DEMOTED
uint32 lte_backup_chain_position  # 1 = primary, 2 = secondary, ...
bool   lte_active                 # 실제 LTE 트래픽 처리 중인지
string reason                     # "hub_lte_down" | "hub_recovered" | "init" | "manual"
uint64 timestamp_ms
