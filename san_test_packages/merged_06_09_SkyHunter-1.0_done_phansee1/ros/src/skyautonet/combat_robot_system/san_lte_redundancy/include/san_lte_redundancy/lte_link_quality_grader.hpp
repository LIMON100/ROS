// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 5b - RSRP/RSRQ → LTE grade mapping.
//
// Pure function used by both the publisher (LteLinkQualityNode) and the
// consumer (AutoRateController). Thresholds match 3GPP 36.214 §5.1.1 and
// the bands carriers publish in coverage maps - we don't reinvent them
// here. Kept header-only so unit tests can link without the libuci /
// libubus stack.
//
// [DCN-2026-006 EXT D-020] Hysteresis added.
//   The stateless grade(rsrp) below is preserved for backwards
//   compatibility (and tests that exercise pure threshold logic).
//   Production code SHOULD use StatefulLteLinkQualityGrader, which
//   widens the upgrade threshold by HYSTERESIS_DB to suppress
//   chatter around the cliff between GOOD/FAIR (~-100 dBm in urban
//   coverage). Field observation (v1.5.1 W-2 drive test, Suwon
//   uplink): unhysteresized grader oscillated GOOD↔FAIR at ~3 Hz
//   when stationary in -98 .. -102 dBm zone, causing AutoRateController
//   to thrash bitrate and visibly flicker the operator UI.

#pragma once

#include <cstdint>

#include <combat_robot_msgs/msg/lte_link_quality.hpp>

namespace san_lte_redundancy
{

struct LteSignalRaw
{
  int16_t rsrp_dbm = 0;
  int8_t rsrq_db = 0;
  int8_t sinr_db = 0;
  bool valid = false;
};

class LteLinkQualityGrader
{
public:
  using Msg = combat_robot_msgs::msg::LteLinkQuality;

  // Single source of truth - returns LTE_GRADE_* values from the msg.
  // STATELESS — no hysteresis. Use StatefulLteLinkQualityGrader in
  // production code; this overload remains for unit-test clarity and
  // direct integration paths that need a pure function.
  static uint8_t grade(int16_t rsrp_dbm)
  {
    if (rsrp_dbm >= -85) {return Msg::LTE_GRADE_EXCELLENT;}
    if (rsrp_dbm >= -100) {return Msg::LTE_GRADE_GOOD;}
    if (rsrp_dbm >= -110) {return Msg::LTE_GRADE_FAIR;}
    return Msg::LTE_GRADE_POOR;
  }

  static const char * gradeToString(uint8_t g)
  {
    switch (g) {
      case Msg::LTE_GRADE_EXCELLENT: return "EXCELLENT";
      case Msg::LTE_GRADE_GOOD:      return "GOOD";
      case Msg::LTE_GRADE_FAIR:      return "FAIR";
      case Msg::LTE_GRADE_POOR:      return "POOR";
      default:                       return "UNKNOWN";
    }
  }
};

// [DCN-2026-006 EXT D-020] Hysteresis grader.
//
// Stateful wrapper around LteLinkQualityGrader::grade() that widens
// the upgrade threshold by HYSTERESIS_DB. Asymmetric thresholds:
//   downgrade boundaries: -85 / -100 / -110  (same as stateless)
//   upgrade   boundaries: -83 / -98  / -108  (2 dB tighter)
//
// State is one byte (last_grade_). Reset() forces re-evaluation on
// next sample — useful for unit tests and for explicitly invalidating
// state after a SIM swap / cell handover.
//
// Thread-safety: NOT thread-safe. LteLinkQualityNode is single-
// threaded; if shared across executors, wrap in a mutex.
class StatefulLteLinkQualityGrader
{
public:
  using Msg = combat_robot_msgs::msg::LteLinkQuality;

  // 2 dB matches 3GPP 36.331 §5.5.4 cell-reselection hysteresis
  // recommendation (Qhyst range 0..24 dB, typical 2 dB).
  static constexpr int16_t HYSTERESIS_DB = 2;

  uint8_t grade(int16_t rsrp_dbm)
  {
    const uint8_t new_grade = gradeWithHysteresis(rsrp_dbm, last_grade_);
    last_grade_ = new_grade;
    return new_grade;
  }

  void reset() {last_grade_ = Msg::LTE_GRADE_UNKNOWN;}

  uint8_t lastGrade() const {return last_grade_;}

private:
  // Pure function — testable in isolation. Given current rsrp and
  // previous grade, returns the new grade applying hysteresis on
  // the upward direction only (downgrade is immediate).
  static uint8_t gradeWithHysteresis(int16_t rsrp_dbm, uint8_t prev)
  {
    // Downgrade: use stateless thresholds.
    const uint8_t bare = LteLinkQualityGrader::grade(rsrp_dbm);
    if (prev == Msg::LTE_GRADE_UNKNOWN) {return bare;}
    if (bare <= prev) {
      return bare;                      // downgrade or same — immediate
    }
    // Upgrade candidate — require margin above the boundary that
    // separates `prev` from the next-better grade.
    // Boundaries: POOR | -110 | FAIR | -100 | GOOD | -85 | EXCELLENT
    switch (prev) {
      case Msg::LTE_GRADE_POOR:
        if (rsrp_dbm >= -110 + HYSTERESIS_DB) {return bare;}
        return prev;
      case Msg::LTE_GRADE_FAIR:
        if (rsrp_dbm >= -100 + HYSTERESIS_DB) {return bare;}
        return prev;
      case Msg::LTE_GRADE_GOOD:
        if (rsrp_dbm >= -85 + HYSTERESIS_DB) {return bare;}
        return prev;
      case Msg::LTE_GRADE_EXCELLENT:
      default:
        return bare;
    }
  }

  uint8_t last_grade_ = Msg::LTE_GRADE_UNKNOWN;
};

}  // namespace san_lte_redundancy
