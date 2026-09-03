#pragma once
#include "IDetector.h"

#include <memory>
#include <string>
#include <vector>

namespace riposte {

// HailoRT-backed detector (Hailo-8, M.2 PCIe). Loads a compiled .hef and runs
// async inference. The HailoRT dependency is confined to HailoDetector.cpp and
// compiled only when RIPOSTE_WITH_HAILO=ON, so the rest of the tree builds on a
// plain dev PC.
class HailoDetector final : public IDetector {
public:
    struct Params {
        std::string hef_path;
        float score_threshold = 0.4f;
        float nms_iou = 0.45f;
        int target_class = 0;
        // Square network input edge. Overwritten at init() from the HEF's actual
        // input shape — the config value is only the pre-load expectation.
        int model_size = 640;
        int num_classes = 1;
    };

    explicit HailoDetector(Params p);
    ~HailoDetector() override;

    bool init() override;
    bool detect(const Frame& f, std::vector<Detection>& out) override;
    bool healthy() const override;
    const char* name() const override { return "HailoDetector"; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace riposte
