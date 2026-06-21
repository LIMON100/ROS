// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 8 — LinkSelector (PATCHED 2026-05-13).
//
// DEPRECATED — see header. Retained for backward-compat only.
//
// PATCH 2026-05-13 (C7): all state-mutating + reading operations
// take an internal mutex so the class can survive a
// MultiThreadedExecutor without tearing.

// Suppress the [[deprecated]] warning ONLY in this implementation file
// — consumers outside this module still get the warning. We have to
// implement what we deprecate.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"

#include "san_comm/link_selector.hpp"

namespace san_comm
{

const char * toString(ActiveLink link)
{
  switch (link) {
    case ActiveLink::None:  return "none";
    case ActiveLink::Wifi6: return "wifi6";
    case ActiveLink::Lte:   return "lte";
  }
  return "?";
}

LinkSelector::LinkSelector(LinkSelectorConfig cfg)
: cfg_(cfg) {}

ActiveLink LinkSelector::update(const LinkProbe & probe)
{
  std::lock_guard<std::mutex> lock(mu_);

  const bool lte_usable = probe.lte_registered && probe.lte_pdp_active;

  if (current_ == ActiveLink::Lte) {
    if (probe.wifi6_reachable) {
      ++wifi_recovery_count_;
    } else {
      wifi_recovery_count_ = 0;
    }
  } else {
    wifi_recovery_count_ = 0;
  }

  ActiveLink next = current_;

  switch (current_) {
    case ActiveLink::None:
      if (probe.wifi6_reachable) {
        next = ActiveLink::Wifi6;
      } else if (lte_usable) {
        next = ActiveLink::Lte;
        ++failover_count_;
      } else {
        next = ActiveLink::None;
      }
      break;

    case ActiveLink::Wifi6:
      if (probe.wifi6_reachable) {
        next = ActiveLink::Wifi6;
      } else if (lte_usable) {
        next = ActiveLink::Lte;
        ++failover_count_;
      } else {
        next = ActiveLink::None;
      }
      break;

    case ActiveLink::Lte:
      if (wifi_recovery_count_ >= cfg_.wifi_recovery_threshold) {
        next = ActiveLink::Wifi6;
        ++recovery_count_;
        wifi_recovery_count_ = 0;
      } else if (!lte_usable) {
        if (probe.wifi6_reachable) {
          next = ActiveLink::Wifi6;
          ++recovery_count_;
          wifi_recovery_count_ = 0;
        } else {
          next = ActiveLink::None;
        }
      } else {
        next = ActiveLink::Lte;
      }
      break;
  }

  current_ = next;
  return current_;
}

// ─── Thread-safe accessors (PATCH 2026-05-13 C7) ───────────────────────

ActiveLink LinkSelector::current() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return current_;
}

uint32_t LinkSelector::wifi_recovery_count() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return wifi_recovery_count_;
}

uint32_t LinkSelector::failover_count() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return failover_count_;
}

uint32_t LinkSelector::recovery_count() const
{
  std::lock_guard<std::mutex> lock(mu_);
  return recovery_count_;
}

void LinkSelector::reconfigure(const LinkSelectorConfig & cfg)
{
  std::lock_guard<std::mutex> lock(mu_);
  cfg_ = cfg;
  current_ = ActiveLink::None;
  wifi_recovery_count_ = 0;
  failover_count_ = 0;
  recovery_count_ = 0;
}

}  // namespace san_comm

#pragma GCC diagnostic pop
