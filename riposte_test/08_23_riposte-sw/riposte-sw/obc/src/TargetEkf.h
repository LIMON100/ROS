#pragma once
#include <cstdint>

namespace riposte {

// Moving-target state estimator (RIPOSTE-ESTIMATION-REQ-001 EST-P1/EST-P3).
//
// Estimates the target's ABSOLUTE NED position, velocity AND acceleration with a
// constant-acceleration (CA) EKF, so the vehicle's own motion is separated from
// the target's (EST-A / EST-3): the measurement is the target's absolute
// position, formed from the monocular relative-FRD geometry plus the PX4-EKF2
// own state (position + attitude) the telemetry already carries. Own velocity
// divides out when the relative velocity is read back, so a maneuvering
// ownship no longer smears the target's own track.
//
// Carrying acceleration (CA over CV) is what lets the filter follow a
// MANEUVERING target — a turning or accelerating target no longer lags behind a
// constant-velocity assumption (EST-4). White jerk drives the process noise.
//
// The hard part is range observability (EST-2): a monocular bearing gives no
// range; range comes from apparent size and is only as certain as the assumed
// target size. That uncertainty is carried HONESTLY as an ANISOTROPIC
// measurement covariance — small across the line of sight (bearing is precise),
// large along it (range from size is not). Own-maneuver parallax then tightens
// the along-LOS direction over time. No range sensor (§5.3).
//
// Pure logic: no vendor SDK, no shm. Own state and the monocular measurement
// are injected as values, so the whole filter is host unit-tested against
// synthetic trajectories, the same split as ModelIo / AssocCost / TrackFusion.
class TargetEkf {
public:
    struct Params {
        // Target jerk process-noise std (m/s^3): how fast the target's
        // acceleration may change. Larger => filter follows maneuvers faster but
        // trusts noisier estimates.
        float sigma_jerk = 4.0F;
        // Measurement std ACROSS the line of sight (m per unit range): bearing
        // precision. Multiplied by range to get metres.
        float sigma_bearing_rel = 0.01F;
        // Relative range uncertainty (sigma_L / L): the assumed-target-size error
        // that dominates along-LOS accuracy. Multiplied by range to get metres.
        float sigma_range_rel = 0.30F;
        // Initial position / velocity / acceleration std for a fresh track.
        float init_pos_unc = 100.0F;
        float init_vel_unc = 30.0F;
        float init_acc_unc = 10.0F;
    };

    // PX4-EKF2 own-vehicle state (from TelemetrySnapshot).
    struct OwnState {
        float pos_ned[3] = {0, 0, 0};
        float vel_ned[3] = {0, 0, 0};
        float roll_rad = 0.F, pitch_rad = 0.F, yaw_rad = 0.F;
    };

    TargetEkf() = default;
    explicit TargetEkf(const Params& p) : p_(p) {}

    bool initialized() const { return init_; }
    void reset() { init_ = false; }

    // Constant-acceleration time update. No-op until the first measurement.
    void predict(double dt_s);

    // Measurement update from the monocular relative-FRD position and the own
    // state. `rel_pos_frd` is what TargetEstimator's geometry already produces
    // (range from size, bearing from bbox centre). The first call initializes.
    void update(const float rel_pos_frd[3], const OwnState& own);

    // Relative target state in BODY FRD for TrackBus (own velocity removed).
    // Requires initialized(); writes zeros otherwise.
    void relative_state(const OwnState& own, float rel_pos_frd[3],
                        float rel_vel_frd[3]) const;

    // 1-sigma position uncertainty (m), sqrt of the position covariance trace /3.
    // Feeds the published quality: a poorly observed target reports low quality.
    float position_sigma() const;

    // Absolute-NED accessors (diagnostics / tests).
    const float* pos_ned() const { return x_; }
    const float* vel_ned() const { return x_ + 3; }
    const float* acc_ned() const { return x_ + 6; }

    static constexpr int N = 9; // state dimension [p(3) v(3) a(3)]

    // --- IMM support (RIPOSTE-ESTIMATION-REQ-001 EST-P3) --------------------
    // Read/overwrite the raw filter state so an IMM can mix models between
    // steps; likelihood of the last measurement given this model (Gaussian of
    // the innovation), which the IMM uses to update model probabilities.
    void get_state(float x_out[N], float P_out[N][N]) const;
    void set_state(const float x_in[N], const float P_in[N][N]);
    float last_likelihood() const { return likelihood_; }
    void force_initialized() { init_ = true; }

private:
    Params p_{};
    bool init_ = false;
    float likelihood_ = 1.0F; // of the last measurement given this model (IMM)
    float x_[N] = {};         // [pn pe pd vn ve vd an ae ad], absolute NED
    float P_[N][N] = {};
};

} // namespace riposte
