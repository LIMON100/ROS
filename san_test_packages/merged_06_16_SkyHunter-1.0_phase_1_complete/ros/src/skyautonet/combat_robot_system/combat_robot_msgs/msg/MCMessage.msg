# [DCN-2026-019] Mission Control wire-level message.
#
# Wraps an operator command with sequence + checksum for reliable
# delivery. The receiver (mc_protocol_node) validates crc32, dedups
# by seq within a 16-deep sliding window, and republishes accepted
# payloads on /mc/command_validated. The sender (mc_sender_node)
# retransmits unacked entries on a 500 ms timer (max 3 retries).
#
# Distinct from the higher-level MissionStateCommand / ManualOverride
# / FormationCommand: those are mission-orchestration semantics. This
# is the wire layer that carries them.
#
# QoS: RELIABLE, KEEP_LAST(100) on /mc/command and /mc/raw_command.

builtin_interfaces/Time stamp
uint32 seq                  # monotonically increasing per sender
uint32 crc32_checksum       # boost::crc_32_type of (command + payload)
string command              # "LOAD_PATH" | "START" | "PAUSE" | "RESUME" | "STOP"
string payload              # JSON-encoded command-specific args
