# SAN v1.3 IDS §5.11 — Hub-fused global SLAM map.
#
# Published every 5 s by san_hub_slam on /global/slam_aggregated.
# Pixel value semantics on the PNG-encoded uint8 grid:
#   0   = free
#   127 = unknown
#   255 = occupied
#
# Cost-map subsystem (PHASE 1 / san_costmap) consumes this as the
# static layer input.

std_msgs/Header header
uint8[] occupancy_grid_png
geometry_msgs/Pose2D origin
float32 resolution_m
uint32 width_cells
uint32 height_cells
uint32 contributing_robots        # how many robots contributed to this snapshot
uint64 timestamp_ms
