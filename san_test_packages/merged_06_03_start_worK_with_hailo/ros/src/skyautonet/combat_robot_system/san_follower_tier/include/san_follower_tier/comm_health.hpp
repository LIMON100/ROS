// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — Comm-link + breadcrumb health monitor (pure C++17).
//
// PATCH 2026-05-13 (Tier deep-dive review):
//   The previous TierNode had hardcoded values for two TierInput fields:
//
//     in.comm_link_alive      = true;  // PDR-5: subscribe to /comm_link/status
//     in.breadcrumb_available = true;  // PDR-5: track from breadcrumb sub
//
//   This made T4 BREADCRUMB_RECOVERY effectively unreachable via the
//   60-s comm timeout path (one of two KPP-2 branches per SDD §6.2).
//
//   This module subscribes to the real telemetry topics and exposes
//   thread-safe accessors that the TierNode plugs into TierInput.
//
// Pure C++17, no rclcpp — caller wires the subscriptions and feeds
// timestamped samples in. This keeps the module gtest-friendly and
// usable from both rclpy and rclcpp wrappers.

#ifndef SAN_FOLLOWER_TIER__COMM_HEALTH_HPP_
#define SAN_FOLLOWER_TIER__COMM_HEALTH_HPP_

#include <cstdint>
#include <mutex>
#include <optional>

namespace san_follower_tier
{

/// Snapshot of comm + breadcrumb health at a point in time.
struct CommSnapshot
{
  bool comm_link_alive = false;
  uint32_t comm_loss_ms = 0;                // accumulated since last "alive"
  bool breadcrumb_available = false;
  uint32_t breadcrumb_age_ms = 0;           // since last breadcrumb update
};

/// Thread-safe comm/breadcrumb health tracker.
///
/// Two independent state sources:
///   * CommLink — periodic /comm_link/status (san_comm_link node).
///     Each call to `observeCommLink(now_ms, alive)` updates state.
///     When `alive=true`, comm_loss_ms resets to 0.
///     When `alive=false`, comm_loss_ms tracks elapsed since the last
///     alive observation.
///   * Breadcrumb — last breadcrumb timestamp from /breadcrumb topic.
///     `observeBreadcrumb(now_ms)` records the latest receipt.
///     The TTL parameter decides when the breadcrumb is considered stale.
class CommHealth
{
public:
  /// Construct with breadcrumb TTL (default 30 s per SDD §6.2).
  explicit CommHealth(uint32_t breadcrumb_ttl_ms = 30000);

  /// Observe a CommLink heartbeat / status update.
  void observeCommLink(uint64_t now_ms, bool alive);

  /// Observe a breadcrumb arrival.
  void observeBreadcrumb(uint64_t now_ms);

  /// Read a coherent snapshot. Safe to call from any thread.
  CommSnapshot snapshot(uint64_t now_ms) const;

  /// Configuration accessors.
  uint32_t breadcrumbTtlMs() const {return breadcrumb_ttl_ms_;}

private:
  mutable std::mutex mu_;
  std::optional<uint64_t> last_alive_ms_;       // last observed alive=true
  std::optional<uint64_t> down_since_ms_;       // stamp of alive→down edge
  std::optional<uint64_t> last_breadcrumb_ms_;  // last breadcrumb seen
  bool last_comm_alive_{false};
  uint32_t breadcrumb_ttl_ms_;
};

}  // namespace san_follower_tier

#endif  // SAN_FOLLOWER_TIER__COMM_HEALTH_HPP_
