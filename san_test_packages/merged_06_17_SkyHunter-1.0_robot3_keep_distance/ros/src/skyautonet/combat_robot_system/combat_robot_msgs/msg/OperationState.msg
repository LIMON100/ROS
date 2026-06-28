# SAN v1.5 IDS §3 — 1 Hz operation-state heartbeat.
#
# Published by combat_robot_operation_system so every consumer
# (operator UI, audit log, swarm coordinator, fire authorizer) sees
# the resolved deployment posture without having to re-read the yaml
# overlay tree.
#
# PHASE 0 v1.4 fields: deployment_mode + operator_banner.
# v1.5 (DCN-2026-001 D-004): in_limp_mode flag added so the fire
# authorization gate can apply Limp Mode safeguards (HMAC + Two-key +
# audit). Set by LimpModeManager when Hub + Deputy both go silent.

uint32 LEADER_ROBOT_ID  = 1
uint32 HUB_ROBOT_ID     = 2
uint32 DEPUTY_ROBOT_ID  = 3

std_msgs/Header header
uint32 sequence

# v1.3 PHASE 0: 5-tier deployment_mode.
# Valid values: "production" | "demo" | "lab_test" | "bench" | "development"
string deployment_mode

# Operator-screen banner (single ASCII token).
# Mapping (default; overridable by operator yaml):
#   production  -> "ARMED"
#   demo        -> "DEMO"
#   lab_test    -> "LAB"
#   bench       -> "BENCH"
#   development -> "DEV"
string operator_banner

# Active swarm topology - convenient duplicates of the v1.4 8-robot
# constants so the operator UI does not need to embed them.
uint32 robot_id                            # this node's swarm id
uint32 leader_robot_id        # convention: 1
uint32 hub_robot_id           # convention: 2
uint32 deputy_robot_id        # convention: 3
uint32 n_alive_robots                      # currently-healthy swarm size

# v1.5 (DCN-2026-001 D-004): Limp Mode active flag.
# Set true by LimpModeManager when both Hub UGV (S2) and Deputy UGV (S3)
# are silent (heartbeat lost). Consumed by the fire authorization gate
# (san_fire_authorization) so it can apply Limp Mode safeguards
# (HMAC mesh-direct + Two-key arming + audit log tagging) per SDD-SWARM
# v1.5 §5.7.2.1 Option A — fire permitted, situation-awareness reduced.
bool in_limp_mode

uint64 timestamp_ms
