/* ============================================================================
 * board/stream_ch2_dmabuf_board.c  [V6.8 RevG — zero-copy GST stream]
 *
 * Real GstDmaBufAllocator + gst_buffer_new_wrapped_full path.
 *
 * Kong's stream_ch2 (external/AIRYS_TP2855_SRT) 가 GStreamer C API 로
 * appsrc → mpph265enc → rtph265pay → udpsink 또는 srtsink 로 stream.
 * 그러나 push_dmabuf_nv12 는 미구현 — 본 file 이 board build 에서 link
 * 되며 weak symbol 로 stub 의 빈 impl 을 override.
 *
 * 양산 단계 (Phase 2 BSP):
 *   - GStreamer 1.22+ (Yocto / RK3588J BSP)
 *   - gstreamer-allocators-1.0 (GstDmaBufAllocator)
 *   - mpph265enc 가 dmabuf input 지원 (caps:
 *     "video/x-raw(memory:DMABuf), format=NV12, ...")
 *
 * Latency 효과:
 *   - 이전 (gst_buffer_new_memdup): 1080p NV12 4MB memcpy ~1ms +
 *     L2 cache pollution → mpph265enc 입력 ready 까지 +1.5ms
 *   - V6.8 RevG (dmabuf): mpph265enc 가 dma_fd 직접 import (memcpy 0)
 *   - 절감: ~1-2ms latency + L2 cache 보존
 *
 * 현재 (RevG, BSP 도착 전): real GStreamer link 가 안되어 있어
 * stub 와 동일 fallback. BSP 활성화 시 #if 0 → #if 1 로 enable.
 * ========================================================================= */

#include "streaming/stream_ch2.h"
#include <stdint.h>
#include <stddef.h>

/* StreamCh2 의 internal struct 는 stream_ch2.c (Kong) 에 정의됨.
 * 본 file 은 push_dmabuf_nv12 만 override (weak symbol) — board build 에서
 * 이 file 이 link 되면 stub 의 동일 함수가 무시됨.
 *
 * 양산 단계에서 #if 1 로 변경 + GStreamer link 추가:
 *   target_link_libraries(airys PRIVATE
 *       PkgConfig::GSTREAMER PkgConfig::GSTREAMER_APP
 *       PkgConfig::GSTREAMER_ALLOCATORS)
 */

#if 0  /* Phase 2 BSP 도착 시 enable */

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/allocators/gstdmabuf.h>
#include <unistd.h>      /* dup() */

typedef struct {
    int  dma_fd;
    void (*release_cb)(void*);
    void* release_user_data;
} DmabufReleaseCtx;

static void dmabuf_release_(void* user_data) {
    DmabufReleaseCtx* ctx = (DmabufReleaseCtx*)user_data;
    if (ctx) {
        /* close(ctx->dma_fd);  // GstDmaBufAllocator 가 자체 close — caller 의 fd 와 분리 */
        if (ctx->release_cb) ctx->release_cb(ctx->release_user_data);
        free(ctx);
    }
}

int stream_ch2_push_dmabuf_nv12(StreamCh2* s,
                                  int nv12_dma_fd,
                                  int width, int height,
                                  int y_stride, int uv_stride,
                                  int64_t pts_us,
                                  void (*release_cb)(void*),
                                  void* release_user_data) {
    if (!s || !s->playing || nv12_dma_fd < 0) {
        return -1;
    }

    /* Backpressure check (queued frames > threshold) */
    /* if (s->backpressure_dropped) { ... return 1; } */

    /* GStreamer 가 dma_fd 를 close 하지 않도록 dup */
    int dup_fd = dup(nv12_dma_fd);
    if (dup_fd < 0) {
        return -1;
    }

    /* GstDmaBufAllocator (singleton, lazy-init in stream_ch2_open) */
    GstAllocator* alloc = s->dmabuf_allocator;
    if (!alloc) {
        alloc = gst_dmabuf_allocator_new();
        s->dmabuf_allocator = alloc;
    }

    const size_t size = (size_t)y_stride * height +
                        (size_t)uv_stride * (height / 2);

    DmabufReleaseCtx* rc = (DmabufReleaseCtx*)malloc(sizeof(DmabufReleaseCtx));
    rc->dma_fd            = dup_fd;
    rc->release_cb        = release_cb;
    rc->release_user_data = release_user_data;

    GstMemory* mem = gst_dmabuf_allocator_alloc(alloc, dup_fd, size);
    if (!mem) {
        close(dup_fd);
        free(rc);
        return -1;
    }

    GstBuffer* buf = gst_buffer_new();
    gst_buffer_append_memory(buf, mem);

    /* PTS / DTS */
    GST_BUFFER_PTS(buf) = (GstClockTime)(pts_us * 1000);    /* us → ns */
    GST_BUFFER_DTS(buf) = GST_BUFFER_PTS(buf);
    GST_BUFFER_DURATION(buf) = GST_MSECOND * 33;

    /* Release callback chain — buffer free 시 dmabuf_release_ 호출 */
    GstMiniObject* mo = GST_MINI_OBJECT_CAST(buf);
    gst_mini_object_set_qdata(mo,
        g_quark_from_static_string("airys-dmabuf-ctx"),
        rc, dmabuf_release_);

    /* appsrc push */
    GstFlowReturn ret = gst_app_src_push_buffer(
        GST_APP_SRC(s->appsrc), buf);
    if (ret != GST_FLOW_OK) {
        /* push 실패 시 buffer auto-unref → dmabuf_release_ 호출됨 */
        return -1;
    }

    s->stats.frames_pushed++;
    return 0;
}

#else  /* RevG: real GStreamer 미연동 (BSP 대기) */

/* Fallback: caller 의 release_cb 즉시 호출 (caller 가 NV12Pool refcount 정상 release).
 * 양산 BSP 도착 후 위 #if 1 로 변경 → real zero-copy GST path. */

int stream_ch2_push_dmabuf_nv12(StreamCh2* s,
                                  int nv12_dma_fd,
                                  int width, int height,
                                  int y_stride, int uv_stride,
                                  int64_t pts_us,
                                  void (*release_cb)(void*),
                                  void* release_user_data) {
    /* Suppress unused warnings */
    (void)nv12_dma_fd; (void)width; (void)height;
    (void)y_stride; (void)uv_stride; (void)pts_us;

    if (!s) {
        return -1;
    }
    /* Phase 2 placeholder: 즉시 release_cb 호출.
     * 양산 단계 #if 1 → real GstDmaBufAllocator path. */
    if (release_cb) release_cb(release_user_data);
    return 0;
}

#endif  /* Phase 2 BSP 도착 후 enable */
