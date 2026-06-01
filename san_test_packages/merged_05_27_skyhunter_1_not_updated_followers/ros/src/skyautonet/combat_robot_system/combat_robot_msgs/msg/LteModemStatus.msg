# SAN v1.5 IDS — LTE modem AT-derived status (Phase 2-E Turn 3).
#
# Published by san_lte_redundancy::LteModemNode at ~0.5 Hz. Carries
# information that's only available via direct AT commands to the
# modem (operator name, cell_id, APN, IP address). Complements the
# existing /lte/link_quality topic which is sourced from OpenWRT
# mwan3 + ubus.
#
# 권원:
#   * SDD-SWARM v1.5 §3.1.4 (LTE 모뎀 HW 명세)
#   * SDD-SWARM v1.5 §5.5   (LTE 이중화)
#   * IDS v1.5 §5 (LTE topic naming)
#   * DCN-2026-002 D-008 (IPC 통일 — Python LteModemAdapter 폐기)

# Registration status (3GPP TS 27.007 §7.2 +CREG values).
uint8 REG_NOT_REGISTERED       = 0
uint8 REG_HOME                 = 1
uint8 REG_SEARCHING            = 2
uint8 REG_DENIED               = 3
uint8 REG_UNKNOWN              = 4
uint8 REG_ROAMING              = 5

std_msgs/Header header
uint8  registered                 # REG_*
string operator_name              # MCC/MNC 또는 이름 (+COPS?)
string rat                        # "LTE", "WCDMA", "GSM", "" — 측정 불가
int32  rsrp_dbm                   # -140..-44 dBm, INT32_MIN = unknown
int32  rsrq_db                    # -20..0 dB, INT32_MIN = unknown
int32  sinr_db                    # -10..30 dB, INT32_MIN = unknown
uint16 band                       # LTE band number (1..71)
uint32 cell_id                    # serving cell ID, 0 = unknown
bool   pdp_active                 # PDP context active (data 가능)
string ip_address                 # IPv4 string, "0.0.0.0" = none
string apn                        # active APN (운용 환경 식별 — production / training / dev)
uint32 dropped_at_count           # 통계: AT timeout / parse error 누적
uint64 last_poll_timestamp_ms     # 마지막 성공 폴 시각
