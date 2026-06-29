// ============================================================================
// stream_ch2.h  [Host build STUB header]
//
// Real implementation: external/AIRYS_TP2855_SRT/src/stream_ch2.h (Kong).
// Host build uses this stub (gst_streamer_stub.c provides definitions).
// ============================================================================
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STREAM_TRANSPORT_UDP_RTP      = 0,
    STREAM_TRANSPORT_SRT_LISTENER = 1,
    STREAM_TRANSPORT_SRT_CALLER   = 2,
} StreamTransport;

typedef struct {
    StreamTransport transport;
    const char*     bind_addr;
    const char*     host;
    int             port;
    int             srt_latency_ms;
    int             srt_peer_latency_ms;
    const char*     srt_streamid;
    const char*     srt_passphrase;
    int             srt_pbkeylen;
    int             src_w, src_h;
    int             bitrate_bps;
    int             gop_length;
    bool            defer_start;
} StreamCh2Config;

typedef struct {
    uint64_t frames_pushed;
    uint64_t frames_dropped;
    uint32_t current_bitrate_bps;
    int      is_playing;
} StreamCh2Stats;

typedef struct StreamCh2 StreamCh2;

void           stream_ch2_default_config(StreamCh2Config* cfg);
StreamCh2*     stream_ch2_open(const StreamCh2Config* cfg, const char** err_out);
bool           stream_ch2_start(StreamCh2* s);
void           stream_ch2_stop(StreamCh2* s);
void           stream_ch2_close(StreamCh2* s);
int            stream_ch2_push_uyvy(StreamCh2* s, const uint8_t* data, size_t size,
                                     int64_t pts_us);
int            stream_ch2_push_nv12(StreamCh2* s,
                                     const uint8_t* y_plane,  size_t y_size,
                                     const uint8_t* uv_plane, size_t uv_size,
                                     int y_stride, int uv_stride,
                                     int64_t pts_us);

// V6.6 zero-copy upgrade — V5.23 패턴 (gst_buffer_new_wrapped_full + qdata)
//
// nv12_dma_fd 가 양수일 때 GstDmaBufAllocator + gst_buffer_new_wrapped_full
// 로 GStreamer 가 zero-copy 로 buffer 받음. release_cb 는 GStreamer 가
// buffer 다 사용 후 호출 → NV12Pool refcount 정상 release.
//
// ret: 0 success, >0 backpressure drop, <0 error.
// 양산 board impl 에서 actual zero-copy path. host stub 은 fallback (-1).
int            stream_ch2_push_dmabuf_nv12(StreamCh2* s,
                                            int nv12_dma_fd,
                                            int width, int height,
                                            int y_stride, int uv_stride,
                                            int64_t pts_us,
                                            void (*release_cb)(void*),
                                            void* release_user_data);
bool           stream_ch2_set_bitrate(StreamCh2* s, int bitrate_bps);
StreamCh2Stats stream_ch2_stats(const StreamCh2* s);
bool           stream_ch2_available(void);

#ifdef __cplusplus
}
#endif
