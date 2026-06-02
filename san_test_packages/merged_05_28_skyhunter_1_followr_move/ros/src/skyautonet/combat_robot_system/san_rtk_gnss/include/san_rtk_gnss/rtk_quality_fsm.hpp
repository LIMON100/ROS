// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — RTK Fix→Float mitigation, Layer 3: RTK-quality FSM.
//
// Tracks RTK localization quality over time and derives advisory
// navigation constraints. When an RTK Fix degrades (to Float, or the fix
// goes silent), a 5 s grace window keeps nominal behaviour; sustained
// degradation tightens to a defensive speed/tolerance; prolonged loss
// declares RTK_LOST so the mission layer can escalate Tier + alert.
//
//   OK ──(not Fixed)──▶ DEGRADED_GRACE ──(grace_sec)──▶ DEGRADED_ACTIVE
//                                                  └──(active_lost_sec)─▶ LOST
//   any degraded state ──(Fixed sustained recover_sec)──▶ OK
//
// The escalation clock (bad_since) persists through brief Fix blips so a
// flapping fix still escalates; only a SUSTAINED recovery returns to OK.
//
// Pure C++ (no rclcpp / clock) — the caller supplies `good` (== RTK Fixed)
// and a monotonic `now_sec`, so the FSM is fully unit-testable.

#ifndef SAN_RTK_GNSS__RTK_QUALITY_FSM_HPP_
#define SAN_RTK_GNSS__RTK_QUALITY_FSM_HPP_

#include <cstdint>

namespace san_rtk_gnss
{

enum class RtkQualityState : uint8_t
{
  Ok = 0,
  DegradedGrace = 1,
  DegradedActive = 2,
  Lost = 3,
};

struct RtkQualityParams
{
  double grace_sec = 5.0;          // OK→ACTIVE hold (behaviour unchanged)
  double active_lost_sec = 30.0;   // total bad time → LOST
  double recover_sec = 2.0;        // sustained Fix in a degraded state → OK
  float ok_speed_mps = 1.5f;
  float degraded_speed_mps = 0.5f;
  float ok_tolerance_m = 0.3f;
  float degraded_tolerance_m = 1.0f;
};

class RtkQualityFsm
{
public:
  explicit RtkQualityFsm(const RtkQualityParams & params = {});

  // Advance the FSM. `good` is true iff the current fix is RTK Fixed (a
  // stale/silent fix should be passed as good=false by the caller).
  // `now_sec` is a monotonic clock in seconds. Returns the new state.
  RtkQualityState update(bool good, double now_sec);

  RtkQualityState state() const {return state_;}

  // Advisory navigation constraints for the current state.
  float maxSpeed() const;
  float pathTolerance() const;
  bool formationLoose() const;

  const RtkQualityParams & params() const {return params_;}

private:
  RtkQualityParams params_;
  RtkQualityState state_ = RtkQualityState::Ok;
  double bad_since_ = -1.0;     // when continuous "not Fixed" began (-1 = none)
  double good_since_ = -1.0;    // when current recovery's good run began
};

}  // namespace san_rtk_gnss

#endif  // SAN_RTK_GNSS__RTK_QUALITY_FSM_HPP_
