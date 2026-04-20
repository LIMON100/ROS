#include "ByteTrack/KalmanFilter.h"
#include <cstddef>
#include <cmath> // For std::pow

namespace byte_track
{

KalmanFilter::KalmanFilter(const float& std_weight_position,
                           const float& std_weight_velocity) :
    std_weight_position_(std_weight_position),
    std_weight_velocity_(std_weight_velocity)
{
    constexpr size_t ndim = 4;
    constexpr float dt = 1;

    motion_mat_ = Eigen::MatrixXf::Identity(8, 8);
    update_mat_ = Eigen::MatrixXf::Identity(4, 8);

    for (size_t i = 0; i < ndim; i++)
    {
        motion_mat_(i, ndim + i) = dt;
    }
}

void KalmanFilter::initiate(StateMean &mean, StateCov &covariance, const DetectBox &measurement)
{
    mean.block<1, 4>(0, 0) = measurement.block<1, 4>(0, 0);
    mean.block<1, 4>(0, 4) = Eigen::Vector4f::Zero();

    StateMean std;
    std(0) = 2 * std_weight_position_ * measurement[3];
    std(1) = 2 * std_weight_position_ * measurement[3];
    std(2) = 1e-2;
    std(3) = 2 * std_weight_position_ * measurement[3];
    std(4) = 10 * std_weight_velocity_ * measurement[3];
    std(5) = 10 * std_weight_velocity_ * measurement[3];
    std(6) = 1e-5;
    std(7) = 10 * std_weight_velocity_ * measurement[3];

    StateMean tmp = std.array().square();
    covariance = tmp.asDiagonal();
}

void KalmanFilter::predict(StateMean &mean, StateCov &covariance)
{
    StateMean std;
    std(0) = std_weight_position_ * mean(3);
    std(1) = std_weight_position_ * mean(3);
    std(2) = 1e-2;
    std(3) = std_weight_position_ * mean(3);
    std(4) = std_weight_velocity_ * mean(3);
    std(5) = std_weight_velocity_ * mean(3);
    std(6) = 1e-5;
    std(7) = std_weight_velocity_ * mean(3);

    StateMean tmp = std.array().square();
    StateCov motion_cov = tmp.asDiagonal();

    mean = (motion_mat_ * mean.transpose()).transpose();
    covariance = motion_mat_ * covariance * (motion_mat_.transpose()) + motion_cov;
}

// --- THE UPGRADE: The new adaptive update function ---
void KalmanFilter::update(StateMean &mean, StateCov &covariance, const DetectBox &measurement, float conf)
{
    // --- ADAPTIVE NOISE CALCULATION ---
    constexpr float R_base = 0.2f; // Base noise for position
    constexpr float R_base_size = 0.5f; // Base noise for size
    constexpr float R_alpha = 10.0f; // Amplifies the penalty
    constexpr float R_beta = 2.0f;   // Makes the penalty non-linear (squared)

    float penalty = std::pow(1.0f - conf, R_beta);
    
    float r_xy_noise = R_base + R_alpha * penalty;
    float r_ah_noise = R_base_size + R_alpha * penalty;

    StateHCov adaptive_measurement_noise = StateHCov::Zero();
    adaptive_measurement_noise(0, 0) = r_xy_noise * r_xy_noise;
    adaptive_measurement_noise(1, 1) = r_xy_noise * r_xy_noise;
    adaptive_measurement_noise(2, 2) = r_ah_noise * r_ah_noise;
    adaptive_measurement_noise(3, 3) = r_ah_noise * r_ah_noise;
    // --- END ADAPTIVE NOISE CALCULATION ---

    StateHMean projected_mean;
    StateHCov projected_cov;
    project(projected_mean, projected_cov, mean, covariance, adaptive_measurement_noise);

    Eigen::Matrix<float, 4, 8> B = (covariance * (update_mat_.transpose())).transpose();
    Eigen::Matrix<float, 8, 4> kalman_gain = (projected_cov.llt().solve(B)).transpose();
    Eigen::Matrix<float, 1, 4> innovation = measurement - projected_mean;

    const auto tmp = innovation * (kalman_gain.transpose());
    mean = (mean.array() + tmp.array()).matrix();
    covariance = covariance - kalman_gain * projected_cov * (kalman_gain.transpose());
}

//  The project function now uses the adaptive noise ---
void KalmanFilter::project(StateHMean &projected_mean, StateHCov &projected_covariance,
                           const StateMean& mean, const StateCov& covariance, const StateHCov& measurement_noise)
{
    projected_mean = update_mat_ * mean.transpose();
    projected_covariance = update_mat_ * covariance * (update_mat_.transpose());
    projected_covariance += measurement_noise;
}

} // end namespace byte_track
