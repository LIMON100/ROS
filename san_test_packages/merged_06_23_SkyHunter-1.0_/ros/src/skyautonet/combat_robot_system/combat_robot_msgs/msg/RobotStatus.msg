# SAN v1.3 - per-robot status broadcast (50Hz local, 5Hz swarm)
#
# Field ordering follows the v1.3 IDS register layout so the swarm
# bridge can serialize/deserialize with predictable offsets. The two
# v1.3 LTE flags (#13, #14) extend the v1.0 envelope without breaking
# wire compatibility — older parsers stop at comm_healthy and ignore
# trailing bytes.

std_msgs/Header header               # 1
uint32 robot_id                      # 2
uint8  robot_role                    # 3: 0=follower, 1=leader, 2=hub
float32 battery_soc                  # 4: 0..1
float32 motor_temp_max_c             # 5: °C
uint8  locomotion_mode               # 6: 0=stand, 1=walk, 2=trot, 3=docking, 4=error
geometry_msgs/Pose pose              # 7: world-frame pose
geometry_msgs/Twist twist            # 8: body-frame velocity
uint8  tier                          # 9: SDD §6.7 escape tier code
bool   slam_healthy                  # 10
bool   perception_healthy            # 11
bool   comm_healthy                  # 12
bool   lte_active                    # 13: v1.3 - this robot is the active LTE gateway
bool   is_lte_backup_designated      # 14: v1.3 - listed in lte_backup_chain
# v1.4 PHASE 8 Hub-Deputy + 4-tier Leader + Limp Mode telemetry
bool   is_deputy_ugv                 # 15: pre-designated Deputy UGV (robot_id == 3)
bool   is_hub_role_active            # 16: this robot is currently performing Hub duties (Deputy promoted)
bool   is_leader_role_active         # 17: this robot is currently performing Leader duties
float32 battery_percent              # 18: 0.0..100.0 (Deputy 1순위 / battery-max 3순위 비교에 사용)
uint32 leader_term                   # 19: split-brain term for Leader succession
uint32 hub_term                      # 20: split-brain term for Hub-Deputy redundancy
bool   in_limp_mode                  # 21: Hub + Deputy 둘 다 불능 — Wi-Fi 6 mesh only
bool   sbc1_healthy                  # 22: dual-SBC health (RK3588 #1)
bool   sbc2_healthy                  # 23: dual-SBC health (RK3588 #2)
string[] fault_codes                 # 24
uint32 last_received_command_id      # 25: PHASE 7 - echoes most recent SwarmRobotCommand.command_id; 0 = none
uint64 last_command_received_ms      # 26: PHASE 7 - wall-clock ms when command_id was set
uint64 timestamp_ms                  # 27
