// SAN v1.5 Phase 2-E Turn 8 — Link selector (WiFi6 ↔ LTE).
//
// Mirrors comm/comm_process.py's selection logic per DCN-2026-002 D-007/D-008.
// Pure C++17 state machine — no rclcpp, no sockets. Standalone testable.
//
// Selection rule (matches Python):
//   * Primary: WiFi6 (LAN / mesh exit gateway)
//   * Fallback: LTE (cellular)
//   * Hysteresis: after switching to LTE, don't go back to WiFi6 until
//     `wifi_recovery_threshold` consecutive WiFi6 successes.
//   * Both down → "none" (caller caches to disk).
//
// Inputs (caller polls externally):
//   * wifi6_reachable (bool) — TCP/UDP probe result
//   * lte_registered (bool)  — from san_lte_redundancy LteModemStatus
//   * lte_pdp_active (bool)  — IP address acquired
//
// Output: ActiveLink

#ifndef SAN_COMM__LINK_SELECTOR_HPP_
#define SAN_COMM__LINK_SELECTOR_HPP_

#include <cstdint>
#include <string>

namespace san_comm {

enum class ActiveLink : uint8_t {
  None  = 0,
  Wifi6 = 1,
  Lte   = 2,
};

const char* toString(ActiveLink link);

struct LinkProbe {
  bool wifi6_reachable;
  bool lte_registered;
  bool lte_pdp_active;
};

struct LinkSelectorConfig {
  /// Consecutive WiFi6 successes required to switch back from LTE.
  uint32_t wifi_recovery_threshold = 3;
};

/// Stateful selector. Caller invokes update() with the latest probe;
/// it returns the link to use this cycle.
class LinkSelector {
public:
  explicit LinkSelector(LinkSelectorConfig cfg = {});

  /// Apply a fresh probe; return the active link to use.
  ActiveLink update(const LinkProbe& probe);

  ActiveLink current() const { return current_; }
  uint32_t   wifi_recovery_count() const { return wifi_recovery_count_; }

  /// Stats — useful for health logging.
  uint32_t failover_count()  const { return failover_count_; }
  uint32_t recovery_count()  const { return recovery_count_; }

private:
  LinkSelectorConfig cfg_;
  ActiveLink current_ = ActiveLink::None;
  uint32_t   wifi_recovery_count_ = 0;
  uint32_t   failover_count_      = 0;
  uint32_t   recovery_count_      = 0;
};

}  // namespace san_comm

#endif  // SAN_COMM__LINK_SELECTOR_HPP_
