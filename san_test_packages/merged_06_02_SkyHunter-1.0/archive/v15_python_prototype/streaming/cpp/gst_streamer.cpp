// ============================================================================
// streaming/gst_streamer.cpp  [AIRYS v6]
//
// Thin RAII wrapper around Kong's stream_ch2 C API.
// ============================================================================
#include "streaming/gst_streamer.h"
#include "airys_log.h"

extern "C" {
// Host build: include/streaming/stream_ch2.h (stub header)
// Board build: external/AIRYS_TP2855_SRT/src/stream_ch2.h (Kong real)
#include "streaming/stream_ch2.h"
}

#include <cstring>

namespace airys::stream {

namespace {
    // Kong transport enum mapping
    StreamTransport map_transport(const std::string& s) {
        if (s == "udp")          return STREAM_TRANSPORT_UDP_RTP;
        if (s == "srt_listener") return STREAM_TRANSPORT_SRT_LISTENER;
        if (s == "srt_caller")   return STREAM_TRANSPORT_SRT_CALLER;
        return STREAM_TRANSPORT_UDP_RTP;  // default fallback
    }
}

GstStreamer::GstStreamer() = default;

GstStreamer::~GstStreamer() {
    if (ctx_) close();
}

GstStreamer::GstStreamer(GstStreamer&& other) noexcept
    : ctx_(other.ctx_),
      cfg_(std::move(other.cfg_)),
      last_error_(std::move(other.last_error_)) {
    other.ctx_ = nullptr;
}

GstStreamer& GstStreamer::operator=(GstStreamer&& other) noexcept {
    if (this != &other) {
        if (ctx_) close();
        ctx_ = other.ctx_;
        cfg_ = std::move(other.cfg_);
        last_error_ = std::move(other.last_error_);
        other.ctx_ = nullptr;
    }
    return *this;
}

bool GstStreamer::gstreamer_available() {
    return stream_ch2_available();
}

bool GstStreamer::open(const Config& cfg) {
    if (ctx_) {
        AIRYS_LOG_WARN("stream", "open() called when already open");
        return false;
    }
    cfg_ = cfg;

    if (cfg.transport == "off") {
        AIRYS_LOG_INFO("stream", "transport=off, no pipeline created");
        return true;   // silently ok — push() will no-op
    }

    if (!stream_ch2_available()) {
        AIRYS_LOG_WARN("stream", "stream_ch2 compiled without GStreamer");
        last_error_ = "no GStreamer support at compile time";
        return false;
    }

    // Build Kong config
    StreamCh2Config kc;
    stream_ch2_default_config(&kc);

    kc.transport         = map_transport(cfg.transport);
    kc.bind_addr         = cfg.bind_addr.c_str();
    kc.host              = cfg.host.c_str();
    kc.port              = cfg.port;
    kc.srt_latency_ms    = cfg.srt_latency_ms;
    kc.srt_peer_latency_ms = cfg.srt_peer_latency_ms;
    kc.srt_streamid      = cfg.srt_streamid.c_str();
    kc.srt_passphrase    = cfg.srt_passphrase.empty()
                            ? nullptr : cfg.srt_passphrase.c_str();
    kc.srt_pbkeylen      = cfg.srt_pbkeylen;
    kc.src_w             = cfg.src_w;
    kc.src_h             = cfg.src_h;
    kc.bitrate_bps       = cfg.bitrate_bps;
    kc.gop_length        = cfg.gop_length;
    kc.defer_start       = cfg.defer_start;

    const char* err = nullptr;
    ctx_ = stream_ch2_open(&kc, &err);
    if (!ctx_) {
        last_error_ = err ? err : "stream_ch2_open returned null";
        AIRYS_LOG_ERROR("stream", "open failed: %s", last_error_.c_str());
        return false;
    }

    AIRYS_LOG_INFO("stream",
        "stream_ch2 opened: transport=%s, %dx%d @ %d bps, port=%d, latency=%d ms",
        cfg.transport.c_str(), cfg.src_w, cfg.src_h, cfg.bitrate_bps,
        cfg.port, cfg.srt_latency_ms);
    return true;
}

bool GstStreamer::start() {
    if (!ctx_) return cfg_.transport == "off";   // off mode = trivially "started"
    const bool ok = stream_ch2_start(ctx_);
    if (!ok) {
        AIRYS_LOG_ERROR("stream", "start failed");
    } else {
        AIRYS_LOG_INFO("stream", "pipeline PLAYING");
    }
    return ok;
}

void GstStreamer::stop() {
    if (!ctx_) return;
    stream_ch2_stop(ctx_);
    AIRYS_LOG_INFO("stream", "pipeline NULL");
}

void GstStreamer::close() {
    if (ctx_) {
        stream_ch2_close(ctx_);
        ctx_ = nullptr;
    }
}

int GstStreamer::push_uyvy(const uint8_t* data, size_t size, int64_t pts_us) {
    if (!ctx_) return 0;     // off-mode = swallow
    return stream_ch2_push_uyvy(ctx_, data, size, pts_us);
}

int GstStreamer::push_nv12(const uint8_t* y, size_t y_size,
                            const uint8_t* uv, size_t uv_size,
                            int y_stride, int uv_stride,
                            int64_t pts_us) {
    if (!ctx_) return 0;
    return stream_ch2_push_nv12(ctx_, y, y_size, uv, uv_size,
                                  y_stride, uv_stride, pts_us);
}

// V6.6 zero-copy: V5.23 의 gst_buffer_new_wrapped_full + qdata refcount 패턴.
//   ctx==null (off-mode) 시 release_cb 즉시 호출 후 0 반환 (caller refcount 정리).
//   board impl 이 stream_ch2_push_dmabuf_nv12 의 실 zero-copy 처리를 담당.
int GstStreamer::push_dmabuf_nv12(int nv12_dma_fd,
                                    int width, int height,
                                    int y_stride, int uv_stride,
                                    int64_t pts_us,
                                    void (*release_cb)(void*),
                                    void* release_user_data) {
    if (!ctx_) {
        // off-mode: caller 의 ref 즉시 release
        if (release_cb) release_cb(release_user_data);
        return 0;
    }
    return stream_ch2_push_dmabuf_nv12(ctx_, nv12_dma_fd,
                                         width, height,
                                         y_stride, uv_stride,
                                         pts_us, release_cb, release_user_data);
}

bool GstStreamer::set_bitrate(int bitrate_bps) {
    if (!ctx_) return false;
    cfg_.bitrate_bps = bitrate_bps;
    return stream_ch2_set_bitrate(ctx_, bitrate_bps);
}

GstStreamer::Stats GstStreamer::stats() const {
    Stats out;
    if (!ctx_) return out;
    StreamCh2Stats ks = stream_ch2_stats(ctx_);
    out.frames_pushed       = ks.frames_pushed;
    out.frames_dropped      = ks.frames_dropped;
    out.current_bitrate_bps = ks.current_bitrate_bps;
    out.is_playing          = (ks.is_playing != 0);
    return out;
}

bool GstStreamer::is_playing() const {
    if (!ctx_) return false;
    return stream_ch2_stats(ctx_).is_playing != 0;
}

}  // namespace airys::stream
