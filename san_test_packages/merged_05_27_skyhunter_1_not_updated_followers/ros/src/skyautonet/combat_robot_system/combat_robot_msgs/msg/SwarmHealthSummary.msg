# SAN v1.3 IDS §4.1 / §5.12 - Hub UGV → swarm rollup of dual-SBC + command state.
#
# Companion to the per-robot RobotStatus envelope. While RobotStatus
# carries each robot's local view, SwarmHealthSummary publishes the
# Hub's authoritative view of the whole swarm, including which command
# the operator most recently dispatched (echoed for accountability and
# replay protection).

std_msgs/Header header
uint32 sequence

# Hub UGV dual-SBC failure state (SAN-SDD-SUR-001 v1.1 §3.5).
bool   slam_sbc_failed
bool   comm_sbc_failed
string slam_sbc_peer_id
string comm_sbc_peer_id

# PHASE 7: command_id echo (matches RobotStatus.last_received_command_id
# but at swarm rollup level). 0 = never seen a command.
uint32 last_received_command_id
uint64 last_command_received_ms

uint64 timestamp_ms
