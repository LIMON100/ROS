// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Comm health tracker implementation.

#include "san_follower_tier/comm_health.hpp"

#include <algorithm>

namespace san_follower_tier
{

CommHealth::CommHealth(uint32_t breadcrumb_ttl_ms)
: breadcrumb_ttl_ms_(breadcrumb_ttl_ms) {}

void CommHealth::observeCommLink(uint64_t now_ms, bool alive)
{
  std::lock_guard<std::mutex> g(mu_);
  if (alive) {
    last_alive_ms_ = now_ms;
    down_since_ms_.reset();      // clear the down stamp
  } else {
    if (!down_since_ms_) {
      // First false observation in this down-period — stamp it.
      down_since_ms_ = now_ms;
    }
    // last_alive_ms_ NOT updated here — preserves "last time alive".
  }
  last_comm_alive_ = alive;
}

void CommHealth::observeBreadcrumb(uint64_t now_ms)
{
  std::lock_guard<std::mutex> g(mu_);
  last_breadcrumb_ms_ = now_ms;
}

CommSnapshot CommHealth::snapshot(uint64_t now_ms) const
{
  std::lock_guard<std::mutex> g(mu_);
  CommSnapshot s;
  s.comm_link_alive = last_comm_alive_;

  // comm_loss_ms = duration since alive→down edge.
  // PATCH 2026-05-13: was computed from last_alive_ms_, which made
  // the loss include the pre-down alive period (PT6 expected 3000
  // but got 4000). Now we time from the actual down-edge.
  if (last_comm_alive_) {
    s.comm_loss_ms = 0;
  } else if (down_since_ms_) {
    s.comm_loss_ms = (now_ms > *down_since_ms_) ?
      static_cast<uint32_t>(now_ms - *down_since_ms_) :
      0u;
  } else {
    // Never observed (or only-alive observed). Conservative: 0.
    s.comm_loss_ms = 0;
  }

  if (last_breadcrumb_ms_) {
    const uint64_t age =
      (now_ms > *last_breadcrumb_ms_) ?
      (now_ms - *last_breadcrumb_ms_) : 0ULL;
    s.breadcrumb_age_ms = static_cast<uint32_t>(
      std::min<uint64_t>(age, UINT32_MAX));
    s.breadcrumb_available = (age <= breadcrumb_ttl_ms_);
  } else {
    s.breadcrumb_available = false;
    s.breadcrumb_age_ms = UINT32_MAX;
  }
  return s;
}

}  // namespace san_follower_tier
