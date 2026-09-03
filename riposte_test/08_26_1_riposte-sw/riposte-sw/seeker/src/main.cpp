// riposte-seeker (L2 perception) — RIPOSTE-SAD-001 §3, §6
//
// Wiring of the capture -> infer -> fuse pipeline. The three stages run as one
// thread here (capture is the pacing element at ~60 Hz (SEEKER_FRAME_HZ) and
// Hailo detection is async-offloaded), with one rule: this thread never makes a
// SYNCHRONOUS NPU call. The two that would are isolated behind workers —
// EmbedWorker for T1 embeddings (TR-4) and RecordWorker for the narrow-channel
// overlay detection (P2-06) — because an RKNN call that blocks forever would
// otherwise stop capture, tracking and TrackBus publication with it.
//
// Output: TrackState on shm SeqSlot /riposte_track, SeekerHealth on
// /riposte_seeker_health. This process NEVER touches the FC. A Hailo/camera
// fault simply stops fresh TrackBus publishes, and the OBC disengages via SM-7.

#include "AssocCost.h" // Embedding
#include "CameraIngest.h"
#include "ChannelMap.h"
#include "EmbedWorker.h"
#include "IDetector.h"
#include "IEmbedder.h"
#include "ITemplateTracker.h"
#include "RecordWorker.h"
#include "SearchScheduler.h"
#include "SeekerConfig.h"
#include "SyntheticDetector.h"
#include "TargetEstimator.h"
#include "TrackFusion.h"
#include "Tracker.h"
#include "VideoRecorder.h"
#include "CameraIngest.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "riposte/Clock.h"
#include "riposte/Config.h"
#include "riposte/Log.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"

#ifdef RIPOSTE_WITH_HAILO
#include "HailoDetector.h"
#endif
#ifdef RIPOSTE_WITH_RKNN
#include "RknnEmbedder.h"
#endif

#ifdef RIPOSTE_WITH_GZ
#include "GzCamera.h"
#endif

using namespace riposte;

namespace {
// Cooperative-shutdown flag (G7.7): set by the signal handler, observed by the
// pipeline loop. Mutable by design — a global is the only channel a signal
// handler can reach.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_run{true};
void on_signal(int /*unused*/) {
    g_run.store(false);
}

// Builds the optional recorder from config (G16.4: keeps main() within size).
// Returns nullptr when recording is disabled, or when open() fails (encoder /
// directory unavailable) — in which case main() refuses startup if the
// operator asked for recording (P1-07 fail-closed).
std::unique_ptr<VideoRecorder> make_recorder(const SeekerConfig& sc, const Config& cfg,
                                             int w, int h, bool dual) {
    if (!cfg.get_bool("seeker.record", false)) {
        return nullptr;
    }
    VideoRecorder::Params rp;
    rp.dir = cfg.get_str("seeker.record_dir", "/var/lib/riposte/rec");
    rp.width = w;
    rp.height = h;
    rp.fps = sc.record_fps;
    rp.segment_ns = static_cast<uint64_t>(sc.record_segment_s * 1e9);
    rp.disk_high_frac = sc.record_disk_high;
    rp.disk_free_frac = sc.record_disk_free;
    rp.ffmpeg = cfg.get_str("seeker.record_ffmpeg", "ffmpeg");
    rp.codec = cfg.get_str("seeker.record_codec", "libx264");
    rp.dual = dual; // wide|narrow side-by-side (only if the narrow channel opened)
    rp.overlay = cfg.get_bool("seeker.record_overlay", true);
    auto rec = std::make_unique<VideoRecorder>(rp);
    if (!rec->open()) {
        return nullptr; // encoder/dir unavailable — main() refuses startup
    }
    return rec;
}

// Camera/detector construction split out of main() (G16.4). Each returns nullptr
// on failure (already logged) so main() just checks and exits.
std::unique_ptr<ICamera> make_camera(int w, int h, bool synth,
                                     const std::string& device) {
    std::unique_ptr<ICamera> cam;
  #ifdef RIPOSTE_WITH_GZ
    // Rendered simulation camera, selected by the device string rather than a
    // new config key: "gz:<topic>". An explicit gz: device names a REAL frame
    // source, so it OVERRIDES the synthetic flag - that is what allows rendered
    // pixels to be paired with a synthetic detector while the detector question
    // is still open. Test-only path.
    if (device.rfind("gz:", 0) == 0) {
        cam = std::make_unique<GzCamera>(device.substr(3));
    }
  #endif 
    if (!cam) {
        if (synth) {
            cam = std::make_unique<SyntheticCamera>(w, h);
        } else {
            cam = std::make_unique<V4L2Camera>(V4L2Camera::Params{device, w, h});
        }
    }

    if (!cam->open()) {
        RLOG_ERROR("seeker", "camera open failed (%s @ %s)", cam->name(), device.c_str());
        return nullptr;
    }
    // The V4L2 driver may substitute a different mode than requested; callers
    // must size everything from cam->width()/height(), never the config values.
    if (cam->width() != w || cam->height() != h) {
        RLOG_WARN("seeker", "%s negotiated %dx%d (config %dx%d) — using negotiated",
                  cam->name(), cam->width(), cam->height(), w, h);
    }
    return cam;
}

// Opens the writer ends of the seeker's output buses. A failure is fatal for
// the process: it would otherwise run while silently publishing nothing, which
// downstream reads as "no target" (G16.4: keeps main() within size).
bool open_buses(ShmSeqSlot<TrackState>& track_bus, ShmSeqSlot<SeekerHealth>& health_bus) {
    if (!track_bus.open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::WRITER)) {
        RLOG_ERROR("seeker", "cannot open TrackBus shm %s", tun::SHM_TRACK);
        return false;
    }
    if (!health_bus.open(tun::SHM_SEEKER_HEALTH,
                         ShmSeqSlot<SeekerHealth>::Role::WRITER)) {
        RLOG_ERROR("seeker", "cannot open HealthBus shm %s", tun::SHM_SEEKER_HEALTH);
        return false;
    }
    return true;
}

std::unique_ptr<IDetector> make_detector([[maybe_unused]] const Config& cfg, bool synth) {
    std::unique_ptr<IDetector> det;
    if (synth) {
        det = std::make_unique<SyntheticDetector>();
    } else {
#ifdef RIPOSTE_WITH_HAILO
        HailoDetector::Params hp;
        hp.hef_path = cfg.get_str("seeker.hef", "config/model.hef");
        hp.score_threshold = static_cast<float>(cfg.get_double("seeker.score_thr", 0.4));
        hp.nms_iou = static_cast<float>(cfg.get_double("seeker.nms_iou", 0.45));
        hp.target_class =
            static_cast<int>(cfg.get_int("seeker.target_class", tun::TARGET_CLASS));
        // Expectations only — init() overwrites both from the loaded HEF.
        hp.model_size = static_cast<int>(cfg.get_int("seeker.model_size", 640));
        hp.num_classes = static_cast<int>(cfg.get_int("seeker.model_classes", 1));
        det = std::make_unique<HailoDetector>(hp);
#else
        // Fail-closed (I6): a production profile on a build without the real
        // detector must refuse to start — a synthetic fallback here once meant
        // detections that never saw a pixel could reach the TrackBus.
        RLOG_ERROR("seeker",
                   "synthetic=false but built without RIPOSTE_WITH_HAILO — "
                   "no real detector, refusing synthetic fallback");
        return nullptr;
#endif
    }
    if (!det->init()) {
        RLOG_ERROR("seeker", "detector init failed (%s)", det->name());
        return nullptr;
    }
    return det;
}

// T1 appearance embedder (TRACKER-REQ TR-B). RKNN on target, Synthetic in SIL.
// Never fatal: a null/failed embedder just means association runs motion-only
// (TR-3), so an init failure downgrades rather than stops the seeker.
std::unique_ptr<IEmbedder> make_embedder([[maybe_unused]] const Config& cfg, bool synth) {
    std::unique_ptr<IEmbedder> emb;
    if (synth) {
        emb = std::make_unique<SyntheticEmbedder>();
    } else {
#ifdef RIPOSTE_WITH_RKNN
        RknnEmbedder::Params rp;
        rp.model_path = cfg.get_str("seeker.reid_model", "config/reid.rknn");
        rp.core = static_cast<int>(cfg.get_int("seeker.reid_core", 0));
        emb = std::make_unique<RknnEmbedder>(rp);
#else
        // No real T1 on this build: run motion-only (TR-3) — an honest
        // downgrade, unlike a synthetic embedder scoring real associations.
        RLOG_WARN("seeker",
                  "built without RIPOSTE_WITH_RKNN — T1 unavailable, association "
                  "runs motion-only (TR-3)");
        return nullptr;
#endif
    }
    if (!emb->init()) {
        RLOG_WARN("seeker", "embedder init failed (%s); T1 runs motion-only",
                  emb->name());
        return nullptr;
    }
    return emb;
}

// T2 template tracker (TRACKER-REQ TR-C). The NanoTrack-class RKNN model is a
// bring-up item, so only the SIL stand-in exists today. In production the
// stand-in must NOT run (I6): null means fusion stays detection-authoritative
// and coasts on motion — an honest downgrade instead of a fake visual coast.
std::unique_ptr<ITemplateTracker> make_template_tracker(const Config& cfg, bool synth) {
    (void)cfg;
    if (!synth) {
        RLOG_WARN("seeker",
                  "T2 template tracker not yet available on target — fusion runs "
                  "without visual coast (TR-C fallback)");
        return nullptr;
    }
    auto tmpl = std::make_unique<SyntheticTemplateTracker>();
    if (!tmpl->init()) {
        return nullptr;
    }
    return tmpl;
}

// Opens the narrow-EO (Ch2) camera + detector for dual-channel recording,
// filling both out-params. Leaves them null (single-channel) if dual recording
// is off or the narrow device/detector is unavailable (G16.4: keeps main()
// within size). This channel is RECORDING-ONLY today — the S-11 narrow
// inference pipeline lands with P4. (An IR channel is a future item; the
// recording path is channel-agnostic.)
// The PERCEPTION loop owns the narrow camera whenever anything needs it (S-13):
// a V4L2 device cannot be opened twice, so ownership cannot be split between the
// pipeline and the recorder. `nar_det` is only built for the recording-only
// configuration — with dual_eo on, the pipeline's own detector infers on the
// narrow frame and the recorder reuses that result instead of running a second
// NPU pass.
struct NarrowChannelReq {
    int width = 0;
    int height = 0;
    bool synthetic = false;
    bool want_infer = false;  // dual_eo: the pipeline infers on this channel
    bool want_record = false; // record_dual: the overlay needs its frames
};

void open_narrow_channel(const Config& cfg, const NarrowChannelReq& req,
                         std::unique_ptr<ICamera>& nar_cam,
                         std::unique_ptr<IDetector>& nar_det) {
    const bool want_infer = req.want_infer;
    const bool want_record = req.want_record;
    if (!want_infer && !want_record) {
        return;
    }
    nar_cam = make_camera(req.width, req.height, req.synthetic,
                          cfg.get_str("seeker.narrow_device", "/dev/video1"));
    if (!nar_cam) {
        RLOG_WARN("seeker", "narrow camera unavailable");
        return;
    }
    if (want_record && !want_infer) {
        nar_det = make_detector(cfg, req.synthetic);
        if (!nar_det) {
            RLOG_WARN("seeker", "narrow detector unavailable — overlay box disabled");
        }
    }
}

// Publishes a SeekerHealth sample (~2 Hz) and logs the human-readable line
// (G16.4: keeps the pipeline loop within size).
// G16.6 deviation: flat telemetry fan-in exceeds the parameter bound; a wrapper struct
// would only re-wrap the one call site (G16.6) NOLINTNEXTLINE(readability-function-size)
// `narrow_infers` is the count of inferences run on the narrow channel since
// the previous health line, or -1 when dual-EO is off. It exists because the
// dual-EO failure mode is SILENT: a misconfigured or unwired narrow channel
// still produces a healthy-looking 60 fps track stream from the wide camera
// alone, and nothing in the log used to say otherwise (that is exactly how the
// pre-P4 fail-open state went unnoticed). An operator can now read whether the
// second channel is carrying its half of the schedule.
void log_status(const TrackState& ts, double fps, double infer_ms, size_t num_tracks,
                const SearchScheduler& sched, int narrow_infers) {
    char ch[24] = "";
    if (narrow_infers >= 0) {
        (void)std::snprintf(ch, sizeof(ch), " narrow=%d", narrow_infers);
    }
    RLOG_INFO(
        "seeker",
        "fps=%.1f infer=%.1fms mode=%s(%d/%d) track=%s q=%.2f targets=%u tracks=%zu%s",
        fps, infer_ms, sched.mode_name(), sched.window_hits(), tun::CONFIRM_WINDOW,
        ts.valid ? "yes" : "no", ts.quality, ts.num_targets, num_tracks, ch);
}

// One tick's observable state, bundled so the reporting entry point stays a
// two-argument call rather than a ten-parameter one.
struct StatusSample {
    uint64_t mono_ns = 0;
    double fps = 0.0;
    double infer_ms = 0.0;
    bool detector_ok = false;
    bool synthetic = false;
    size_t num_tracks = 0;
    int narrow_infers = -1; // -1 = dual-EO off
};

void publish_health(ShmSeqSlot<SeekerHealth>& bus, const TrackState& ts,
                    const StatusSample& st, const SearchScheduler& sched) {
    SeekerHealth h{};
    h.mono_ns = st.mono_ns;
    h.fps = static_cast<float>(st.fps);
    h.infer_latency_ms = static_cast<float>(st.infer_ms);
    h.camera_ok = 1;
    h.detector_ok = st.detector_ok ? 1 : 0;
    h.track_valid = ts.valid;
    h.synthetic = st.synthetic ? 1 : 0;
    bus.write(h);
    log_status(ts, st.fps, st.infer_ms, st.num_tracks, sched, st.narrow_infers);
}

// Builds the search scheduler from config (G16.4: keeps main() within size).
// Everything here comes from the VALIDATED struct (CR-07: reading the same key
// twice, once for validation and once for use, is how an unvalidated
// track_recheck_period_near reached the scheduler).
SearchScheduler::Params scheduler_params(const SeekerConfig& sc, float aspect) {
    SearchScheduler::Params sp;
    sp.grid = sc.grid;
    sp.wide_dwell = sc.wide_dwell;
    sp.confirm_window = sc.confirm_window;
    sp.confirm_hits = sc.confirm_hits;
    sp.recheck_period = sc.recheck_period;
    // R-10 close-range narrow 60 Hz boost: inside narrow_boost_range_m the
    // wide-recheck cadence stretches to recheck_period_near.
    sp.recheck_period_near = sc.recheck_period_near;
    sp.narrow_boost_range_m = sc.narrow_boost_range_m;
    sp.track_roi_scale = sc.track_roi_scale;
    sp.track_roi_min = sc.track_roi_min;
    sp.tile_overlap = sc.tile_overlap;         // R-14 seam coverage
    sp.track_roi_growth = sc.track_roi_growth; // R-16 expansion while missing
    sp.track_roi_max = sc.track_roi_max;
    sp.aspect = aspect;
    // S-11 channel allocation. The policy is implemented and tested; the
    // second-camera pipeline that consumes channel() lands with P4, so the
    // single-camera build keeps this off (every slot is the one camera).
    sp.dual_eo = sc.dual_eo; // validate() refuses true until P4 lands
    return sp;
}

// Narrow-channel dequeue budget: NON-BLOCKING on purpose. The narrow ring
// buffers frames, so when the channel is healthy the newest one is already
// there and a zero-timeout poll returns it; when the channel is dead or
// phase-drifted there is nothing to wait for and the slot falls back to the
// wide frame. Any positive budget here is paid out of the 16.7 ms frame budget
// of the wide channel — the one the guidance loop depends on — on EVERY tick a
// stalled narrow device produces nothing (a 10 ms budget dragged the wide loop
// toward ~40 Hz with target-hardware inference in the loop).
constexpr int NARROW_GRAB_MS = 0;

// Highest-scoring target-class detection centre (normalized); false if none.
// Used for the recording overlay hint, in the frame the detection was made in.
bool best_detection(const std::vector<Detection>& dets, int target_cls, float& cx,
                    float& cy) {
    const Detection* best = nullptr;
    for (const auto& d : dets) {
        if (d.cls != target_cls) {
            continue;
        }
        if (best == nullptr || d.score > best->score) {
            best = &d;
        }
    }
    if (best == nullptr) {
        return false;
    }
    cx = best->cx;
    cy = best->cy;
    return true;
}

// Runs one inference pass over the region the scheduler selected, returning the
// detections in FULL-FRAME normalized coordinates. A wide pass runs on the frame
// as-is; a tile/target pass crops first (the crop is what lets a distant target
// reach the model at native resolution instead of being resized away) and maps
// the results back. A failed crop degrades to the full frame rather than
// skipping the frame's inference entirely.
bool detect_region(IDetector& det, const Frame& f, const Roi& roi,
                   std::vector<uint8_t>& scratch, std::vector<Detection>& out) {
    if (roi.is_full()) {
        Frame whole = f;
        whole.src_roi = Roi{}; // this pass covers the entire sensor frame
        return det.detect(whole, out);
    }
    Frame sub{};
    Roi used{};
    if (!crop_nv12(f, roi, scratch, sub, used)) {
        return det.detect(f, out); // degenerate ROI: fall back to the whole frame
    }
    if (!det.detect(sub, out)) {
        return false;
    }
    for (auto& d : out) {
        d = remap_to_frame(d, used);
    }
    return true;
}
} // namespace

// G16.6 deviation: linear pipeline wiring; the logic already lives in helpers
// (G16.4/G16.6) NOLINTNEXTLINE(readability-function-size)
int main(int argc, char** argv) {
    (void)std::signal(SIGINT, on_signal); // prior handler is irrelevant here
    (void)std::signal(SIGTERM, on_signal);

    Config cfg;
    if (argc > 1 && !cfg.load(argv[1])) {
        // An explicitly named config that fails to load is an ops error, and
        // silently running on compiled defaults (fail-open) is exactly how a
        // production start ends up on the synthetic pipeline (AGENTS §7.9).
        RLOG_ERROR("seeker", "cannot load config %s — refusing to start on defaults",
                   argv[1]);
        return 1;
    }

    // Reject an out-of-range configuration at startup (AGENTS §7.9): better a
    // fast, precise failure than a divide-by-zero or a silently wrong detector
    // mid-flight. Read ONCE into the validated struct — everything below builds
    // from `sc`, so a key cannot be validated here and read raw somewhere else
    // (review CR-07).
    SeekerConfig sc;
    {
        sc.score_thr = static_cast<float>(cfg.get_double("seeker.score_thr", 0.4));
        sc.nms_iou = static_cast<float>(cfg.get_double("seeker.nms_iou", 0.45));
        sc.model_size = static_cast<int>(cfg.get_int("seeker.model_size", 640));
        sc.num_classes = static_cast<int>(cfg.get_int("seeker.model_classes", 1));
        sc.target_class =
            static_cast<int>(cfg.get_int("seeker.target_class", tun::TARGET_CLASS));
        sc.grid = static_cast<int>(cfg.get_int("seeker.search_grid", tun::SEARCH_GRID));
        sc.confirm_window =
            static_cast<int>(cfg.get_int("seeker.confirm_window", tun::CONFIRM_WINDOW));
        sc.confirm_hits =
            static_cast<int>(cfg.get_int("seeker.confirm_hits", tun::CONFIRM_HITS));
        sc.recheck_period = static_cast<int>(
            cfg.get_int("seeker.track_recheck_period", tun::TRACK_RECHECK_PERIOD));
        sc.wide_dwell = static_cast<int>(
            cfg.get_int("seeker.search_wide_dwell", tun::SEARCH_WIDE_DWELL));
        sc.recheck_period_near = static_cast<int>(cfg.get_int(
            "seeker.track_recheck_period_near", tun::TRACK_RECHECK_PERIOD_NEAR));
        sc.narrow_boost_range_m = static_cast<float>(
            cfg.get_double("seeker.narrow_boost_range_m", tun::NARROW_BOOST_RANGE_M));
        sc.embed_deadline_ms =
            cfg.get_double("seeker.embed_deadline_ms", tun::EMBED_DEADLINE_MS);
        sc.dual_eo = cfg.get_int("seeker.dual_eo", 0) != 0;
        sc.hfov_rad = static_cast<float>(cfg.get_double("seeker.hfov_rad", 1.05));
        sc.narrow_hfov_rad =
            static_cast<float>(cfg.get_double("seeker.narrow_hfov_rad", 0.269));
        sc.track_roi_scale = static_cast<float>(
            cfg.get_double("seeker.track_roi_scale", tun::TRACK_ROI_SCALE));
        sc.track_roi_min = static_cast<float>(
            cfg.get_double("seeker.track_roi_min", tun::TRACK_ROI_MIN));
        sc.tile_overlap = static_cast<float>(
            cfg.get_double("seeker.tile_overlap", tun::SEARCH_TILE_OVERLAP));
        sc.track_roi_growth = static_cast<float>(
            cfg.get_double("seeker.track_roi_growth", tun::TRACK_ROI_GROWTH));
        sc.track_roi_max = static_cast<float>(
            cfg.get_double("seeker.track_roi_max", tun::TRACK_ROI_MAX));
        sc.width = static_cast<int>(cfg.get_int("seeker.width", 1280));
        sc.height = static_cast<int>(cfg.get_int("seeker.height", 720));
        sc.record_fps = cfg.get_double("seeker.record_fps", 30.0);
        sc.record_segment_s = cfg.get_double("seeker.record_segment_s", 30.0);
        sc.record_disk_high = cfg.get_double("seeker.record_disk_high", 0.80);
        sc.record_disk_free = cfg.get_double("seeker.record_disk_free", 0.10);
        std::string err;
        if (!validate(sc, err)) {
            RLOG_ERROR("seeker", "invalid config: %s", err.c_str());
            return 1;
        }
    }

    const int cam_w = sc.width;
    const int cam_h = sc.height;
    // Default FALSE (production): the SIL/synthetic pipeline must be opted
    // into explicitly (config/sil.ini), never fallen into by a missing key.
    const bool use_synthetic = cfg.get_bool("seeker.synthetic", false);
    RLOG_INFO("seeker", "profile: %s",
              use_synthetic ? "SIL (synthetic pipeline)"
                            : "production (real backends required)");

    // --- Camera + Detector (EO / Ch1) ---
    std::unique_ptr<ICamera> cam = make_camera(
        cam_w, cam_h, use_synthetic, cfg.get_str("seeker.device", "/dev/video0"));
    if (!cam) {
        return 1;
    }
    std::unique_ptr<IDetector> det = make_detector(cfg, use_synthetic);
    if (!det) {
        return 1;
    }
    RLOG_INFO("seeker", "camera=%s detector=%s", cam->name(), det->name());

    // NEGOTIATED dimensions (make_camera warned if they differ from config):
    // everything that sizes buffers or derives geometry uses these.
    const int neg_w = cam->width();
    const int neg_h = cam->height();
    const float aspect = static_cast<float>(neg_w) / static_cast<float>(neg_h);

    // --- Narrow-EO channel (Ch2): perception and/or recording ---
    const bool want_dual_rec =
        cfg.get_bool("seeker.record", false) && cfg.get_bool("seeker.record_dual", false);
    std::unique_ptr<ICamera> nar_cam;
    std::unique_ptr<IDetector> nar_det;
    open_narrow_channel(
        cfg, NarrowChannelReq{neg_w, neg_h, use_synthetic, sc.dual_eo, want_dual_rec},
        nar_cam, nar_det);
    // Fail-closed (S-13): dual-EO inference was asked for and the second camera
    // is not there. Running wide-only would silently deliver none of the narrow
    // channel's range while the operator believes both are working.
    if (sc.dual_eo && nar_cam == nullptr) {
        RLOG_ERROR("seeker",
                   "seeker.dual_eo is on but the narrow camera did not open — "
                   "refusing to run wide-only");
        return 1;
    }
    // The narrow driver may substitute a mode just like the wide one
    // (make_camera only warns). Everything dual is derived from the WIDE
    // negotiated dims — ChannelMap's aspect, the recorder's frame pairing
    // (frame_matches drops every mismatched pair, silently emptying the
    // recording) — so a narrow channel that negotiated anything else cannot
    // participate: refuse startup under dual_eo (fail-closed, same rule as an
    // absent camera), degrade to single-channel recording otherwise.
    if (nar_cam != nullptr && (nar_cam->width() != neg_w || nar_cam->height() != neg_h)) {
        if (sc.dual_eo) {
            RLOG_ERROR("seeker",
                       "narrow camera negotiated %dx%d but the wide channel runs "
                       "%dx%d — dual-EO needs matching modes, refusing to start",
                       nar_cam->width(), nar_cam->height(), neg_w, neg_h);
            return 1;
        }
        RLOG_WARN("seeker",
                  "narrow camera negotiated %dx%d (wide %dx%d) — dual recording "
                  "disabled, continuing single-channel",
                  nar_cam->width(), nar_cam->height(), neg_w, neg_h);
        nar_cam.reset();
    }
    const bool dual_rec = want_dual_rec && (nar_cam != nullptr);
    // Wide<->narrow geometry for the handoff (DUALEO-REQ §3.6). Offsets come
    // from calibration; zero means perfectly coaxial axes.
    const ChannelMap cmap{ChannelMap::Params{
        sc.hfov_rad, sc.narrow_hfov_rad, aspect,
        static_cast<float>(cfg.get_double("seeker.channel_offset_az_rad", 0.0)),
        static_cast<float>(cfg.get_double("seeker.channel_offset_el_rad", 0.0))}};

    // --- Pipeline objects ---
    const int target_cls =
        static_cast<int>(cfg.get_int("seeker.target_class", tun::TARGET_CLASS));
    Tracker tracker(aspect, target_cls);
    TargetEstimator estimator(TargetEstimator::Params{
        sc.hfov_rad, aspect,
        static_cast<float>(cfg.get_double("seeker.target_size_m", 0.35))});
    SearchScheduler scheduler(scheduler_params(sc, aspect));
    std::vector<uint8_t> crop_scratch; // reused ROI crop buffer (no per-frame alloc)

    // --- Assisted tracking (RK NPU): T1 ReID association + T2 visual coast ---
    // Both are optional and fail-soft: a null embedder runs association
    // motion-only (TR-3), a null template coasts on motion. The fusion gate/
    // aspect mirror the tracker so all layers agree on "in gate".
    // The embedder is an NPU call, so it runs on its own thread with a deadline
    // rather than inline: rknn_run can block indefinitely and would take the
    // whole 60 Hz loop down with it (TR-4 / SDD S-12 §4.5).
    const auto embed_deadline_ns = static_cast<uint64_t>(sc.embed_deadline_ms * 1e6);
    EmbedWorker embed_worker(make_embedder(cfg, use_synthetic), embed_deadline_ns);
    embed_worker.start();
    std::unique_ptr<ITemplateTracker> tmpl = make_template_tracker(cfg, use_synthetic);
    TrackFusion fusion(TrackFusion::Params{
        [aspect] {
            const float g = tun::TRACKER_GATE_PX / 1280.0F;
            return g * g;
        }(),
        aspect,
        static_cast<int>(
            cfg.get_int("seeker.reanchor_period", tun::FUSION_REANCHOR_PERIOD)),
        static_cast<int>(cfg.get_int("seeker.mismatch_max", tun::FUSION_MISMATCH_MAX)),
        static_cast<float>(cfg.get_double("seeker.coast_quality_scale",
                                          tun::FUSION_COAST_QUALITY_SCALE))});
    std::vector<Embedding> embs; // reused per frame

    // --- IPC out (fatal if unavailable — see open_buses) ---
    ShmSeqSlot<TrackState> track_bus;
    ShmSeqSlot<SeekerHealth> health_bus;
    if (!open_buses(track_bus, health_bus)) {
        return 1;
    }

    // --- Optional H.264/MP4 recorder (30 s segments, disk auto-reclaim) ---
    std::unique_ptr<VideoRecorder> recorder =
        make_recorder(sc, cfg, neg_w, neg_h, dual_rec);
    if (cfg.get_bool("seeker.record", false) && !recorder) {
        // Recording is the trial's primary evidence: if the operator asked for
        // it and it cannot run, refuse to start rather than fly a mission whose
        // evidence silently never existed (AGENTS §7.9 fail-closed).
        RLOG_ERROR("seeker",
                   "seeker.record=true but the recorder is unavailable — "
                   "refusing to start without it");
        return 1;
    }
    // Dual recording runs OFF the perception thread (P2-06): the narrow-channel
    // overlay detection is a synchronous NPU call that must not block the EO
    // capture cadence. The worker takes ownership of the recorder + narrow
    // channel; single-channel recording stays inline (one non-blocking pipe
    // write). single_rec is the raw handle for that inline path — `recorder`
    // itself is not referenced again once it may have been moved into the
    // worker (rec_worker / single_rec gate every use below).
    std::unique_ptr<RecordWorker> rec_worker;
    VideoRecorder* single_rec = nullptr;
    if (recorder) {
        if (dual_rec) {
            rec_worker = std::make_unique<RecordWorker>(std::move(recorder),
                                                        std::move(nar_det), target_cls);
            rec_worker->start();
        } else {
            single_rec = recorder.get(); // `recorder` retains ownership
        }
    }

    // Every config key has been read by this point. A value that was PRESENT
    // but unparseable silently became its compiled default above — for a
    // threshold or geometry constant that is a fail-open the operator cannot
    // see (e.g. a legacy inline ';' comment), so surface each one and refuse
    // to start (AGENTS §7.9).
    if (!cfg.parse_failures().empty()) {
        for (const auto& e : cfg.parse_failures()) {
            RLOG_ERROR("seeker", "config: %s", e.c_str());
        }
        RLOG_ERROR("seeker", "refusing to start on unparseable config values");
        return 1;
    }

    std::vector<Detection> dets;
    uint64_t prev_frame_ns = 0;
    uint64_t last_health_ns = 0;
    uint32_t track_seq = 0;
    double fps_est = 0.0;
    // Last published target range (m), fed to the next frame's cue so the
    // scheduler's close-range narrow-60 Hz gate has a value (R-10). One-frame
    // lag is immaterial to a coarse 150 m threshold.
    float last_range_m = 0.F;
    int narrow_infers = 0; // narrow-channel inferences since the last health line

    while (g_run.load()) {
        Frame f{};
        if (!cam->grab(f, 200)) {
            RLOG_WARN("seeker", "frame grab timeout");
            continue;
        }
        // Drop stale frames outright (latest-wins guard).
        if (mono_now_ns() - f.mono_ns > tun::FRAME_STALE_NS) {
            continue;
        }
        // The narrow channel streams at the same rate; dequeue it every tick so
        // it stays fresh whether or not this slot infers on it. A miss is not
        // fatal — the slot falls back to the wide frame (S-13).
        Frame nf{};
        if (nar_cam != nullptr && !nar_cam->grab(nf, NARROW_GRAB_MS)) {
            nf = Frame{};
        }
        // The narrow frame gets the SAME staleness guard as the wide one. A
        // backlogged narrow buffer would otherwise be inferred on and published
        // with this tick's timestamp — the target's PAST position presented as
        // current, with no symptom, since the track looks fresh. Dropping it
        // makes the slot fall back to wide, which is the existing behaviour for
        // "no usable narrow frame".
        if (nf.data != nullptr && mono_now_ns() - nf.mono_ns > tun::FRAME_STALE_NS) {
            nf = Frame{};
        }
        // Cross-channel pairing bound. The age-vs-now guard above cannot see a
        // narrow buffer that aged in its ring while the WIDE grab stalled: the
        // recovery tick then pairs a fresh wide frame with a 100+ ms-old narrow
        // one, and since a narrow measurement is published under the wide
        // frame's timestamp and differenced over the wide-loop dt, the
        // position jump becomes a velocity transient carrying a fresh stamp
        // that no downstream freshness check can reject. Bound the skew
        // directly; out-of-bound falls back to wide like any unusable frame.
        if (nf.data != nullptr &&
            !ChannelMap::frames_pairable(f.mono_ns, nf.mono_ns,
                                         tun::NARROW_WIDE_SKEW_MAX_NS)) {
            nf = Frame{};
        }

        const double dt_s =
            (prev_frame_ns != 0U) ? ns_to_s(f.mono_ns - prev_frame_ns) : 0.0;
        prev_frame_ns = f.mono_ns;
        if (dt_s > 1e-3) {
            fps_est = (0.9 * fps_est) + (0.1 * (1.0 / dt_s));
        }

        // Inference runs on the region the search scheduler selected for this
        // frame (whole frame, one tile of the sweep, or the target's
        // neighbourhood); detections come back in full-frame coordinates.
        //
        // With dual-EO on, the scheduler also says WHICH channel owns this slot
        // (S-11). Capture stays 60 Hz on both cameras; only the inference
        // alternates. A narrow slot needs its ROI in narrow coordinates when
        // that ROI came from the cue, and falls back to the wide frame when the
        // target is outside the narrow field of view at all (S-13).
        Roi roi = scheduler.roi();
        const Frame* infer_frame = &f;
        bool narrow_slot = false;
        if (sc.dual_eo && scheduler.channel() == SearchScheduler::Channel::NARROW &&
            nf.data != nullptr) {
            narrow_slot = true;
            if (scheduler.roi_is_cue_window() && !roi.is_full()) {
                Roi nroi;
                if (cmap.wide_roi_to_narrow(roi, nroi)) {
                    roi = nroi;
                } else {
                    narrow_slot = false; // not visible on the narrow channel
                }
            }
            if (narrow_slot) {
                infer_frame = &nf;
                ++narrow_infers;
            }
        }
        const uint64_t t_infer0 = mono_now_ns();
        const bool det_ok = detect_region(*det, *infer_frame, roi, crop_scratch, dets);
        const double infer_ms = ns_to_ms(mono_now_ns() - t_infer0);

        // T1: an appearance embedding per detection (index-aligned), cropped
        // from the frame the detection was actually MADE in, with the boxes
        // still in that frame's coordinates. Running this after the wide remap
        // shrank a narrow box by ~(narrow_hfov/wide_hfov) and cropped the WIDE
        // frame there — a few blurred pixels whose embedding EMA-poisons the
        // track gallery and can hard-reject the true pairing at channel
        // handover (TR-B), while the full-resolution narrow crop sat unused.
        // Anything that goes wrong — device fault, worker still busy, deadline
        // missed, embedder degraded — leaves embs empty and association runs
        // motion-only, which is the T0 baseline, not an error (TR-3).
        embs.clear();
        if (det_ok && !dets.empty()) {
            (void)embed_worker.embed_by_deadline(*infer_frame, dets, embs);
        }
        // Back into the shared wide-channel coordinates before anything
        // downstream (tracker, fusion, estimator) sees them. The paired remap
        // keeps embs index-aligned through any FOV drops.
        bool narrow_hit = false;
        float narrow_cx = 0.F;
        float narrow_cy = 0.F;
        if (narrow_slot && det_ok && !dets.empty()) {
            narrow_hit = best_detection(dets, target_cls, narrow_cx, narrow_cy);
            cmap.remap_detections_to_wide(dets, embs);
        }
        // R-12: a narrow-channel sample resolves ~3.8x finer, so it moves the
        // smoothed estimate further than a wide one. Reset every frame — the
        // weight belongs to the sample, not to the tracker.
        tracker.set_measurement_gain_scale(narrow_slot ? tun::NARROW_GAIN_SCALE : 1.F);
        const Tracker::Track& trk =
            tracker.update(det_ok ? dets : std::vector<Detection>{}, embs, dt_s);

        TrackCue cue;
        cue.alive = trk.valid;
        cue.hit = trk.valid && trk.misses == 0;
        cue.track_id = trk.id; // identity binding (TR-7)
        cue.cx = trk.cx;
        cue.cy = trk.cy;
        cue.size = trk.size;
        cue.range_m = last_range_m; // previous frame's estimate (R-10 boost gate)
        // The channel this frame's detection actually came from — not the
        // slot's allocation, which may have fallen back to wide (R-11).
        cue.hit_from_narrow = narrow_slot;
        scheduler.update(cue);
        // Commit the primary once the target is confirmed: until then the
        // tracker keeps re-picking the largest target each frame (R-8), after
        // which the engaged target is sticky (S-3).
        tracker.lock_primary(scheduler.confirmed());

        // T2 fusion: on a detection frame the output is the detection box; on a
        // coasted frame the template stands in for the LOS (visual coast), at a
        // degraded quality flagged below. Detection stays authoritative (TR-6).
        Tracker::Track fused = trk;
        bool coasting = false;
        if (tmpl && trk.valid) {
            const TrackFusion::Output fo = fusion.fuse(*tmpl, f, trk, cue.hit);
            if (fo.valid && fo.visual_coast) {
                fused.cx = fo.cx;
                fused.cy = fo.cy;
                fused.quality = fo.quality;
                coasting = true;
            }
        }

        TrackState ts{};
        estimator.estimate(fused, f.mono_ns, dt_s, ts);
        ts.visual_coast = coasting ? 1U : 0U; // TR-5: OBC distinguishes coast
        // R-11: was this track confirmed by BOTH channels? Published so
        // downstream can see the strength of the evidence, not just the verdict.
        ts.dual_confirmed = scheduler.dual_confirmed() ? 1U : 0U;
        // Carry this frame's range to the next cue (R-10 boost gate). FRD
        // position magnitude; 0 (no valid target) leaves the boost off.
        last_range_m = (ts.valid != 0U)
                           ? std::sqrt((ts.rel_pos_frd_m[0] * ts.rel_pos_frd_m[0]) +
                                       (ts.rel_pos_frd_m[1] * ts.rel_pos_frd_m[1]) +
                                       (ts.rel_pos_frd_m[2] * ts.rel_pos_frd_m[2]))
                           : 0.F;
        // R-6: a target is only published as valid once it has been detected in
        // CONFIRM_HITS of the last CONFIRM_WINDOW frames. Before that the track
        // exists internally but the OBC is told "no target" — guidance must not
        // commit to something that has not been seen consistently.
        if (!scheduler.confirmed()) {
            ts.valid = 0;
        }
        // R-8: how many targets are being held, whether or not one is engaged.
        ts.num_targets =
            static_cast<uint8_t>(std::min<std::size_t>(tracker.confirmed_count(), 255U));
        ts.seq = ++track_seq; // increments per publish (TrackState contract)
        track_bus.write(ts);  // always publish; valid=0 tells OBC "no target"

        // Record after perception so the overlay carries this frame's
        // detections. Dual: POST to the worker (copies the frame, runs the
        // narrow detect off-thread — P2-06). Single: one inline non-blocking
        // pipe write. Either way the perception loop is never stalled.
        if (rec_worker) {
            rec_worker->post(f, nf, trk, narrow_hit, narrow_cx, narrow_cy);
        } else if (single_rec != nullptr) {
            single_rec->write(f, f.mono_ns); // single-channel: one pipe write
        }

        // Health at ~2 Hz.
        if (f.mono_ns - last_health_ns > 500'000'000ULL) {
            last_health_ns = f.mono_ns;
            publish_health(
                health_bus, ts,
                StatusSample{f.mono_ns, fps_est, infer_ms, det->healthy(), use_synthetic,
                             tracker.tracks().size(), sc.dual_eo ? narrow_infers : -1},
                scheduler);
            narrow_infers = 0;
        }
    }

    if (rec_worker) {
        rec_worker->stop(); // joins the worker and finalizes the recording
    } else if (single_rec != nullptr) {
        single_rec->close();
        RLOG_INFO("seeker", "recorder: %llu frames written, %llu dropped",
                  static_cast<unsigned long long>(single_rec->frames_written()),
                  static_cast<unsigned long long>(single_rec->frames_dropped()));
    }

    RLOG_INFO("seeker", "shutdown");
    return 0;
}
