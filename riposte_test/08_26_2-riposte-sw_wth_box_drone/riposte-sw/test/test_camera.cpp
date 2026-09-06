// V4L2 dequeue boundary predicates (SEEKER-SDD §4.1, P1-05): the driver's
// buffer metadata is untrusted input, and these pure predicates are the only
// thing standing between a short/malformed buffer and an OOB read in the NV12
// conversion. The device-facing dqbuf()/fill() consult them before any
// pointer arithmetic — the malformed matrix is pinned here on the host.
#include "CameraIngest.h"

#include <cstdint>
#include <cstdio>

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

int test_frame_bytes_required() {
    // NV12 (4:2:0): stride x h x 3/2. 1280x720 @ stride 1280 -> 1,382,400.
    CHECK(v4l2_frame_bytes_required(true, 720, 1280) == 1'382'400U);
    // Padded stride counts fully — the consumers walk stride-sized rows.
    CHECK(v4l2_frame_bytes_required(true, 720, 1536) == 1'658'880U);
    // Packed 4:2:2: bytesperline already carries 2 B/px -> stride x h.
    CHECK(v4l2_frame_bytes_required(false, 720, 2560) == 1'843'200U);
    // Degenerate geometry needs nothing (payload_ok rejects it separately).
    CHECK(v4l2_frame_bytes_required(true, 0, 1280) == 0U);
    return 0;
}

// G16.6 deviation: reject-matrix enumeration; one CHECK per malformed input.
// NOLINTNEXTLINE(readability-function-size)
int test_payload_ok_matrix() {
    const std::size_t nv12_need = v4l2_frame_bytes_required(true, 720, 1280);
    // Exact fit and oversize pass.
    CHECK(v4l2_payload_ok(static_cast<uint32_t>(nv12_need), 0, true, 720, 1280));
    CHECK(v4l2_payload_ok(static_cast<uint32_t>(nv12_need) + 64U, 0, true, 720, 1280));
    // One byte short: the frame is not all there — reject.
    CHECK(!v4l2_payload_ok(static_cast<uint32_t>(nv12_need) - 1U, 0, true, 720, 1280));
    // data_offset eats into the payload: full bytesused but offset payload
    // short of a frame — reject.
    CHECK(!v4l2_payload_ok(static_cast<uint32_t>(nv12_need), 64U, true, 720, 1280));
    // Offset accounted for when the buffer is big enough.
    CHECK(v4l2_payload_ok(static_cast<uint32_t>(nv12_need) + 64U, 64U, true, 720, 1280));
    // Nonsense metadata combinations.
    CHECK(!v4l2_payload_ok(0, 0, true, 720, 1280));        // empty
    CHECK(!v4l2_payload_ok(100U, 100U, true, 720, 1280));  // offset == used
    CHECK(!v4l2_payload_ok(200U, 300U, true, 720, 1280));  // offset > used
    CHECK(!v4l2_payload_ok(1'000'000U, 0, true, 0, 1280)); // zero height
    CHECK(!v4l2_payload_ok(1'000'000U, 0, true, 720, 0));  // zero stride
    // Packed 4:2:2 bound (stride 2 x width).
    const std::size_t yuyv_need = v4l2_frame_bytes_required(false, 720, 2560);
    CHECK(v4l2_payload_ok(static_cast<uint32_t>(yuyv_need), 0, false, 720, 2560));
    CHECK(!v4l2_payload_ok(static_cast<uint32_t>(yuyv_need) - 1U, 0, false, 720, 2560));
    return 0;
}

int test_geometry_ok() {
    CHECK(v4l2_geometry_ok(true, 1280, 720, 1280));  // nominal NV12
    CHECK(v4l2_geometry_ok(true, 1280, 720, 1536));  // padded stride fine
    CHECK(v4l2_geometry_ok(false, 1280, 720, 2560)); // packed 4:2:2 nominal
    // Odd dimensions break the 4:2:0/4:2:2 subsampling maths.
    CHECK(!v4l2_geometry_ok(true, 1281, 720, 1281));
    CHECK(!v4l2_geometry_ok(true, 1280, 719, 1280));
    // A stride narrower than one row cannot hold the image it claims.
    CHECK(!v4l2_geometry_ok(true, 1280, 720, 1279));
    CHECK(!v4l2_geometry_ok(false, 1280, 720, 1280)); // packed needs 2 x width
    // Degenerate sizes.
    CHECK(!v4l2_geometry_ok(true, 0, 720, 1280));
    CHECK(!v4l2_geometry_ok(true, 1280, 0, 1280));
    CHECK(!v4l2_geometry_ok(true, -1280, 720, 1280));
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_frame_bytes_required();
    rc = rc != 0 ? rc : test_payload_ok_matrix();
    rc = rc != 0 ? rc : test_geometry_ok();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_camera: %d checks passed\n", checks);
    return 0;
}
