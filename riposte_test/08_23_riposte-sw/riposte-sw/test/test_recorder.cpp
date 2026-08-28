// Recorder tests, two tiers (G11.2 / SEEKER-SDD §8.1):
//  - pure logic: disk-reclaim policy (plan_eviction), NV12 stride compaction
//    (compact_nv12) and the burn-in subtitle format (format_overlay_text) —
//    no filesystem, camera or encoder;
//  - integration (real ffmpeg, auto-SKIP when absent): segment file creation
//    and MP4 magic, segment rotation, and survival of an encoder that dies
//    mid-run (respawn, drop-not-crash).
#include "IDetector.h" // Frame
#include "VideoRecorder.h"

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <ios>
#include <string>
#include <utility>
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

constexpr double HIGH = 0.80; // evict once 80% used
constexpr double FREE = 0.10; // reclaim ~10% of capacity

// Ten equal 10-unit files, oldest first (names sort chronologically).
std::vector<RecFile> ten_files() {
    std::vector<RecFile> v;
    v.reserve(10);
    for (int i = 0; i < 10; ++i) {
        v.push_back({"riposte_2026070" + std::to_string(i) + ".mp4", 10});
    }
    return v;
}

int test_below_high_water_keeps_all() {
    // 79% used (< 80%): nothing is evicted.
    CHECK(plan_eviction(ten_files(), 1000, 790, HIGH, FREE) == 0);
    return 0;
}

int test_at_high_water_evicts_to_free_target() {
    // 80% used, capacity 1000 => reclaim >= 100 bytes. Files are 10 each => 10.
    CHECK(plan_eviction(ten_files(), 1000, 800, HIGH, FREE) == 10);
    return 0;
}

int test_evicts_oldest_first_minimum_needed() {
    // Reclaim target 100; files of 40 each => two files (80) is short, need three.
    const std::vector<RecFile> v = {
        {"a.mp4", 40}, {"b.mp4", 40}, {"c.mp4", 40}, {"d.mp4", 40}};
    CHECK(plan_eviction(v, 1000, 850, HIGH, FREE) == 3); // 40+40+40=120 >= 100
    return 0;
}

int test_free_target_exceeds_all_evicts_all() {
    // Only 50 bytes of recordings but target is 100: evict everything available.
    const std::vector<RecFile> v = {{"a.mp4", 20}, {"b.mp4", 30}};
    CHECK(plan_eviction(v, 1000, 999, HIGH, FREE) == 2);
    return 0;
}

int test_empty_list() {
    const std::vector<RecFile> v;
    CHECK(plan_eviction(v, 1000, 999, HIGH, FREE) == 0);
    return 0;
}

int test_zero_capacity_guard() {
    // Degenerate statvfs (total 0) must not divide-by-zero or evict.
    CHECK(plan_eviction(ten_files(), 0, 0, HIGH, FREE) == 0);
    return 0;
}

int test_single_big_file_covers_target() {
    // One large old file already covers the reclaim target: evict just it.
    const std::vector<RecFile> v = {{"old.mp4", 200}, {"new.mp4", 10}};
    CHECK(plan_eviction(v, 1000, 900, HIGH, FREE) == 1);
    return 0;
}

int test_compact_nv12_strided() {
    // 4x2 NV12 with stride 6: Y rows at offsets 0 and 6, the interleaved UV
    // plane (h/2 rows) at stride*height = 12. Padding bytes (0xEE) must not
    // leak into the compacted output and every payload byte must survive.
    const int w = 4;
    const int h = 2;
    const size_t stride = 6;
    std::vector<uint8_t> src(stride * h * 3 / 2, 0xEE);
    for (int r = 0; r < h; ++r) {
        for (int c = 0; c < w; ++c) {
            src.at((static_cast<size_t>(r) * stride) + static_cast<size_t>(c)) =
                static_cast<uint8_t>((10 * r) + c);
        }
    }
    for (int c = 0; c < w; ++c) {
        src.at((stride * h) + static_cast<size_t>(c)) = static_cast<uint8_t>(100 + c);
    }
    std::vector<uint8_t> dst(static_cast<size_t>(w) * h * 3 / 2, 0);
    compact_nv12(src.data(), stride, w, h, dst.data());
    const std::vector<uint8_t> want = {0, 1, 2, 3, 10, 11, 12, 13, 100, 101, 102, 103};
    CHECK(dst == want);
    return 0;
}

// ------------------------------------------------- overlay text (pure) --

int test_overlay_text_full_and_placeholders() {
    OverlayMeta m;
    m.gps_ok = true;
    m.lat_deg = 36.350411;
    m.lon_deg = 127.384548;
    m.alt_m = 80.5F;
    m.wide_valid = true;
    m.wide_cx = 0.5F;
    m.wide_cy = 0.5F;
    const std::string s = format_overlay_text(m, "2026-08-16 01:02:03Z", 1280, 720);
    CHECK(s.find("2026-08-16 01:02:03Z") != std::string::npos);
    CHECK(s.find("GPS 36.350411,127.384548 alt 80.5m") != std::string::npos);
    CHECK(s.find("WIDE (640,360)") != std::string::npos);
    CHECK(s.find("NARROW --") != std::string::npos); // absent detection: placeholder
    const std::string n = format_overlay_text(OverlayMeta{}, "T", 1280, 720);
    CHECK(n.find("GPS NO-FIX") != std::string::npos);
    CHECK(n.find("WIDE --") != std::string::npos);
    return 0;
}

// ------------------------------------------------- integration (ffmpeg) --

constexpr int REC_W = 64;
constexpr int REC_H = 48;

VideoRecorder::Params rec_params(const std::string& dir) {
    VideoRecorder::Params p;
    p.dir = dir;
    p.width = REC_W;
    p.height = REC_H;
    p.fps = 25.0;
    p.overlay = false; // drawtext availability varies per ffmpeg build; the
                       // subtitle format itself is pinned by the pure test
    return p;
}

std::string fresh_dir(const char* tag) {
    const std::string d =
        "/tmp/riposte_rec_" + std::string(tag) + "_" + std::to_string(::getpid());
    (void)::mkdir(d.c_str(), 0755);
    return d;
}

// NV12 frame filled with a mid-gray; lifetime owned by the caller's vector.
Frame make_frame(std::vector<uint8_t>& buf) {
    buf.assign(static_cast<size_t>(REC_W) * REC_H * 3 / 2, 128);
    Frame f{};
    f.width = REC_W;
    f.height = REC_H;
    f.stride = REC_W;
    f.data = buf.data();
    f.fourcc = 0x3231564E;
    return f;
}

std::vector<std::string> mp4s_in(const std::string& dir) {
    std::vector<std::string> out;
    DIR* d = ::opendir(dir.c_str());
    if (d == nullptr) {
        return out;
    }
    // Single-threaded test scan (readdir_r is deprecated).
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    while (const dirent* e = ::readdir(d)) {
        const std::string n = e->d_name;
        if (n.size() > 4 && n.compare(n.size() - 4, 4, ".mp4") == 0) {
            std::string full = dir;
            full += '/';
            full += n;
            out.push_back(std::move(full));
        }
    }
    (void)::closedir(d);
    return out;
}

void wipe_dir(const std::string& dir) {
    for (const auto& f : mp4s_in(dir)) {
        (void)::unlink(f.c_str());
    }
    (void)::rmdir(dir.c_str());
}

// Open descriptors of this process. The dual path now allocates a SECOND pipe
// per segment and hands one end to the encoder, so a missed close would leak a
// descriptor on every rotation — invisible until the process hits its limit
// after hours of recording.
std::size_t open_fd_count() {
    std::size_t n = 0;
    DIR* d = ::opendir("/proc/self/fd");
    if (d == nullptr) {
        return 0;
    }
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    while (::readdir(d) != nullptr) {
        ++n;
    }
    (void)::closedir(d);
    return n;
}

// Count of FIFO artifacts (dual-mode narrow inputs) left in the directory.
std::size_t fifos_in(const std::string& dir) {
    std::size_t n = 0;
    DIR* d = ::opendir(dir.c_str());
    if (d == nullptr) {
        return 0;
    }
    // Single-threaded test scan (readdir_r is deprecated).
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    while (const dirent* e = ::readdir(d)) {
        if (std::string(e->d_name).find(".fifo") != std::string::npos) {
            ++n;
        }
    }
    (void)::closedir(d);
    return n;
}

// The finalized file must start with an MP4 ftyp box and carry real payload.
bool looks_like_mp4(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    char head[12] = {};
    in.read(head, sizeof(head));
    return in.gcount() == sizeof(head) && head[4] == 'f' && head[5] == 't' &&
           head[6] == 'y' && head[7] == 'p';
}

int test_records_a_playable_segment() {
    const std::string dir = fresh_dir("seg");
    VideoRecorder rec(rec_params(dir));
    if (!rec.open()) {
        std::printf("SKIP: ffmpeg unavailable — integration tier not run\n");
        wipe_dir(dir);
        return 0;
    }
    std::vector<uint8_t> buf;
    const Frame f = make_frame(buf);
    uint64_t t = 1'000'000'000ULL;
    for (int i = 0; i < 20; ++i) {
        rec.write(f, t);
        t += 40'000'000ULL; // 40 ms >= the 25 fps pacing period
    }
    rec.close();
    CHECK(rec.frames_written() >= 15); // pacing may skip the first boundary
    const auto files = mp4s_in(dir);
    CHECK(files.size() == 1);
    struct stat st {};
    CHECK(::stat(files.front().c_str(), &st) == 0);
    CHECK(st.st_size > 500); // real payload, not an empty shell
    CHECK(looks_like_mp4(files.front()));
    wipe_dir(dir);
    return 0;
}

int test_rotates_segments_on_schedule() {
    const std::string dir = fresh_dir("rot");
    VideoRecorder::Params p = rec_params(dir);
    p.segment_ns = 150'000'000ULL; // 150 ms segments force several rotations
    VideoRecorder rec(p);
    if (!rec.open()) {
        std::printf("SKIP: ffmpeg unavailable — integration tier not run\n");
        wipe_dir(dir);
        return 0;
    }
    std::vector<uint8_t> buf;
    const Frame f = make_frame(buf);
    uint64_t t = 1'000'000'000ULL;
    for (int i = 0; i < 12; ++i) {
        rec.write(f, t);
        t += 40'000'000ULL; // mono time drives rotation; filenames carry a
                            // sequence, so same-second segments never collide
    }
    rec.close();
    const auto files = mp4s_in(dir);
    CHECK(files.size() >= 2); // rotated at least once
    for (const auto& file : files) {
        CHECK(looks_like_mp4(file));
    }
    wipe_dir(dir);
    return 0;
}

int test_survives_an_encoder_that_dies() {
    // A fake "ffmpeg" that passes the open()-time -version probe but exits
    // immediately when run as an encoder: every segment's reader vanishes at
    // once. The recorder must detect the death, drop (never block or crash),
    // respawn on later frames, and shut down clean.
    const std::string dir = fresh_dir("die");
    const std::string fake = dir + "/fake_ffmpeg.sh";
    {
        std::ofstream out(fake);
        out << "#!/bin/sh\n[ \"$1\" = \"-version\" ] && exit 0\nexit 1\n";
    }
    (void)::chmod(fake.c_str(), 0755);
    VideoRecorder::Params p = rec_params(dir);
    p.ffmpeg = fake;
    VideoRecorder rec(p);
    CHECK(rec.open()); // the -version probe passes
    std::vector<uint8_t> buf;
    const Frame f = make_frame(buf);
    uint64_t t = 1'000'000'000ULL;
    for (int i = 0; i < 6; ++i) {
        rec.write(f, t);
        t += 40'000'000ULL;
        ::usleep(30'000); // give the fake encoder time to die between writes
    }
    rec.close();
    // Nothing real could be encoded; the point is survival and honest drops.
    CHECK(rec.frames_written() + rec.frames_dropped() > 0);
    (void)::unlink(fake.c_str());
    wipe_dir(dir);
    return 0;
}

int test_dual_composite_records() {
    // Dual wide|narrow EO with overlay REQUESTED: on builds without drawtext the
    // overlay must degrade away (recording continues) — asking for the
    // subtitle can never break the recording itself.
    const std::string dir = fresh_dir("dual");
    VideoRecorder::Params p = rec_params(dir);
    p.dual = true;
    p.overlay = true;
    VideoRecorder rec(p);
    if (!rec.open()) {
        std::printf("SKIP: ffmpeg/hstack unavailable — dual tier not run\n");
        wipe_dir(dir);
        return 0;
    }
    std::vector<uint8_t> wide_buf;
    std::vector<uint8_t> nar_buf;
    const Frame wide = make_frame(wide_buf);
    const Frame nar = make_frame(nar_buf);
    OverlayMeta m;
    m.gps_ok = true;
    m.lat_deg = 36.35;
    m.lon_deg = 127.38;
    uint64_t t = 1'000'000'000ULL;
    for (int i = 0; i < 20; ++i) {
        rec.write_dual(wide, nar, m, t);
        t += 40'000'000ULL;
    }
    rec.close();
    CHECK(rec.frames_written() >= 15);
    const auto files = mp4s_in(dir);
    CHECK(files.size() == 1);
    CHECK(looks_like_mp4(files.front()));
    wipe_dir(dir);
    return 0;
}

int test_dual_rotation_uses_fresh_fifo_per_segment() {
    // Segment rotation in dual mode: reopening ONE shared FIFO path O_RDWR
    // revived a writer on the pipe object the previous segment's encoder was
    // still draining — that encoder never saw EOF (never exited, never
    // reaped), and old and new encoders byte-split the new segment's frames.
    // With a fresh FIFO per segment every rotation must yield an independent,
    // playable file, and close() must leave no FIFO behind.
    const std::string dir = fresh_dir("drot");
    VideoRecorder::Params p = rec_params(dir);
    p.dual = true;
    p.segment_ns = 150'000'000ULL; // 150 ms segments force several rotations
    VideoRecorder rec(p);
    if (!rec.open()) {
        std::printf("SKIP: ffmpeg/hstack unavailable — dual tier not run\n");
        wipe_dir(dir);
        return 0;
    }
    std::vector<uint8_t> wide_buf;
    std::vector<uint8_t> nar_buf;
    const Frame wide = make_frame(wide_buf);
    const Frame nar = make_frame(nar_buf);
    uint64_t t = 1'000'000'000ULL;
    // One rotation's worth first, so the steady-state descriptor count is
    // measured with a segment already open (the baseline must not include
    // one-time setup).
    for (int i = 0; i < 5; ++i) {
        rec.write_dual(wide, nar, OverlayMeta{}, t);
        t += 40'000'000ULL;
    }
    const std::size_t fds_before = open_fd_count();
    for (int i = 0; i < 40; ++i) { // many more rotations
        rec.write_dual(wide, nar, OverlayMeta{}, t);
        t += 40'000'000ULL;
    }
    // Each rotation allocates a fresh stdin pipe AND a fresh narrow pipe; both
    // ends must be accounted for or the count grows with every segment.
    CHECK(open_fd_count() <= fds_before + 2);
    rec.close();
    const auto files = mp4s_in(dir);
    CHECK(files.size() >= 2); // rotated at least once
    for (const auto& file : files) {
        CHECK(looks_like_mp4(file)); // each segment finalized independently
    }
    CHECK(fifos_in(dir) == 0); // every per-segment FIFO cleaned up
    wipe_dir(dir);
    return 0;
}

int test_open_fails_closed_without_encoder() {
    const std::string dir = fresh_dir("noff");
    VideoRecorder::Params p = rec_params(dir);
    p.ffmpeg = "/nonexistent/ffmpeg";
    VideoRecorder rec(p);
    CHECK(!rec.open()); // P1-07: record=true + no encoder = refused startup
    wipe_dir(dir);
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_below_high_water_keeps_all();
    rc = rc != 0 ? rc : test_at_high_water_evicts_to_free_target();
    rc = rc != 0 ? rc : test_evicts_oldest_first_minimum_needed();
    rc = rc != 0 ? rc : test_free_target_exceeds_all_evicts_all();
    rc = rc != 0 ? rc : test_empty_list();
    rc = rc != 0 ? rc : test_zero_capacity_guard();
    rc = rc != 0 ? rc : test_single_big_file_covers_target();
    rc = rc != 0 ? rc : test_compact_nv12_strided();
    rc = rc != 0 ? rc : test_overlay_text_full_and_placeholders();
    rc = rc != 0 ? rc : test_records_a_playable_segment();
    rc = rc != 0 ? rc : test_rotates_segments_on_schedule();
    rc = rc != 0 ? rc : test_survives_an_encoder_that_dies();
    rc = rc != 0 ? rc : test_dual_composite_records();
    rc = rc != 0 ? rc : test_dual_rotation_uses_fresh_fifo_per_segment();
    rc = rc != 0 ? rc : test_open_fails_closed_without_encoder();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_recorder: %d checks passed\n", checks);
    return 0;
}
