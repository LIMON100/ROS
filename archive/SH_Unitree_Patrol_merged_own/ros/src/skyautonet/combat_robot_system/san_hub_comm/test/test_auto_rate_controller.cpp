// SAN v1.3 PHASE 5b - AutoRateController hysteresis FSM unit test.

#include <gtest/gtest.h>

#include <vector>

#include "san_hub_comm/auto_rate_controller.hpp"

using san_hub_comm::AutoRateController;
using Lq  = combat_robot_msgs::msg::LteLinkQuality;
using Req = combat_robot_msgs::msg::VideoStreamRequest;

namespace {

struct ApplierSpy {
    std::vector<uint8_t> calls;
    AutoRateController::QualityApplier fn() {
        return [this](uint8_t q) { calls.push_back(q); };
    }
};

}  // namespace

TEST(AutoRateController, GradeToQualityMapping) {
    using AR = AutoRateController;
    EXPECT_EQ(AR::gradeToQuality(Lq::LTE_GRADE_EXCELLENT, Req::QUALITY_FHD),
              Req::QUALITY_FHD);
    EXPECT_EQ(AR::gradeToQuality(Lq::LTE_GRADE_GOOD,      Req::QUALITY_FHD),
              Req::QUALITY_HD);
    EXPECT_EQ(AR::gradeToQuality(Lq::LTE_GRADE_FAIR,      Req::QUALITY_FHD),
              Req::QUALITY_LOW);
    EXPECT_EQ(AR::gradeToQuality(Lq::LTE_GRADE_POOR,      Req::QUALITY_FHD),
              Req::QUALITY_THUMBNAIL);
}

TEST(AutoRateController, CeilingCapsExcellent) {
    using AR = AutoRateController;
    EXPECT_EQ(AR::gradeToQuality(Lq::LTE_GRADE_EXCELLENT, Req::QUALITY_HD),
              Req::QUALITY_HD)
        << "ceiling=HD must clamp EXCELLENT down";
}

TEST(AutoRateController, DemotionIsImmediate) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality = Req::QUALITY_HD;
    cfg.promote_hold_ticks = 5;
    AutoRateController c(spy.fn(), cfg);

    EXPECT_EQ(c.onGrade(Lq::LTE_GRADE_POOR), Req::QUALITY_THUMBNAIL);
    ASSERT_EQ(spy.calls.size(), 1u);
    EXPECT_EQ(spy.calls.back(), Req::QUALITY_THUMBNAIL);
}

TEST(AutoRateController, PromotionRequiresConsecutiveHoldTicks) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality = Req::QUALITY_THUMBNAIL;
    cfg.promote_hold_ticks = 3;
    // [DCN-2026-006 EXT — bitrate smoothing] This test predates the
    // single-step gate; disable so the original THUMBNAIL → HD jump
    // assertion stays valid. The smoothing path is exercised by the
    // SingleStepUpgrade* cases below.
    cfg.single_step_upgrade = false;
    AutoRateController c(spy.fn(), cfg);

    // 2 good ticks not enough.
    EXPECT_EQ(c.onGrade(Lq::LTE_GRADE_GOOD), Req::QUALITY_THUMBNAIL);
    EXPECT_EQ(c.onGrade(Lq::LTE_GRADE_GOOD), Req::QUALITY_THUMBNAIL);
    EXPECT_TRUE(spy.calls.empty());

    // 3rd good tick promotes.
    EXPECT_EQ(c.onGrade(Lq::LTE_GRADE_GOOD), Req::QUALITY_HD);
    ASSERT_EQ(spy.calls.size(), 1u);
    EXPECT_EQ(spy.calls.back(), Req::QUALITY_HD);
}

TEST(AutoRateController, FairTickInBetweenResetsPromotionCounter) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality = Req::QUALITY_THUMBNAIL;
    cfg.promote_hold_ticks = 3;
    cfg.single_step_upgrade = false;   // see PromotionRequires... rationale
    AutoRateController c(spy.fn(), cfg);

    // 2 good ticks build pending HD promotion (counter=2).
    c.onGrade(Lq::LTE_GRADE_GOOD);    // pending HD=1
    c.onGrade(Lq::LTE_GRADE_GOOD);    // pending HD=2
    EXPECT_TRUE(spy.calls.empty());

    // FAIR maps to LOW, which is still an upgrade from THUMBNAIL — so
    // there is no immediate apply. The intent of this test is that the
    // FAIR tick resets the in-flight HD promotion counter (target
    // switches from HD to LOW with ticks=1). The next GOOD tick will
    // restart the HD candidate from 1, NOT continue from 3.
    c.onGrade(Lq::LTE_GRADE_FAIR);    // pending target → LOW, ticks=1
    EXPECT_TRUE(spy.calls.empty());

    // Restart of HD candidate: 3 consecutive GOOD ticks required.
    c.onGrade(Lq::LTE_GRADE_GOOD);    // pending HD=1 (reset from LOW)
    c.onGrade(Lq::LTE_GRADE_GOOD);    // pending HD=2
    EXPECT_TRUE(spy.calls.empty());
    c.onGrade(Lq::LTE_GRADE_GOOD);    // pending HD=3 → promote HD
    ASSERT_EQ(spy.calls.size(), 1u);
    EXPECT_EQ(spy.calls.back(), Req::QUALITY_HD);
}

TEST(AutoRateController, IdempotentTickIsNoApply) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality = Req::QUALITY_HD;
    AutoRateController c(spy.fn(), cfg);

    c.onGrade(Lq::LTE_GRADE_GOOD);
    c.onGrade(Lq::LTE_GRADE_GOOD);
    c.onGrade(Lq::LTE_GRADE_GOOD);
    EXPECT_TRUE(spy.calls.empty())
        << "stable matching grade must not fire applier";
}

TEST(AutoRateController, ExternalLockSuppressesApplyButTracksGrade) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality = Req::QUALITY_HD;
    AutoRateController c(spy.fn(), cfg);

    c.setExternalThumbnailLock(true);
    c.onGrade(Lq::LTE_GRADE_POOR);
    EXPECT_TRUE(spy.calls.empty())
        << "lock must veto applier even though target changed";
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_THUMBNAIL)
        << "target tracking still updates while locked";

    // Releasing the lock: next sample should fire normally.
    c.setExternalThumbnailLock(false);
    c.onGrade(Lq::LTE_GRADE_GOOD);   // promotion candidate from THUMBNAIL
    EXPECT_TRUE(spy.calls.empty())
        << "promotion needs hold ticks, no immediate apply";
}

TEST(AutoRateController, UnknownGradeIsNoChange) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality = Req::QUALITY_HD;
    cfg.promote_hold_ticks = 2;
    AutoRateController c(spy.fn(), cfg);

    c.onGrade(Lq::LTE_GRADE_GOOD);
    EXPECT_EQ(c.pendingPromotionTicks(), 0)
        << "GOOD matches current HD target, no pending counter";

    c.onGrade(Lq::LTE_GRADE_EXCELLENT);    // pending=1
    EXPECT_EQ(c.pendingPromotionTicks(), 1);

    c.onGrade(Lq::LTE_GRADE_UNKNOWN);
    EXPECT_EQ(c.pendingPromotionTicks(), 0)
        << "UNKNOWN must reset promotion counter";
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_HD);
    EXPECT_TRUE(spy.calls.empty());
}

// ─── DCN-2026-006 EXT source deep analysis §4.1 ─────────────────────────
// Single-step upgrade gating: when LTE recovers (POOR → GOOD or beyond)
// the underlying StatefulLteLinkQualityGrader returns the new grade
// immediately, so without smoothing the controller would jump the encoder
// from THUMBNAIL to HD/FHD inside one GOP boundary → visible UI flicker.

TEST(AutoRateController, SingleStepUpgradeClampsMultiStepJump) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality   = Req::QUALITY_THUMBNAIL;
    cfg.promote_hold_ticks = 2;
    cfg.single_step_upgrade = true;   // default — explicit for clarity
    AutoRateController c(spy.fn(), cfg);

    // Pin to FHD-equivalent grade for many ticks. With smoothing,
    // each promote_hold_ticks window advances exactly one quality level.
    auto bump = [&] { c.onGrade(Lq::LTE_GRADE_EXCELLENT); };

    // Tick 1, 2 — first promotion: THUMBNAIL → LOW (one step).
    bump(); bump();
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_LOW);
    ASSERT_EQ(spy.calls.size(), 1u);
    EXPECT_EQ(spy.calls.back(), Req::QUALITY_LOW);

    // Tick 3, 4 — second promotion: LOW → HD.
    bump(); bump();
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_HD);
    EXPECT_EQ(spy.calls.size(), 2u);

    // Tick 5, 6 — third promotion: HD → FHD (now matches pending; settled).
    bump(); bump();
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_FHD);
    EXPECT_EQ(spy.calls.size(), 3u);

    // Tick 7, 8 — no further promotion (already at target).
    bump(); bump();
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_FHD);
    EXPECT_EQ(spy.calls.size(), 3u);
}

TEST(AutoRateController, SingleStepUpgradeDisableRestoresJump) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality      = Req::QUALITY_THUMBNAIL;
    cfg.promote_hold_ticks   = 2;
    cfg.single_step_upgrade  = false;   // disable smoothing
    AutoRateController c(spy.fn(), cfg);

    c.onGrade(Lq::LTE_GRADE_EXCELLENT);  // pending=1
    c.onGrade(Lq::LTE_GRADE_EXCELLENT);  // pending=2 → fires

    // Without smoothing the jump is THUMBNAIL → FHD in a single promotion.
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_FHD);
    ASSERT_EQ(spy.calls.size(), 1u);
    EXPECT_EQ(spy.calls.back(), Req::QUALITY_FHD);
}

TEST(AutoRateController, DemotionRemainsImmediateUnderSmoothing) {
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality     = Req::QUALITY_FHD;
    cfg.promote_hold_ticks  = 2;
    cfg.single_step_upgrade = true;
    AutoRateController c(spy.fn(), cfg);

    // POOR → multi-step demotion FHD → THUMBNAIL — must fire in one tick,
    // bypassing smoothing (we need to clear bandwidth fast).
    c.onGrade(Lq::LTE_GRADE_POOR);
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_THUMBNAIL);
    ASSERT_EQ(spy.calls.size(), 1u);
    EXPECT_EQ(spy.calls.back(), Req::QUALITY_THUMBNAIL);
}

TEST(AutoRateController, SingleStepUpgradeOneLevelStillFiresAtOnce) {
    // Adjacent quality levels (e.g. HD → FHD) must promote in one
    // promote_hold_ticks window — no artificial slowdown.
    ApplierSpy spy;
    AutoRateController::Config cfg;
    cfg.initial_quality      = Req::QUALITY_HD;
    cfg.promote_hold_ticks   = 2;
    cfg.single_step_upgrade  = true;
    AutoRateController c(spy.fn(), cfg);

    c.onGrade(Lq::LTE_GRADE_EXCELLENT);
    c.onGrade(Lq::LTE_GRADE_EXCELLENT);
    EXPECT_EQ(c.currentTargetQuality(), Req::QUALITY_FHD);
    EXPECT_EQ(spy.calls.size(), 1u);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
