// SAN v1.5 Phase 2-E Turn 8 — Link health monitor implementation.

#include "san_comm_link/link_health_monitor.hpp"

namespace san_comm_link {

LinkDecision LinkHealthMonitor::update(const LinkProbeUpdate& p) {
  // Update streak counters from the latest probe result.
  if (p.wifi6_ok) {
    ++consec_ok_;
    consec_fail_ = 0;
  } else {
    ++consec_fail_;
    consec_ok_ = 0;
  }

  const ActiveLink prev = active_;
  std::string reason;

  switch (active_) {
    case ActiveLink::None: {
      // Bootstrap — first decision: prefer WiFi6 if up, else LTE
      if (p.wifi6_ok) {
        active_ = ActiveLink::Wifi6;
        reason  = "initial wifi6 up";
      } else if (p.lte_ok) {
        active_ = ActiveLink::Lte;
        reason  = "initial wifi6 down → lte";
      } else {
        reason  = "no uplink available";
      }
      break;
    }

    case ActiveLink::Wifi6: {
      // Downgrade rule: N_FAIL consecutive failures
      if (consec_fail_ >= cfg_.consec_fail_to_downgrade) {
        if (p.lte_ok) {
          active_ = ActiveLink::Lte;
          reason  = "wifi6 fail streak (" +
                    std::to_string(consec_fail_) + ") → lte";
        } else {
          active_ = ActiveLink::None;
          reason  = "wifi6 fail + lte also down";
        }
      }
      break;
    }

    case ActiveLink::Lte: {
      // Upgrade rule: N_OK consecutive WiFi6 probe successes
      if (p.wifi6_ok &&
          consec_ok_ >= cfg_.consec_ok_to_upgrade) {
        active_ = ActiveLink::Wifi6;
        reason  = "wifi6 ok streak (" +
                  std::to_string(consec_ok_) + ") — upgrade";
      } else if (!p.lte_ok) {
        // LTE went down too; we'd have to fall back to none
        active_ = ActiveLink::None;
        reason  = "lte down, wifi6 still recovering";
      }
      break;
    }
  }

  const bool switched = (active_ != prev);
  if (switched) ++switch_count_;

  LinkDecision d;
  d.active_link  = active_;
  d.switch_event = switched;
  d.reason       = switched ? reason : std::string();
  return d;
}

void LinkHealthMonitor::reset() {
  active_       = ActiveLink::None;
  consec_ok_    = 0;
  consec_fail_  = 0;
  switch_count_ = 0;
}

}  // namespace san_comm_link
