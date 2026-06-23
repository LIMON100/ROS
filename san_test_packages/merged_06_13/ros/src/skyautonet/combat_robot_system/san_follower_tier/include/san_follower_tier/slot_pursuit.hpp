#ifndef SAN_FOLLOWER_TIER__SLOT_PURSUIT_HPP_
#define SAN_FOLLOWER_TIER__SLOT_PURSUIT_HPP_

#include <algorithm>
#include <cmath>

namespace san_follower_tier
{

struct SlotGains
{
  double kp_linear{0.8};
  double kp_angular{2.0};
  double max_linear{1.2};
  double max_angular{1.5};
  double stop_distance{0.4};
};

struct SlotCmd
{
  double linear{0.0};
  double angular{0.0};
  bool arrived{false};
};

inline double wrapPiSlot(double a)
{
  while (a > M_PI) {a -= 2.0 * M_PI;}
  while (a <= -M_PI) {a += 2.0 * M_PI;}
  return a;
}

/// Drive own (x,y,yaw) toward a world-frame slot (tx,ty). Same P-control +
/// turn-in-place scaling the leader-chase loop already uses, factored out so
/// it unit-tests without rclcpp.
inline SlotCmd computeSlotCmd(
  double own_x, double own_y, double own_yaw,
  double tx, double ty, const SlotGains & g)
{
  SlotCmd out;
  const double dx = tx - own_x;
  const double dy = ty - own_y;
  const double dist = std::hypot(dx, dy);

  if (dist < g.stop_distance) {
    out.arrived = true;
    return out; // hold position, no command
  }

  const double desired_yaw = std::atan2(dy, dx);
  const double yaw_err = wrapPiSlot(desired_yaw - own_yaw);
  const double turn_factor = std::max(0.0, 1.0 - std::fabs(yaw_err) / M_PI);

  out.linear = std::clamp(g.kp_linear * dist, 0.0, g.max_linear) * turn_factor;
  out.angular = std::clamp(g.kp_angular * yaw_err, -g.max_angular, g.max_angular);

  return out;
}

}  // namespace san_follower_tier
#endif  // SAN_FOLLOWER_TIER__SLOT_PURSUIT_HPP_