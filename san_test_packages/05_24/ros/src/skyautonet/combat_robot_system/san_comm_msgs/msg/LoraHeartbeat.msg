# Per-robot heartbeat over LoRa
std_msgs/Header header
uint8 robot_id
uint8 battery_pct
float64 latitude
float64 longitude
uint8 status_flags             # Bitfield: bit0=armed, bit1=moving, bit2=error, bit3=estop_active
uint16 seq_number
