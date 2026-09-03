#include "CameraIngest.h"

#include "IDetector.h" // Frame

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <thread>

#include "riposte/Clock.h"
#include "riposte/Log.h"
#include "riposte/Tunables.h"

namespace riposte {

// ---- SyntheticCamera ------------------------------------------------------
bool SyntheticCamera::open() {
    next_ns_ = mono_now_ns();
    return true;
}

void SyntheticCamera::fill(Frame& f, uint64_t mono_ns) const {
    f.mono_ns = mono_ns;
    f.width = w_;
    f.height = h_;
    f.data = buf_.data();
    f.stride = static_cast<size_t>(w_);
    f.fourcc = 0x3231564E; // 'NV12'
}

bool SyntheticCamera::grab(Frame& f, int timeout_ms) {
    // Pace to the seeker frame rate using CLOCK_MONOTONIC so SIL exercises the
    // real loop timing.
    const uint64_t period = tun::FRAME_PERIOD_NS;
    const uint64_t now = mono_now_ns();
    uint64_t due = next_ns_ + period;
    if (due <= now) {
        due = now; // fell behind; resync
    } else {
        const uint64_t timeout_ns = static_cast<uint64_t>(timeout_ms) * 1'000'000ULL;
        if (due - now > timeout_ns) {
            // Non-blocking caller (timeout 0): model a BUFFERED camera — the
            // latest produced frame stays available with its ORIGINAL stamp,
            // exactly like the newest buffer of a V4L2 ring. Only the very
            // first call (nothing produced yet) has no frame to hand out.
            if (timeout_ms == 0 && last_ns_ != 0U) {
                fill(f, last_ns_);
                return true;
            }
            // Timeout contract: the next frame is beyond this call's budget, so
            // wait out the timeout and report no frame WITHOUT advancing the
            // schedule (else short-timeout callers push next_ns_ into the
            // future and burn a full timeout on every call).
            std::this_thread::sleep_for(std::chrono::nanoseconds(timeout_ns));
            return false;
        }
        std::this_thread::sleep_for(std::chrono::nanoseconds(due - now));
    }
    next_ns_ = due; // advance only when a frame is actually returned
    last_ns_ = mono_now_ns();
    fill(f, last_ns_);
    return true;
}

// ---- V4L2Camera (target only) ---------------------------------------------
#ifdef RIPOSTE_WITH_V4L2
} // namespace riposte

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cmath>
#include <utility>
#include <vector>

namespace riposte {
namespace {
// EINTR-retrying ioctl wrapper (V4L2 calls are restartable).
int xioctl(int fd, unsigned long req, void* arg) {
    int r = 0;
    do {
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg) — ioctl is the V4L2 API
        r = ::ioctl(fd, req, arg);
    } while (r < 0 && errno == EINTR);
    return r;
}

// Chroma-subsample a packed 4:2:2 frame (YUYV or UYVY) to planar NV12: copy the
// Y samples, and take one U/V pair per 2x2 block from the even rows (4:2:2->4:2:0).
// Used only for UVC sensors; MIPI/ISP delivers NV12 natively (zero-copy).
void yuv422_to_nv12(const uint8_t* src, size_t src_stride, int w, int h, bool uyvy,
                    uint8_t* dst) {
    uint8_t* y_plane = dst;
    uint8_t* uv_plane = dst + (static_cast<size_t>(w) * static_cast<size_t>(h));
    for (int row = 0; row < h; ++row) {
        const uint8_t* s = src + (static_cast<size_t>(row) * src_stride);
        uint8_t* yr = y_plane + (static_cast<size_t>(row) * static_cast<size_t>(w));
        uint8_t* uvr = uv_plane + (static_cast<size_t>(row / 2) * static_cast<size_t>(w));
        const bool even = (row & 1) == 0;
        for (int col = 0; col < w; col += 2) {
            const uint8_t b0 = s[0];
            const uint8_t b1 = s[1];
            const uint8_t b2 = s[2];
            const uint8_t b3 = s[3];
            yr[col] = uyvy ? b1 : b0;
            yr[col + 1] = uyvy ? b3 : b2;
            if (even) {
                uvr[col] = uyvy ? b0 : b1;     // U
                uvr[col + 1] = uyvy ? b2 : b3; // V
            }
            s += 4;
        }
    }
}
} // namespace

// Full mmap-streaming V4L2 capture. open(): QUERYCAP -> S_FMT (NV12>YUYV>UYVY,
// accept on read-back) -> REQBUFS(MMAP) -> QUERYBUF+mmap -> QBUF -> STREAMON.
// grab(): requeue the previously handed-out buffer, poll(POLLIN), then DQBUF-drain
// keeping only the newest (latest-wins). mono_ns is stamped from CLOCK_MONOTONIC
// so the whole pipeline shares one time base.
struct V4L2Camera::Impl {
    explicit Impl(V4L2Camera::Params p) : params(std::move(p)) {}
    ~Impl() { stop(); }
    Impl(const Impl&) = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&) = delete;
    Impl& operator=(Impl&&) = delete;

    bool start();
    void stop();
    bool grab(Frame& f, int timeout_ms);

    uint32_t buf_type() const {
        return mplane ? static_cast<uint32_t>(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
                      : static_cast<uint32_t>(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    }
    bool negotiate();
    void set_frame_rate(); // VIDIOC_S_PARM request + G_PARM verify (P2-05)
    bool init_buffers();
    bool qbuf(int index) const;
    // Dequeued index, or -1 (EAGAIN / error / short / wild-index frame —
    // P1-05 boundary validation lives here). ts_ns is the driver's
    // CLOCK_MONOTONIC capture timestamp, or 0 when unavailable; data_offset is
    // the validated payload start within the mapped buffer (mplane).
    int dqbuf(uint64_t& ts_ns, uint32_t& data_offset) const;
    void fill(Frame& f, int index, uint64_t driver_ns, uint32_t data_offset);

    struct Buf {
        void* start = nullptr;
        size_t length = 0;
    };

    V4L2Camera::Params params;
    int fd = -1;
    bool mplane = false;
    bool streaming = false;
    bool native_nv12 = false;
    int width = 0;
    int height = 0;
    size_t stride = 0;
    uint32_t pixfmt = 0;
    std::vector<Buf> bufs;
    int held = -1;             // buffer handed to the caller (requeue next grab)
    std::vector<uint8_t> nv12; // conversion target (empty when native NV12)
};

// The V4L2 uAPI is built on tagged unions (v4l2_format.fmt.{pix,pix_mp},
// v4l2_buffer.m.{planes,offset}); accessing them is unavoidable and the tag is
// carried by buf_type()/mplane. Suppress pro-type-union-access for this region
// (documented deviation, SAD-001 §10.1).
// NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
bool V4L2Camera::Impl::negotiate() {
    const std::array<uint32_t, 3> cands = {static_cast<uint32_t>(V4L2_PIX_FMT_NV12),
                                           static_cast<uint32_t>(V4L2_PIX_FMT_YUYV),
                                           static_cast<uint32_t>(V4L2_PIX_FMT_UYVY)};
    for (const uint32_t fourcc : cands) {
        v4l2_format fmt{};
        fmt.type = buf_type();
        if (mplane) {
            fmt.fmt.pix_mp.width = static_cast<uint32_t>(params.width);
            fmt.fmt.pix_mp.height = static_cast<uint32_t>(params.height);
            fmt.fmt.pix_mp.pixelformat = fourcc;
            fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
            fmt.fmt.pix_mp.num_planes = 1;
        } else {
            fmt.fmt.pix.width = static_cast<uint32_t>(params.width);
            fmt.fmt.pix.height = static_cast<uint32_t>(params.height);
            fmt.fmt.pix.pixelformat = fourcc;
            fmt.fmt.pix.field = V4L2_FIELD_NONE;
        }
        if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            continue;
        }
        // Accept whatever supported format the driver actually set (it may
        // substitute, e.g. a UVC cam ignoring NV12 and returning YUYV).
        const uint32_t got =
            mplane ? fmt.fmt.pix_mp.pixelformat : fmt.fmt.pix.pixelformat;
        if (got != V4L2_PIX_FMT_NV12 && got != V4L2_PIX_FMT_YUYV &&
            got != V4L2_PIX_FMT_UYVY) {
            continue; // MJPG or similar: unusable without a decoder
        }
        width = static_cast<int>(mplane ? fmt.fmt.pix_mp.width : fmt.fmt.pix.width);
        height = static_cast<int>(mplane ? fmt.fmt.pix_mp.height : fmt.fmt.pix.height);
        stride =
            mplane ? fmt.fmt.pix_mp.plane_fmt[0].bytesperline : fmt.fmt.pix.bytesperline;
        if (stride == 0) {
            // Driver reported no stride: NV12's Y plane is `width` bytes/row,
            // but packed 4:2:2 (YUYV/UYVY) carries 2 bytes per pixel per row.
            stride = static_cast<size_t>(width) * ((got == V4L2_PIX_FMT_NV12) ? 1U : 2U);
        }
        // Fail closed on unusable geometry (P1-05): odd dimensions break the
        // 4:2:0/4:2:2 subsampling maths everywhere downstream, and a stride
        // narrower than a row cannot hold the image it claims to.
        if (!v4l2_geometry_ok(got == V4L2_PIX_FMT_NV12, width, height, stride)) {
            RLOG_ERROR("cam", "V4L2 %s: unusable geometry %dx%d stride=%zu",
                       params.device.c_str(), width, height, stride);
            continue; // try the next format candidate; open fails if none fit
        }
        pixfmt = got;
        native_nv12 = (got == V4L2_PIX_FMT_NV12);
        if (!native_nv12) {
            nv12.assign(
                static_cast<size_t>(width) * static_cast<size_t>(height) * 3U / 2U, 0);
        }
        RLOG_INFO(
            "cam", "V4L2 %s: %dx%d fmt=%c%c%c%c%s", params.device.c_str(), width, height,
            static_cast<char>(pixfmt & 0xFFU), static_cast<char>((pixfmt >> 8U) & 0xFFU),
            static_cast<char>((pixfmt >> 16U) & 0xFFU),
            static_cast<char>((pixfmt >> 24U) & 0xFFU), native_nv12 ? "" : " (->NV12)");
        return true;
    }
    RLOG_ERROR("cam", "V4L2 %s: no supported format (NV12/YUYV/UYVY)",
               params.device.c_str());
    return false;
}

bool V4L2Camera::Impl::init_buffers() {
    v4l2_requestbuffers req{};
    req.count = static_cast<uint32_t>(params.num_buffers);
    req.type = buf_type();
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        RLOG_ERROR("cam", "VIDIOC_REQBUFS failed (%u granted)", req.count);
        return false;
    }
    bufs.resize(req.count);
    for (uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        std::array<v4l2_plane, VIDEO_MAX_PLANES> planes{};
        buf.type = buf_type();
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (mplane) {
            buf.m.planes = planes.data();
            buf.length = 1;
        }
        if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            RLOG_ERROR("cam", "VIDIOC_QUERYBUF[%u] failed", i);
            return false;
        }
        const size_t len = mplane ? planes.at(0).length : buf.length;
        const off_t off =
            static_cast<off_t>(mplane ? planes.at(0).m.mem_offset : buf.m.offset);
        void* p = ::mmap(nullptr, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, off);
        if (p == MAP_FAILED) {
            RLOG_ERROR("cam", "mmap[%u] failed", i);
            return false;
        }
        bufs.at(i) = Buf{p, len};
    }
    for (uint32_t i = 0; i < req.count; ++i) {
        if (!qbuf(static_cast<int>(i))) {
            return false;
        }
    }
    uint32_t type = buf_type();
    if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        RLOG_ERROR("cam", "VIDIOC_STREAMON failed");
        return false;
    }
    streaming = true;
    return true;
}

bool V4L2Camera::Impl::qbuf(int index) const {
    v4l2_buffer buf{};
    std::array<v4l2_plane, VIDEO_MAX_PLANES> planes{};
    buf.type = buf_type();
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = static_cast<uint32_t>(index);
    if (mplane) {
        buf.m.planes = planes.data();
        buf.length = 1;
    }
    if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        RLOG_WARN("cam", "VIDIOC_QBUF[%d] failed", index);
        return false;
    }
    return true;
}

int V4L2Camera::Impl::dqbuf(uint64_t& ts_ns, uint32_t& data_offset) const {
    v4l2_buffer buf{};
    std::array<v4l2_plane, VIDEO_MAX_PLANES> planes{};
    buf.type = buf_type();
    buf.memory = V4L2_MEMORY_MMAP;
    if (mplane) {
        buf.m.planes = planes.data();
        buf.length = 1;
    }
    if (xioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        return -1; // EAGAIN (queue drained) or error
    }
    // Driver metadata is untrusted (P1-05). An index outside OUR ring cannot
    // even be requeued safely — drop it; it was never one of our buffers.
    if (buf.index >= bufs.size()) {
        RLOG_WARN("cam", "DQBUF returned wild index %u (have %zu buffers) — dropped",
                  buf.index, bufs.size());
        return -1;
    }
    const int index = static_cast<int>(buf.index);
    const uint32_t used = mplane ? planes.at(0).bytesused : buf.bytesused;
    data_offset = mplane ? planes.at(0).data_offset : 0U;
    // Beyond the error flag, the payload must actually HOLD a whole frame of
    // the negotiated geometry — a short buffer would send the NV12 conversion
    // or the consumers reading past what the driver filled (P1-05).
    const bool bad =
        (buf.flags & V4L2_BUF_FLAG_ERROR) != 0 ||
        !v4l2_payload_ok(used, data_offset, native_nv12, height, stride) ||
        static_cast<std::size_t>(used) > bufs.at(static_cast<size_t>(index)).length;
    if (bad) {
        (void)qbuf(index); // corrupt/short frame: return it and report none
        return -1;
    }
    // Driver capture timestamp: usable as Frame.mono_ns only when the driver
    // stamps CLOCK_MONOTONIC (the pipeline's shared time base); else 0 tells
    // fill() to fall back to mono_now_ns().
    ts_ns = 0;
    if ((buf.flags & V4L2_BUF_FLAG_TIMESTAMP_MASK) == V4L2_BUF_FLAG_TIMESTAMP_MONOTONIC) {
        ts_ns = (static_cast<uint64_t>(buf.timestamp.tv_sec) * 1'000'000'000ULL) +
                (static_cast<uint64_t>(buf.timestamp.tv_usec) * 1'000ULL);
    }
    return index;
}
// NOLINTEND(cppcoreguidelines-pro-type-union-access)

void V4L2Camera::Impl::fill(Frame& f, int index, uint64_t driver_ns,
                            uint32_t data_offset) {
    // Prefer the driver's CLOCK_MONOTONIC capture timestamp so the stale-frame
    // guard measures true frame age; fall back to "now" when unavailable.
    f.mono_ns = (driver_ns != 0) ? driver_ns : mono_now_ns();
    f.width = width;
    f.height = height;
    f.fourcc = 0x3231564E; // 'NV12'
    // data_offset: dqbuf validated it against bytesused; the payload starts
    // there, not at the mapping base (mplane drivers may prepend metadata).
    const auto* raw =
        static_cast<const uint8_t*>(bufs.at(static_cast<size_t>(index)).start) +
        data_offset;
    if (native_nv12) {
        f.data = raw;
        f.stride = stride;
    } else {
        yuv422_to_nv12(raw, stride, width, height, pixfmt == V4L2_PIX_FMT_UYVY,
                       nv12.data());
        f.data = nv12.data();
        f.stride = static_cast<size_t>(width);
    }
}

bool V4L2Camera::Impl::start() {
    fd = ::open(params.device.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        char eb[64];
        RLOG_ERROR("cam", "open %s failed: %s", params.device.c_str(),
                   log::errno_str(errno, eb, sizeof(eb)));
        return false;
    }
    v4l2_capability cap{};
    if (xioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        RLOG_ERROR("cam", "VIDIOC_QUERYCAP failed");
        stop();
        return false;
    }
    // device_caps (when advertised) describes THIS node; capabilities describes
    // the whole device and can carry sibling nodes' mplane bit.
    const uint32_t caps = ((cap.capabilities & V4L2_CAP_DEVICE_CAPS) != 0U)
                              ? cap.device_caps
                              : cap.capabilities;
    mplane = (caps & V4L2_CAP_VIDEO_CAPTURE_MPLANE) != 0;
    if (!negotiate()) {
        stop();
        return false;
    }
    set_frame_rate(); // best-effort: request the rate and report what was granted
    if (!init_buffers()) {
        stop();
        return false;
    }
    return true;
}

// Requests params.fps via VIDIOC_S_PARM and reads back what the driver granted
// (P2-05). The frame-count policies downstream (Tracker miss budget, the R-6
// window, the R-7 recheck cadence) are calibrated to SEEKER_FRAME_HZ, so a
// silently different capture rate shifts every one of their time meanings; the
// old code stored params.fps and never told the driver. A mismatch is logged
// loudly here so bring-up catches it. (Converting those policies to
// elapsed-time is the fuller fix and stays a bring-up item — REQ §4.1.)
void V4L2Camera::Impl::set_frame_rate() {
    if (params.fps <= 0) {
        return;
    }
    v4l2_streamparm sp{};
    sp.type = buf_type();
    // v4l2_streamparm.parm.capture (v4l2_captureparm) covers both single- and
    // multi-plane capture types.
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) — V4L2 tagged union
    v4l2_fract& tpf = sp.parm.capture.timeperframe;
    tpf.numerator = 1;
    tpf.denominator = static_cast<uint32_t>(params.fps);
    if (xioctl(fd, VIDIOC_S_PARM, &sp) < 0) {
        RLOG_WARN("cam",
                  "VIDIOC_S_PARM unsupported — capture rate is the driver's "
                  "default, NOT %d fps (frame-count timing may be off)",
                  params.fps);
        return;
    }
    // Read back the GRANTED period; drivers clamp to a supported rate.
    v4l2_streamparm got{};
    got.type = buf_type();
    if (xioctl(fd, VIDIOC_G_PARM, &got) < 0) {
        return;
    }
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-union-access) — V4L2 tagged union
    const v4l2_fract& g = got.parm.capture.timeperframe;
    if (g.numerator == 0 || g.denominator == 0) {
        return; // driver does not report a rate
    }
    const double granted =
        static_cast<double>(g.denominator) / static_cast<double>(g.numerator);
    if (std::abs(granted - static_cast<double>(params.fps)) > 0.5) {
        RLOG_WARN("cam",
                  "capture rate granted %.1f fps != requested %d fps — frame-count "
                  "timing (miss budget / confirm window / recheck) is calibrated to "
                  "%d and will drift",
                  granted, params.fps, tun::SEEKER_FRAME_HZ);
    } else {
        RLOG_INFO("cam", "capture rate %.1f fps (requested %d)", granted, params.fps);
    }
}

void V4L2Camera::Impl::stop() {
    if (fd < 0) {
        return;
    }
    if (streaming) {
        uint32_t type = buf_type();
        (void)xioctl(fd, VIDIOC_STREAMOFF, &type);
        streaming = false;
    }
    for (auto& b : bufs) {
        if (b.start != nullptr && b.length > 0) {
            (void)::munmap(b.start, b.length);
        }
    }
    bufs.clear();
    (void)::close(fd);
    fd = -1;
    held = -1;
}

bool V4L2Camera::Impl::grab(Frame& f, int timeout_ms) {
    if (fd < 0 || !streaming) {
        return false;
    }
    if (held >= 0) { // the frame from the last grab() is now stale
        (void)qbuf(held);
        held = -1;
    }
    // NOLINTBEGIN(misc-include-cleaner) — pollfd/POLLIN/poll are from <poll.h>
    pollfd pfd{fd, POLLIN, 0};
    if (::poll(&pfd, 1, timeout_ms) <= 0 || (pfd.revents & POLLIN) == 0) {
        return false; // timeout / error
    }
    // NOLINTEND(misc-include-cleaner)
    int newest = -1;
    uint64_t newest_ns = 0;
    uint32_t newest_off = 0;
    uint64_t ts_ns = 0;
    uint32_t off = 0;
    for (int idx = dqbuf(ts_ns, off); idx >= 0; idx = dqbuf(ts_ns, off)) {
        if (newest >= 0) {
            (void)qbuf(newest); // keep only the newest (latest-wins)
        }
        newest = idx;
        newest_ns = ts_ns;
        newest_off = off;
    }
    if (newest < 0) {
        return false;
    }
    held = newest;
    fill(f, newest, newest_ns, newest_off);
    return true;
}

V4L2Camera::V4L2Camera(const Params& params) : impl_(new Impl(params)) {}
V4L2Camera::~V4L2Camera() {
    delete impl_;
}
bool V4L2Camera::open() {
    return impl_->start();
}
bool V4L2Camera::grab(Frame& f, int timeout_ms) {
    return impl_->grab(f, timeout_ms);
}
int V4L2Camera::width() const {
    return impl_->width;
}
int V4L2Camera::height() const {
    return impl_->height;
}
#else
struct V4L2Camera::Impl {};
V4L2Camera::V4L2Camera(const Params& /*params*/) {}
V4L2Camera::~V4L2Camera() = default;
bool V4L2Camera::open() {
    RLOG_ERROR("cam", "built without RIPOSTE_WITH_V4L2");
    return false;
}
bool V4L2Camera::grab(Frame& /*f*/, int /*timeout_ms*/) {
    return false;
}
int V4L2Camera::width() const {
    return 0; // open() always fails in this build
}
int V4L2Camera::height() const {
    return 0;
}
#endif

} // namespace riposte
