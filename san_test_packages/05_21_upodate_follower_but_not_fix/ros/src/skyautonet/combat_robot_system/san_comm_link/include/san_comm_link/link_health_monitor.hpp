// SAN v1.5 Phase 2-E Turn 8 — Link health state machine (pure logic).
//
// Encapsulates the WiFi6 ↔ LTE failover decision with hysteresis so
// the comm node doesn't oscillate when WiFi6 quality is marginal.
//
// State machine inputs (per tick):
//   * wifi6_ok    — current probe result (independent of historical state)
//   * lte_ok      — current LTE link state (RobotStatus.lte_active)
//
// State machine outputs:
//   * active_link  — current selection (none / wifi6 / lte)
//   * switch_event — true if active_link just changed this tick
//   * reason       — human-readable trigger
//
// Hysteresis rules:
//   * Prefer WiFi6 when available.
//   * To downgrade WiFi6 → LTE: require N_FAIL consecutive probe
//     failures (default 3).
//   * To upgrade LTE → WiFi6: require N_OK consecutive probe successes
//     (default 5; higher to avoid bouncing on a flapping AP).
//
// Pure C++17, no ROS — fully standalone testable.

#ifndef SAN_COMM_LINK__LINK_HEALTH_MONITOR_HPP_
#define SAN_COMM_LINK__LINK_HEALTH_MONITOR_HPP_

#include <cstdint>
#include <string>

namespace san_comm_link {

enum class ActiveLink : uint8_t {
  None  = 0,
  Wifi6 = 1,
  Lte   = 2,
};

struct LinkHysteresisConfig {
  uint16_t consec_ok_to_upgrade   = 5;   // LTE→WiFi6
  uint16_t consec_fail_to_downgrade = 3; // WiFi6→LTE
};

struct LinkProbeUpdate {
  bool wifi6_ok;
  bool lte_ok;
};

struct LinkDecision {
  ActiveLink   active_link;
  bool         switch_event;
  std::string  reason;
};

class LinkHealthMonitor {
public:
  explicit LinkHealthMonitor(LinkHysteresisConfig cfg = {})
      : cfg_(cfg) {}

  /// Feed one probe update; returns the resulting decision.
  /// Idempotent — the same input always produces the same output
  /// given the same internal state.
  LinkDecision update(const LinkProbeUpdate& p);

  // ─── Introspection (for status publishing + tests) ──────────────
  ActiveLink activeLink()        const { return active_; }
  uint16_t   consecOk()          const { return consec_ok_; }
  uint16_t   consecFail()        const { return consec_fail_; }
  uint32_t   switchCount()       const { return switch_count_; }

  /// Reset to initial state (None).
  void reset();

private:
  LinkHysteresisConfig cfg_;
  ActiveLink           active_       = ActiveLink::None;
  uint16_t             consec_ok_    = 0;
  uint16_t             consec_fail_  = 0;
  uint32_t             switch_count_ = 0;
};

}  // namespace san_comm_link

#endif  // SAN_COMM_LINK__LINK_HEALTH_MONITOR_HPP_
