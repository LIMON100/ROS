# SAN v1.3 IDS §5.10 — per-robot SLAM occupancy-grid delta.
#
# Published at 1 Hz by san_slam on /{robot_id}/local/slam_delta. The
# payload is a PNG-encoded uint8 grid where pixel value semantics are:
#   0   = free  (changed since previous delta)
#   127 = no change since previous delta
#   255 = occupied (changed since previous delta)
#
# The coverage window endpoints let the Hub aggregator order deltas
# from different robots that arrive out-of-sequence on the mesh.

std_msgs/Header header
string robot_id                   # "1".."8"
uint8[] occupancy_grid_delta_png
geometry_msgs/Pose2D origin       # world coords of grid (0, 0)
float32 resolution_m              # 0.05 m default
uint64 coverage_start_ms          # window start (producer wall-clock)
uint64 coverage_end_ms            # window end
uint64 timestamp_ms               # publish wall-clock
