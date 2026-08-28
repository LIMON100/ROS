#pragma once
#include "IDetector.h" // Frame

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace riposte {

// Telemetry stamped onto recorded video as a burn-in subtitle (time is added by
// the recorder). All fields optional — invalid ones render as placeholders.
// Channels are the DUALEO pair (S-11): wide = EO wide-angle (Ch1, left),
// nar = EO narrow-angle (Ch2, right). An IR channel is a future item — the
// pipeline is channel-agnostic, only the second input would change.
struct OverlayMeta {
    double lat_deg = 0.0;
    double lon_deg = 0.0;
    float alt_m = 0.F;
    bool gps_ok = false;
    float wide_cx = 0.F, wide_cy = 0.F; // wide-EO detection center, normalized [0,1]
    bool wide_valid = false;
    float nar_cx = 0.F, nar_cy = 0.F; // narrow-EO detection center, normalized [0,1]
    bool nar_valid = false;
};

// Records the camera stream to timestamped H.264 MP4 segments and reclaims disk
// space as the output volume fills. It runs DECOUPLED from perception: encoding
// is offloaded to an ffmpeg subprocess fed raw NV12 over a pipe, so a slow
// encoder drops whole frames (latest-wins) and never stalls the seeker loop.
// There is no compile-time codec dependency, and the target can select a
// hardware encoder (e.g. h264_rkmpp) via config. Enabled at runtime; if ffmpeg
// is missing the recorder disables itself and perception is unaffected.
//
// Dual mode composites wide EO (Ch1, left) and narrow EO (Ch2, right)
// side-by-side into one file and burns a subtitle (time / GPS / per-channel
// detection coords) via drawtext.
class VideoRecorder {
public:
    struct Params {
        std::string dir = "/var/lib/riposte/rec"; // output directory
        int width = 1280;                         // per-channel width
        int height = 720;                         // per-channel height
        double fps = 30.0;                        // input rate hint for the encoder
        uint64_t segment_ns = 30'000'000'000ULL;  // 30 s per MP4 file
        double disk_high_frac = 0.80;             // start evicting at 80 % used
        double disk_free_frac = 0.10;             // reclaim ~10 % of capacity
        std::string ffmpeg = "ffmpeg";            // binary (PATH or absolute)
        std::string codec = "libx264";            // -c:v (h264_rkmpp on target)
        std::string preset = "ultrafast";         // libx264 preset (ignored else)
        bool dual = false;   // EO wide|narrow side-by-side + subtitle
        bool overlay = true; // burn-in subtitle (dual mode)
        std::string font = "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf";
    };

    explicit VideoRecorder(Params params) : p_(std::move(params)) {}
    ~VideoRecorder();

    // Owns pipe fds + child process; copy/move would double-close / double-reap.
    VideoRecorder(const VideoRecorder&) = delete;
    VideoRecorder& operator=(const VideoRecorder&) = delete;
    VideoRecorder(VideoRecorder&&) = delete;
    VideoRecorder& operator=(VideoRecorder&&) = delete;

    // Creates the output directory and arms recording. Returns false if the
    // directory cannot be created; the caller then runs without recording.
    bool open();

    // Single-channel: feeds one NV12 frame (opens/rotates the segment as needed).
    void write(const Frame& f, uint64_t mono_ns);

    // Dual-channel (wide EO left, narrow EO right): feeds both NV12 frames and
    // the subtitle metadata for this instant. Both frames are written as a unit
    // or both dropped, so the composite never desyncs. Never blocks the caller.
    void write_dual(const Frame& wide, const Frame& nar, const OverlayMeta& meta,
                    uint64_t mono_ns);

    // Finalizes the current segment (flushes the MP4) and reaps the encoder.
    void close();

    uint64_t frames_written() const { return frames_written_; }
    uint64_t frames_dropped() const { return frames_dropped_; }
    // Frames dropped specifically for mismatched dimensions (a subset of
    // frames_dropped): the dual path's silent-empty-recording signature, so the
    // owner reports it at shutdown.
    uint64_t dim_mismatch() const { return dim_mismatch_; }

private:
    bool start_segment(uint64_t mono_ns);
    void finish_segment();          // closes pipes (EOF => mp4 finalizes); reap is async
    void sweep_stale_fifos() const; // removes legacy named-FIFO artifacts
    void reap_pending();            // WNOHANG-reap rotated-out encoders
    void reclaim_disk() const;      // statvfs + evict oldest per plan_eviction()
    bool rotate(uint64_t mono_ns);  // open/roll the segment as needed
    // True only if a WHOLE frame fits now (so the blocking write cannot stall the
    // caller); sets dead=true if the encoder has died (reader gone).
    bool can_write(int fd, bool& dead) const;
    static bool write_all(int fd, const uint8_t* buf,
                          size_t n);              // blocking whole-frame write
    void update_overlay(const OverlayMeta& meta); // rewrite the drawtext sidecar
    // False (with a rate-limited error) when f's dimensions differ from the
    // recorder's — writing frame_bytes_ from a smaller buffer would over-read.
    bool frame_matches(const Frame& f);
    // Contiguous NV12 view of f: f.data when stride == width, else a row-by-row
    // repack into pack_ (padded ISP output would shear the encoded video).
    const uint8_t* contiguous(const Frame& f);

    Params p_;
    int pipe_fd_ = -1; // blocking write end -> ffmpeg EO/primary input (stdin)
    // Dual mode: write end of the SECOND anonymous pipe, inherited by the
    // encoder as fd 3 (`-i pipe:3`). A fresh pipe per segment, exactly like
    // stdin — the named-FIFO design it replaces either revived a writer on the
    // previous encoder's pipe object (shared path: no EOF, two readers
    // byte-splitting one stream) or raced encoder startup with an open()
    // rendezvous (per-segment path). -1 in single-channel mode.
    int pipe_fd_nar_ = -1;
    long child_pid_ = -1; // ffmpeg pid (-1 = none)
    std::string overlay_path_;
    std::vector<uint8_t> pack_; // stride-compaction scratch (empty until needed)
    uint64_t segment_start_ns_ = 0;
    uint64_t last_overlay_ns_ = 0;
    // Input pacing: the capture loop runs at SEEKER_FRAME_HZ (60) but the
    // encoder is fed at Params::fps — frames arriving early are skipped
    // silently (expected decimation, NOT counted as drops, which flag trouble).
    uint64_t next_frame_due_ns_ = 0;
    // Rotated-out encoders not yet reaped: finish_segment() must not block the
    // perception loop on an MP4 finalize (moov write can take 100s of ms), so
    // it reaps WNOHANG and parks the pid here; later writes retry, close()
    // reaps for real.
    std::vector<long> reap_;
    int consec_spawn_fail_ = 0; // repeated encoder spawn failures -> disable
    // Per-instance segment counter, part of the filename: two segments started
    // within one wall-clock second (a death-restart, a fast rotation in tests)
    // would otherwise share a name and -y silently overwrites the first.
    uint64_t segment_seq_ = 0;
    size_t frame_bytes_ = 0; // NV12 = width * height * 3 / 2
    bool enabled_ = false;
    // Frame does not fit the pipe even at the system max size: writing would
    // block the perception loop, so recording stays off (one ERROR logged).
    bool pipe_too_small_ = false;
    uint64_t dim_mismatch_ = 0; // frames dropped for mismatched dimensions
    uint64_t frames_written_ = 0;
    uint64_t frames_dropped_ = 0;
};

// Burn-in subtitle line (time / GPS / wide+narrow detection pixel coords) for
// the drawtext sidecar. Pure so the placeholder/format contract is host-tested;
// `utc` is the caller-formatted wall-clock string (the only impure input).
std::string format_overlay_text(const OverlayMeta& meta, const char* utc, int width,
                                int height);

// Repacks a strided NV12 frame (stride > width, padded ISP output) into the
// contiguous width*height*3/2 layout a rawvideo consumer expects: `height` Y
// rows then height/2 interleaved-UV rows, each `width` bytes taken at `stride`
// intervals. Pure; unit-tested without a camera (G11.2).
void compact_nv12(const uint8_t* src, size_t stride, int width, int height, uint8_t* dst);

// --- Pure disk-eviction policy (unit-tested without a real filesystem, G11.2) --
struct RecFile {
    std::string name;
    uint64_t bytes = 0;
};

// Given recordings sorted OLDEST-FIRST plus the volume's total capacity and used
// bytes, decide how many leading (oldest) files to delete: if usage has reached
// high_frac of capacity, evict oldest-first until at least free_frac of capacity
// is reclaimed (or the list empties). Returns 0 when below the high-water mark.
size_t plan_eviction(const std::vector<RecFile>& oldest_first, uint64_t total_bytes,
                     uint64_t used_bytes, double high_frac, double free_frac);

} // namespace riposte
