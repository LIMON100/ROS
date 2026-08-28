#include "SizeRangeEkf.h"

#include <algorithm>
#include <cmath>

namespace riposte {

namespace {

constexpr int N = SizeRangeEkf::N;

void dcm_frd_to_ned(float roll, float pitch, float yaw, float m[3][3]) {
    const float cr = std::cos(roll);
    const float sr = std::sin(roll);
    const float cp = std::cos(pitch);
    const float sp = std::sin(pitch);
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    m[0][0] = cp * cy;
    m[0][1] = (sr * sp * cy) - (cr * sy);
    m[0][2] = (cr * sp * cy) + (sr * sy);
    m[1][0] = cp * sy;
    m[1][1] = (sr * sp * sy) + (cr * cy);
    m[1][2] = (cr * sp * sy) - (sr * cy);
    m[2][0] = -sp;
    m[2][1] = sr * cp;
    m[2][2] = cr * cp;
}

// Transpose(m) * v — inverse rotation for a DCM.
void mat3_t_vec(const float m[3][3], const float v[3], float out[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = (m[0][i] * v[0]) + (m[1][i] * v[1]) + (m[2][i] * v[2]);
    }
}

bool inv3(const float a[3][3], float out[3][3]) {
    const float c00 = (a[1][1] * a[2][2]) - (a[1][2] * a[2][1]);
    const float c01 = (a[1][2] * a[2][0]) - (a[1][0] * a[2][2]);
    const float c02 = (a[1][0] * a[2][1]) - (a[1][1] * a[2][0]);
    const float det = (a[0][0] * c00) + (a[0][1] * c01) + (a[0][2] * c02);
    if (std::fabs(det) < 1e-18F) {
        return false;
    }
    const float id = 1.0F / det;
    out[0][0] = c00 * id;
    out[1][0] = c01 * id;
    out[2][0] = c02 * id;
    out[0][1] = ((a[0][2] * a[2][1]) - (a[0][1] * a[2][2])) * id;
    out[1][1] = ((a[0][0] * a[2][2]) - (a[0][2] * a[2][0])) * id;
    out[2][1] = ((a[0][1] * a[2][0]) - (a[0][0] * a[2][1])) * id;
    out[0][2] = ((a[0][1] * a[1][2]) - (a[0][2] * a[1][1])) * id;
    out[1][2] = ((a[0][2] * a[1][0]) - (a[0][0] * a[1][2])) * id;
    out[2][2] = ((a[0][0] * a[1][1]) - (a[0][1] * a[1][0])) * id;
    return true;
}

} // namespace

void SizeRangeEkf::observe(const float x[N], const OwnState& own, float z[3]) {
    float dcm[3][3];
    dcm_frd_to_ned(own.roll_rad, own.pitch_rad, own.yaw_rad, dcm);
    float rel_ned[3];
    for (int i = 0; i < 3; ++i) {
        rel_ned[i] = x[i] - own.pos_ned[i];
    }
    float rel_frd[3];
    mat3_t_vec(dcm, rel_ned, rel_frd);
    const float horiz = std::sqrt((rel_frd[0] * rel_frd[0]) + (rel_frd[1] * rel_frd[1]));
    const float rng = std::sqrt((horiz * horiz) + (rel_frd[2] * rel_frd[2]));
    z[0] = std::atan2(rel_frd[1], rel_frd[0]);             // az
    z[1] = std::atan2(rel_frd[2], std::max(horiz, 1e-6F)); // el
    // log apparent angular size = log(size / range) = s - log(range).
    z[2] = x[9] - std::log(std::max(rng, 1e-3F));
}

void SizeRangeEkf::predict(double dt_s) {
    if (!init_ || dt_s <= 0.0) {
        return;
    }
    const auto dt = static_cast<float>(dt_s);
    const float h2 = 0.5F * dt * dt;

    for (int i = 0; i < 3; ++i) {
        x_[i] += (x_[i + 3] * dt) + (x_[i + 6] * h2);
        x_[i + 3] += x_[i + 6] * dt;
    }
    // s (x_[9]) is time-invariant: unchanged.

    float F[N][N] = {};
    for (int i = 0; i < N; ++i) {
        F[i][i] = 1.0F;
    }
    for (int i = 0; i < 3; ++i) {
        F[i][i + 3] = dt;
        F[i][i + 6] = h2;
        F[i + 3][i + 6] = dt;
    }

    float FP[N][N] = {};
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            float s = 0.F;
            for (int k = 0; k < N; ++k) {
                s += F[i][k] * P_[k][j];
            }
            FP[i][j] = s;
        }
    }
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            float s = 0.F;
            for (int k = 0; k < N; ++k) {
                s += FP[i][k] * F[j][k];
            }
            P_[i][j] = s;
        }
    }

    const float sj2 = p_.sigma_jerk * p_.sigma_jerk;
    const float d3 = dt * dt * dt;
    const float qpp = sj2 * d3 * dt * dt / 20.0F;
    const float qpv = sj2 * dt * dt * dt * dt / 8.0F;
    const float qpa = sj2 * d3 / 6.0F;
    const float qvv = sj2 * d3 / 3.0F;
    const float qva = sj2 * dt * dt / 2.0F;
    const float qaa = sj2 * dt;
    for (int i = 0; i < 3; ++i) {
        P_[i][i] += qpp;
        P_[i][i + 3] += qpv;
        P_[i + 3][i] += qpv;
        P_[i][i + 6] += qpa;
        P_[i + 6][i] += qpa;
        P_[i + 3][i + 3] += qvv;
        P_[i + 3][i + 6] += qva;
        P_[i + 6][i + 3] += qva;
        P_[i + 6][i + 6] += qaa;
    }
    P_[9][9] += p_.sigma_logsize * p_.sigma_logsize * dt;
}

// G16.6 deviation: one cohesive EKF measurement update; splitting scatters the covariance
// algebra (G16.6) NOLINTNEXTLINE(readability-function-size)
void SizeRangeEkf::update(float az, float el, float log_apparent, const OwnState& own) {
    if (!init_) {
        // Seed size from the prior, then range from apparent size:
        //   log(apparent) = s - log(range) => range = exp(s - log_apparent).
        const float s0 = p_.log_size_prior;
        const float rng = std::exp(s0 - log_apparent);
        // Bearing -> unit LOS in FRD, rotate to NED, place the target.
        const float ch = std::cos(el);
        const float los_frd[3] = {ch * std::cos(az), ch * std::sin(az), std::sin(el)};
        float dcm[3][3];
        dcm_frd_to_ned(own.roll_rad, own.pitch_rad, own.yaw_rad, dcm);
        float los_ned[3];
        for (int i = 0; i < 3; ++i) {
            los_ned[i] = (dcm[i][0] * los_frd[0]) + (dcm[i][1] * los_frd[1]) +
                         (dcm[i][2] * los_frd[2]);
        }
        for (int i = 0; i < N; ++i) {
            x_[i] = 0.F;
            for (int j = 0; j < N; ++j) {
                P_[i][j] = 0.F;
            }
        }
        for (int i = 0; i < 3; ++i) {
            x_[i] = own.pos_ned[i] + (rng * los_ned[i]);
        }
        x_[9] = s0;
        const float pp = (p_.init_range_unc_rel * rng) * (p_.init_range_unc_rel * rng);
        const float vv = p_.init_vel_unc * p_.init_vel_unc;
        const float aa = p_.init_acc_unc * p_.init_acc_unc;
        for (int i = 0; i < 3; ++i) {
            P_[i][i] = pp;
            P_[i + 3][i + 3] = vv;
            P_[i + 6][i + 6] = aa;
        }
        P_[9][9] = p_.init_size_unc * p_.init_size_unc;
        init_ = true;
        return;
    }

    const float z_meas[3] = {az, el, log_apparent};
    float h[3];
    observe(x_, own, h);

    // Numerical Jacobian H (3 x N): finite difference of observe().
    float H[3][N];
    for (int j = 0; j < N; ++j) {
        float xp[N];
        for (int k = 0; k < N; ++k) {
            xp[k] = x_[k];
        }
        // Scale the step to the state: metres for position, unit for log-size.
        const float eps = (j == 9) ? 1e-3F : 1e-2F;
        xp[j] += eps;
        float hp[3];
        observe(xp, own, hp);
        for (int i = 0; i < 3; ++i) {
            H[i][j] = (hp[i] - h[i]) / eps;
        }
    }

    // Innovation y (wrap the two bearing components into [-pi, pi]).
    float y[3];
    for (int i = 0; i < 3; ++i) {
        y[i] = z_meas[i] - h[i];
    }
    for (int i = 0; i < 2; ++i) {
        while (y[i] > 3.14159265F) {
            y[i] -= 6.28318531F;
        }
        while (y[i] < -3.14159265F) {
            y[i] += 6.28318531F;
        }
    }

    // S = H P H^T + R.
    const float R[3] = {p_.sigma_bearing_rad * p_.sigma_bearing_rad,
                        p_.sigma_bearing_rad * p_.sigma_bearing_rad,
                        p_.sigma_logsize_meas * p_.sigma_logsize_meas};
    float PHt[N][3];
    for (int i = 0; i < N; ++i) {
        for (int c = 0; c < 3; ++c) {
            float s = 0.F;
            for (int k = 0; k < N; ++k) {
                s += P_[i][k] * H[c][k];
            }
            PHt[i][c] = s;
        }
    }
    float S[3][3];
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            float s = 0.F;
            for (int k = 0; k < N; ++k) {
                s += H[r][k] * PHt[k][c];
            }
            S[r][c] = s + (r == c ? R[r] : 0.F);
        }
    }
    float Sinv[3][3];
    if (!inv3(S, Sinv)) {
        return;
    }

    // K = P H^T S^-1  (N x 3).
    float K[N][3];
    for (int i = 0; i < N; ++i) {
        for (int c = 0; c < 3; ++c) {
            float s = 0.F;
            for (int k = 0; k < 3; ++k) {
                s += PHt[i][k] * Sinv[k][c];
            }
            K[i][c] = s;
        }
    }

    // x += K y.
    for (int i = 0; i < N; ++i) {
        x_[i] += (K[i][0] * y[0]) + (K[i][1] * y[1]) + (K[i][2] * y[2]);
    }

    // P -= K H P  (Joseph-free simple form; K H P = K (H P)).
    float HP[3][N];
    for (int r = 0; r < 3; ++r) {
        for (int j = 0; j < N; ++j) {
            float s = 0.F;
            for (int k = 0; k < N; ++k) {
                s += H[r][k] * P_[k][j];
            }
            HP[r][j] = s;
        }
    }
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            const float d =
                (K[i][0] * HP[0][j]) + (K[i][1] * HP[1][j]) + (K[i][2] * HP[2][j]);
            P_[i][j] -= d;
        }
    }
}

float SizeRangeEkf::range(const OwnState& own) const {
    if (!init_) {
        return 0.F;
    }
    float d[3];
    for (int i = 0; i < 3; ++i) {
        d[i] = x_[i] - own.pos_ned[i];
    }
    return std::sqrt((d[0] * d[0]) + (d[1] * d[1]) + (d[2] * d[2]));
}

float SizeRangeEkf::range_sigma() const {
    if (!init_) {
        return 0.F;
    }
    const float tr = P_[0][0] + P_[1][1] + P_[2][2];
    return std::sqrt(tr / 3.0F);
}

} // namespace riposte
