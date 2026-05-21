# SAN v1.3 IDS §3 - electronic-warfare jamming on/off.

uint8 ACTION_STOP  = 0
uint8 ACTION_START = 1

uint8 BAND_GPS_L1   = 0
uint8 BAND_GPS_L2   = 1
uint8 BAND_2_4G     = 2
uint8 BAND_5_8G     = 3
uint8 BAND_CUSTOM   = 4

std_msgs/Header header
uint32 command_id                 # PHASE 7
uint32 sequence
uint32 target_robot_id
uint8  action                     # ACTION_*
uint8  frequency_band             # BAND_*
float32 custom_freq_mhz           # only when band == BAND_CUSTOM
float32 power_dbm
uint32 duration_sec               # 0 = until ACTION_STOP
uint64 timestamp_ms
