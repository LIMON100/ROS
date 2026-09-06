#pragma once
#include "TargetEkf.h"

namespace riposte {

// Interacting Multiple Model (IMM) target estimator (ESTIMATION-REQ EST-P3).
//
// Runs two constant-acceleration TargetEkf models in parallel that differ only
// in process noise — a LOW-jerk model (smooth, trusted while the target flies
// straight) and a HIGH-jerk model (responsive, trusted while it maneuvers) —
// and blends them by their measurement likelihoods. A single fixed-Q filter has
// to compromise: too little noise lags a maneuver, too much stays jittery on a
// straight leg. The IMM gets both because the model probabilities shift toward
// whichever model is currently explaining the measurements, so straight legs
// ride the smooth model and maneuvers switch to the agile one with no lag
// (EST-4). Standard IMM: interaction/mixing -> per-model filter -> probability
// update -> combination.
//
// Same value-injected, pure-logic contract as TargetEkf; the relative-state
// output mirrors it, so this can later replace the single filter behind the
// GuidanceSource integration (EST-P4) without changing that seam.
class TargetImm {
public:
    struct Params {
        TargetEkf::Params low;  // smooth model (small sigma_jerk)
        TargetEkf::Params high; // agile model (large sigma_jerk)
        // Markov transition probabilities: p_stay is the chance a model stays
        // itself between steps; (1-p_stay) leaks to the other.
        float p_stay = 0.95F;
    };

    using OwnState = TargetEkf::OwnState;

    TargetImm() {
        p_.low.sigma_jerk = 1.0F;
        p_.high.sigma_jerk = 12.0F;
        m_[0] = TargetEkf(p_.low);
        m_[1] = TargetEkf(p_.high);
    }
    explicit TargetImm(const Params& p) : p_(p) {
        m_[0] = TargetEkf(p_.low);
        m_[1] = TargetEkf(p_.high);
    }

    bool initialized() const { return m_[0].initialized(); }
    void reset() {
        m_[0].reset();
        m_[1].reset();
        mu_[0] = 0.5F;
        mu_[1] = 0.5F;
    }

    void predict(double dt_s);
    void update(const float rel_pos_frd[3], const OwnState& own);

    void relative_state(const OwnState& own, float rel_pos_frd[3],
                        float rel_vel_frd[3]) const;
    float position_sigma() const;

    // Combined absolute-NED estimate and the current model probabilities.
    void combined_pos(float out[3]) const;
    float model_prob(int i) const { return mu_[i]; }

private:
    static constexpr int N = TargetEkf::N;
    void mix();

    Params p_{};
    TargetEkf m_[2];
    float mu_[2] = {0.5F, 0.5F};
    float cbar_[2] = {0.5F, 0.5F}; // predicted model probs from the last mix()
};

} // namespace riposte
