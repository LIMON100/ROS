// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — Link health monitor (PATCHED 2026-05-13).

#include "san_comm_link/link_health_monitor.hpp"

namespace san_comm_link
{

LinkDecision LinkHealthMonitor::update(const LinkProbeUpdate & p)
{
  std::lock_guard<std::mutex> lock(mu_);

  // WiFi6 streak counters.
  if (p.wifi6_ok) {
    ++consec_ok_;
    consec_fail_ = 0;
  } else {
    ++consec_fail_;
    consec_ok_ = 0;
  }

  // ★ PATCH 2026-05-13 (C6): LTE streak tracking.
  // Mirrors the WiFi6 logic so we don't bounce on single-tick LTE
  // false-positives. Counter resets on any failure observation.
  if (p.lte_ok) {
    if (lte_consec_ok_ < UINT16_MAX) {++lte_consec_ok_;}
  } else {
    lte_consec_ok_ = 0;
  }

  // Named stability gates (single-expression so brace style is
  // unambiguous for both uncrustify and cpplint).
  const bool lte_stable =
    p.lte_ok && lte_consec_ok_ >= cfg_.lte_consec_ok_to_stabilize;
  const bool wifi6_upgrade =
    p.wifi6_ok && consec_ok_ >= cfg_.consec_ok_to_upgrade;

  const ActiveLink prev = active_;
  std::string reason;

  switch (active_) {
    case ActiveLink::None: {
        // Bootstrap — prefer WiFi6 if up; LTE only after stabilisation.
        if (p.wifi6_ok) {
          active_ = ActiveLink::Wifi6;
          reason = "initial wifi6 up";
        } else if (lte_stable) {
          // ★ PATCH 2026-05-13 (C6): require LTE stable for N ticks
          // before claiming it as active. Previously a single lte_ok
          // tick was enough — vulnerable to registration blips.
          active_ = ActiveLink::Lte;
          reason = "initial wifi6 down → lte (stable " +
            std::to_string(lte_consec_ok_) + ")";
        } else {
          reason = "no uplink available";
        }
        break;
      }

    case ActiveLink::Wifi6: {
        if (consec_fail_ >= cfg_.consec_fail_to_downgrade) {
          // ★ PATCH 2026-05-13 (C6): LTE must also be stable, not just
          // up for one tick.
          if (lte_stable) {
            active_ = ActiveLink::Lte;
            reason = "wifi6 fail streak (" +
              std::to_string(consec_fail_) + ") → lte (stable)";
          } else {
            active_ = ActiveLink::None;
            reason = (p.lte_ok ? "wifi6 fail + lte unstable" :
              "wifi6 fail + lte also down");
          }
        }
        break;
      }

    case ActiveLink::Lte: {
        if (wifi6_upgrade) {
          active_ = ActiveLink::Wifi6;
          reason = "wifi6 ok streak (" +
            std::to_string(consec_ok_) + ") — upgrade";
        } else if (!p.lte_ok) {
          active_ = ActiveLink::None;
          reason = "lte down, wifi6 still recovering";
        }
        break;
      }
  }

  const bool switched = (active_ != prev);
  if (switched) {++switch_count_;}

  LinkDecision d;
  d.active_link = active_;
  d.switch_event = switched;
  d.reason = switched ? reason : std::string();
  return d;
}

// ─── Thread-safe accessors (PATCH 2026-05-13 M8) ───────────────────────

ActiveLink LinkHealthMonitor::activeLink() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return active_;
}

uint16_t LinkHealthMonitor::consecOk() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return consec_ok_;
}

uint16_t LinkHealthMonitor::consecFail() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return consec_fail_;
}

uint16_t LinkHealthMonitor::lteConsecOk() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return lte_consec_ok_;
}

uint32_t LinkHealthMonitor::switchCount() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return switch_count_;
}

void LinkHealthMonitor::reset()
{
  std::lock_guard<std::mutex> lock(mu_);
  active_ = ActiveLink::None;
  consec_ok_ = 0;
  consec_fail_ = 0;
  lte_consec_ok_ = 0;
  switch_count_ = 0;
}

void LinkHealthMonitor::reconfigure(const LinkHysteresisConfig & cfg)
{
  std::lock_guard<std::mutex> lock(mu_);
  cfg_ = cfg;
  active_ = ActiveLink::None;
  consec_ok_ = 0;
  consec_fail_ = 0;
  lte_consec_ok_ = 0;
  switch_count_ = 0;
}

}  // namespace san_comm_link
