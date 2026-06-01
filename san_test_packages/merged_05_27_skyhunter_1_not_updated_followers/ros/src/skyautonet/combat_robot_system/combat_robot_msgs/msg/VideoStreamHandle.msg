# SAN v1.3 IDS §4.5 - robot → Android APP 영상 스트림 핸들.
#
# Hub publishes one VideoStreamHandle per VideoStreamRequest transition
# (active / quality_change / stopped / error). `srt_uri` is the fully
# formed `srt://...` URL the tablet can hand straight to ExoPlayer or
# any libsrt-compatible client.

uint8 STATUS_ACTIVE   = 0
uint8 STATUS_STOPPED  = 1
uint8 STATUS_ERROR    = 2

# Mirror VideoStreamRequest enums so consumers do not need both files.
uint8 QUALITY_THUMBNAIL = 0
uint8 QUALITY_LOW       = 1
uint8 QUALITY_HD        = 2
uint8 QUALITY_FHD       = 3

uint8 CODEC_H264 = 0
uint8 CODEC_H265 = 1

std_msgs/Header header
uint32 sequence                   # echoes the originating request seq
uint32 target_robot_id
string srt_uri                    # srt://hub_ip:8888?mode=listener&latency=120...
string passphrase                 # AES-128 secret (empty when encryption=false)
uint8  codec
uint8  quality
uint32 actual_bitrate_kbps
uint8  status
string error_msg
uint32 active_stream_count        # for the operator UI to flag thumbnail mode
uint64 stream_start_ms
uint64 timestamp_ms
