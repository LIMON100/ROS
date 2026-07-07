/*  stream_ch2.h — Ch 2 video over Wi-Fi (UDP/RTP or SRT listener) for Android app
 *  encoding: UTF-8 (no BOM)
 *
 *  의도:
 *    Kong's TP2855 reference (camera_v4l2.c / bg_video.c / test_main.c) 는
 *    그대로 두고, 이 모듈만 추가해 RK3588/RK3576 보드가 TP2855 로 받은
 *    UYVY 1920x1080 프레임을 H.265 로 인코딩해 Android 앱에 Wi-Fi 전송한다.
 *
 *  데이터 경로:
 *      TP2855 -> camera_v4l2_try_get() -> CamFrame (UYVY or NV12, MMAP mapped)
 *                           │
 *                           ├─> bg_video_upload() -> local OLED (Kong 원본 그대로)
 *                           │
 *                           └─> stream_ch2_push_uyvy() -> GStreamer appsrc
 *                                                       -> mpph265enc (RK3588 MPP HW)
 *                                                       -> h265parse
 *                                                       -> [UDP branch]    rtph265pay -> udpsink
 *                                                          [SRT branch]    mpegtsmux  -> srtsink
 *                                                       -> Wi-Fi -> Android 앱
 *
 *  두 가지 전송 모드 선택 가능 (airys_prod.ini [stream].transport):
 *      udp           : RTP/UDP (간단, LAN, 손실 복구 없음)
 *      srt_listener  : 기본. Wi-Fi 손실 ARQ 복구 (120ms 윈도우). Android가 caller.
 *      srt_caller    : Scope가 caller. Android listener 가 필요한 특수 경우.
 *
 *  의존성 (optional — 없으면 전체가 no-op로 빌드됨):
 *      pkg-config --exists gstreamer-1.0 gstreamer-app-1.0
 *      OS 패키지:
 *          RK3588 BSP: gstreamer1.0-plugins-{base,good,bad,rockchip}
 *                      libsrt-openssl1.5  (gst-plugins-bad에 srtsink 포함)
 *
 *  Doc ID: SAN-STREAM-SW-001 Rev.D  (Kong base + SRT/UDP 송신)
 */

#ifndef STREAM_CH2_H
#define STREAM_CH2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct StreamCh2 StreamCh2;

typedef enum {
    STREAM_TRANSPORT_UDP_RTP      = 0,   /* RTP over UDP (dest: host:port)   */
    STREAM_TRANSPORT_SRT_LISTENER = 1,   /* Scope listens on bind:port       */
    STREAM_TRANSPORT_SRT_CALLER   = 2,   /* Scope calls out to host:port     */
} StreamTransport;

typedef struct {
    StreamTransport transport;

    /* UDP_RTP: destination         (host,port)
     * SRT_LISTENER: bind address   (bind_addr,port)
     * SRT_CALLER:  remote address  (host,port) */
    const char* bind_addr;          /* e.g. "0.0.0.0"                       */
    const char* host;               /* e.g. "192.168.42.100"                */
    int         port;               /* e.g. 5000                            */

    /* SRT tuning (ignored for UDP) */
    int         srt_latency_ms;     /* ARQ retransmit window (default 120)  */
    int         srt_peer_latency_ms;/* symmetric, match peer                */
    const char* srt_streamid;       /* e.g. "airys-ch2"                     */
    const char* srt_passphrase;     /* NULL/"" = no AES                      */
    int         srt_pbkeylen;       /* 0|16|24|32 — AES key length           */

    /* Source geometry produced by camera_v4l2_try_get() */
    int         src_w;              /* 1920                                  */
    int         src_h;              /* 1080                                  */

    /* Encoder */
    int         bitrate_bps;        /* 4000000 default                       */
    int         gop_length;         /* 30 (1s @30fps)                        */

    /* If true, build pipeline but don't go to PLAYING until
     * stream_ch2_start_on_demand() is called (e.g. when Android sends
     * {"cmd":"start_video"} over a side channel). */
    bool        defer_start;
} StreamCh2Config;

/** Get a sensible default config (SRT listener 0.0.0.0:5000, 4 Mbps). */
void stream_ch2_default_config(StreamCh2Config* cfg);

/** Build the pipeline but don't start streaming.
 *  Returns NULL if GStreamer is not compiled in (stub build) or the
 *  pipeline construction fails. `err_out` (optional) receives a static
 *  string describing the failure reason. */
StreamCh2* stream_ch2_open(const StreamCh2Config* cfg, const char** err_out);

/** Transition the pipeline to PLAYING. Idempotent. */
bool stream_ch2_start(StreamCh2* s);

/** Stop pipeline (READY state, ready to start again). */
void stream_ch2_stop(StreamCh2* s);

/** Tear down + free. Safe with NULL. */
void stream_ch2_close(StreamCh2* s);

/** Push one UYVY 4:2:2 frame from the TP2855 capture buffer.
 *  - `data` must remain valid until this call returns (the frame is copied
 *    into a GstBuffer via appsrc; caller can camera_v4l2_release() after).
 *  - Non-blocking; drops the frame if appsrc queue is full (leaky=downstream).
 *  Returns:
 *     1 if pushed OK
 *     0 if dropped on back-pressure (not an error — streaming continues)
 *    -1 if the pipeline is not in a pushable state (closed / error)       */
int stream_ch2_push_uyvy(StreamCh2* s,
                         const uint8_t* data, size_t size,
                         int64_t pts_us);

/** Push an NV12 frame (for IMX678 CSI path; unused on TP2855 AHD but kept
 *  symmetric with camera_v4l2_try_get() CamFrame.fmt). */
int stream_ch2_push_nv12(StreamCh2* s,
                         const uint8_t* y_plane,  size_t y_size,
                         const uint8_t* uv_plane, size_t uv_size,
                         int y_stride, int uv_stride,
                         int64_t pts_us);

/** Runtime bitrate change (driven by Android app asking for different
 *  quality / link quality monitor). Returns true if the encoder accepted. */
bool stream_ch2_set_bitrate(StreamCh2* s, int bitrate_bps);

/** Snapshot statistics for Ch 3 telemetry (bytes sent, frames pushed, drops). */
typedef struct {
    uint64_t frames_pushed;
    uint64_t frames_dropped;
    uint32_t current_bitrate_bps;
    int      is_playing;       /* 1 if pipeline is PLAYING, 0 otherwise    */
} StreamCh2Stats;

StreamCh2Stats stream_ch2_stats(const StreamCh2* s);

/** True if the binary was built with GStreamer support, false if stubbed. */
bool stream_ch2_available(void);

#ifdef __cplusplus
}
#endif

#endif /* STREAM_CH2_H */
