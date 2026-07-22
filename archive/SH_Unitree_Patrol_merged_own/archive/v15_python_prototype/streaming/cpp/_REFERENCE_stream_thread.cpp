// ============================================================================
// threads/stream_thread.cpp  [T_STREAM — AIRYS v6]
//
// Role:  nv12_slot → stream_ch2_push_nv12() (or _uyvy if TP2855 PoC) →
//        GStreamer pipeline (mpph265enc → SRT/UDP).
//
// CPU:   pinned to CPU6 (RK3588 A76 #2)
// Prio:  fair (encoder runs in HW; CPU just pushes buffers)
//
// Backpressure (4-layer, SDD §7.7):
//   1. appsrc leaky-type=2 (downstream): drop OLDEST when queue full
//   2. max-buffers=3: ≈100 ms (3 frames @ 30 fps) max queue
//   3. block=FALSE: push() never blocks T_STREAM
//   4. Drop counter in StreamCh2Stats — telemetry monitoring
//
// Critical: T_STREAM dropping a frame is OK; AI/Display continue.
//          Capture rate is NEVER affected by stream backpressure.
// ============================================================================
#include "pipeline.h"
#include "airys_log.h"
#include "thread_utils.h"
#include "streaming/gst_streamer.h"
#include "nv12_pool.h"

#include <chrono>

namespace airys {

void Pipeline::stream_thread_fn() {
    thread_utils::configure_current_thread(
        "airys_stream",
        cfg::thread_cfg::CPU_STREAM,
        /*rt_priority=*/0);

    AIRYS_LOG_INFO("stream", "T_STREAM entering main loop on CPU%d (fair)",
                  cfg::thread_cfg::CPU_STREAM);

    if (!streamer_) {
        AIRYS_LOG_INFO("stream", "no streamer instance, T_STREAM exiting");
        return;
    }

    std::uint64_t nv12_seq = 0;
    int frames_pushed = 0;
    int frames_dropped = 0;
    auto last_log = std::chrono::steady_clock::now();

    // ★ V6.12.0: Single image path — RAW_ONLY only.
    //   User req: "녹화와 streaming data 는 cropping image 의 720x720 부분."
    //   Streaming 도 nv12_slot (720x720 pre-OSD) 에서 consume.
    AIRYS_LOG_INFO("stream",
        "image path: RAW_ONLY (consume from nv12_slot, 720x720 pre-OSD)");

    auto& src_slot = shared_.nv12_slot;

    NV12Pool* src_pool = nv12_pool_.get();
    if (!src_pool) {
        AIRYS_LOG_ERROR("stream",
            "T_STREAM: nv12_pool_ null — exiting");
        return;
    }

    while (!shared_.stop_requested.load(std::memory_order_relaxed)) {

        // ★ V6.11.0 (2026-04-30): On-demand streaming gate.
        //   사용자 요구: "Gstreamer UDP SRT는 Android app 요청에 의하여 수행"
        //   stream_active_=false 면 frame push skip (drop, no encoder load).
        //   Android 가 "stream/start" 보낼 때 active=true → frame push 시작.
        //   Polling sleep 50ms (idle 시 CPU 부하 0.0%, latency tradeoff 무시).
        if (!stream_active_.load(std::memory_order_acquire)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        // ── 1. Acquire latest NV12Buf (peek with refcount inc) ──
        NV12Buf* buf = nullptr;
        const bool got = src_slot.peek_with_action(
            nv12_seq, buf,
            [src_pool](NV12Buf*& b) { src_pool->add_ref(b); });

        if (!got || !buf) {
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
            continue;
        }

        // ── 2. Push to stream_ch2 ──
        // V6.6 zero-copy 분기 (V5.23 gst_buffer_new_wrapped_full + qdata 패턴):
        //   - NV12 + dma_fd ≥ 0: push_dmabuf_nv12 (zero-copy)
        //                         release_cb 가 GStreamer buffer 마감 시 호출 →
        //                         NV12Pool refcount 정상 release
        //   - UYVY 또는 dma_fd 미가용: push_uyvy/push_nv12 (caller 가 release)
        //
        // 양산 board impl 에서 stream_ch2_push_dmabuf_nv12 가 GstDmaBufAllocator +
        // gst_buffer_new_wrapped_full(nv12_dma_fd, ...) 로 zero-copy 처리.
        // host stub 은 즉시 release_cb 호출 (테스트 path).
        const int64_t pts_us = buf->timestamp_ns / 1000;
        int rc = -1;

        if (buf->pixel_format == NV12Buf::FMT_UYVY) {
            // TP2855 PoC path: UYVY 는 단일 plane, push_uyvy 그대로
            rc = streamer_->push_uyvy(
                static_cast<const uint8_t*>(buf->virt_addr),
                buf->size_bytes, pts_us);
            // UYVY path: caller 가 즉시 release (stream_ch2 가 internal copy)
            src_pool->release(buf);
        } else if (buf->dma_fd >= 0) {
            // ★ V6.6 zero-copy NV12 path (V5.23 동등)
            // release_cb capture: T_STREAM 의 buf 와 src_pool 만 사용해야 함.
            // GStreamer 가 buffer 다 사용 후 (encoder 가 mpph265enc 입력 받은 후)
            // release_nv12_for_stream 호출 → NV12Pool refcount 감소.
            //
            // 주의: rc==0 이어도 GStreamer 가 release_cb 콜할 때까지 buf 가 살아있음.
            //       backpressure drop (rc>0) 또는 error (rc<0) 시 stream_ch2 가
            //       release_cb 를 호출했는지 contract 정의 필요. 현재 contract:
            //       "stream_ch2 가 push 받았으면 (rc==0) release_cb 책임짐.
            //        rc!=0 이면 caller 가 release."
            struct ReleaseCtx { NV12Pool* pool; NV12Buf* buf; };
            auto* ctx = new ReleaseCtx{src_pool, buf};
            const size_t y_size  = buf->stride_y * buf->height;
            const size_t uv_size = buf->stride_uv * buf->height / 2;
            (void)y_size; (void)uv_size;   // 향후 cap negotiation 용

            rc = streamer_->push_dmabuf_nv12(
                buf->dma_fd, buf->width, buf->height,
                buf->stride_y, buf->stride_uv, pts_us,
                /*release_cb=*/[](void* p) {
                    auto* c = static_cast<ReleaseCtx*>(p);
                    if (c && c->pool && c->buf) c->pool->release(c->buf);
                    delete c;
                },
                /*release_user_data=*/ctx);

            if (rc != 0) {
                // push_dmabuf_nv12 가 실패 시 release_cb 호출 안함 → 여기서 release
                if (ctx) {
                    ctx->pool->release(ctx->buf);
                    delete ctx;
                }
            }
        } else {
            // Fallback: NV12 인데 dma_fd 미가용 → push_nv12 (internal copy)
            const uint8_t* y  = static_cast<const uint8_t*>(buf->virt_addr);
            const uint8_t* uv = y + buf->stride_y * buf->height;
            const size_t y_size  = buf->stride_y * buf->height;
            const size_t uv_size = buf->stride_uv * buf->height / 2;
            rc = streamer_->push_nv12(y, y_size, uv, uv_size,
                                       buf->stride_y, buf->stride_uv,
                                       pts_us);
            // Fallback path: caller 가 즉시 release
            src_pool->release(buf);
        }

        if (rc == 0) {
            frames_pushed++;
        } else if (rc > 0) {
            // backpressure drop
            frames_dropped++;
        } else {
            AIRYS_LOG_WARN("stream", "push failed (rc=%d)", rc);
        }

        // ── 3. Periodic stats log + WiFi-aware throttle ──
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed_ms = std::chrono::duration_cast<
            std::chrono::milliseconds>(now - last_log).count();
        if (elapsed_ms > 5000) {
            AIRYS_LOG_DEBUG("stream",
                "fps=%.1f pushed, %.1f dropped, total_pushed=%d",
                frames_pushed / (elapsed_ms / 1000.0),
                frames_dropped / (elapsed_ms / 1000.0),
                frames_pushed);

            // Update SharedState wifi_state — FaultMonitor uses this
            const auto wifi = shared_.wifi_state.load(std::memory_order_relaxed);
            if (wifi != WifiState::AP_ACTIVE) {
                AIRYS_LOG_TRACE("stream", "Wi-Fi not active, T_STREAM idle");
            }

            frames_pushed = 0;
            frames_dropped = 0;
            last_log = now;
        }

        // ── 4. Throttle to capture rate (~30 fps) ──
        // We don't want to spin if no new frames. The peek_with_action
        // is non-blocking so we sleep briefly to avoid CPU burn.
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
    }

    AIRYS_LOG_INFO("stream", "T_STREAM exiting");
}

}  // namespace airys
