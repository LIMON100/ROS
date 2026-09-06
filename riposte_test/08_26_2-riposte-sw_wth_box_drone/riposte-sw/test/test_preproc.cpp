// CPU letterbox preprocessing tests (SDD §4.4.3).
//
// This is the accuracy REFERENCE for the model input: the RGA production path
// is validated against it at bring-up, so an error here silently becomes an
// error in what the network sees on the target too. Colour checks use the
// float BT.601 studio-swing equations independently re-derived per test (not
// the implementation's own helpers) with a small tolerance.
#include "IDetector.h"
#include "ModelIo.h"
#include "Preproc.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using namespace riposte;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
int checks = 0;

#define CHECK(c)                                                    \
    do {                                                            \
        ++checks;                                                   \
        if (!(c)) {                                                 \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            return 1;                                               \
        }                                                           \
    } while (0)

constexpr uint32_t FOURCC_NV12 = 0x3231564E;
constexpr uint8_t PAD = 114;

// An NV12 image owning its bytes, with Frame accessors.
struct Nv12 {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> bytes;

    static Nv12 solid(int w, int h, uint8_t y, uint8_t u, uint8_t v) {
        Nv12 img;
        img.w = w;
        img.h = h;
        img.bytes.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 3U / 2U, y);
        uint8_t* uv =
            img.bytes.data() + (static_cast<size_t>(w) * static_cast<size_t>(h));
        for (int i = 0; i < (w / 2) * (h / 2); ++i) {
            uv[static_cast<size_t>(i) * 2U] = u;
            uv[(static_cast<size_t>(i) * 2U) + 1U] = v;
        }
        return img;
    }

    Frame frame() const {
        Frame f{};
        f.width = w;
        f.height = h;
        f.data = bytes.data();
        f.stride = static_cast<size_t>(w);
        f.fourcc = FOURCC_NV12;
        return f;
    }
};

std::vector<uint8_t> padded_dst(int model_size) {
    return std::vector<uint8_t>(
        static_cast<size_t>(model_size) * static_cast<size_t>(model_size) * 3U, PAD);
}

const uint8_t* px(const std::vector<uint8_t>& dst, int model_size, int x, int y) {
    return dst.data() + (((static_cast<size_t>(y) * static_cast<size_t>(model_size)) +
                          static_cast<size_t>(x)) *
                         3U);
}

// Independent BT.601 studio-swing reference (float, unclamped inputs).
void bt601(uint8_t y, uint8_t u, uint8_t v, float& r, float& g, float& b) {
    const float c = static_cast<float>(y) - 16.F;
    const float d = static_cast<float>(u) - 128.F;
    const float e = static_cast<float>(v) - 128.F;
    r = (1.164F * c) + (1.596F * e);
    g = (1.164F * c) - (0.392F * d) - (0.813F * e);
    b = (1.164F * c) + (2.017F * d);
}

bool rgb_near(const uint8_t* p, float r, float g, float b, float tol = 3.F) {
    const auto clip = [](float v) { return std::min(255.F, std::max(0.F, v)); };
    return std::fabs(static_cast<float>(p[0]) - clip(r)) <= tol &&
           std::fabs(static_cast<float>(p[1]) - clip(g)) <= tol &&
           std::fabs(static_cast<float>(p[2]) - clip(b)) <= tol;
}

// ---------------------------------------------------------------- colour --

int test_solid_gray_converts_correctly() {
    const Nv12 img = Nv12::solid(64, 64, 128, 128, 128);
    const Letterbox lb = letterbox_fit(64, 64, 64);
    auto dst = padded_dst(64);
    CHECK(letterbox_nv12_rgb888(img.frame(), lb, dst.data(), dst.size()));
    float r = 0.F;
    float g = 0.F;
    float b = 0.F;
    bt601(128, 128, 128, r, g, b);
    CHECK(rgb_near(px(dst, 64, 32, 32), r, g, b));
    CHECK(rgb_near(px(dst, 64, 0, 0), r, g, b)); // corners too (edge clamping)
    CHECK(rgb_near(px(dst, 64, 63, 63), r, g, b));
    return 0;
}

int test_solid_primaries_convert_correctly() {
    // Limited-range YUV encodings of pure red / green / blue.
    const struct {
        uint8_t y, u, v;
    } cases[] = {{81, 90, 240} /*red*/, {145, 54, 34} /*green*/, {41, 240, 110} /*blue*/};
    for (const auto& c : cases) {
        const Nv12 img = Nv12::solid(32, 32, c.y, c.u, c.v);
        const Letterbox lb = letterbox_fit(32, 32, 32);
        auto dst = padded_dst(32);
        CHECK(letterbox_nv12_rgb888(img.frame(), lb, dst.data(), dst.size()));
        float r = 0.F;
        float g = 0.F;
        float b = 0.F;
        bt601(c.y, c.u, c.v, r, g, b);
        CHECK(rgb_near(px(dst, 32, 16, 16), r, g, b));
    }
    return 0;
}

// ------------------------------------------------------------- placement --

int test_content_lands_in_the_letterbox_rect_and_padding_survives() {
    // The 3x3-tile shape that motivated the letterbox: 426x240 -> 640x640 puts
    // content rows at [pad_y, pad_y + 361) and full-width columns.
    const Nv12 img = Nv12::solid(426, 240, 128, 128, 128);
    const Letterbox lb = letterbox_fit(426, 240, 640);
    auto dst = padded_dst(640);
    CHECK(letterbox_nv12_rgb888(img.frame(), lb, dst.data(), dst.size()));

    float r = 0.F;
    float g = 0.F;
    float b = 0.F;
    bt601(128, 128, 128, r, g, b);
    CHECK(rgb_near(px(dst, 640, 320, lb.pad_y), r, g, b)); // first row in
    CHECK(
        rgb_near(px(dst, 640, 320, lb.pad_y + lb.content_h - 1), r, g, b)); // last row in
    // One row outside on each side must still hold the caller's pad fill.
    const uint8_t* above = px(dst, 640, 320, lb.pad_y - 1);
    const uint8_t* below = px(dst, 640, 320, lb.pad_y + lb.content_h);
    CHECK(above[0] == PAD && above[1] == PAD && above[2] == PAD);
    CHECK(below[0] == PAD && below[1] == PAD && below[2] == PAD);
    return 0;
}

int test_tall_source_pads_horizontally() {
    const Nv12 img = Nv12::solid(240, 426, 128, 128, 128);
    const Letterbox lb = letterbox_fit(240, 426, 640);
    auto dst = padded_dst(640);
    CHECK(letterbox_nv12_rgb888(img.frame(), lb, dst.data(), dst.size()));
    const uint8_t* left = px(dst, 640, lb.pad_x - 1, 320);
    const uint8_t* right = px(dst, 640, lb.pad_x + lb.content_w, 320);
    CHECK(left[0] == PAD && right[0] == PAD);
    float r = 0.F;
    float g = 0.F;
    float b = 0.F;
    bt601(128, 128, 128, r, g, b);
    CHECK(rgb_near(px(dst, 640, lb.pad_x, 320), r, g, b));
    return 0;
}

// -------------------------------------------------------------- fidelity --

int test_horizontal_gradient_stays_monotonic() {
    // A luma ramp must come out non-decreasing after bilinear scaling — any
    // inversion means the sampler is mixing coordinates.
    Nv12 img = Nv12::solid(128, 64, 0, 128, 128);
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            img.bytes[(static_cast<size_t>(y) * 128U) + static_cast<size_t>(x)] =
                static_cast<uint8_t>(16 + x); // 16..143, inside studio swing
        }
    }
    const Letterbox lb = letterbox_fit(128, 64, 96);
    auto dst = padded_dst(96);
    CHECK(letterbox_nv12_rgb888(img.frame(), lb, dst.data(), dst.size()));
    const int row = lb.pad_y + (lb.content_h / 2);
    for (int x = 1; x < lb.content_w; ++x) {
        CHECK(px(dst, 96, x, row)[1] + 1 >= px(dst, 96, x - 1, row)[1]); // G channel
    }
    // And it must actually span the ramp, not flatten it.
    CHECK(px(dst, 96, lb.content_w - 1, row)[1] > px(dst, 96, 0, row)[1] + 100);
    return 0;
}

int test_respects_row_stride() {
    // Same image content, stride > width: junk bytes past each row must not
    // leak into the output.
    const int w = 32;
    const int h = 16;
    const size_t stride = 48;
    std::vector<uint8_t> bytes(stride * (static_cast<size_t>(h) * 3U / 2U), 0xEE);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            bytes[(static_cast<size_t>(y) * stride) + static_cast<size_t>(x)] = 128;
        }
    }
    for (int y = 0; y < h / 2; ++y) {
        for (int x = 0; x < w; ++x) {
            bytes[(static_cast<size_t>(h) * stride) + (static_cast<size_t>(y) * stride) +
                  static_cast<size_t>(x)] = 128;
        }
    }
    Frame f{};
    f.width = w;
    f.height = h;
    f.data = bytes.data();
    f.stride = stride;
    f.fourcc = FOURCC_NV12;

    const Letterbox lb = letterbox_fit(w, h, 32);
    auto dst = padded_dst(32);
    CHECK(letterbox_nv12_rgb888(f, lb, dst.data(), dst.size()));
    float r = 0.F;
    float g = 0.F;
    float b = 0.F;
    bt601(128, 128, 128, r, g, b);
    CHECK(rgb_near(px(dst, 32, 16, lb.pad_y + 4), r, g, b));
    return 0;
}

// ------------------------------------------------- stretch resize (ReID) --

int test_resize_solid_color_and_size() {
    const Nv12 img = Nv12::solid(64, 32, 81, 90, 240); // pure red
    std::vector<uint8_t> dst(static_cast<size_t>(16) * 8 * 3, 0);
    Frame const f = img.frame();
    CHECK(resize_nv12_rgb888(f, dst.data(), dst.size(), 16, 8));
    float r = 0.F;
    float g = 0.F;
    float b = 0.F;
    bt601(81, 90, 240, r, g, b);
    constexpr size_t CENTRE_OFF = ((size_t{4} * 16) + 8) * 3;
    CHECK(rgb_near(dst.data() + CENTRE_OFF, r, g, b)); // centre
    CHECK(rgb_near(dst.data(), r, g, b));              // corner
    return 0;
}

int test_resize_gradient_stays_monotonic() {
    Nv12 img = Nv12::solid(64, 32, 0, 128, 128);
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            img.bytes[(static_cast<size_t>(y) * 64U) + static_cast<size_t>(x)] =
                static_cast<uint8_t>(16 + (2 * x)); // 16..142 luma ramp
        }
    }
    std::vector<uint8_t> dst(static_cast<size_t>(24) * 12 * 3, 0);
    CHECK(resize_nv12_rgb888(img.frame(), dst.data(), dst.size(), 24, 12));
    const uint8_t* row = dst.data() + (static_cast<size_t>(6) * 24U * 3U);
    for (int x = 1; x < 24; ++x) {
        CHECK(row[(x * 3) + 1] + 1 >= row[((x - 1) * 3) + 1]);
    }
    CHECK(row[(23 * 3) + 1] > row[1] + 80); // spans the ramp, not flattened
    return 0;
}

int test_resize_rejects_contract_violations() {
    const Nv12 img = Nv12::solid(32, 16, 128, 128, 128);
    std::vector<uint8_t> dst(static_cast<size_t>(8) * 8 * 3, 7);
    Frame bad = img.frame();
    bad.fourcc = 0x56595559; // 'YUYV'
    CHECK(!resize_nv12_rgb888(bad, dst.data(), dst.size(), 8, 8));
    CHECK(!resize_nv12_rgb888(img.frame(), dst.data(), dst.size() - 1, 8, 8));
    CHECK(!resize_nv12_rgb888(img.frame(), dst.data(), dst.size(), 0, 8));
    for (const uint8_t v : dst) {
        if (v != 7) {
            CHECK(false); // rejected calls wrote nothing
        }
    }
    return 0;
}

// ------------------------------------------------------------- contract --

int test_rejects_contract_violations() {
    const Nv12 img = Nv12::solid(64, 64, 128, 128, 128);
    const Letterbox lb = letterbox_fit(64, 64, 64);
    auto dst = padded_dst(64);

    Frame bad = img.frame();
    bad.fourcc = 0x56595559; // 'YUYV'
    CHECK(!letterbox_nv12_rgb888(bad, lb, dst.data(), dst.size()));

    bad = img.frame();
    bad.data = nullptr;
    CHECK(!letterbox_nv12_rgb888(bad, lb, dst.data(), dst.size()));

    // Letterbox computed for a different image size.
    const Letterbox other = letterbox_fit(128, 128, 64);
    CHECK(!letterbox_nv12_rgb888(img.frame(), other, dst.data(), dst.size()));

    // Undersized destination.
    CHECK(!letterbox_nv12_rgb888(img.frame(), lb, dst.data(), dst.size() - 1));

    // Invalid letterbox.
    const Letterbox invalid{};
    CHECK(!letterbox_nv12_rgb888(img.frame(), invalid, dst.data(), dst.size()));

    // Nothing may have been written by any rejected call.
    for (const uint8_t v : dst) {
        if (v != PAD) {
            CHECK(false);
        }
    }
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_solid_gray_converts_correctly();
    rc = rc != 0 ? rc : test_solid_primaries_convert_correctly();
    rc = rc != 0 ? rc : test_content_lands_in_the_letterbox_rect_and_padding_survives();
    rc = rc != 0 ? rc : test_tall_source_pads_horizontally();
    rc = rc != 0 ? rc : test_horizontal_gradient_stays_monotonic();
    rc = rc != 0 ? rc : test_respects_row_stride();
    rc = rc != 0 ? rc : test_resize_solid_color_and_size();
    rc = rc != 0 ? rc : test_resize_gradient_stays_monotonic();
    rc = rc != 0 ? rc : test_resize_rejects_contract_violations();
    rc = rc != 0 ? rc : test_rejects_contract_violations();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_preproc: %d checks passed\n", checks);
    return 0;
}
