// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_rtk_gnss/rtk_quality_fsm.hpp"

namespace san_rtk_gnss
{

RtkQualityFsm::RtkQualityFsm(const RtkQualityParams & params)
: params_(params)
{}

RtkQualityState RtkQualityFsm::update(bool good, double now_sec)
{
  if (good) {
    if (state_ == RtkQualityState::Ok) {
      bad_since_ = -1.0;
      good_since_ = -1.0;
      return state_;
    }
    // In a degraded state — require a SUSTAINED Fix before returning to OK
    // (a single Fix sample mid-flapping must not clear the degradation).
    if (good_since_ < 0.0) {good_since_ = now_sec;}
    if (now_sec - good_since_ >= params_.recover_sec) {
      state_ = RtkQualityState::Ok;
      bad_since_ = -1.0;
      good_since_ = -1.0;
    }
    return state_;
  }

  // Not Fixed. Reset any in-progress recovery; the escalation clock
  // (bad_since_) persists so a flapping fix still escalates over time.
  good_since_ = -1.0;
  if (state_ == RtkQualityState::Ok) {
    bad_since_ = now_sec;
    state_ = RtkQualityState::DegradedGrace;
    return state_;
  }
  if (bad_since_ < 0.0) {bad_since_ = now_sec;}
  const double bad_elapsed = now_sec - bad_since_;
  if (bad_elapsed >= params_.active_lost_sec) {
    state_ = RtkQualityState::Lost;
  } else if (bad_elapsed >= params_.grace_sec) {
    state_ = RtkQualityState::DegradedActive;
  }
  // else remain in DEGRADED_GRACE
  return state_;
}

float RtkQualityFsm::maxSpeed() const
{
  return (state_ == RtkQualityState::Ok ||
         state_ == RtkQualityState::DegradedGrace) ?
         params_.ok_speed_mps : params_.degraded_speed_mps;
}

float RtkQualityFsm::pathTolerance() const
{
  return (state_ == RtkQualityState::Ok ||
         state_ == RtkQualityState::DegradedGrace) ?
         params_.ok_tolerance_m : params_.degraded_tolerance_m;
}

bool RtkQualityFsm::formationLoose() const
{
  return state_ == RtkQualityState::DegradedActive ||
         state_ == RtkQualityState::Lost;
}

}  // namespace san_rtk_gnss
