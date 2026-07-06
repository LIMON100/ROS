# SAN v1.5 — RTK-quality-driven navigation constraints (LOC RTK Fix→Float
# mitigation, Layer 3).
#
# Published per-robot by rtk_quality_manager on
# /{robot_id}/nav_constraints as an ADVISORY for the navigation/formation
# layer when RTK localization quality degrades. The controller may lower
# its speed cap and widen its cross-track tolerance, and the formation may
# loosen spacing strictness, so a robot navigating on a degraded (RTK_FLOAT
# / lost) fix does so defensively instead of chasing a jumpy pose.
#
# This is advisory: consumers that don't subscribe keep their nominal
# behaviour, so emitting it changes nothing until a consumer opts in.

std_msgs/Header header

# RTK quality FSM state.
uint8 RTK_OK              = 0   # RTK Fixed — nominal
uint8 RTK_DEGRADED_GRACE  = 1   # just lost Fix; behaviour held (grace window)
uint8 RTK_DEGRADED_ACTIVE = 2   # sustained Float — defensive constraints
uint8 RTK_LOST            = 3   # prolonged loss — Tier escalation + ThreatAlert
uint8 rtk_state

float32 max_speed_mps        # advisory speed cap
float32 path_tolerance_m     # allowed cross-track error
bool    formation_loose      # relax formation spacing strictness

string source_robot_id
uint64 timestamp_ms
