// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5b - LTE-driven video auto-rate controller.
//
// Maps LteLinkQuality.grade → target VideoStreamRequest.quality and
// emits CHANGE_QUALITY transitions through a caller-supplied applier
// callback. The class is standalone (no rclcpp / GStreamer deps) so
// the hysteresis FSM can be unit-tested without spinning a node.
//
// Hysteresis policy:
//   * Demotion (signal got worse): take effect immediately - we want
//     to clear bandwidth before LTE saturates.
//   * Promotion (signal got better): require N consecutive ticks of
//     the better grade before upgrading - this absorbs single-sample
//     glitches that would otherwise cause oscillation around the
//     RSRP threshold.
//
// External thumbnail lock:
//   The GStreamerRelayNode forces all streams to THUMBNAIL when 4+
//   are active. While that lock is engaged, the auto-rate controller
//   stays passive (it does not try to upgrade) - the concurrency
//   policy wins over the link-quality policy.

#pragma once

#include <combat_robot_msgs/msg/lte_link_quality.hpp>
#include <combat_robot_msgs/msg/video_stream_request.hpp>

#include <cstdint>
#include <functional>

namespace san_hub_comm
{

class AutoRateController
{
public:
  using Quality = uint8_t;
  using Grade = uint8_t;
  using LinkQualityMsg = combat_robot_msgs::msg::LteLinkQuality;
  using Req = combat_robot_msgs::msg::VideoStreamRequest;

  // Caller-supplied: invoked once per actual target change with the
  // new target quality. Idempotent ticks (no change) do not fire.
  using QualityApplier = std::function<void (Quality target_quality)>;

  struct Config
  {
    // How many consecutive better-grade ticks must accumulate before
    // we promote the active streams. Set to 1 to disable hysteresis
    // (not recommended).
    int promote_hold_ticks = 5;

    // The starting target the controller assumes before any LTE
    // sample arrives. Defaults to HD so the relay defaults match
    // the v1.3 spec.
    Quality initial_quality = Req::QUALITY_HD;

    // Promotion / demotion are computed against this ceiling - the
    // controller will never request quality higher than this even
    // if LTE is excellent. Useful when the operator wants to cap
    // the costliest streams.
    Quality ceiling_quality = Req::QUALITY_FHD;

    // [DCN-2026-006 EXT — source deep analysis §4.1] Demo Day flicker
    // mitigation: when LTE signal recovers (e.g. POOR → GOOD), the
    // bare D-020 grader returns the new grade in a single tick, so
    // AutoRateController would otherwise jump THUMBNAIL → HD (or
    // beyond) in one step. That step-change pushes the GStreamer
    // encoder through 4× bitrate inside one GOP boundary which is
    // visible to operators as a brief flicker. With
    // single_step_upgrade=true (the default), each successful
    // promotion advances the target by at most one quality level
    // (THUMBNAIL → LOW → HD → FHD); the next promote_hold_ticks
    // window grants the next step. Downgrades remain immediate.
    bool single_step_upgrade = true;

    // Explicit default ctor — declared here, defined out-of-line
    // in the .cpp so the NSDMI references to Req::QUALITY_* don't
    // need to be evaluated during the enclosing class's own parse.
    Config();
  };

  // Two overloads instead of an in-class `Config cfg = Config()`
  // default argument. GCC 11 refuses to parse the in-class default
  // arg because evaluating `Config()` triggers the NSDMI requirement
  // ("default member initializer ... required before the end of its
  // enclosing class") for promote_hold_ticks / initial_quality /
  // ceiling_quality before AutoRateController itself is complete.
  // Splitting into two overloads — single-arg delegates to two-arg
  // with a `Config{}` constructed in the .cpp — moves the NSDMI
  // evaluation to a translation unit where the class is fully
  // visible.
  explicit AutoRateController(QualityApplier applier);
  AutoRateController(QualityApplier applier, Config cfg);

  // Feed one LinkQuality observation. Returns the current target
  // quality after applying the new sample.
  Quality onLinkQuality(const LinkQualityMsg & msg);

  // Convenience overload for tests: feed a raw grade.
  Quality onGrade(Grade grade);

  // Concurrency-driven thumbnail lock from the relay node. While set,
  // the controller still tracks the latest grade but does not call
  // the applier. When the lock clears, the next sample resumes
  // normal operation.
  void setExternalThumbnailLock(bool locked);
  bool externalThumbnailLock() const {return external_lock_;}

  Quality currentTargetQuality() const {return current_target_;}
  Grade   lastGrade() const {return last_grade_;}
  int     pendingPromotionTicks() const {return pending_promotion_ticks_;}

  // Pure mapping for tests.
  static Quality gradeToQuality(Grade grade, Quality ceiling);

private:
  Config cfg_;
  QualityApplier applier_;
  Quality current_target_;
  Quality pending_promotion_target_;
  int pending_promotion_ticks_ = 0;
  Grade last_grade_ = LinkQualityMsg::LTE_GRADE_UNKNOWN;
  bool external_lock_ = false;

  static bool isUpgrade(Quality from, Quality to);
};

}  // namespace san_hub_comm
