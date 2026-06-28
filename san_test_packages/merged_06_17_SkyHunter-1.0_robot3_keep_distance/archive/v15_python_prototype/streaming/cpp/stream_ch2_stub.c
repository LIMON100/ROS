// ============================================================================
// stream_ch2_stub.c  [Host build]
//
// Real implementation in external/AIRYS_TP2855_SRT/src/stream_ch2.c (Kong).
// This stub satisfies the linker for host build (no GStreamer required).
// ============================================================================
#include "streaming/stream_ch2.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct StreamCh2 {
    StreamCh2Config cfg;
    StreamCh2Stats  stats;
    int             playing;
};

void stream_ch2_default_config(StreamCh2Config* cfg) {
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->transport      = STREAM_TRANSPORT_SRT_LISTENER;
    cfg->bind_addr      = "0.0.0.0";
    cfg->host           = "192.168.42.100";
    cfg->port           = 5000;
    cfg->srt_latency_ms = 120;
    cfg->srt_peer_latency_ms = 120;
    cfg->srt_streamid   = "airys-ch2";
    cfg->src_w          = 1920;
    cfg->src_h          = 1080;
    cfg->bitrate_bps    = 4000000;
    cfg->gop_length     = 30;
}

StreamCh2* stream_ch2_open(const StreamCh2Config* cfg, const char** err_out) {
    StreamCh2* s = (StreamCh2*)calloc(1, sizeof(StreamCh2));
    if (!s) { if (err_out) *err_out = "OOM"; return NULL; }
    s->cfg = *cfg;
    fprintf(stderr, "[stream_ch2 STUB] open: transport=%d, %dx%d @ %d bps\n",
            cfg->transport, cfg->src_w, cfg->src_h, cfg->bitrate_bps);
    return s;
}

bool stream_ch2_start(StreamCh2* s) {
    if (!s) return false;
    s->playing = 1;
    s->stats.is_playing = 1;
    fprintf(stderr, "[stream_ch2 STUB] start (PLAYING)\n");
    return true;
}

void stream_ch2_stop(StreamCh2* s) {
    if (!s) return;
    s->playing = 0;
    s->stats.is_playing = 0;
    fprintf(stderr, "[stream_ch2 STUB] stop\n");
}

void stream_ch2_close(StreamCh2* s) {
    if (!s) return;
    free(s);
}

int stream_ch2_push_uyvy(StreamCh2* s, const uint8_t* data, size_t size, int64_t pts_us) {
    (void)data; (void)size; (void)pts_us;
    if (!s || !s->playing) return -1;
    s->stats.frames_pushed++;
    return 0;
}

int stream_ch2_push_nv12(StreamCh2* s,
                          const uint8_t* y, size_t y_sz,
                          const uint8_t* uv, size_t uv_sz,
                          int ys, int uvs, int64_t pts_us) {
    (void)y; (void)y_sz; (void)uv; (void)uv_sz;
    (void)ys; (void)uvs; (void)pts_us;
    if (!s || !s->playing) return -1;
    s->stats.frames_pushed++;
    return 0;
}

// V6.6 zero-copy upgrade — host stub 은 release_cb 즉시 호출 후 fallback
// (실제 양산 board impl 에서 GstDmaBufAllocator + gst_buffer_new_wrapped_full
//  로 zero-copy 처리)
int stream_ch2_push_dmabuf_nv12(StreamCh2* s,
                                 int nv12_dma_fd,
                                 int width, int height,
                                 int y_stride, int uv_stride,
                                 int64_t pts_us,
                                 void (*release_cb)(void*),
                                 void* release_user_data) {
    (void)nv12_dma_fd; (void)width; (void)height;
    (void)y_stride; (void)uv_stride; (void)pts_us;
    if (!s || !s->playing) {
        // Caller refcount 보존 — release 안함 (caller 가 fallback 으로 release)
        return -1;
    }
    // Host stub: 즉시 release_cb 호출 (GStreamer 가 dma_fd 사용 안함)
    if (release_cb) release_cb(release_user_data);
    s->stats.frames_pushed++;
    return 0;
}

bool stream_ch2_set_bitrate(StreamCh2* s, int bitrate_bps) {
    if (!s) return false;
    s->cfg.bitrate_bps = bitrate_bps;
    s->stats.current_bitrate_bps = bitrate_bps;
    return true;
}

StreamCh2Stats stream_ch2_stats(const StreamCh2* s) {
    StreamCh2Stats z = {0};
    return s ? s->stats : z;
}

bool stream_ch2_available(void) {
    return false;   // host build has no real GStreamer
}
