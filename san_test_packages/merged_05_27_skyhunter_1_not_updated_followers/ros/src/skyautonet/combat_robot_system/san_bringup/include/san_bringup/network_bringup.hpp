// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-014 Item 8.
//
// Pure-logic helpers for the network_bringup executable. Header-only
// so the gtest can include without linking the binary.

#ifndef SAN_BRINGUP__NETWORK_BRINGUP_HPP_
#define SAN_BRINGUP__NETWORK_BRINGUP_HPP_

#include <cctype>
#include <optional>
#include <string>

namespace san_bringup::network
{

// Returns the static mesh0 IP for a (robot_id, sbc_id) pair, or
// nullptr if the combo is not in the deterministic swarm map.
//
// IMPORTANT: keep this mapping in lockstep with
//   san_bringup/config/fastrtps_profile.xml initialPeersList
// and test_fastrtps_profile_selection T9. Changing any IP here
// without updating those two will silently break peer discovery in
// production.
inline const char * lookupIp(int robot_id, int sbc_id)
{
  if (robot_id == 1 && sbc_id == 0) {
    return "192.168.50.10";                                   // Leader Go2
  }
  if (robot_id == 2 && sbc_id == 1) {
    return "192.168.50.20";                                   // Hub SBC #1
  }
  if (robot_id == 2 && sbc_id == 2) {
    return "192.168.50.21";                                   // Hub SBC #2
  }
  if (robot_id == 3 && sbc_id == 0) {
    return "192.168.50.30";                                   // Deputy UGV
  }
  if (robot_id == 4 && sbc_id == 0) {
    return "192.168.50.40";                                   // Follower 1
  }
  if (robot_id == 5 && sbc_id == 0) {
    return "192.168.50.41";                                   // Follower 2
  }
  if (robot_id == 6 && sbc_id == 0) {
    return "192.168.50.42";                                   // Follower 3
  }
  if (robot_id == 7 && sbc_id == 0) {
    return "192.168.50.43";                                   // Follower 4
  }
  if (robot_id == 8 && sbc_id == 0) {
    return "192.168.50.44";                                   // Follower 5
  }
  return nullptr;
}

inline std::string trim(std::string s)
{
  while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
    s.pop_back();
  }
  std::size_t start = 0;
  while (start < s.size() &&
    std::isspace(static_cast<unsigned char>(s[start])))
  {
    ++start;
  }
  return s.substr(start);
}

// Strict integer parse: optional leading sign + digits only.
// Returns std::nullopt on empty input, garbage characters, or
// out-of-range. Trims surrounding whitespace.
inline std::optional<int> parseInt(const std::string & raw)
{
  const std::string s = trim(raw);
  if (s.empty()) {return std::nullopt;}
  std::size_t i = 0;
  if (s[0] == '+' || s[0] == '-') {
    if (s.size() == 1) {return std::nullopt;}
    ++i;
  }
  for (; i < s.size(); ++i) {
    if (!std::isdigit(static_cast<unsigned char>(s[i]))) {return std::nullopt;}
  }
  try {
    return std::stoi(s);
  } catch (...) {
    return std::nullopt;
  }
}

}  // namespace san_bringup::network

#endif  // SAN_BRINGUP__NETWORK_BRINGUP_HPP_
