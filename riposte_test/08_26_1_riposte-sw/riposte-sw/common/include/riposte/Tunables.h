#pragma once
#include <cstdint>

// All magic numbers live here (AIRYS convention). Values marked [CFG] can be
// overridden at runtime from config/riposte.ini; the rest are fixed by design.

namespace riposte::tun {

// ---- Control loop (riposte-obc) ----
constexpr uint64_t CONTROL_PERIOD_NS = 50'000'000; // 20 Hz fixed
constexpr double JITTER_BUDGET_FRAC = 0.20;        // SM-5: ±20 % of period
constexpr int JITTER_MAX_CONSEC = 3;               // SM-5: consecutive violations
constexpr uint64_t PRESTREAM_NS =
    1'200'000'000; // >=1.0 s pre-stream before Offboard start
constexpr uint64_t CONNECT_TIMEOUT_NS = 30'000'000'000ULL;
// Offboard start is acked before the flight-mode telemetry (heartbeat, ~1 Hz)
// reflects it; OFFBOARD_ACTIVE is entered only after telemetry confirms, else
// SM-2 false-fires on the very next tick (found in PX4 SITL integration).
constexpr uint64_t OFFBOARD_CONFIRM_NS = 3'000'000'000ULL;
// DISENGAGING may leave for READY only once telemetry confirms the FC is out
// of Offboard (P1-03). If neither our stop/hold nor the PX4 offboard-loss
// failsafe produces that confirmation within this window, the mode stream is
// effectively dead -> FAULT (streaming already stopped, D-1 owns the vehicle).
constexpr uint64_t DISENGAGE_CONFIRM_NS = 5'000'000'000ULL;
// Consecutive setpoint send failures tolerated while OFFBOARD_ACTIVE before
// the control session is ended (SB_CMD_LINK, P1-03): flying "active" without
// knowing whether the stream reaches the FC desynchronizes the OBC state from
// the PX4 offboard-loss failsafe.
constexpr int SETPOINT_FAIL_MAX_CONSEC = 5;

// ---- Supervisor (L5) ----
// A shm bus value older than this is STALE: read() only proves a value
// EXISTS, not that its producer is still alive (P2-01). A dead OBC/seeker
// leaves its last sample frozen in shm; beyond this age the supervisor logs
// it as stale and stops treating a frozen OFFBOARD_ACTIVE as current.
constexpr uint64_t SUPERVISOR_STALE_NS = 500'000'000;

// ---- Freshness / safety ----
constexpr uint64_t TELEM_STALE_NS = 500'000'000; // SM-1 (position/velocity stream)
// SM-1 field-level bound for the LOW-RATE telemetry streams (flight mode,
// armed, global position, EKF health, battery — mostly 1 Hz heartbeat
// derived): loose enough that a healthy 1 Hz stream never trips it, tight
// enough that a stopped stream is caught within seconds (OBC-SDD §6, P0-04).
constexpr uint64_t TELEM_FLAG_STALE_NS = 3'000'000'000ULL; // [CFG]
constexpr uint64_t TRACK_STALE_NS = 300'000'000; // SM-7: track older than this => coast
constexpr uint64_t TRACK_COAST_NS = 500'000'000; // SM-7: max coast before disengage
constexpr uint64_t ENGAGE_TIMEBOX_NS = 60'000'000'000ULL; // SM-8 [CFG]

// ---- Velocity clamps (SM-4, applied unconditionally before send) ----
constexpr float VMAX_HORIZONTAL_MPS = 8.0F; // [CFG]
constexpr float VMAX_VERTICAL_MPS = 3.0F;   // [CFG]

// ---- Attitude control (pitch/yaw), SM-4 clamp applied before send [CFG] ----
constexpr float ATT_MAX_TILT_DEG = 35.0F;    // clamp |roll| and |pitch| (safety)
constexpr float ATT_MIN_THRUST = 0.10F;      // never command motors fully off in offboard
constexpr float ATT_MAX_THRUST = 0.80F;      // cap climb authority
constexpr float ATT_HOVER_THRUST = 0.50F;    // nominal hover collective
constexpr float ATT_TRACK_PITCH_DEG = 15.0F; // forward nose-down lean [flight-tune]
constexpr float ATT_THRUST_ELEV_GAIN = 0.15F; // thrust bias from target elevation

// ---- Auto-landing disarm bound ----
// A source-requested disarm (which reads a CONFIGURED land altitude) is refused
// above this AGL; only the FC's own landed flag may disarm higher than this.
constexpr float DISARM_MAX_ALT_M = 0.5F;

// ---- Soft geofence (SM-3, relative to home captured at engage) [CFG] ----
constexpr float GEOFENCE_RADIUS_M = 150.0F;
constexpr float ALT_MIN_M = 2.0F; // AGL, relative altitude
constexpr float ALT_MAX_M = 60.0F;

// ---- Battery gate (SM-9) [CFG] ----
// Remaining-capacity FRACTIONS 0..1 (FcuLink normalizes MAVSDK v3's 0..100
// percent at the boundary). A value of 0 disables the respective check.
// PX4's own low-battery failsafe (COM_LOW_BAT_ACT) stays the final layer;
// SM-9 is the OBC-side gate that refuses to START or CONTINUE an control session
// on a battery that PX4 would soon fail-safe on anyway.
constexpr float BAT_ENGAGE_MIN_FRAC = 0.30F; // READY: engage denied below this
constexpr float BAT_LAND_FRAC = 0.20F;       // in flight: disengage below this

// ---- Guidance (L4) [CFG — flight-test tuning items] ----
constexpr float PN_GAIN = 3.0F;            // N, proportional navigation
constexpr float ENGAGE_SPEED_MPS = 6.0F;   // commanded closing speed
constexpr float MIN_TRACK_QUALITY = 0.30F; // below this, treat as no track

// TARGET command payload sanity bounds (TargetGate). Coarse physical
// plausibility only, NOT mission logic: an authenticated datagram outside these
// is malformed or non-cooperative, never a real cue, so it is rejected before latching.
constexpr float TARGET_POS_MAX_M = 10'000.F;      // |NED coordinate| ceiling
constexpr float TARGET_VEL_MAX_MPS = 100.F;       // |NED velocity| ceiling
constexpr float TARGET_HEADING_MAX_RAD = 6.2832F; // |heading|, +/- 2 pi

// ---- Seeker (L2) ----
// The camera paces the perception loop. FRAME-COUNT constants below are
// calibrated to this rate (durations in their comments assume it) — when the
// rate changes, retune them TOGETHER or their time semantics silently shift
// (e.g. a coast of 8 frames is 267 ms at 30 Hz but 89 ms at 90 Hz).
constexpr int SEEKER_FRAME_HZ = 60; // [CFG]
constexpr uint64_t FRAME_PERIOD_NS =
    1'000'000'000ULL / static_cast<uint64_t>(SEEKER_FRAME_HZ);
constexpr uint64_t FRAME_STALE_NS = 200'000'000; // drop frames older than this
constexpr int TRACKER_MAX_MISSES = 16;           // frames without match => track drop
                                                 // (~267 ms coast @60 Hz)
constexpr int TRACKER_MAX_TRACKS = 8;            // concurrent tracks kept (multi-target)
constexpr float TRACKER_GATE_PX = 120.0F;        // association gate, pixels
constexpr float TRACKER_ALPHA = 0.55F;           // alpha-beta filter position gain
constexpr float TRACKER_BETA = 0.25F;            // alpha-beta filter velocity gain
constexpr int MIN_TRACK_HITS = 2; // detections before a track may publish (glint filter)
// Pre-lock primary reselection hysteresis: before scheduler confirmation locks
// the pick, a challenger takes the primary only when its smoothed size exceeds
// the incumbent's by this factor. Without it, two similar-size tracks flip rank
// on per-frame box noise, and every flip restarts the confirmation window
// (TR-7 identity binding) — confirmation can then never complete (livelock).
// A genuinely nearer object grows 1/range, so 20 % is crossed quickly; the
// locked (post-confirmation) stickiness is unchanged.
constexpr float TRACKER_PRIMARY_SWITCH_MARGIN = 1.2F;
constexpr int TARGET_CLASS = 0; // detector class id treated as the target [CFG]

// ---- Hailo detector (L2) — SEEKER-SDD §4.4 ----
constexpr uint64_t INFER_TIMEOUT_NS = 1'000'000'000; // async infer wait bound
// S-8: device-side score floor, compiled/configured low so recall is a HOST
// decision (seeker.score_thr, mode-dependent) and never needs a HEF recompile.
constexpr float SCORE_THR_DEVICE = 0.15F;
constexpr int HAILO_MAX_CONSEC_FAULTS = 30;  // healthy() drops after ~0.5 s @60 Hz
constexpr int NMS_MAX_DETS = 64;             // per-frame parser output cap
constexpr uint8_t LETTERBOX_PAD_VALUE = 114; // YOLO training-time letterbox fill
// Raw-head fallback decode: cap the threshold-passing candidates (top-K by
// score) before the O(N^2) NMS — an adversarial/garbage tensor could otherwise
// pass thousands of anchors and turn NMS into an 8400^2 stall (P1-06).
constexpr int RAW_DECODE_MAX_CANDS = 512;

// ---- T1 ReID embedder (L2) — RIPOSTE-TRACKER-REQ-001 TR-A/TR-4 ----
// Per-frame cap on detections embedded (highest score first); the rest stay
// invalid = motion-only (TR-3). Bounds the synchronous NPU calls a crowded
// frame can issue (P1-10). Matches TRACKER_MAX_TRACKS: embedding more
// candidates than the tracker can hold buys nothing.
constexpr int REID_EMBED_MAX_PER_FRAME = 8;

// ---- Dual-EO detection accuracy (S-15, DUALEO R-11/R-12) ----
// R-11: hits from BOTH channels are stronger evidence than the same count from
// one, because the channels have independent optics, resolution and noise — a
// false positive rarely reappears at the same bearing in the other. So the
// required hit count drops by this much (floor 2). It never rises: a target
// missing from the narrow channel is not evidence of absence, since the narrow
// FOV is a small window.
constexpr int DUAL_CONFIRM_RELAX = 2;
// R-12: the narrow channel resolves ~3.8x finer, so its measurements deserve
// more trust in the alpha-beta smoother. Scale (clamped below 1) applied to the
// position/velocity gains for a narrow-channel sample only.
constexpr float NARROW_GAIN_SCALE = 1.4F;
constexpr float TRACKER_ALPHA_MAX = 0.85F;
constexpr float TRACKER_BETA_MAX = 0.45F;
// Cross-channel pairing bound: a narrow-slot measurement is published with the
// WIDE frame's timestamp and differenced over the wide-loop dt, so the two
// frames must describe (nearly) the same instant. Latest-wins dequeue keeps
// the skew under one frame period in normal operation; after a wide-grab stall
// the narrow ring's newest buffer can be far older than the recovering wide
// frame while still passing the age-vs-now guard (FRAME_STALE_NS). Three frame
// periods of margin at 60 Hz — beyond it the slot falls back to the wide frame.
constexpr uint64_t NARROW_WIDE_SKEW_MAX_NS = 50'000'000;

// EmbedWorker deadline (TR-4, SDD S-12): how long the perception thread waits
// for the NPU before running the frame motion-only. A 60 Hz frame period is
// 16.6 ms; a third of that keeps the capture cadence intact even if every
// frame misses. [CFG] seeker.embed_deadline_ms — tune against the measured
// INT8 latency at bring-up.
constexpr double EMBED_DEADLINE_MS = 6.0;
// Consecutive deadline misses that degrade the embedder permanently (no
// recovery test, by design — SDD §4.5). At 60 Hz this is half a second of a
// device that cannot keep up, which is not a transient.
constexpr int EMBED_DEADLINE_FAULTS = 30;
// Destructor: how long to wait for the worker to notice `stop` before
// detaching it. A worker blocked inside rknn_run never will, and join() would
// hang shutdown; the shared state outlives the owner, so detaching is safe.
constexpr uint64_t EMBED_DRAIN_NS = 200'000'000ULL;

// ---- T1 ReID association (L2) — RIPOSTE-TRACKER-REQ-001 TR-B [CFG] ----
// Bench items (K-1): both appearance values are placeholders until the
// crossing-scenario ID-retention bench fixes them against the real INT8 model.
constexpr float ASSOC_APPEARANCE_WEIGHT = 0.5F; // cost = motion + this * cos_dist
constexpr float ASSOC_APPEARANCE_REJECT = 0.7F; // in-gate impostor hard reject
constexpr float ASSOC_EMBED_EMA_ALPHA = 0.2F;   // track gallery embedding EMA

// ---- Target EKF quality (L4) — RIPOSTE-ESTIMATION-REQ-001 EST-6 [CFG] ----
// The published track quality is degraded toward MIN_TRACK_QUALITY as the EKF
// position 1-sigma grows: a poorly-observed target (far range, no own-maneuver
// parallax to constrain distance) is flagged low-confidence downstream, without
// dropping below the gate — guidance keeps flying but the OBC/blackbox see it.
constexpr float EKF_SIGMA_QUALITY_FULL_M = 20.0F;  // <= this: full quality
constexpr float EKF_SIGMA_QUALITY_ZERO_M = 120.0F; // >= this: floor quality

// ---- T2 template fusion (L2) — RIPOSTE-TRACKER-REQ-001 TR-C [CFG] ----
constexpr int FUSION_REANCHOR_PERIOD = 15; // re-anchor every N det frames
constexpr int FUSION_MISMATCH_MAX = 5;     // cross-check disagreements -> re-anchor
constexpr float FUSION_COAST_QUALITY_SCALE = 0.6F; // visual-coast quality multiplier

// ---- Search scheduler (L2) — RIPOSTE-DUALEO-REQ-001 R-5/R-6/R-7 [CFG] ----
constexpr int SEARCH_GRID = 3; // R-5: 3x3 tiling of the wide frame
// R-14: fractional ENLARGEMENT of each tile so neighbours overlap. Without it
// the tiles butt together and a target sitting on a seam is split between two
// crops: each half is below the detector's size floor, and any box that does
// come back is truncated — which the monocular range estimate reads as a
// further-away target. 0 reproduces the original touching grid exactly.
constexpr float SEARCH_TILE_OVERLAP = 0.12F; // [CFG]
constexpr int SEARCH_WIDE_DWELL = 2;         // wide passes before falling back to tiles
constexpr int CONFIRM_WINDOW = 10;           // R-6: sliding window length, frames
                                             // (~167 ms @60 Hz; bitmask cap 32)
constexpr int CONFIRM_HITS = 8;              // R-6: >= 80 % of the window
constexpr int TRACK_RECHECK_PERIOD = 30;     // R-7: wide re-detect cadence while
                                             // tracking (~0.5 s @60 Hz)
// Within this range the terminal phase begins: the narrow-EO channel is boosted
// toward true 60 Hz inference (R-10 close-range refinement) by stretching the
// wide-recheck cadence below. 150 m matches the SM-3 operating radius — inside
// it, precision beats coverage. 0 disables the boost. [CFG]
constexpr float NARROW_BOOST_RANGE_M = 150.0F;
// Stretched wide-recheck cadence used inside NARROW_BOOST_RANGE_M: the narrow
// channel keeps all but one frame in ~90 (≈59.3 Hz effective), yet the periodic
// full-frame AI verification (R-7) is never abandoned — it just runs rarer.
constexpr int TRACK_RECHECK_PERIOD_NEAR = 90; // ~1.5 s @60 Hz
constexpr float TRACK_ROI_SCALE = 4.0F;       // target ROI = bbox size x this
constexpr float TRACK_ROI_MIN = 0.15F; // ...never narrower than this (frame fraction)
// R-16 progressive ROI expansion. Each CONSECUTIVE frame without a detection
// widens the inspection window by this factor, because the window is centred on
// a prediction that is drifting exactly while the detections are missing. It is
// not free: a wider crop resizes the target to fewer model-input pixels, so the
// growth is capped rather than run to the whole frame. Reset on the first hit.
constexpr float TRACK_ROI_GROWTH = 1.25F; // [CFG] per consecutive miss
constexpr float TRACK_ROI_MAX = 0.60F;    // [CFG] cap, frame-width fraction

// ---- Wide-channel motion candidates (L2) — DUALEO R-13 [CFG] ----
// Ego-motion is estimated on a DECIMATED image (a global transform does not
// need full resolution) while the residual is examined at FULL resolution — a
// 300 m target spans only ~2-5 px on the wide channel, so decimating the
// residual erases exactly what this is looking for.
constexpr int MOTION_DECIMATE = 4;
// FAST-style corner threshold on the decimated image (0..255 luma step).
constexpr int MOTION_FAST_THRESHOLD = 20;
// Block-matching correspondence search: half-window of the patch and of the
// search range, in DECIMATED pixels. The search range bounds the inter-frame
// ego-motion this can follow; beyond it the fit fails closed (no candidates).
constexpr int MOTION_PATCH_RADIUS = 3;
constexpr int MOTION_SEARCH_RADIUS = 8;
// Correspondences needed before a global fit is attempted at all.
constexpr int MOTION_MIN_CORRESPONDENCES = 12;
// RANSAC: iterations and the inlier distance bound (decimated pixels). The
// sampler is a fixed-seed LCG — a non-reproducible fit cannot be regression-tested.
constexpr int MOTION_RANSAC_ITERS = 64;
// Search half-range of the GUIDED second matching pass, centred on the first
// fit's predicted displacement. Rotation and scale displace a point in
// proportion to its distance from the transform's fixed point, so one
// zero-centred window cannot cover the frame centre and its corners at once —
// without this pass a rotating scene with nothing moving in it aligned from the
// centre correspondences alone and reported candidates across the periphery.
constexpr int MOTION_GUIDED_RADIUS = 3;
constexpr float MOTION_RANSAC_INLIER_PX = 1.5F;
// Fraction of correspondences that must agree with the winning model, else the
// scene is treated as unmodelled (independent motion everywhere / no texture)
// and NO candidates are emitted — a wrong alignment invents moving objects.
constexpr float MOTION_MIN_INLIER_FRAC = 0.5F;
// Residual threshold (full-resolution luma step) and the blob size band, as a
// fraction of image width. The upper bound is what rejects parallax on near
// structures: an independently moving small target is small by definition.
constexpr int MOTION_RESIDUAL_THRESHOLD = 24;
constexpr float MOTION_BLOB_MIN_FRAC = 0.0015F;
constexpr float MOTION_BLOB_MAX_FRAC = 0.08F;
// Per-frame cap on emitted candidates (highest score first).
constexpr int MOTION_MAX_CANDIDATES = 16;

// ---- Video recording (L2, optional) [CFG] ----
constexpr uint64_t RECORD_SEGMENT_NS = 30'000'000'000ULL; // 30 s per MP4 file
constexpr double RECORD_DISK_HIGH_FRAC = 0.80;            // start evicting at 80 % used
constexpr double RECORD_DISK_FREE_FRAC = 0.10;            // reclaim ~10 % of capacity

// ---- IPC names ----
constexpr const char* SHM_TRACK = "/riposte_track";
constexpr const char* SHM_OBC_STATUS = "/riposte_obc_status";
constexpr const char* SHM_SEEKER_HEALTH = "/riposte_seeker_health";
constexpr const char* SHM_GPS = "/riposte_gps"; // obc -> seeker (recording overlay)
constexpr const char* OBC_CMD_SOCKET = "/run/riposte/obc.sock";

} // namespace riposte::tun
