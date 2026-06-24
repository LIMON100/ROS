// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// DCN-2026-026 C-2 — Encircle combat trigger + lifecycle impl.

#include "san_formation/encircle_combat.hpp"

#include <cmath>
#include <cstdio>

namespace san_formation
{

bool passesEncircleGate(
  uint8_t severity, uint8_t threat_type,
  std::optional<float> confidence, bool has_position, float range_m,
  float min_confidence)
{
  if (severity < threat_alert::SEVERITY_CRITICAL) {return false;}
  if (threat_type != threat_alert::TYPE_DRONE_DETECTED &&
    threat_type != threat_alert::TYPE_OTHER)
  {
    return false;
  }
  if (!confidence || *confidence < min_confidence) {return false;}
  if (!has_position || !(range_m > 0.0f)) {return false;}
  return true;
}

std::optional<float> parseConfidenceFromDetail(const std::string & detail)
{
  // detection_to_threat emits ...,"confidence":0.9123,... — locate the
  // key and scan the number. No JSON library by project convention.
  static constexpr char kKey[] = "\"confidence\":";
  const auto pos = detail.find(kKey);
  if (pos == std::string::npos) {return std::nullopt;}
  float v = 0.0f;
  if (std::sscanf(detail.c_str() + pos + sizeof(kKey) - 1, "%f", &v) != 1) {
    return std::nullopt;
  }
  if (!std::isfinite(v) || v < 0.0f || v > 1.0f) {return std::nullopt;}
  return v;
}

std::optional<uint32_t> parseRobotIdString(const std::string & source)
{
  static constexpr char kPrefix[] = "robot_";
  std::size_t pos = 0;
  if (source.rfind(kPrefix, 0) == 0) {pos = sizeof(kPrefix) - 1;}
  if (pos >= source.size()) {return std::nullopt;}
  uint32_t id = 0;
  for (std::size_t i = pos; i < source.size(); ++i) {
    const char c = source[i];
    if (c < '0' || c > '9') {return std::nullopt;}
    id = id * 10u + static_cast<uint32_t>(c - '0');
    if (id > 1000000u) {return std::nullopt;}
  }
  return id;
}

std::pair<float, float> threatWorldXY(
  float reporter_x, float reporter_y, float bearing_deg, float range_m)
{
  const float b = bearing_deg * static_cast<float>(M_PI) / 180.0f;
  return {
    reporter_x + range_m * std::cos(b),
    reporter_y + range_m * std::sin(b)};
}

bool EncircleCombat::onQualifiedThreat(
  float world_x, float world_y, uint64_t now_ms)
{
  switch (phase_) {
    case EncirclePhase::Cooldown:
      return false;                       // hysteresis — ignore
    case EncirclePhase::Idle:
      anchor_x_ = world_x;
      anchor_y_ = world_y;
      last_threat_ms_ = now_ms;
      phase_ = cfg_.auto_engage ?
        EncirclePhase::Active : EncirclePhase::PendingConfirm;
      return true;
    case EncirclePhase::PendingConfirm:
    case EncirclePhase::Active:
      anchor_x_ = world_x;                // newest fix wins
      anchor_y_ = world_y;
      last_threat_ms_ = now_ms;           // TTL refresh
      return false;
  }
  return false;
}

bool EncircleCombat::onOperatorConfirm(uint64_t now_ms)
{
  if (phase_ != EncirclePhase::PendingConfirm) {return false;}
  last_threat_ms_ = now_ms;
  phase_ = EncirclePhase::Active;
  return true;
}

bool EncircleCombat::onOperatorRelease(uint64_t now_ms)
{
  if (phase_ != EncirclePhase::PendingConfirm &&
    phase_ != EncirclePhase::Active)
  {
    return false;
  }
  cooldown_since_ms_ = now_ms;
  phase_ = EncirclePhase::Cooldown;
  return true;
}

bool EncircleCombat::tick(uint64_t now_ms)
{
  switch (phase_) {
    case EncirclePhase::PendingConfirm:
    case EncirclePhase::Active:
      if (now_ms - last_threat_ms_ > cfg_.ttl_ms) {
        cooldown_since_ms_ = now_ms;
        phase_ = EncirclePhase::Cooldown;
        return true;
      }
      return false;
    case EncirclePhase::Cooldown:
      if (now_ms - cooldown_since_ms_ > cfg_.reentry_block_ms) {
        phase_ = EncirclePhase::Idle;
        return true;
      }
      return false;
    case EncirclePhase::Idle:
      return false;
  }
  return false;
}

}  // namespace san_formation
