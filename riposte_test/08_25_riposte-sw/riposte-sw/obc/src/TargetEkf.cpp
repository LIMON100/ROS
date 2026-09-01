#include "TargetEkf.h"

#include <cmath>

namespace riposte {

namespace {

// Body-FRD -> NED direction cosine matrix (aerospace ZYX: yaw*pitch*roll), the
// same convention GuidanceSource uses. Rotates a body-frame vector into NED.
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

void mat3_vec(const float m[3][3], const float v[3], float out[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = (m[i][0] * v[0]) + (m[i][1] * v[1]) + (m[i][2] * v[2]);
    }
}

// Transpose(m) * v — for a rotation matrix this is the inverse rotation.
void mat3_t_vec(const float m[3][3], const float v[3], float out[3]) {
    for (int i = 0; i < 3; ++i) {
        out[i] = (m[0][i] * v[0]) + (m[1][i] * v[1]) + (m[2][i] * v[2]);
    }
}

// Inverse of a 3x3 via cofactors. Returns false if singular.
bool inv3(const float a[3][3], float out[3][3]) {
    const float c00 = (a[1][1] * a[2][2]) - (a[1][2] * a[2][1]);
    const float c01 = (a[1][2] * a[2][0]) - (a[1][0] * a[2][2]);
    const float c02 = (a[1][0] * a[2][1]) - (a[1][1] * a[2][0]);
    const float det = (a[0][0] * c00) + (a[0][1] * c01) + (a[0][2] * c02);
    if (std::fabs(det) < 1e-12F) {
        return false;
    }
    const float inv_det = 1.0F / det;
    out[0][0] = c00 * inv_det;
    out[1][0] = c01 * inv_det;
    out[2][0] = c02 * inv_det;
    out[0][1] = ((a[0][2] * a[2][1]) - (a[0][1] * a[2][2])) * inv_det;
    out[1][1] = ((a[0][0] * a[2][2]) - (a[0][2] * a[2][0])) * inv_det;
    out[2][1] = ((a[0][1] * a[2][0]) - (a[0][0] * a[2][1])) * inv_det;
    out[0][2] = ((a[0][1] * a[1][2]) - (a[0][2] * a[1][1])) * inv_det;
    out[1][2] = ((a[0][2] * a[1][0]) - (a[0][0] * a[1][2])) * inv_det;
    out[2][2] = ((a[0][0] * a[1][1]) - (a[0][1] * a[1][0])) * inv_det;
    return true;
}

constexpr int N = TargetEkf::N;

} // namespace

void TargetEkf::predict(double dt_s) {
    if (!init_ || dt_s <= 0.0) {
        return;
    }
    const auto dt = static_cast<float>(dt_s);
    const float h2 = 0.5F * dt * dt;

    // Constant-acceleration state transition, per axis:
    //   p += v dt + 1/2 a dt^2 ; v += a dt ; a unchanged.
    for (int i = 0; i < 3; ++i) {
        x_[i] += (x_[i + 3] * dt) + (x_[i + 6] * h2);
        x_[i + 3] += x_[i + 6] * dt;
    }

    // State transition matrix F (9x9): identity plus the CA coupling blocks.
    float F[N][N] = {};
    for (int i = 0; i < N; ++i) {
        F[i][i] = 1.0F;
    }
    for (int i = 0; i < 3; ++i) {
        F[i][i + 3] = dt;     // p <- v
        F[i][i + 6] = h2;     // p <- a
        F[i + 3][i + 6] = dt; // v <- a
    }

    // P = F P F^T + Q.
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
                s += FP[i][k] * F[j][k]; // (FP) F^T
            }
            P_[i][j] = s;
        }
    }

    // Process noise from white jerk (continuous, per axis). Standard CA blocks:
    //   Qpp=dt^5/20, Qpv=dt^4/8, Qpa=dt^3/6,
    //   Qvv=dt^3/3,  Qva=dt^2/2, Qaa=dt, all * sigma_jerk^2.
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
}

// G16.6 deviation: one cohesive EKF measurement update; splitting scatters the covariance
// algebra (G16.6) NOLINTNEXTLINE(readability-function-size)
void TargetEkf::update(const float rel_pos_frd[3], const OwnState& own) {
    // Monocular relative FRD -> absolute NED measurement of the target position.
    float dcm[3][3];
    dcm_frd_to_ned(own.roll_rad, own.pitch_rad, own.yaw_rad, dcm);
    float rel_ned[3];
    mat3_vec(dcm, rel_pos_frd, rel_ned);
    float z[3];
    for (int i = 0; i < 3; ++i) {
        z[i] = own.pos_ned[i] + rel_ned[i];
    }

    if (!init_) {
        for (int i = 0; i < N; ++i) {
            x_[i] = 0.F;
            for (int j = 0; j < N; ++j) {
                P_[i][j] = 0.F;
            }
        }
        for (int i = 0; i < 3; ++i) {
            x_[i] = z[i]; // seed position; zero velocity/acceleration
        }
        const float pp = p_.init_pos_unc * p_.init_pos_unc;
        const float vv = p_.init_vel_unc * p_.init_vel_unc;
        const float aa = p_.init_acc_unc * p_.init_acc_unc;
        for (int i = 0; i < 3; ++i) {
            P_[i][i] = pp;
            P_[i + 3][i + 3] = vv;
            P_[i + 6][i + 6] = aa;
        }
        init_ = true;
        return;
    }

    // Anisotropic measurement covariance R (EST-2): small across the LOS,
    // large along it. u = unit LOS in NED; range sets the metric scale.
    const float range = std::sqrt((rel_ned[0] * rel_ned[0]) + (rel_ned[1] * rel_ned[1]) +
                                  (rel_ned[2] * rel_ned[2]));
    float u[3] = {0.F, 0.F, 1.F};
    if (range > 1e-3F) {
        u[0] = rel_ned[0] / range;
        u[1] = rel_ned[1] / range;
        u[2] = rel_ned[2] / range;
    }
    const float var_perp =
        (p_.sigma_bearing_rel * range) * (p_.sigma_bearing_rel * range);
    const float var_range = (p_.sigma_range_rel * range) * (p_.sigma_range_rel * range);
    float R[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            R[i][j] = (var_range - var_perp) * u[i] * u[j];
        }
        R[i][i] += var_perp;
    }

    // Innovation covariance S = H P H^T + R = Ppp + R (H = [I 0 0]).
    float S[3][3];
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            S[i][j] = P_[i][j] + R[i][j];
        }
    }
    float Sinv[3][3];
    if (!inv3(S, Sinv)) {
        return; // degenerate; skip rather than corrupt the state
    }

    // Innovation y and its Gaussian likelihood N(y; 0, S) for IMM model
    // weighting. Computed here where y and S^-1 are available; det(S) via the
    // cofactor expansion.
    {
        const float y0 = z[0] - x_[0];
        const float y1 = z[1] - x_[1];
        const float y2 = z[2] - x_[2];
        const float m =
            (y0 * ((Sinv[0][0] * y0) + (Sinv[0][1] * y1) + (Sinv[0][2] * y2))) +
            (y1 * ((Sinv[1][0] * y0) + (Sinv[1][1] * y1) + (Sinv[1][2] * y2))) +
            (y2 * ((Sinv[2][0] * y0) + (Sinv[2][1] * y1) + (Sinv[2][2] * y2)));
        const float detS = (S[0][0] * ((S[1][1] * S[2][2]) - (S[1][2] * S[2][1]))) -
                           (S[0][1] * ((S[1][0] * S[2][2]) - (S[1][2] * S[2][0]))) +
                           (S[0][2] * ((S[1][0] * S[2][1]) - (S[1][1] * S[2][0])));
        constexpr float TWO_PI_CUBED = 248.050213F; // (2*pi)^3
        const float denom = std::sqrt(TWO_PI_CUBED * std::fabs(detS));
        likelihood_ = (denom > 1e-30F) ? (std::exp(-0.5F * m) / denom) : 0.F;
    }

    // Kalman gain K = P H^T S^-1 = P[:,0:3] S^-1  (9x3).
    float K[N][3];
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < 3; ++j) {
            float s = 0.F;
            for (int k = 0; k < 3; ++k) {
                s += P_[i][k] * Sinv[k][j];
            }
            K[i][j] = s;
        }
    }

    // State update x += K (z - Hx).
    float y[3];
    for (int i = 0; i < 3; ++i) {
        y[i] = z[i] - x_[i];
    }
    for (int i = 0; i < N; ++i) {
        x_[i] += (K[i][0] * y[0]) + (K[i][1] * y[1]) + (K[i][2] * y[2]);
    }

    // Covariance update P -= K H P = K * P[0:3, :].
    float dP[N][N];
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            dP[i][j] = (K[i][0] * P_[0][j]) + (K[i][1] * P_[1][j]) + (K[i][2] * P_[2][j]);
        }
    }
    for (int i = 0; i < N; ++i) {
        for (int j = 0; j < N; ++j) {
            P_[i][j] -= dP[i][j];
        }
    }
}

void TargetEkf::relative_state(const OwnState& own, float rel_pos_frd[3],
                               float rel_vel_frd[3]) const {
    for (int i = 0; i < 3; ++i) {
        rel_pos_frd[i] = 0.F;
        rel_vel_frd[i] = 0.F;
    }
    if (!init_) {
        return;
    }
    float dcm[3][3];
    dcm_frd_to_ned(own.roll_rad, own.pitch_rad, own.yaw_rad, dcm);
    float rp_ned[3];
    float rv_ned[3];
    for (int i = 0; i < 3; ++i) {
        rp_ned[i] = x_[i] - own.pos_ned[i];
        // Own velocity divides out here — the whole point of EST-3.
        rv_ned[i] = x_[i + 3] - own.vel_ned[i];
    }
    mat3_t_vec(dcm, rp_ned, rel_pos_frd);
    mat3_t_vec(dcm, rv_ned, rel_vel_frd);
}

float TargetEkf::position_sigma() const {
    if (!init_) {
        return 0.F;
    }
    const float tr = P_[0][0] + P_[1][1] + P_[2][2];
    return std::sqrt(tr / 3.0F);
}

void TargetEkf::get_state(float x_out[N], float P_out[N][N]) const {
    for (int i = 0; i < N; ++i) {
        x_out[i] = x_[i];
        for (int j = 0; j < N; ++j) {
            P_out[i][j] = P_[i][j];
        }
    }
}

void TargetEkf::set_state(const float x_in[N], const float P_in[N][N]) {
    for (int i = 0; i < N; ++i) {
        x_[i] = x_in[i];
        for (int j = 0; j < N; ++j) {
            P_[i][j] = P_in[i][j];
        }
    }
}

} // namespace riposte
