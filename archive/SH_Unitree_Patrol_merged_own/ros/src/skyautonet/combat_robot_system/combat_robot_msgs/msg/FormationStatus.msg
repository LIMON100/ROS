# SAN v1.5 IDS §4.2 — Formation status telemetry.
#
# Hub-side aggregate published at 1 Hz so the operator tablet shows
# the live formation state.
#
# 권원:
#   * SDD-SWARM v1.5 §7 (9 Formations + Hungarian)
#   * IDS-CMD v1.5 §4.2

std_msgs/Header header

# Current formation in effect
uint8 FORMATION_COLUMN         = 1
uint8 FORMATION_LINE           = 2
uint8 FORMATION_V_SHAPE        = 3
uint8 FORMATION_DIAMOND        = 4
uint8 FORMATION_ECHELON_LEFT   = 5
uint8 FORMATION_ECHELON_RIGHT  = 6
uint8 FORMATION_BOX            = 7
uint8 FORMATION_VEE_INVERTED   = 8
uint8 FORMATION_FREE_SPREAD    = 9
uint8 formation_id

# Active preset (1=narrow_passage 2=recon_defence 3=wide_recon 4=assault)
uint8 PRESET_NARROW_PASSAGE = 1
uint8 PRESET_RECON_DEFENCE  = 2
uint8 PRESET_WIDE_RECON     = 3
uint8 PRESET_ASSAULT        = 4
uint8 preset_id

float32 spacing_d_m
float32 spread_theta_deg

# Live metrics — KPP-1 compliance evidence
float32 avg_alignment_error_m   # mean distance from each robot to its slot
float32 max_alignment_error_m   # worst follower
uint8   robots_in_formation     # count meeting alignment_error < 2m
uint8   robots_out_of_formation # KPP-1 violators

# Re-assignment tracking
uint32  current_epoch
uint64  last_reassignment_ms

# Formation transition phase (A→F per SDD §7.3)
uint8 PHASE_ASSEMBLY    = 0
uint8 PHASE_MOVE_IN     = 1
uint8 PHASE_RECON       = 2
uint8 PHASE_ENGAGE      = 3
uint8 PHASE_HOLD        = 4
uint8 PHASE_WITHDRAW    = 5
uint8 phase
