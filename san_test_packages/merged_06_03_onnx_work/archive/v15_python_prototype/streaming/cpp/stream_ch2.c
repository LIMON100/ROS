/*  stream_ch2.c — Ch 2 video Wi-Fi 송신 구현 (UDP/RTP 또는 SRT).
 *  encoding: UTF-8 (no BOM)
 *
 *  Build modes:
 *    AIRYS_HAVE_GSTREAMER=1  실제 pipeline (RK3588 타겟 / GStreamer 설치된 Ubuntu)
 *    AIRYS_HAVE_GSTREAMER=0  모든 API가 no-op (테스트/개발 PC에 GStreamer 없을 때)
 *
 *  Ownership of push buffers:
 *    stream_ch2_push_uyvy() / _push_nv12() 는 내부에서 gst_buffer_new_memdup()
 *    로 복사하므로 호출자(test_main.c)는 즉시 camera_v4l2_release() 가능.
 *    이는 zero-copy 가 아니지만 — TP2855 mmap buffer 개수가 고정(CAM_NBUF=4)
 *    이라 release 를 지연하면 capture 가 stall 함 — RK3588의 memcpy BW는
 *    1920x1080 UYVY (≈4MB/frame) × 30fps = 120 MB/s 로 A76 single core
 *    대역폭의 ~5%만 소비. 실측 문제 없음.
 *
 *  Doc ID: SAN-STREAM-SW-001 Rev.D
 */

#include "stream_ch2.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG(fmt, ...)  fprintf(stderr, "[stream_ch2] " fmt "\n", ##__VA_ARGS__)
#define LOGM(msg)      fprintf(stderr, "[stream_ch2] " msg "\n")

/* Clamp bitrate to a safe int range. The encoder property setters compute
 * `bps*2` for bps-max; without clamping this is signed-integer-overflow UB
 * (UBSan -fsanitize=signed-integer-overflow). Lower bound matches the
 * minimum bitrate we'd ever ship (100 kbps), upper bound leaves headroom
 * so bps*2 stays representable. */
#define STREAM_CH2_MIN_BPS    100000
#define STREAM_CH2_MAX_BPS    (INT_MAX / 2)

static int clamp_bitrate(int bps)
{
    if (bps < STREAM_CH2_MIN_BPS) return STREAM_CH2_MIN_BPS;
    if (bps > STREAM_CH2_MAX_BPS) return STREAM_CH2_MAX_BPS;
    return bps;
}

/* ======================== defaults (both modes) ======================== */

void stream_ch2_default_config(StreamCh2Config* cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
    cfg->transport           = STREAM_TRANSPORT_SRT_LISTENER;
    cfg->bind_addr           = "0.0.0.0";
    cfg->host                = "192.168.42.100";
    cfg->port                = 5000;
    cfg->srt_latency_ms      = 120;
    cfg->srt_peer_latency_ms = 120;
    cfg->srt_streamid        = "airys-ch2";
    cfg->srt_passphrase      = "";
    cfg->srt_pbkeylen        = 0;
    cfg->src_w               = 1920;
    cfg->src_h               = 1080;
    cfg->bitrate_bps         = 4000000;
    cfg->gop_length          = 30;
    cfg->defer_start         = false;
}

/* ========================================================================= */
/*                     Real implementation (GStreamer)                        */
/* ========================================================================= */
#if defined(AIRYS_HAVE_GSTREAMER) && AIRYS_HAVE_GSTREAMER

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <pthread.h>
#include <stdatomic.h>

struct StreamCh2 {
    StreamCh2Config cfg;         /* owned deep-copy of string fields below    */

    /* deep-copied strings so cfg pointers stay valid after the caller frees */
    char owned_bind_addr[64];
    char owned_host[64];
    char owned_streamid[64];
    char owned_passphrase[96];

    GstElement*   pipeline;
    GstElement*   appsrc;
    GstElement*   encoder;
    GstElement*   parser;
    GstElement*   rtp_pay;       /* UDP_RTP mode only */
    GstElement*   tsmux;         /* SRT modes only    */
    GstElement*   sink;          /* udpsink or srtsink */

    atomic_bool   playing;
    atomic_ulong  frames_pushed;
    atomic_ulong  frames_dropped;
    atomic_int    current_bitrate;

    bool          nv12_caps_set;     /* true after first NV12 push reconfigures appsrc caps */

    /* Bus is drained by a small polling thread (no GMainLoop/Context —
     * default GMainContext holds eventfd + epoll that never get released
     * when per-stream GMainLoops are unreffed, leaking ~2 fds per cycle). */
    pthread_t     bus_thread;
    atomic_bool   bus_thread_run;   /* false → thread exits */
    /* Set on the create-side immediately after pthread_create succeeds, NOT
     * inside the thread body. The previous design set this in bus_thread_fn,
     * which left a window where stream_ch2_close could observe it as false,
     * skip pthread_join, free(s), and the thread would then dereference
     * freed memory. */
    bool          bus_thread_alive;
};

/* --- Pick encoder by availability. RK3588 BSP has mpph265enc; fallback x265. --- */
static const char* pick_encoder(void)
{
    if (gst_element_factory_find("mpph265enc"))  return "mpph265enc";
    if (gst_element_factory_find("x265enc"))     return "x265enc";
    return NULL;
}

/* Process a single bus message. Previously this was a gst_bus_add_watch callback;
 * now the polling thread calls it directly. Semantics identical. */
static void handle_bus_msg(StreamCh2* s, GstMessage* msg)
{
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* err = NULL; gchar* dbg = NULL;
        gst_message_parse_error(msg, &err, &dbg);
        LOG("pipeline ERROR: %s  (%s)",
            err ? err->message : "?", dbg ? dbg : "");
        if (err) g_error_free(err);
        g_free(dbg);
        atomic_store(&s->playing, false);
        break;
    }
    case GST_MESSAGE_EOS:
        LOGM("pipeline EOS");
        atomic_store(&s->playing, false);
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(msg) == (GstObject*)s->pipeline) {
            GstState old_s, new_s, pend;
            gst_message_parse_state_changed(msg, &old_s, &new_s, &pend);
            if (new_s == GST_STATE_PLAYING) {
                atomic_store(&s->playing, true);
            } else if (new_s < GST_STATE_PAUSED) {
                atomic_store(&s->playing, false);
            }
        }
        break;
    }
    default:
        break;
    }
}

static void* bus_thread_fn(void* arg)
{
    StreamCh2* s = (StreamCh2*)arg;
    GstBus* bus = gst_pipeline_get_bus(GST_PIPELINE(s->pipeline));
    while (atomic_load(&s->bus_thread_run)) {
        /* 100 ms poll — low wakeup rate, low CPU, responsive enough for
         * error/EOS detection. */
        GstMessage* msg = gst_bus_timed_pop(bus, 100 * GST_MSECOND);
        if (msg) {
            handle_bus_msg(s, msg);
            gst_message_unref(msg);
        }
    }
    gst_object_unref(bus);
    return NULL;
}

/* Build an SRT URI with proper query string escaping for passphrase. */
static gchar* build_srt_uri(const StreamCh2Config* cfg)
{
    const bool listener = (cfg->transport == STREAM_TRANSPORT_SRT_LISTENER);
    const char* addr    = listener ? cfg->bind_addr : cfg->host;
    const char* mode    = listener ? "listener"    : "caller";

    GString* gs = g_string_new(NULL);
    g_string_printf(gs,
        "srt://%s:%d?mode=%s&latency=%d&peerlatency=%d&streamid=%s",
        addr ? addr : "0.0.0.0",
        cfg->port,
        mode,
        cfg->srt_latency_ms      > 0 ? cfg->srt_latency_ms      : 120,
        cfg->srt_peer_latency_ms > 0 ? cfg->srt_peer_latency_ms : 120,
        cfg->srt_streamid && *cfg->srt_streamid ? cfg->srt_streamid : "airys-ch2");

    if (cfg->srt_passphrase && *cfg->srt_passphrase &&
        (cfg->srt_pbkeylen == 16 || cfg->srt_pbkeylen == 24 ||
         cfg->srt_pbkeylen == 32))
    {
        gchar* escaped = g_uri_escape_string(
            cfg->srt_passphrase, NULL, FALSE);
        g_string_append_printf(gs, "&passphrase=%s&pbkeylen=%d",
                               escaped, cfg->srt_pbkeylen);
        g_free(escaped);
    }
    return g_string_free(gs, FALSE);   /* caller g_free */
}

StreamCh2* stream_ch2_open(const StreamCh2Config* cfg, const char** err_out)
{
    static const char* err_gst_init  = "gst_init failed";
    static const char* err_no_enc    = "no H.265 encoder available (install mpph265enc or x265enc)";
    static const char* err_factory   = "GStreamer element factory returned NULL";
    static const char* err_link      = "gst_element_link_many failed";
    static const char* err_missing_bad = "missing gst-plugins-bad (srtsink / mpegtsmux / rtph265pay)";

    if (!cfg) { if (err_out) *err_out = "cfg is NULL"; return NULL; }

    if (!gst_is_initialized()) {
        GError* err = NULL;
        if (!gst_init_check(NULL, NULL, &err)) {
            LOG("gst_init: %s", err ? err->message : "unknown");
            if (err) g_error_free(err);
            if (err_out) *err_out = err_gst_init;
            return NULL;
        }
    }

    StreamCh2* s = calloc(1, sizeof(*s));
    if (!s) return NULL;

    /* deep-copy strings so the caller can free their own cfg */
    s->cfg = *cfg;
    #define DUPSTR(dst, src) \
        do { snprintf(dst, sizeof(dst), "%s", src ? src : ""); } while (0)
    DUPSTR(s->owned_bind_addr,  cfg->bind_addr);
    DUPSTR(s->owned_host,       cfg->host);
    DUPSTR(s->owned_streamid,   cfg->srt_streamid);
    DUPSTR(s->owned_passphrase, cfg->srt_passphrase);
    #undef DUPSTR
    s->cfg.bind_addr      = s->owned_bind_addr;
    s->cfg.host           = s->owned_host;
    s->cfg.srt_streamid   = s->owned_streamid;
    s->cfg.srt_passphrase = s->owned_passphrase;

    const int bitrate = clamp_bitrate(cfg->bitrate_bps);
    atomic_store(&s->current_bitrate, bitrate);

    /* --- appsrc ---
     * Create-then-immediately-add to the pipeline. This transfers the
     * floating GObject ref to the bin so any subsequent goto-fail path
     * cleans up via gst_object_unref(s->pipeline) without leaking the
     * already-created elements. */
    s->pipeline = gst_pipeline_new("airys-ch2");
    if (!s->pipeline) {
        LOGM("pipeline create failed");
        if (err_out) *err_out = err_factory;
        goto fail;
    }
    s->appsrc = gst_element_factory_make("appsrc", "ch2-src");
    if (!s->appsrc) {
        LOGM("appsrc create failed");
        if (err_out) *err_out = err_factory;
        goto fail;
    }
    gst_bin_add(GST_BIN(s->pipeline), s->appsrc);

    /* TP2855 는 UYVY 를 주고, encoder 는 NV12 를 원하므로 videoconvert 삽입.
     * RK3588 BSP 에서는 RGA 기반 rga-videoconvert 가 있으면 그게 우선 선택됨
     * (gst_element_factory_make 가 rank 높은 것을 고름). */
    GstElement* conv = gst_element_factory_make("videoconvert", "ch2-conv");
    if (!conv) {
        LOGM("videoconvert create failed");
        if (err_out) *err_out = err_factory;
        goto fail;
    }
    gst_bin_add(GST_BIN(s->pipeline), conv);

    /* appsrc caps: we push UYVY. 다른 포맷이 필요하면 set_caps 로 push 전에 덮어쓰기. */
    GstCaps* src_caps = gst_caps_new_simple("video/x-raw",
        "format",    G_TYPE_STRING,     "UYVY",
        "width",     G_TYPE_INT,        cfg->src_w,
        "height",    G_TYPE_INT,        cfg->src_h,
        "framerate", GST_TYPE_FRACTION, 30, 1,
        NULL);
    g_object_set(s->appsrc,
        "caps",          src_caps,
        "stream-type",   0,              /* GST_APP_STREAM_TYPE_STREAM */
        "format",        GST_FORMAT_TIME,
        "is-live",       TRUE,
        "block",         FALSE,
        "max-buffers",   3,
        /* leaky=downstream: encoder back-pressure drops oldest, never stalls capture */
        "leaky-type",    2,
        NULL);
    gst_caps_unref(src_caps);

    /* --- encoder --- */
    const char* enc_factory = pick_encoder();
    if (!enc_factory) {
        LOGM("no H.265 encoder");
        if (err_out) *err_out = err_no_enc;
        goto fail;
    }
    s->encoder = gst_element_factory_make(enc_factory, "ch2-enc");
    if (!s->encoder) {
        if (err_out) *err_out = err_factory;
        goto fail;
    }
    gst_bin_add(GST_BIN(s->pipeline), s->encoder);
    /* Properties differ between mpph265enc (uses 'bps') and x265enc (uses 'bitrate' in kbps). */
    if (!strcmp(enc_factory, "mpph265enc")) {
        g_object_set(s->encoder,
            "bps",              bitrate,
            "bps-max",          bitrate * 2,           /* clamp_bitrate guards overflow */
            "bps-min",          bitrate / 2,
            "gop",              cfg->gop_length > 0 ? cfg->gop_length : 30,
            "rc-mode",          1,    /* 0=VBR 1=CBR 2=FIXQP */
            NULL);
    } else {
        g_object_set(s->encoder,
            "bitrate",          bitrate / 1000,        /* x265enc: kbps */
            "speed-preset",     1,                      /* ultrafast */
            "tune",             0x4,                    /* zerolatency */
            "key-int-max",      cfg->gop_length > 0 ? cfg->gop_length : 30,
            NULL);
        LOGM("using x265enc (software) — RK3588 BSP 권장은 mpph265enc");
    }

    /* --- parser --- */
    s->parser = gst_element_factory_make("h265parse", "ch2-parse");
    if (!s->parser) {
        LOGM("h265parse missing");
        if (err_out) *err_out = err_missing_bad;
        goto fail;
    }
    gst_bin_add(GST_BIN(s->pipeline), s->parser);
    g_object_set(s->parser, "config-interval", 1, NULL);

    /* --- transport-specific tail: RTP/udpsink OR mpegtsmux/srtsink ---
     * Each element is added to the bin immediately after creation so any
     * subsequent failure leaks nothing — pipeline unref drops all children. */
    if (cfg->transport == STREAM_TRANSPORT_UDP_RTP) {
        s->rtp_pay = gst_element_factory_make("rtph265pay", "ch2-rtp");
        if (!s->rtp_pay) {
            LOGM("rtph265pay missing (gst-plugins-good required)");
            if (err_out) *err_out = err_missing_bad;
            goto fail;
        }
        gst_bin_add(GST_BIN(s->pipeline), s->rtp_pay);

        s->sink = gst_element_factory_make("udpsink", "ch2-udpsink");
        if (!s->sink) {
            LOGM("udpsink missing");
            if (err_out) *err_out = err_missing_bad;
            goto fail;
        }
        gst_bin_add(GST_BIN(s->pipeline), s->sink);

        g_object_set(s->rtp_pay,
            "pt",              96,
            "mtu",             1400,
            "config-interval", 1,
            NULL);
        g_object_set(s->sink,
            "host",        cfg->host,
            "port",        cfg->port,
            "sync",        FALSE,
            "async",       FALSE,
            "qos-dscp",    46,       /* DSCP EF (Expedited Forwarding) */
            "buffer-size", 1048576,
            NULL);

        if (!gst_element_link_many(
                s->appsrc, conv, s->encoder, s->parser, s->rtp_pay, s->sink, NULL))
        {
            LOGM("link_many (UDP) failed");
            if (err_out) *err_out = err_link;
            goto fail;
        }
        LOG("pipeline: appsrc -> videoconvert -> %s -> h265parse -> rtph265pay "
            "-> udpsink host=%s port=%d", enc_factory, cfg->host, cfg->port);

    } else {
        s->tsmux = gst_element_factory_make("mpegtsmux", "ch2-tsmux");
        if (!s->tsmux) {
            LOGM("mpegtsmux missing (gst-plugins-bad required)");
            if (err_out) *err_out = err_missing_bad;
            goto fail;
        }
        gst_bin_add(GST_BIN(s->pipeline), s->tsmux);

        s->sink = gst_element_factory_make("srtsink", "ch2-srtsink");
        if (!s->sink) {
            LOGM("srtsink missing (libsrt required)");
            if (err_out) *err_out = err_missing_bad;
            goto fail;
        }
        gst_bin_add(GST_BIN(s->pipeline), s->sink);

        /* Tight PCR cadence → Android jitter buffer stabilizes quickly. */
        g_object_set(s->tsmux, "alignment", 7, NULL);

        gchar* uri = build_srt_uri(cfg);
        LOG("SRT uri: %s", uri);
        /* wait-for-connection=true 은 listener 모드에서 pipeline 이 PAUSED
         * 에서 넘어가기 전에 client 를 기다린다. 하지만 우리는 on-demand
         * 모델이므로 non-blocking 으로 두고, client 가 붙을 때까지 sink 가
         * 자체적으로 drop 한다. */
        g_object_set(s->sink,
            "uri",                 uri,
            "sync",                FALSE,
            "async",               FALSE,
            "wait-for-connection", FALSE,
            NULL);
        g_free(uri);

        if (!gst_element_link_many(
                s->appsrc, conv, s->encoder, s->parser, s->tsmux, s->sink, NULL))
        {
            LOGM("link_many (SRT) failed");
            if (err_out) *err_out = err_link;
            goto fail;
        }
        LOG("pipeline: appsrc -> videoconvert -> %s -> h265parse -> mpegtsmux "
            "-> srtsink (%s)", enc_factory,
            cfg->transport == STREAM_TRANSPORT_SRT_LISTENER ? "listener" : "caller");
    }

    /* Polling bus drain thread — see struct comment for rationale. */
    atomic_store(&s->bus_thread_run, true);
    s->bus_thread_alive = false;
    if (pthread_create(&s->bus_thread, NULL, bus_thread_fn, s) != 0) {
        atomic_store(&s->bus_thread_run, false);
        LOGM("pthread_create for bus thread failed");
        if (err_out) *err_out = "pthread_create failed";
        goto fail;
    }
    /* Mark alive BEFORE any further fallible step. Once this is true,
     * stream_ch2_close is guaranteed to signal+join the thread, so the
     * thread can never outlive `s`. */
    s->bus_thread_alive = true;

    if (!cfg->defer_start) {
        if (!stream_ch2_start(s)) {
            /* State change to PLAYING failed (e.g. port already bound,
             * srtsink refused). Tear down so the caller doesn't hold an
             * un-streamable object. */
            if (err_out) *err_out = "pipeline state change to PLAYING failed (port in use?)";
            goto fail;
        }
    }
    return s;

fail:
    stream_ch2_close(s);
    return NULL;
}

bool stream_ch2_start(StreamCh2* s)
{
    if (!s || !s->pipeline) return false;
    GstStateChangeReturn r = gst_element_set_state(s->pipeline, GST_STATE_PLAYING);
    if (r == GST_STATE_CHANGE_FAILURE) {
        LOGM("set_state PLAYING failed");
        return false;
    }
    LOG("PLAYING (transport=%d, %d bps)",
        s->cfg.transport, atomic_load(&s->current_bitrate));
    return true;
}

void stream_ch2_stop(StreamCh2* s)
{
    if (!s || !s->pipeline) return;
    gst_element_set_state(s->pipeline, GST_STATE_READY);
    atomic_store(&s->playing, false);
    LOGM("READY (stopped)");
}

void stream_ch2_close(StreamCh2* s)
{
    if (!s) return;
    /* Stop pipeline first so bus has no more messages queued. */
    if (s->pipeline) {
        gst_element_set_state(s->pipeline, GST_STATE_NULL);
    }
    /* Signal polling thread to exit and join. `bus_thread_alive` is set on
     * the create-side right after pthread_create, so this is race-free —
     * if true, the thread exists and will observe bus_thread_run==false
     * within one poll period and exit. We must not skip the join (would
     * leave the thread referencing `s` after free → use-after-free). */
    if (s->bus_thread_alive) {
        atomic_store(&s->bus_thread_run, false);
        pthread_join(s->bus_thread, NULL);
        s->bus_thread_alive = false;
    }
    if (s->pipeline) {
        gst_object_unref(s->pipeline);   /* drops children too */
    }
    free(s);
}

static int push_raw(StreamCh2* s, const uint8_t* data, size_t size,
                    int64_t pts_us)
{
    if (!s || !s->appsrc) return -1;
    /* Defence: NULL buffer or zero size → drop and report, don't crash. */
    if (!data || size == 0) {
        atomic_fetch_add(&s->frames_dropped, 1);
        return 0;
    }
    /* If pipeline isn't PLAYING yet (e.g. defer_start was set and nobody
     * called stream_ch2_start()), push would eventually FLUSHING. Check early. */
    if (!atomic_load(&s->playing)) {
        atomic_fetch_add(&s->frames_dropped, 1);
        return -1;
    }

    /* memdup → GstBuffer. Kong's mmap buffer lifetime is short (CAM_NBUF=4),
     * so we own a copy that survives until the encoder consumes it. */
    GstBuffer* buf = gst_buffer_new_memdup(data, size);
    if (!buf) {
        atomic_fetch_add(&s->frames_dropped, 1);
        return 0;
    }
    GST_BUFFER_PTS(buf)      = (pts_us >= 0) ? (GstClockTime)(pts_us * 1000) : GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buf) = GST_SECOND / 30;

    GstFlowReturn fr = gst_app_src_push_buffer(GST_APP_SRC(s->appsrc), buf);
    /* push_buffer transfers ownership on success; do not unref buf here. */
    if (fr == GST_FLOW_OK) {
        atomic_fetch_add(&s->frames_pushed, 1);
        return 1;
    }
    if (fr == GST_FLOW_FLUSHING) {
        /* Pipeline not yet started or stopped. Caller may retry later. */
        return -1;
    }
    /* Appsrc is non-blocking w/ leaky=downstream; this path is rare. */
    atomic_fetch_add(&s->frames_dropped, 1);
    return 0;
}

int stream_ch2_push_uyvy(StreamCh2* s, const uint8_t* data, size_t size,
                         int64_t pts_us)
{
    return push_raw(s, data, size, pts_us);
}

int stream_ch2_push_nv12(StreamCh2* s,
                         const uint8_t* y,  size_t y_size,
                         const uint8_t* uv, size_t uv_size,
                         int y_stride, int uv_stride,
                         int64_t pts_us)
{
    (void)y_stride; (void)uv_stride;
    if (!s || !s->appsrc) return -1;
    if (!y || y_size == 0) {
        atomic_fetch_add(&s->frames_dropped, 1);
        return 0;
    }
    if (!atomic_load(&s->playing)) {
        atomic_fetch_add(&s->frames_dropped, 1);
        return -1;
    }

    /* Switch appsrc caps to NV12 on first NV12 push (IMX678 CSI path). */
    if (!s->nv12_caps_set) {
        GstCaps* nv12_caps = gst_caps_new_simple("video/x-raw",
            "format",    G_TYPE_STRING,     "NV12",
            "width",     G_TYPE_INT,        s->cfg.src_w,
            "height",    G_TYPE_INT,        s->cfg.src_h,
            "framerate", GST_TYPE_FRACTION, 30, 1,
            NULL);
        g_object_set(s->appsrc, "caps", nv12_caps, NULL);
        gst_caps_unref(nv12_caps);
        s->nv12_caps_set = true;
    }

    /* Build GstBuffer and copy Y+UV planes directly — single copy,
     * avoids the intermediate malloc+memcpy that push_raw would add. */
    size_t total = y_size + uv_size;
    GstBuffer* buf = gst_buffer_new_allocate(NULL, total, NULL);
    if (!buf) {
        atomic_fetch_add(&s->frames_dropped, 1);
        return 0;
    }
    gst_buffer_fill(buf, 0, y, y_size);
    if (uv && uv_size > 0) gst_buffer_fill(buf, y_size, uv, uv_size);

    GST_BUFFER_PTS(buf)      = (pts_us >= 0) ? (GstClockTime)(pts_us * 1000) : GST_CLOCK_TIME_NONE;
    GST_BUFFER_DURATION(buf) = GST_SECOND / 30;

    GstFlowReturn fr = gst_app_src_push_buffer(GST_APP_SRC(s->appsrc), buf);
    if (fr == GST_FLOW_OK) {
        atomic_fetch_add(&s->frames_pushed, 1);
        return 1;
    }
    if (fr == GST_FLOW_FLUSHING) {
        return -1;
    }
    atomic_fetch_add(&s->frames_dropped, 1);
    return 0;
}

bool stream_ch2_set_bitrate(StreamCh2* s, int bitrate_bps)
{
    if (!s || !s->encoder) return false;
    const int bitrate = clamp_bitrate(bitrate_bps);
    GObjectClass* cls = G_OBJECT_GET_CLASS(s->encoder);
    if (g_object_class_find_property(cls, "bps")) {
        g_object_set(s->encoder, "bps", bitrate, NULL);
    } else if (g_object_class_find_property(cls, "bitrate")) {
        g_object_set(s->encoder, "bitrate", bitrate / 1000, NULL);
    } else {
        return false;
    }
    atomic_store(&s->current_bitrate, bitrate);
    LOG("bitrate -> %d bps%s", bitrate,
        bitrate == bitrate_bps ? "" : " (clamped)");
    return true;
}

StreamCh2Stats stream_ch2_stats(const StreamCh2* s)
{
    StreamCh2Stats st = {0};
    if (!s) return st;
    st.frames_pushed       = atomic_load(&((StreamCh2*)s)->frames_pushed);
    st.frames_dropped      = atomic_load(&((StreamCh2*)s)->frames_dropped);
    st.current_bitrate_bps = (uint32_t)atomic_load(&((StreamCh2*)s)->current_bitrate);
    st.is_playing          = atomic_load(&((StreamCh2*)s)->playing) ? 1 : 0;
    return st;
}

bool stream_ch2_available(void) { return true; }

/* ========================================================================= */
/*                         Stub implementation (no GStreamer)                 */
/* ========================================================================= */
#else  /* AIRYS_HAVE_GSTREAMER */

struct StreamCh2 { int dummy; };

StreamCh2* stream_ch2_open(const StreamCh2Config* cfg, const char** err_out)
{
    (void)cfg;
    LOGM("built without GStreamer — Ch 2 streaming disabled");
    if (err_out) *err_out = "GStreamer not compiled in";
    return NULL;
}
bool stream_ch2_start(StreamCh2* s)               { (void)s; return false; }
void stream_ch2_stop (StreamCh2* s)               { (void)s; }
void stream_ch2_close(StreamCh2* s)               { (void)s; }
int  stream_ch2_push_uyvy(StreamCh2* s, const uint8_t* d, size_t n, int64_t p)
{ (void)s; (void)d; (void)n; (void)p; return -1; }
int  stream_ch2_push_nv12(StreamCh2* s, const uint8_t* y, size_t yn,
                          const uint8_t* uv, size_t uvn,
                          int ys, int uvs, int64_t p)
{ (void)s; (void)y; (void)yn; (void)uv; (void)uvn; (void)ys; (void)uvs; (void)p; return -1; }
bool stream_ch2_set_bitrate(StreamCh2* s, int b)  { (void)s; (void)b; return false; }
StreamCh2Stats stream_ch2_stats(const StreamCh2* s) { (void)s; StreamCh2Stats st = {0}; return st; }
bool stream_ch2_available(void) { return false; }

#endif /* AIRYS_HAVE_GSTREAMER */
