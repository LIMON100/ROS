# SAN v1.3 IDS §3 - leader → robot weapons engagement authorization.

uint8 AUTH_DENIED       = 0   # absolute hold
uint8 AUTH_WATCH        = 1   # track only, no fire
uint8 AUTH_WEAPONS_TIGHT = 2  # fire only with explicit confirm
uint8 AUTH_WEAPONS_FREE = 3   # fire on positive ID per ROE

std_msgs/Header header
uint32 command_id                 # PHASE 7
uint32 sequence
uint32 target_robot_id            # which robot is being authorized
uint8  authorization_level        # AUTH_*
float64 target_lat                # 0.0 means "wherever currently aimed"
float64 target_lon
uint32 valid_for_sec              # auto-revert to AUTH_DENIED after this
string operator_id                # accountability trail
uint64 timestamp_ms
