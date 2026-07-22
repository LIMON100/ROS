#include "san_hub_comm/auto_rate_controller.hpp"

namespace san_hub_comm {

namespace {

uint8_t clampQuality(uint8_t q, uint8_t ceiling) {
    return q > ceiling ? ceiling : q;
}

}  // namespace

// Out-of-line Config default ctor — leaves the NSDMIs to do the work.
// Must be defined in the .cpp (not = default in the header) so the
// NSDMI evaluation happens in a TU where AutoRateController and its
// nested Config are fully visible. Otherwise GCC 11 complains that
// the NSDMI is "required before the end of its enclosing class" when
// it parses the ctor declaration list.
AutoRateController::Config::Config() = default;

// Single-arg overload delegates to the two-arg form with an empty
// Config{}; the brace-init happens here where the class is complete.
AutoRateController::AutoRateController(QualityApplier applier)
    : AutoRateController(std::move(applier), Config{}) {}

AutoRateController::AutoRateController(QualityApplier applier, Config cfg)
    : cfg_(cfg),
      applier_(std::move(applier)),
      current_target_(cfg.initial_quality),
      pending_promotion_target_(cfg.initial_quality)
{
    if (cfg_.promote_hold_ticks < 1) cfg_.promote_hold_ticks = 1;
}

bool AutoRateController::isUpgrade(Quality from, Quality to) {
    // VideoStreamRequest.QUALITY_* are ordered THUMBNAIL=0 < LOW=1 <
    // HD=2 < FHD=3, so a numeric comparison is the right "is_upgrade"
    // predicate.
    return static_cast<int>(to) > static_cast<int>(from);
}

AutoRateController::Quality
AutoRateController::gradeToQuality(Grade grade, Quality ceiling) {
    Quality q;
    switch (grade) {
        case LinkQualityMsg::LTE_GRADE_EXCELLENT: q = Req::QUALITY_FHD;       break;
        case LinkQualityMsg::LTE_GRADE_GOOD:      q = Req::QUALITY_HD;        break;
        case LinkQualityMsg::LTE_GRADE_FAIR:      q = Req::QUALITY_LOW;       break;
        case LinkQualityMsg::LTE_GRADE_POOR:      q = Req::QUALITY_THUMBNAIL; break;
        default:                                  q = Req::QUALITY_HD;        break;
    }
    return clampQuality(q, ceiling);
}

AutoRateController::Quality
AutoRateController::onGrade(Grade grade) {
    last_grade_ = grade;

    // UNKNOWN samples do not move the FSM either direction - we have
    // no evidence to act on. The pending promotion counter is reset
    // so a glitch can't accumulate hold time.
    if (grade == LinkQualityMsg::LTE_GRADE_UNKNOWN) {
        pending_promotion_ticks_ = 0;
        pending_promotion_target_ = current_target_;
        return current_target_;
    }

    const Quality computed = gradeToQuality(grade, cfg_.ceiling_quality);

    // Demotion: takes effect immediately.
    if (!isUpgrade(current_target_, computed) && computed != current_target_) {
        current_target_ = computed;
        pending_promotion_target_ = computed;
        pending_promotion_ticks_ = 0;
        if (applier_ && !external_lock_) applier_(current_target_);
        return current_target_;
    }

    // Same quality - reset any in-flight promotion attempt.
    if (computed == current_target_) {
        pending_promotion_ticks_ = 0;
        pending_promotion_target_ = current_target_;
        return current_target_;
    }

    // Promotion candidate: needs `promote_hold_ticks` consecutive
    // ticks of the same (or better) target before we commit.
    if (computed == pending_promotion_target_) {
        ++pending_promotion_ticks_;
    } else {
        pending_promotion_target_ = computed;
        pending_promotion_ticks_ = 1;
    }
    if (pending_promotion_ticks_ >= cfg_.promote_hold_ticks) {
        // [DCN-2026-006 EXT — source deep analysis §4.1] Single-step
        // upgrade gate: clamp the per-promotion advance to one quality
        // level (THUMBNAIL → LOW → HD → FHD). Without this, a multi-
        // step jump (e.g. THUMBNAIL → HD on LTE recovery) pushes the
        // encoder bitrate through ~4× inside one GOP boundary which is
        // visible as a flicker to the operator. Subsequent ticks at
        // the same elevated grade continue the climb one step per
        // promote_hold_ticks window. Caller can disable with
        // single_step_upgrade=false for tests / aggressive recovery.
        Quality next_target = pending_promotion_target_;
        if (cfg_.single_step_upgrade) {
            const int diff = static_cast<int>(pending_promotion_target_) -
                              static_cast<int>(current_target_);
            if (diff > 1) {
                next_target = static_cast<Quality>(
                    static_cast<int>(current_target_) + 1);
            }
        }
        current_target_ = next_target;
        pending_promotion_ticks_ = 0;
        if (applier_ && !external_lock_) applier_(current_target_);
    }
    return current_target_;
}

AutoRateController::Quality
AutoRateController::onLinkQuality(const LinkQualityMsg& msg) {
    return onGrade(msg.grade);
}

void AutoRateController::setExternalThumbnailLock(bool locked) {
    external_lock_ = locked;
}

}  // namespace san_hub_comm
