// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — Link health state machine (PATCHED 2026-05-13).
//
// Encapsulates the WiFi6 ↔ LTE failover decision with hysteresis so
// the comm node doesn't oscillate when WiFi6 quality is marginal.
//
// PATCH 2026-05-13 (comm deep-dive review):
//   * LTE streak tracking (C6) — previously the monitor only counted
//     WiFi6 consecutive ok/fail. LTE was treated as a single-tick
//     boolean, so a momentary LTE blip could push state from Lte to
//     None and back. New `lte_consec_ok_to_stabilize` parameter
//     (default 2 ticks) prevents bouncing.
//   * Thread-safe accessors (M8) — getters now lock an internal
//     mutex so producers and observers can be on different callback
//     groups under a MultiThreadedExecutor.
//   * Last-decision snapshot — node can read the most recent reason
//     + timestamp without re-running update().
//
// SDD-SWARM v1.5 §10 declares this module as the SINGLE SOURCE OF
// TRUTH for active link state. san_comm should subscribe to its
// CommLinkStatus output rather than running a parallel selector.

#ifndef SAN_COMM_LINK__LINK_HEALTH_MONITOR_HPP_
#define SAN_COMM_LINK__LINK_HEALTH_MONITOR_HPP_

#include <cstdint>
#include <mutex>
#include <string>

namespace san_comm_link
{

enum class ActiveLink : uint8_t
{
  None  = 0,
  Wifi6 = 1,
  Lte   = 2,
};

struct LinkHysteresisConfig
{
  uint16_t consec_ok_to_upgrade = 5;          // LTE → WiFi6
  uint16_t consec_fail_to_downgrade = 3;      // WiFi6 → LTE
  // ★ PATCH 2026-05-13 (C6): LTE stabilisation streak.
  // Number of consecutive LTE-ok observations required before the
  // None state may transition to Lte. Default 1 preserves v1.5.0
  // single-tick semantics; production config recommends 2+ to
  // prevent registration-blip glitches.
  uint16_t lte_consec_ok_to_stabilize = 1;
};

struct LinkProbeUpdate
{
  bool wifi6_ok;
  bool lte_ok;
};

struct LinkDecision
{
  ActiveLink active_link;
  bool switch_event;
  std::string reason;
};

class LinkHealthMonitor
{
public:
  explicit LinkHealthMonitor(LinkHysteresisConfig cfg = LinkHysteresisConfig{})
  : cfg_(cfg) {}

  /// Feed one probe update; returns the resulting decision.
  LinkDecision update(const LinkProbeUpdate & p);

  // ─── Thread-safe introspection (PATCH 2026-05-13 M8) ──────────────
  ActiveLink activeLink() const;
  uint16_t   consecOk()   const;
  uint16_t   consecFail() const;
  uint16_t   lteConsecOk() const;     // ★ PATCH 2026-05-13
  uint32_t   switchCount() const;

  /// Reset to initial state (None).
  void reset();

  /// Swap in a new hysteresis config and reset state to None.
  /// Needed because the class is non-copyable / non-movable (mutex
  /// member), so callers can't do `monitor_ = LinkHealthMonitor(cfg)`.
  void reconfigure(const LinkHysteresisConfig & cfg);

private:
  mutable std::mutex mu_;
  LinkHysteresisConfig cfg_;
  ActiveLink active_ = ActiveLink::None;
  uint16_t consec_ok_ = 0;                      // WiFi6 streak
  uint16_t consec_fail_ = 0;                    // WiFi6 fail streak
  uint16_t lte_consec_ok_ = 0;                  // ★ PATCH 2026-05-13
  uint32_t switch_count_ = 0;
};

}  // namespace san_comm_link

#endif  // SAN_COMM_LINK__LINK_HEALTH_MONITOR_HPP_
