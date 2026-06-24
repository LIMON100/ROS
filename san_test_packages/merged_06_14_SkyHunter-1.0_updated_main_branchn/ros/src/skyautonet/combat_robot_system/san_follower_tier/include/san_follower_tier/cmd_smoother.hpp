// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#ifndef SAN_FOLLOWER_TIER__CMD_SMOOTHER_HPP_
#define SAN_FOLLOWER_TIER__CMD_SMOOTHER_HPP_

#include <algorithm>
#include <cmath>
#include <utility>

namespace san_follower_tier
{

/// Pure-logic velocity-command smoother for the follower's cmd_vel.
///
/// The follower's P-controller reacts to noisy heading error and a jumping
/// breadcrumb target, so the raw angular command oscillates (the "shaking
/// while moving" reported in testing — controller-origin, not friction). This
/// applies, per tick:
///   1. an optional first-order low-pass (EMA) toward the target, and
///   2. a slew-rate (acceleration) limit.
/// The linear slew limit is ASYMMETRIC — braking (toward 0) is allowed to be
/// faster than speeding up — so obstacle/anti-collision stops stay responsive.
///
/// Header-only and ROS-free so it unit-tests without rclcpp or a live graph.
struct CmdSmoother
{
  double max_lin_accel{2.5};  ///< m/s^2, while |v| is increasing
  double max_lin_decel{5.0};  ///< m/s^2, while |v| is decreasing (fast braking)
  double max_ang_accel{6.0};  ///< rad/s^2, magnitude of angular-rate change
  double lp_alpha_v{1.0};     ///< EMA weight on linear target (1 = low-pass off)
  double lp_alpha_w{0.5};     ///< EMA weight on angular target (1 = off)

  double v_{0.0};  ///< last emitted linear
  double w_{0.0};  ///< last emitted angular
  bool init_{false};

  /// Force the smoother's state to a full stop (use on safety holds so the
  /// next motion ramps up from 0 instead of from a stale command).
  void reset()
  {
    v_ = 0.0;
    w_ = 0.0;
    init_ = true;
  }

  /// Smooth one (v, w) target given the elapsed dt [s]; returns the limited
  /// command to publish. The first call after construction passes through.
  std::pair<double, double> apply(double v_target, double w_target, double dt)
  {
    if (!(dt > 0.0)) {
      dt = 1e-3;
    }
    if (!init_) {
      v_ = v_target;
      w_ = w_target;
      init_ = true;
      return {v_, w_};
    }

    // 1) First-order low-pass toward the target (alpha = 1 → no filtering).
    const double a_v = std::clamp(lp_alpha_v, 0.0, 1.0);
    const double a_w = std::clamp(lp_alpha_w, 0.0, 1.0);
    const double v_des = a_v * v_target + (1.0 - a_v) * v_;
    const double w_des = a_w * w_target + (1.0 - a_w) * w_;

    // 2) Slew-rate limit. Linear is asymmetric: decelerating (|v| shrinking,
    //    incl. toward a stop) may use the larger decel budget so braking is
    //    not delayed by smoothing.
    const bool slowing = std::fabs(v_des) < std::fabs(v_);
    const double lin_step = (slowing ? max_lin_decel : max_lin_accel) * dt;
    v_ += std::clamp(v_des - v_, -lin_step, lin_step);

    const double ang_step = max_ang_accel * dt;
    w_ += std::clamp(w_des - w_, -ang_step, ang_step);

    return {v_, w_};
  }
};

}  // namespace san_follower_tier

#endif  // SAN_FOLLOWER_TIER__CMD_SMOOTHER_HPP_
