# SAN v1.3 PHASE 5b - LTE 신호 품질 publish (auto-rate 입력).
#
# Sources: ModemManager via D-Bus / qmicli / mmcli — implementations
# differ per modem, so the message itself is hardware-agnostic. The
# `grade` field is the only thing the AutoRateController cares about.

uint8 LTE_GRADE_UNKNOWN   = 0
uint8 LTE_GRADE_POOR      = 1     # RSRP < -110 dBm
uint8 LTE_GRADE_FAIR      = 2     # -110 <= RSRP < -100
uint8 LTE_GRADE_GOOD      = 3     # -100 <= RSRP <  -85
uint8 LTE_GRADE_EXCELLENT = 4     # RSRP >= -85

std_msgs/Header header
string source_iface              # e.g. "lte0" or "wwan0"
int16  rsrp_dbm                  # Reference Signal Received Power
int8   rsrq_db                   # Reference Signal Received Quality
int8   sinr_db                   # Signal-to-Interference-plus-Noise
uint8  grade
uint64 timestamp_ms
