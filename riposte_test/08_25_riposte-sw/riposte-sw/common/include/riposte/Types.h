#pragma once
#include <cstdint>

namespace riposte {

// ---------------------------------------------------------------------------
// TrackBus payload: seeker -> obc/supervisor  (shm SeqSlot /riposte_track)
//
// Relative target state in BODY FRD frame (x fwd, y right, z down), taken at
// the camera frame timestamp. The seeker has no attitude source; conversion
// to NED happens in the OBC's GuidanceSource using the FC telemetry snapshot
// (full roll/pitch/yaw attitude DCM).
// ---------------------------------------------------------------------------
struct TrackState {
    uint64_t mono_ns; // camera frame timestamp (freshness basis, SM-7)
    uint32_t seq;     // increments per publish
    uint32_t track_id;
    float rel_pos_frd_m[3];   // target position relative to vehicle, body FRD
    float rel_vel_frd_mps[3]; // target velocity relative to vehicle, body FRD
    float quality;            // 0..1 (confidence x continuity)
    uint8_t valid;            // 0 = no target
    // Number of CONFIRMED targets the seeker is holding, of which this sample is
    // the primary (REQ-001 R-8). Placed in existing padding so sizeof(TrackState)
    // — the seqlock's cross-process ABI check — is unchanged; a writer that never
    // sets it simply reports 0, which reads as "not published" rather than as a
    // wrong count. TRACKER_MAX_TRACKS is 8, so a byte is ample.
    uint8_t num_targets;
    // 1 = this sample's position came from the T2 template ("visual coast"),
    // not from a fresh detection this frame (RIPOSTE-TRACKER-REQ-001 TR-5).
    // The publish is fresh (SM-7 sees a current mono_ns), but it is NOT
    // detection-anchored, so the OBC treats it more conservatively. Placed in
    // existing padding so sizeof(TrackState) is unchanged; a writer that never
    // sets it reports 0 = "detection-anchored", the safe reading.
    uint8_t visual_coast;
    // 1 = this track was confirmed with hits from BOTH EO channels (R-11).
    // Independent optics and noise make a cross-channel agreement stronger
    // evidence than the same hit count from one channel, and downstream is
    // entitled to see WHY a track was believed. Placed in existing padding so
    // sizeof(TrackState) — the seqlock's cross-process ABI check — is
    // unchanged; a writer that never sets it reports 0, which reads as
    // "single-channel evidence", the conservative answer.
    uint8_t dual_confirmed;
};

// The seqlock rejects a peer whose sizeof(T) differs (SeqSlot ABI check), so a
// layout change here silently stops the seeker and the OBC talking until both
// are redeployed together. Pin the size: fields may only be added by consuming
// `pad`, and growing past it is a deliberate, coordinated change — not something
// to discover in the field.
static_assert(sizeof(TrackState) == 48, "TrackBus ABI: extend via pad, not size");

// FC telemetry snapshot: MAVSDK callbacks write, control thread reads (G1).
struct TelemetrySnapshot {
    // Per-stream arrival stamps (OBC-SDD §6 SM-1 field-level freshness, P0-04).
    // One shared stamp hid stopped streams behind a live position feed: a stale
    // in_offboard=1 masked a pilot override, a stale rel_alt fed SM-3. Every
    // consumer of a flag/field below must check the stamp of ITS stream —
    // mono_ns (the position stream) alone proves nothing about the others.
    uint64_t mono_ns;        // position/velocity stream (SM-1 basis, 500 ms bound)
    uint64_t att_mono_ns;    // attitude stream
    uint64_t gpos_mono_ns;   // global-position stream (rel_alt/lat/lon/abs_alt)
    uint64_t mode_mono_ns;   // flight-mode stream (in_offboard, SM-2)
    uint64_t armed_mono_ns;  // armed flag (SM-6)
    uint64_t landed_mono_ns; // landed state (AUTO_LANDING disarm gate)
    uint64_t health_mono_ns; // EKF health (position_ok)
    uint64_t bat_mono_ns;    // battery (SM-9)
    float pos_ned_m[3];      // local position NED (origin = EKF origin)
    float vel_ned_mps[3];
    float roll_rad; // body attitude (full FRD->NED DCM in guidance)
    float pitch_rad;
    float yaw_rad;
    float rel_alt_m; // altitude above home (SM-3)
    double lat_deg;  // global position (for recording overlay / logging)
    double lon_deg;  //   "
    float abs_alt_m; // AMSL altitude
    // Remaining battery as a FRACTION 0..1 (SM-9). MAVSDK v3 reports
    // remaining_percent as 0..100 percent (v1/v2 was a 0..1 fraction);
    // FcuLink normalizes at the boundary so nothing downstream ever sees
    // percent. Meaningful only while battery_ok != 0.
    float battery_frac;
    uint8_t armed;
    uint8_t in_offboard; // current flight mode == OFFBOARD
    uint8_t position_ok; // local position valid (EKF healthy)
    uint8_t connected;
    uint8_t gps_ok;     // global position (lat/lon) valid
    uint8_t landed;     // FC reports landed/on-ground state when available
    uint8_t battery_ok; // battery telemetry received and in range (SM-9)
    uint8_t pad;

    // Marks every stream as having arrived at `now` — for the SIL stub and
    // test fixtures, where all fields are synthesized in one place. The real
    // FcuLink stamps each stream individually in its own callback.
    void stamp_all_streams(uint64_t now) {
        mono_ns = now;
        att_mono_ns = now;
        gpos_mono_ns = now;
        mode_mono_ns = now;
        armed_mono_ns = now;
        landed_mono_ns = now;
        health_mono_ns = now;
        bat_mono_ns = now;
    }
};

// GpsBus payload: obc -> seeker (shm SeqSlot /riposte_gps). Read-only in the
// seeker; used solely to stamp GPS onto recorded video, never for flight.
struct GpsSample {
    uint64_t mono_ns;
    double lat_deg;
    double lon_deg;
    float alt_m; // AMSL
    uint8_t fix_ok;
    uint8_t pad[3];
};

// MissionTarget is the low-rate external target cue from an operator tablet or
// external device. It is intentionally used only for initial navigation to a
// search/hold point; seeker tracks remain the only close-range visual cue.
struct MissionTarget {
    uint64_t mono_ns = 0;
    uint32_t seq = 0;
    float pos_ned_m[3] = {0.F, 0.F, 0.F};
    // Target velocity, NED. All-zero means the cue reports no motion, which the
    // rendezvous solver treats as a static point. Without this the OBC can only
    // fly at where the target WAS, and the whole transit is spent trailing it.
    float vel_ned_mps[3] = {0.F, 0.F, 0.F};
    float heading_rad = 0.F;
    uint8_t valid = 0;
};

struct VelocitySetpointNed {
    float vn_mps = 0.F;
    float ve_mps = 0.F;
    float vd_mps = 0.F;
    float yaw_rad = 0.F;
};

// Attitude setpoint for offboard attitude control: body roll/pitch/yaw angles
// (deg) plus normalized collective thrust [0,1] (hover ~0.5). Enables direct
// pitch/yaw steering (terminal guidance) instead of the velocity path. PX4/MAVSDK
// convention: +pitch = nose up, +roll = right-wing down, yaw = heading (NED).
struct AttitudeSetpoint {
    float roll_deg = 0.F;
    float pitch_deg = 0.F;
    float yaw_deg = 0.F;
    float thrust = 0.F;
};

enum class ObcState : uint8_t {
    IDLE,
    CONNECTING,
    READY,
    PRESTREAM,
    OFFBOARD_ACTIVE,
    DISENGAGING,
    AUTO_LANDING,
    FAULT
};

inline const char* to_string(ObcState s) {
    switch (s) {
        case ObcState::IDLE:
            return "IDLE";
        case ObcState::CONNECTING:
            return "CONNECTING";
        case ObcState::READY:
            return "READY";
        case ObcState::PRESTREAM:
            return "PRESTREAM";
        case ObcState::OFFBOARD_ACTIVE:
            return "OFFBOARD_ACTIVE";
        case ObcState::DISENGAGING:
            return "DISENGAGING";
        case ObcState::AUTO_LANDING:
            return "AUTO_LANDING";
        case ObcState::FAULT:
            return "FAULT";
    }
    return "?";
}

// SafetyMonitor violation bits (SM-x mapping, see RIPOSTE-SAD-001 §8).
// Deliberately a plain enum with uint32_t base: values are OR-ed into
// violation_mask (uint32_t) and bit positions are a fixed external contract —
// never renumber, append only (G8.2). enum class would force casts at every
// mask operation for no safety gain here.
// NOLINTNEXTLINE(cppcoreguidelines-use-enum-class,performance-enum-size)
enum SafetyBit : uint32_t {
    SB_TELEM_STALE = 1U << 0,    // SM-1
    SB_MODE_OVERRIDE = 1U << 1,  // SM-2
    SB_GEOFENCE = 1U << 2,       // SM-3
    SB_JITTER = 1U << 4,         // SM-5
    SB_DISARMED = 1U << 5,       // SM-6
    SB_TRACK_STALE = 1U << 6,    // SM-7 (source reported it cannot compute)
    SB_ENGAGE_TIMEBOX = 1U << 7, // SM-8
    SB_BATTERY = 1U << 8,        // SM-9 (known-low battery while engaged)
    SB_FENCE_POLY = 1U << 9,     // SM-10 (outside the configured polygon boundary)
    SB_FIELD_STALE = 1U << 10,   // SM-1 field-level: a low-rate safety stream
                                 // (global position / armed / EKF health) went
                                 // stale while ACTIVE (OBC-SDD §6, P0-04)
    SB_POS_INVALID = 1U << 11,   // SM-1 field-level: EKF declared the local
                                 // position invalid (position_ok == 0) while
                                 // ACTIVE — guiding on it is prohibited
    SB_CMD_LINK = 1U << 12,      // consecutive setpoint send failures while
                                 // ACTIVE (P1-03): the command link to the FC
                                 // cannot be trusted, end the control session
};

// StatusBus payload: obc -> supervisor (shm SeqSlot /riposte_obc_status)
struct ObcStatus {
    uint64_t mono_ns;
    uint32_t violation_mask; // last evaluated SafetyBit set
    float loop_jitter_ms;    // period error measured on the last evaluated tick
    uint32_t engage_count;
    ObcState state;
    uint8_t pad[3];
};

// HealthBus payload: seeker -> supervisor (shm SeqSlot /riposte_seeker_health)
struct SeekerHealth {
    uint64_t mono_ns;
    float fps;
    float infer_latency_ms;
    uint8_t camera_ok;
    uint8_t detector_ok;
    uint8_t track_valid;
    // 1 = SIL synthetic pipeline. Downstream (supervisor blackbox, GCS) must
    // be able to tell simulated perception from real perception without the
    // seeker's logs. Repurposed pad byte — struct size/ABI unchanged.
    uint8_t synthetic;
};

// ---------------------------------------------------------------------------
// Command channel (UDS datagram /run/riposte/obc.sock).
// ENGAGE requires the operator token (D-2 / SI-2: no autonomous engage path).
// ---------------------------------------------------------------------------
enum class ObcCommandType : uint8_t {
    ENGAGE = 1,
    DISENGAGE = 2,
    TARGET = 3,
    OPERATOR_HOLD = 4,
    RETURN_HOME = 5
};

struct ObcCommand {
    uint32_t magic; // must equal OBC_COMMAND_MAGIC
    ObcCommandType type;
    uint8_t pad[3];
    char token[32]; // NUL-padded operator token (ENGAGE/TARGET only)
    // TARGET command payload. Coordinates are local NED metres in the same EKF
    // origin as TelemetrySnapshot::pos_ned_m. target_down_m is negative above
    // the origin. heading_rad is the target drone's reported course/yaw.
    float target_pos_ned_m[3];
    // Reported target velocity (NED). Zero for a cue that carries no motion.
    // Adding this changed sizeof(ObcCommand); the receiver requires an EXACT
    // size match, so an older sender is rejected outright rather than parsed
    // into misaligned fields — rebuild engage_cli and the OBC together.
    float target_vel_ned_mps[3];
    float target_heading_rad;
    uint32_t target_seq;
};

constexpr uint32_t OBC_COMMAND_MAGIC = 0x52495032; // "RIP2"

} // namespace riposte
