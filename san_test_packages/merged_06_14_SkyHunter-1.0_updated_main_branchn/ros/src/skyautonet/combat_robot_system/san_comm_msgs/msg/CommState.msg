# Current communication tier (published 2 Hz by swarm_comm_manager)
std_msgs/Header header
uint8 current_tier             # 1=WIFI6, 2=LTE, 3=LORA, 0=DISCONNECTED
uint8 previous_tier
float32 time_in_current_s
float32 wifi6_avg_rssi_dbm
float32 wifi6_packet_loss_pct
int32 wifi6_active_links
bool lte_connected
float32 lte_rtt_ms
bool lora_active
float32 lora_last_heartbeat_age_s
