# SAN v1.3 IDS §5.13 — 4-layer Local Cost Map snapshot.
#
# Published at 10 Hz by san_costmap. The grid is a PNG-encoded
# uint8 image (280×280 default at 5 cm resolution = 14 m × 14 m).
# Pixel values map onto the standard Nav2 cost constants:
#   0   = FREE_SPACE
#   100 = WARN_LOW (slope band)
#   200 = INSCRIBED_INFLATED_OBSTACLE
#   254 = LETHAL_OBSTACLE
#   255 = NO_INFORMATION

std_msgs/Header header
uint32 sequence
string robot_id                   # "1".."8"
uint8[] cost_grid_png             # PNG-deflated uint8 grid (~10 kB)
geometry_msgs/Pose2D origin       # world coords of grid (0, 0)
float32 resolution_m              # 0.05 m default
uint32 width_cells                # 280 default
uint32 height_cells               # 280 default
uint64 computed_at_ms             # producer wall-clock at compute start
uint32 lethal_count               # number of cells == 254 (monitoring)
uint32 inflated_count             # number of cells in [100, 253]
uint64 timestamp_ms               # publish wall-clock
