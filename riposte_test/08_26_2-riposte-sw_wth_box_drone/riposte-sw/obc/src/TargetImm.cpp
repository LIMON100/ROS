#include "TargetImm.h"

#include "TargetEkf.h"

#include <cmath>

namespace riposte {

namespace {
constexpr int N = TargetEkf::N;
} // namespace

// Interaction / mixing: form each model's mixed prior from BOTH models weighted
// by the Markov transition and the current model probabilities. This is what
// lets probability (and state) flow between models between steps.
void TargetImm::mix() {
    if (!m_[0].initialized() || !m_[1].initialized()) {
        return;
    }
    // transition[i][j] = P(model j now | model i prev).
    const float t[2][2] = {{p_.p_stay, 1.0F - p_.p_stay}, {1.0F - p_.p_stay, p_.p_stay}};
    // Predicted model probabilities cbar_j = sum_i t[i][j] mu_i.
    for (int j = 0; j < 2; ++j) {
        cbar_[j] = (t[0][j] * mu_[0]) + (t[1][j] * mu_[1]);
        if (cbar_[j] < 1e-9F) {
            cbar_[j] = 1e-9F;
        }
    }

    float x[2][N];
    float P[2][N][N];
    m_[0].get_state(x[0], P[0]);
    m_[1].get_state(x[1], P[1]);

    for (int j = 0; j < 2; ++j) {
        // Mixing weights w_ij = t[i][j] mu_i / cbar_j.
        const float w0 = (t[0][j] * mu_[0]) / cbar_[j];
        const float w1 = (t[1][j] * mu_[1]) / cbar_[j];
        float x0[N];
        for (int k = 0; k < N; ++k) {
            x0[k] = (w0 * x[0][k]) + (w1 * x[1][k]);
        }
        float P0[N][N];
        for (int r = 0; r < N; ++r) {
            const float d0 = x[0][r] - x0[r];
            const float d1 = x[1][r] - x0[r];
            for (int c = 0; c < N; ++c) {
                const float e0 = x[0][c] - x0[c];
                const float e1 = x[1][c] - x0[c];
                P0[r][c] =
                    (w0 * (P[0][r][c] + (d0 * e0))) + (w1 * (P[1][r][c] + (d1 * e1)));
            }
        }
        // Write the mixed prior back into model j.
        m_[j].set_state(x0, P0);
    }
}

void TargetImm::predict(double dt_s) {
    mix(); // interaction uses the probabilities from the last update
    m_[0].predict(dt_s);
    m_[1].predict(dt_s);
}

void TargetImm::update(const float rel_pos_frd[3], const OwnState& own) {
    const bool was_init = m_[0].initialized();
    m_[0].update(rel_pos_frd, own);
    m_[1].update(rel_pos_frd, own);
    if (!was_init) {
        // First measurement initialized both models identically; keep equal
        // probabilities until there is a likelihood to weigh them by.
        mu_[0] = 0.5F;
        mu_[1] = 0.5F;
        return;
    }
    // Model probability update: mu_j = cbar_j L_j / sum_k cbar_k L_k.
    const float l0 = m_[0].last_likelihood();
    const float l1 = m_[1].last_likelihood();
    const float u0 = cbar_[0] * l0;
    const float u1 = cbar_[1] * l1;
    const float sum = u0 + u1;
    if (sum < 1e-30F) {
        // Both models vanishingly unlikely (e.g. an outlier): hold the prior
        // rather than divide by zero.
        return;
    }
    mu_[0] = u0 / sum;
    mu_[1] = u1 / sum;
}

void TargetImm::combined_pos(float out[3]) const {
    for (int i = 0; i < 3; ++i) {
        out[i] = 0.F;
    }
    if (!m_[0].initialized()) {
        return;
    }
    for (int j = 0; j < 2; ++j) {
        const float* p = m_[j].pos_ned();
        for (int i = 0; i < 3; ++i) {
            out[i] += mu_[j] * p[i];
        }
    }
}

void TargetImm::relative_state(const OwnState& own, float rel_pos_frd[3],
                               float rel_vel_frd[3]) const {
    for (int i = 0; i < 3; ++i) {
        rel_pos_frd[i] = 0.F;
        rel_vel_frd[i] = 0.F;
    }
    if (!m_[0].initialized()) {
        return;
    }
    // Probability-weighted blend of the two models' relative states.
    for (int j = 0; j < 2; ++j) {
        float rp[3];
        float rv[3];
        m_[j].relative_state(own, rp, rv);
        for (int i = 0; i < 3; ++i) {
            rel_pos_frd[i] += mu_[j] * rp[i];
            rel_vel_frd[i] += mu_[j] * rv[i];
        }
    }
}

float TargetImm::position_sigma() const {
    if (!m_[0].initialized()) {
        return 0.F;
    }
    // IMM mixture 1-sigma (P2-07). The combined position covariance is
    //   P_c = sum_j mu_j [ P_j + (p_j - p_c)(p_j - p_c)^T ],
    // so its per-axis mean variance is
    //   sigma^2 = sum_j mu_j ( sigma_j^2 + ||p_j - p_c||^2 / 3 ).
    // The old code averaged the sigmas linearly, which both blended sigma
    // instead of variance AND dropped the spread term — underestimating
    // uncertainty exactly when the two models disagree (a maneuver
    // transition), which is when the EST-6 quality degrade matters most.
    float pc[3];
    combined_pos(pc);
    float var = 0.F;
    for (int j = 0; j < 2; ++j) {
        const float sj = m_[j].position_sigma();
        const float* pj = m_[j].pos_ned();
        float spread = 0.F;
        for (int i = 0; i < 3; ++i) {
            const float d = pj[i] - pc[i];
            spread += d * d;
        }
        var += mu_[j] * ((sj * sj) + (spread / 3.0F));
    }
    return std::sqrt(var);
}

} // namespace riposte
