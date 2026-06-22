// ============================================================================
// streaming/gst_streamer.h  [AIRYS v6 — SAN-STREAM-SW-001 RevD.1]
//
// AIRYS v6 GstStreamer is a thin C++ RAII wrapper around Kong's stream_ch2 C
// API (from AIRYS_TP2855_SRT). The C library is byte-identical to Kong
// reference; this wrapper provides:
//   - C++ resource management (RAII, no manual close)
//   - Type-safe Config struct (mirrors StreamCh2Config)
//   - Integration with AIRYS Pipeline (NV12Buf push)
//   - Statistics aggregation
//
// All transport modes supported (UDP/RTP, SRT Listener, SRT Caller, OFF):
//   AIRYS_STREAM=srt_listener (default)
//
// Phase B port migration:
//   - SRT/UDP : 5000 (industry standard, was UDP only in v5.23)
//   - WebSocket telemetry : 8080 (was 5001 — moved to avoid SRT conflict)
// ============================================================================
#pragma once

#include <string>
#include <memory>
#include <cstdint>

// Forward declaration of Kong's opaque type
extern "C" {
    struct StreamCh2;
}

namespace airys::stream {

class GstStreamer {
 public:
    struct Config {
        // Transport: "udp" | "srt_listener" | "srt_caller" | "off"
        std::string transport      = "srt_listener";
        std::string bind_addr      = "0.0.0.0";    // SRT_LISTENER bind
        std::string host           = "192.168.42.100";  // UDP dest / SRT caller
        int         port           = 5000;
        int         srt_latency_ms = 120;
        int         srt_peer_latency_ms = 120;
        std::string srt_streamid   = "airys-ch2";
        std::string srt_passphrase;                  // empty = AES OFF
        int         srt_pbkeylen   = 0;              // 0/16/24/32

        // Source format
        int src_w       = 1920;
        int src_h       = 1080;

        // Encoder
        int bitrate_bps = 4'000'000;
        int gop_length  = 30;          // 1 sec IDR @ 30 fps
        bool defer_start = false;
    };

    struct Stats {
        uint64_t frames_pushed     = 0;
        uint64_t frames_dropped    = 0;
        uint32_t current_bitrate_bps = 0;
        bool     is_playing        = false;
    };

    GstStreamer();
    ~GstStreamer();

    // Non-copyable, movable. Move must null out the source ctx_ so the
    // moved-from destructor doesn't double-close the same StreamCh2*.
    GstStreamer(const GstStreamer&) = delete;
    GstStreamer& operator=(const GstStreamer&) = delete;
    GstStreamer(GstStreamer&& other) noexcept;
    GstStreamer& operator=(GstStreamer&& other) noexcept;

    // Lifecycle
    bool open(const Config& cfg);     // creates Kong context, returns false on err
    bool start();                      // sets pipeline to PLAYING
    void stop();                       // sets pipeline to NULL
    void close();                      // releases Kong context

    // Push frame data (T_STREAM thread)
    int push_uyvy(const uint8_t* data, size_t size, int64_t pts_us);
    int push_nv12(const uint8_t* y_plane,  size_t y_size,
                  const uint8_t* uv_plane, size_t uv_size,
                  int y_stride, int uv_stride,
                  int64_t pts_us);

    // V6.6 zero-copy push (V5.23 패턴 — gst_buffer_new_wrapped_full + qdata).
    // release_cb 는 GStreamer 가 buffer 다 사용 후 호출 → NV12Pool 정상 release.
    // host build (stub) 에서는 release_cb 즉시 호출 후 0 반환 (fallback).
    // board build 에서 GstDmaBufAllocator + appsrc DMA-BUF 처리.
    int push_dmabuf_nv12(int nv12_dma_fd,
                         int width, int height,
                         int y_stride, int uv_stride,
                         int64_t pts_us,
                         void (*release_cb)(void*),
                         void* release_user_data);

    // Runtime control
    bool set_bitrate(int bitrate_bps);

    // Telemetry
    Stats stats() const;
    bool  is_playing() const;

    // Static — check if Kong's stream_ch2 was compiled with GStreamer
    static bool gstreamer_available();

 private:
    StreamCh2*  ctx_{nullptr};
    Config      cfg_;
    std::string last_error_;
};

}  // namespace airys::stream
