// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — Link selector (PATCHED 2026-05-13).
//
// ★★★ DEPRECATION NOTICE — PATCH 2026-05-13 ★★★
//
//   This LinkSelector duplicates logic already canonical in
//   san_comm_link::LinkHealthMonitor. Per SDD-SWARM v1.5 §10 the
//   single source of truth for active link state is the
//   san_comm_link CommLinkStatus topic.
//
//   New code should NOT instantiate LinkSelector directly. Instead,
//   subscribe to /comm_link_node/status (CommLinkStatus) and route
//   telemetry by the published `active_link` field.
//
//   This class is retained for backward compatibility with deployments
//   that still run CommUplinkNode in self-selecting mode (see
//   CommUplinkNode::use_external_link_status_ parameter, default true
//   after this patch).
//
// PATCH 2026-05-13 (comm deep-dive review):
//   * [[deprecated]] attribute on the class — compile-time warning
//     for new consumers.
//   * Thread-safe (C7) — internal mutex protects all fields. Previous
//     versions assumed single-threaded executor exclusively.
//   * `currentSafe()` returns a snapshot — guaranteed not torn.

#ifndef SAN_COMM__LINK_SELECTOR_HPP_
#define SAN_COMM__LINK_SELECTOR_HPP_

#include <cstdint>
#include <mutex>
#include <string>

namespace san_comm
{

enum class ActiveLink : uint8_t
{
  None  = 0,
  Wifi6 = 1,
  Lte   = 2,
};

const char * toString(ActiveLink link);

struct LinkProbe
{
  bool wifi6_reachable;
  bool lte_registered;
  bool lte_pdp_active;
};

struct LinkSelectorConfig
{
  uint32_t wifi_recovery_threshold = 3;
};

/// [[deprecated]] — use san_comm_link::LinkHealthMonitor instead.
/// Subscribe to /comm_link_node/status (CommLinkStatus) and route
/// by `active_link` field per SDD-SWARM v1.5 §10.
///
/// Retained for backward-compat (CommUplinkNode self-selecting mode).
class
  [[deprecated(
    "Use san_comm_link::CommLinkStatus topic instead. "
    "See SDD-SWARM v1.5 §10.")]]
  LinkSelector
{
public:
  explicit LinkSelector(LinkSelectorConfig cfg = LinkSelectorConfig{});

  /// Apply a fresh probe; return the active link to use.
  ActiveLink update(const LinkProbe & probe);

  /// Swap in a new config and reset state. Needed because the class
  /// is non-copyable / non-movable (mutex member), so callers can't
  /// do `selector_ = LinkSelector(cfg)`.
  void reconfigure(const LinkSelectorConfig & cfg);

  // ─── Thread-safe accessors (PATCH 2026-05-13 C7) ─────────────────
  ActiveLink current() const;
  uint32_t   wifi_recovery_count() const;
  uint32_t   failover_count()      const;
  uint32_t   recovery_count()      const;

private:
  mutable std::mutex mu_;
  LinkSelectorConfig cfg_;
  ActiveLink current_ = ActiveLink::None;
  uint32_t wifi_recovery_count_ = 0;
  uint32_t failover_count_ = 0;
  uint32_t recovery_count_ = 0;
};

}  // namespace san_comm

#endif  // SAN_COMM__LINK_SELECTOR_HPP_
