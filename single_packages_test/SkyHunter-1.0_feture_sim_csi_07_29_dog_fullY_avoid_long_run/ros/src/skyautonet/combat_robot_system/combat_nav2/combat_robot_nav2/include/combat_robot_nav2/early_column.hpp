#ifndef COMBAT_ROBOT_NAV2__EARLY_COLUMN_HPP_
#define COMBAT_ROBOT_NAV2__EARLY_COLUMN_HPP_

#include <cstdint>

namespace combat_robot_nav2
{

// Operation mode (SwarmControlCommand.formation_type domain) that geometryForMode()
// maps to a single-file COLUMN. The leader forces this on every robot while it
// threads an obstacle, so followers queue behind it instead of funnelling their
// wide formation slots into the same gap. Value matches MODE_RECON in
// swarm_path_executor.cpp.
constexpr uint8_t kEarlyColumnMode = 1;

// Effective operation mode a robot should assume this tick: the leader's
// obstacle-pass override (COLUMN) wins while active, otherwise the FSM/operator
// commanded mode. Pure, side-effect free.
inline uint8_t effectiveFormationMode(bool t_override_active, uint8_t t_fsm_mode)
{
  return t_override_active ? kEarlyColumnMode : t_fsm_mode;
}

// Leader-side latch driving the early-column override. It engages when a blocking
// obstacle sits close enough ahead for `persist` consecutive ticks, and stays
// engaged until the leader has driven clear of the obstacle (caller-computed) —
// NOT merely until the obstacle leaves view, so trailing followers keep threading
// behind the leader through the whole gap. Pure state machine, no ROS: unit
// testable and cheap to call every control tick.
class EarlyColumnLatch
{
public:
  // Advance one control tick; returns the (possibly updated) active state.
  //   t_obstacle_dist : body-frame forward-obstacle distance in metres;
  //                     <= 0 means "nothing ahead".
  //   t_lookahead_m   : engage distance in metres. Keep it LARGER than the swerve
  //                     trigger so the column forms in open space BEFORE the gap
  //                     narrows.
  //   t_persist_ticks : consecutive in-range ticks required to engage (debounce).
  //   t_leader_clear  : true once the leader has passed the obstacle by margin
  //                     (caller may fold "no obstacle seen for a while" into this).
  bool update(double t_obstacle_dist, double t_lookahead_m,
              int t_persist_ticks, bool t_leader_clear)
  {
    if (active_) {
      if (t_leader_clear) {
        active_ = false;
        streak_ = 0;
      }
      return active_;
    }
    const bool in_range =
      (t_obstacle_dist > 0.0) && (t_obstacle_dist < t_lookahead_m);
    streak_ = in_range ? (streak_ + 1) : 0;
    if (streak_ >= t_persist_ticks) {
      active_ = true;
    }
    return active_;
  }

  bool active() const { return active_; }

  void reset()
  {
    active_ = false;
    streak_ = 0;
  }

private:
  bool active_ = false;
  int streak_ = 0;
};

}  // namespace combat_robot_nav2

#endif  // COMBAT_ROBOT_NAV2__EARLY_COLUMN_HPP_
