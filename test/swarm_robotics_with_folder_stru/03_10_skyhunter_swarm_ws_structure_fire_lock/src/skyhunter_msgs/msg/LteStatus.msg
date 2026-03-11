# LTE link health (published 2 Hz by lte_simulator)
std_msgs/Header header
uint8 state                    # 0=DISCONNECTED, 1=CONNECTED, 2=DEGRADED
float32 rtt_ms
float32 packet_loss_pct
float32 bandwidth_mbps
float32 uptime_s
