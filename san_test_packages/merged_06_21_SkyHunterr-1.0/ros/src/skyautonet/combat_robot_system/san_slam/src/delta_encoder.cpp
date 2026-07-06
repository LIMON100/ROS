// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_slam/delta_encoder.hpp"

namespace san_slam
{

std::vector<uint8_t> computeDelta(
  const std::vector<int8_t> & previous,
  const std::vector<int8_t> & current)
{
  std::vector<uint8_t> delta(current.size(), DELTA_NO_CHANGE);
  const bool have_prev = (previous.size() == current.size());

  for (std::size_t i = 0; i < current.size(); ++i) {
    const uint8_t cur = encodeCurrent(current[i]);
    if (!have_prev) {
      // First snapshot - report every defined cell as changed.
      if (cur != DELTA_NO_CHANGE) {delta[i] = cur;}
      continue;
    }
    const uint8_t prv = encodeCurrent(previous[i]);
    if (cur != prv) {delta[i] = cur;}
  }
  return delta;
}

}  // namespace san_slam
