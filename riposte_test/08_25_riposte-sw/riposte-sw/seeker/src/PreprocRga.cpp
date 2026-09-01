#include "Preproc.h"

#include <cstddef>
#include <cstdint>

#include "riposte/Log.h"

// RK3588 RGA (im2d) production path for the model-input letterbox: NV12 ->
// RGB888 scale + colour conversion in a single 2D-engine pass, zero CPU pixels.
// Compiled only when RIPOSTE_WITH_RGA=ON; on any failure the caller
// (HailoDetector) falls back to the CPU reference in Preproc.cpp, whose output
// this path must match pixel-wise at bring-up (SDD §4.4.5 item 4).
#include <rga/RgaUtils.h>
#include <rga/im2d.h>
#include <rga/rga.h>

namespace riposte {

namespace {
constexpr uint32_t FOURCC_NV12 = 0x3231564E; // 'N','V','1','2'
} // namespace

bool letterbox_nv12_rgb888_rga(const Frame& f, const Letterbox& lb, uint8_t* dst,
                               std::size_t dst_size) {
    // Same contract gates as the CPU reference: refuse rather than hand the 2D
    // engine a geometry that doesn't belong to this frame.
    if (f.data == nullptr || dst == nullptr || f.fourcc != FOURCC_NV12 || !lb.valid() ||
        lb.scale <= 0.F) {
        return false;
    }
    if (lb.src_w != f.width || lb.src_h != f.height || f.width < 2 || f.height < 2) {
        return false;
    }
    const std::size_t need = static_cast<std::size_t>(lb.model_size) *
                             static_cast<std::size_t>(lb.model_size) * 3U;
    if (dst_size < need) {
        return false;
    }

    // wstride is in pixels; NV12 luma is 1 byte/pixel so the byte stride maps
    // directly. RGA reads the interleaved UV plane at hstride rows below.
    const int wstride = (f.stride != 0U) ? static_cast<int>(f.stride) : f.width;
    rga_buffer_t src = wrapbuffer_virtualaddr_t(
        // RGA's C API takes a mutable pointer but only reads the source.
        const_cast<uint8_t*>(f.data), f.width, f.height, wstride, f.height,
        RK_FORMAT_YCbCr_420_SP);
    rga_buffer_t dst_buf =
        wrapbuffer_virtualaddr_t(dst, lb.model_size, lb.model_size, lb.model_size,
                                 lb.model_size, RK_FORMAT_RGB_888);

    im_rect src_rect{};
    src_rect.x = 0;
    src_rect.y = 0;
    src_rect.width = f.width;
    src_rect.height = f.height;
    // The destination rect IS the letterbox: content only, padding untouched —
    // the caller owns the pad fill, same contract as the CPU path.
    im_rect dst_rect{};
    dst_rect.x = lb.pad_x;
    dst_rect.y = lb.pad_y;
    dst_rect.width = lb.content_w;
    dst_rect.height = lb.content_h;

    rga_buffer_t pat{};
    im_rect pat_rect{};
    const IM_STATUS st =
        improcess(src, dst_buf, pat, src_rect, dst_rect, pat_rect, IM_SYNC);
    if (st != IM_STATUS_SUCCESS) {
        // WARN, not ERROR: the CPU fallback keeps the frame alive; a persistent
        // RGA failure shows up as CPU load, not lost detections.
        RLOG_WARN("preproc", "rga improcess failed: %s", imStrError(st));
        return false;
    }
    return true;
}

} // namespace riposte
