#pragma once

namespace riposte {

// Augmented-state moving-target EKF with joint size/range estimation
// (RIPOSTE-ESTIMATION-REQ-001 §5.5 / EST-P7a).
//
// State (10): [p_ned(3), v_ned(3), a_ned(3), s], s = log(target_size_m). The
// kinematics are the same constant-acceleration model as TargetEkf; augmenting
// the log-size makes the assumed-size error an UNOBSERVABLE bias rather than one
// a random measurement noise averages away (the §5.2 systematic-error limit
// that made EST-6 only partial). Size is time-invariant, so its process noise is
// tiny — it moves only under measurement.
//
// Observation (3) is the RAW monocular measurement, not a size-baked position:
//   z = [az, el, log(apparent_angular_size)]
//   h(x): rel_frd = DCM^T(att) (p - p_own); az/el = bearing; range = |rel_frd|;
//         log(apparent) = s - log(range) + const.
// Bearing is precise (small R); the size term couples s and range through
// d/ds = +1, d/drange = -1/range. So own-STATIONARY keeps the size/range
// ambiguity in the covariance (honest low confidence), and own-WEAVING (parallax)
// separates them — self-calibrating range without a range sensor.
//
// The Jacobian is computed numerically (finite difference of h) to avoid a
// hand-derived error; h(x) is exact. Pure logic — own state and the raw
// observation are injected as values, host unit-tested against synthetic
// trajectories. No pipeline wiring here (that is EST-P7b).
class SizeRangeEkf {
public:
    struct Params {
        float sigma_jerk = 4.0F;          // kinematic process noise (m/s^3)
        float sigma_logsize = 0.02F;      // log-size process noise (near-constant)
        float sigma_bearing_rad = 0.002F; // bearing measurement std (rad)
        float sigma_logsize_meas = 0.15F; // apparent-size measurement std (log)
        // Prior on the target's true size, used to seed range from the first
        // apparent size. Wide prior => range trusted less until parallax.
        float log_size_prior = -1.05F; // log(0.35 m) ~ drone span
        float init_size_unc = 0.6F;    // log-size 1-sigma at init
        float init_vel_unc = 30.0F;
        float init_acc_unc = 10.0F;
        float init_range_unc_rel = 0.5F; // position 1-sigma as a fraction of range
    };

    struct OwnState {
        float pos_ned[3] = {0, 0, 0};
        float vel_ned[3] = {0, 0, 0};
        float roll_rad = 0.F, pitch_rad = 0.F, yaw_rad = 0.F;
    };

    SizeRangeEkf() = default;
    explicit SizeRangeEkf(const Params& p) : p_(p) {}

    bool initialized() const { return init_; }
    void reset() { init_ = false; }

    void predict(double dt_s);
    // Measurement: az/el bearing (rad) and log apparent angular size. First call
    // initializes: range is seeded from the size prior and the apparent size.
    void update(float az, float el, float log_apparent, const OwnState& own);

    // Estimated range to target (m) and its 1-sigma.
    float range(const OwnState& own) const;
    float range_sigma() const;
    float log_size() const { return x_[9]; }
    const float* pos_ned() const { return x_; }
    const float* vel_ned() const { return x_ + 3; }

    static constexpr int N = 10;

private:
    // Pure measurement model h(x): no filter state involved, so static — it
    // reads only the state vector and own-vehicle pose it is handed.
    static void observe(const float x[N], const OwnState& own, float z[3]);

    Params p_{};
    bool init_ = false;
    float x_[N] = {};
    float P_[N][N] = {};
};

} // namespace riposte
