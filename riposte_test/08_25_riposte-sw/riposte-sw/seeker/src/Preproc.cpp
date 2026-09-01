#include "Preproc.h"

#include "IDetector.h"
#include "ModelIo.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace riposte {

namespace {

// NV12 fourcc, matching what CameraIngest negotiates and crop_nv12 produces.
constexpr uint32_t FOURCC_NV12 = 0x3231564E; // 'N','V','1','2'

uint8_t clamp_u8(float v) {
    return static_cast<uint8_t>(std::lround(std::min(255.F, std::max(0.F, v))));
}

// Bilinear sample of one byte plane. Coordinates are in the plane's own pixel
// grid; out-of-range positions clamp to the edge (the frame has no context
// beyond it, and extrapolating would invent luma at the borders).
float sample_bilinear(const uint8_t* plane, int w, int h, std::size_t stride, float x,
                      float y) {
    x = std::min(static_cast<float>(w - 1), std::max(0.F, x));
    y = std::min(static_cast<float>(h - 1), std::max(0.F, y));
    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const float p00 = plane[(static_cast<std::size_t>(y0) * stride) + x0];
    const float p10 = plane[(static_cast<std::size_t>(y0) * stride) + x1];
    const float p01 = plane[(static_cast<std::size_t>(y1) * stride) + x0];
    const float p11 = plane[(static_cast<std::size_t>(y1) * stride) + x1];
    const float top = p00 + (fx * (p10 - p00));
    const float bot = p01 + (fx * (p11 - p01));
    return top + (fy * (bot - top));
}

// Interleaved-UV bilinear sample at chroma-grid coordinates: the UV plane is
// (w/2) pairs per row, so a "pixel" is a 2-byte pair and the byte column is
// pair index * 2 (+1 for V).
float sample_bilinear_uv(const uint8_t* uv, int pairs_w, int h, std::size_t stride,
                         float x, float y, int channel) {
    x = std::min(static_cast<float>(pairs_w - 1), std::max(0.F, x));
    y = std::min(static_cast<float>(h - 1), std::max(0.F, y));
    const int x0 = static_cast<int>(x);
    const int y0 = static_cast<int>(y);
    const int x1 = std::min(x0 + 1, pairs_w - 1);
    const int y1 = std::min(y0 + 1, h - 1);
    const float fx = x - static_cast<float>(x0);
    const float fy = y - static_cast<float>(y0);
    const auto at = [&](int px, int py) -> float {
        return uv[(static_cast<std::size_t>(py) * stride) +
                  (static_cast<std::size_t>(px) * 2U) +
                  static_cast<std::size_t>(channel)];
    };
    const float top = at(x0, y0) + (fx * (at(x1, y0) - at(x0, y0)));
    const float bot = at(x0, y1) + (fx * (at(x1, y1) - at(x0, y1)));
    return top + (fy * (bot - top));
}

} // namespace

bool resize_nv12_rgb888(const Frame& f, uint8_t* dst, std::size_t dst_size, int dst_w,
                        int dst_h) {
    if (f.data == nullptr || dst == nullptr || f.fourcc != FOURCC_NV12 || dst_w <= 0 ||
        dst_h <= 0 || f.width < 2 || f.height < 2) {
        return false;
    }
    const std::size_t need =
        static_cast<std::size_t>(dst_w) * static_cast<std::size_t>(dst_h) * 3U;
    if (dst_size < need) {
        return false;
    }
    const std::size_t stride =
        (f.stride != 0U) ? f.stride : static_cast<std::size_t>(f.width);
    const uint8_t* y_plane = f.data;
    const uint8_t* uv_plane = f.data + (static_cast<std::size_t>(f.height) * stride);
    const int uv_pairs_w = f.width / 2;
    const int uv_h = f.height / 2;
    const float sx_step = static_cast<float>(f.width) / static_cast<float>(dst_w);
    const float sy_step = static_cast<float>(f.height) / static_cast<float>(dst_h);

    uint8_t* row = dst;
    for (int dy = 0; dy < dst_h; ++dy) {
        const float sy = ((static_cast<float>(dy) + 0.5F) * sy_step) - 0.5F;
        for (int dx = 0; dx < dst_w; ++dx) {
            const float sx = ((static_cast<float>(dx) + 0.5F) * sx_step) - 0.5F;
            const float yv = sample_bilinear(y_plane, f.width, f.height, stride, sx, sy);
            const float uu = sample_bilinear_uv(uv_plane, uv_pairs_w, uv_h, stride,
                                                sx * 0.5F, sy * 0.5F, 0);
            const float vv = sample_bilinear_uv(uv_plane, uv_pairs_w, uv_h, stride,
                                                sx * 0.5F, sy * 0.5F, 1);
            const float c = yv - 16.F;
            const float d = uu - 128.F;
            const float e = vv - 128.F;
            row[0] = clamp_u8((1.164F * c) + (1.596F * e));
            row[1] = clamp_u8((1.164F * c) - (0.392F * d) - (0.813F * e));
            row[2] = clamp_u8((1.164F * c) + (2.017F * d));
            row += 3;
        }
    }
    return true;
}

bool letterbox_nv12_rgb888(const Frame& f, const Letterbox& lb, uint8_t* dst,
                           std::size_t dst_size) {
    if (f.data == nullptr || dst == nullptr || f.fourcc != FOURCC_NV12 || !lb.valid() ||
        lb.scale <= 0.F) {
        return false;
    }
    // The letterbox must have been computed for THIS frame — a mismatch means
    // the caller is scaling one image with another's geometry.
    if (lb.src_w != f.width || lb.src_h != f.height || f.width < 2 || f.height < 2) {
        return false;
    }
    const std::size_t need = static_cast<std::size_t>(lb.model_size) *
                             static_cast<std::size_t>(lb.model_size) * 3U;
    if (dst_size < need) {
        return false;
    }

    const std::size_t stride =
        (f.stride != 0U) ? f.stride : static_cast<std::size_t>(f.width);
    const uint8_t* y_plane = f.data;
    const uint8_t* uv_plane = f.data + (static_cast<std::size_t>(f.height) * stride);
    const int uv_pairs_w = f.width / 2;
    const int uv_h = f.height / 2;
    const float inv_scale = 1.F / lb.scale;

    for (int dy = 0; dy < lb.content_h; ++dy) {
        // Half-pixel-centre mapping: dst pixel centre -> src pixel centre, so
        // the content is sampled symmetrically instead of biased half a texel
        // toward the top-left.
        const float sy = ((static_cast<float>(dy) + 0.5F) * inv_scale) - 0.5F;
        uint8_t* row = dst + ((static_cast<std::size_t>(dy + lb.pad_y) *
                               static_cast<std::size_t>(lb.model_size)) +
                              static_cast<std::size_t>(lb.pad_x)) *
                                 3U;
        for (int dx = 0; dx < lb.content_w; ++dx) {
            const float sx = ((static_cast<float>(dx) + 0.5F) * inv_scale) - 0.5F;
            const float yv = sample_bilinear(y_plane, f.width, f.height, stride, sx, sy);
            // 4:2:0 chroma: one pair per 2x2 luma block, left-sited.
            const float uu = sample_bilinear_uv(uv_plane, uv_pairs_w, uv_h, stride,
                                                sx * 0.5F, sy * 0.5F, 0);
            const float vv = sample_bilinear_uv(uv_plane, uv_pairs_w, uv_h, stride,
                                                sx * 0.5F, sy * 0.5F, 1);
            // BT.601 studio swing (what V4L2 sensors and the ISP deliver).
            const float c = yv - 16.F;
            const float d = uu - 128.F;
            const float e = vv - 128.F;
            row[0] = clamp_u8((1.164F * c) + (1.596F * e));                // R
            row[1] = clamp_u8((1.164F * c) - (0.392F * d) - (0.813F * e)); // G
            row[2] = clamp_u8((1.164F * c) + (2.017F * d));                // B
            row += 3;
        }
    }
    return true;
}

} // namespace riposte
