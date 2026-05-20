// SAN v1.5 Phase 2-E Turn 8 — LinkSelector implementation.

#include "san_comm/link_selector.hpp"

namespace san_comm {

const char* toString(ActiveLink link) {
  switch (link) {
    case ActiveLink::None:  return "none";
    case ActiveLink::Wifi6: return "wifi6";
    case ActiveLink::Lte:   return "lte";
  }
  return "?";
}

LinkSelector::LinkSelector(LinkSelectorConfig cfg) : cfg_(cfg) {}

ActiveLink LinkSelector::update(const LinkProbe& probe) {
  const bool lte_usable = probe.lte_registered && probe.lte_pdp_active;

  // Maintain WiFi6 recovery counter:
  //   - If currently on LTE and WiFi6 is reachable, count up
  //   - If WiFi6 unreachable, reset counter
  if (current_ == ActiveLink::Lte) {
    if (probe.wifi6_reachable) {
      ++wifi_recovery_count_;
    } else {
      wifi_recovery_count_ = 0;
    }
  } else {
    wifi_recovery_count_ = 0;
  }

  // Decision tree
  ActiveLink next = current_;

  switch (current_) {
    case ActiveLink::None:
      // From cold start, prefer WiFi6, fall back to LTE if usable.
      if (probe.wifi6_reachable) {
        next = ActiveLink::Wifi6;
      } else if (lte_usable) {
        next = ActiveLink::Lte;
        ++failover_count_;     // counted as failover even from None
      } else {
        next = ActiveLink::None;
      }
      break;

    case ActiveLink::Wifi6:
      if (probe.wifi6_reachable) {
        next = ActiveLink::Wifi6;       // sticky
      } else if (lte_usable) {
        next = ActiveLink::Lte;
        ++failover_count_;
      } else {
        next = ActiveLink::None;
      }
      break;

    case ActiveLink::Lte:
      // Only switch back to WiFi6 after hysteresis threshold reached.
      if (wifi_recovery_count_ >= cfg_.wifi_recovery_threshold) {
        next = ActiveLink::Wifi6;
        ++recovery_count_;
        wifi_recovery_count_ = 0;
      } else if (!lte_usable) {
        // LTE dropped too — try whatever's available right now.
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

}  // namespace san_comm
