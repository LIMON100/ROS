# SAN v1.3 IDS §3.9 - Android APP → robot 영상 스트림 시작/중지 요청.
#
# Pull-mode trigger. The tablet asks; the robot brings up its
# GStreamer pipeline and replies with a VideoStreamHandle that carries
# the actual SRT URI + passphrase.
#
# `action` decides whether to start, stop, or merely renegotiate
# the quality of an already-running stream.

uint8 ACTION_START          = 0
uint8 ACTION_STOP           = 1
uint8 ACTION_CHANGE_QUALITY = 2

uint8 QUALITY_THUMBNAIL = 0     # 320x240, ~100 kbps
uint8 QUALITY_LOW       = 1     # 640x480, ~500 kbps
uint8 QUALITY_HD        = 2     # 1280x720, 2 Mbps  (default — v1.5.1 DCN-2026-003 D-001)
uint8 QUALITY_FHD       = 3     # 1920x1080, ~4 Mbps

uint8 CODEC_H264 = 0
uint8 CODEC_H265 = 1

uint8 PROTOCOL_SRT = 0
uint8 PROTOCOL_UDP = 1

std_msgs/Header header
uint32 command_id                 # PHASE 7: echoed by RobotStatus.last_received_command_id
uint32 sequence
uint32 target_robot_id
uint8  action
uint8  codec
uint8  quality
uint8  protocol
bool   encryption                 # request AES-128
string operator_ip                # where Hub should send the SRT egress
uint64 timestamp_ms
