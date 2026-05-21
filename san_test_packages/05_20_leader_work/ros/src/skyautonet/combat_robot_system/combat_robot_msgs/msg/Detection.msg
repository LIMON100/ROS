# SAN v1.5 IDS — Single object detection (Phase 2-E Turn 11-12).
#
# 권원:
#   * SDD-SWARM v1.5 §4.2 (Perception output spec)
#   * IDS v1.5 §5.21 (Detection topic naming)

# Object classes — matches the COCO-derived training set
uint8 CLASS_UNKNOWN  = 0
uint8 CLASS_PERSON   = 1
uint8 CLASS_VEHICLE  = 2
uint8 CLASS_DRONE    = 3
uint8 CLASS_WEAPON   = 4
uint8 CLASS_ANIMAL   = 5

uint8 class_id

# Confidence 0..1
float32 confidence

# Bounding box in source-image pixel coordinates
uint32 bbox_x1
uint32 bbox_y1
uint32 bbox_x2
uint32 bbox_y2

uint32 x
uint32 y
uint32 w
uint32 h


# SAN-specific augmentation from sensor fusion
float32 estimated_depth_m       # from LRF / LiDAR / stereo; 0 = unknown
float32 thermal_avg_temp_c      # NaN if no thermal fusion (or no overlap)
float32 thermal_max_temp_c
bool    has_thermal_signature   # true if thermal stats are present
