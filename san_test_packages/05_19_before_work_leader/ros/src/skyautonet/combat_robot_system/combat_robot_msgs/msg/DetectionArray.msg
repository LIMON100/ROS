# SAN v1.5 IDS — Array of detections from a single perception cycle
# (Phase 2-E Turn 11-12).
#
# Published by san_perception on ~/detections at the camera's
# native rate (~30 Hz for IMX678, but inference rate may be lower).

std_msgs/Header header

# Source image dimensions for client-side bbox normalization
uint32 source_width
uint32 source_height
string source_frame_id           # "imx678" | "thermal"

Detection[] detections

# Inference timing for QoS / SLA monitoring
uint32  inference_time_ms
uint64  cycle_timestamp_ms
