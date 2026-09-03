#pragma once
#include "IEmbedder.h"

#include <memory>
#include <string>

namespace riposte {

// RK3588 NPU ReID embedder (RKNN runtime). Loads a compiled .rknn model and
// produces one L2-normalized appearance vector per detection crop. The RKNN
// dependency is confined to RknnEmbedder.cpp and compiled only when
// RIPOSTE_WITH_RKNN=ON — the same isolation HailoDetector keeps for HailoRT.
class RknnEmbedder final : public IEmbedder {
public:
    struct Params {
        std::string model_path;
        // RK3588 NPU core to pin this model to (0..2, -1 = auto). Pinning keeps
        // the ReID net off the core the T2 template tracker will use
        // (TRACKER-REQ §5 core placement).
        int core = -1;
    };

    explicit RknnEmbedder(Params p);
    ~RknnEmbedder() override;

    // Owns an rknn_context through the PIMPL; copying or moving would split
    // ownership of a live device handle (G5.2 RAII), same as IEmbedder's base
    // contract already states.
    RknnEmbedder(const RknnEmbedder&) = delete;
    RknnEmbedder& operator=(const RknnEmbedder&) = delete;
    RknnEmbedder(RknnEmbedder&&) = delete;
    RknnEmbedder& operator=(RknnEmbedder&&) = delete;

    bool init() override;
    bool embed(const Frame& f, const std::vector<Detection>& dets,
               std::vector<Embedding>& out) override;
    bool healthy() const override;
    const char* name() const override { return "RknnEmbedder"; }

private:
    // Real bodies; init()/embed() wrap these in the exception boundary that
    // keeps a vendor throw from reaching the worker thread (CR-05).
    bool init_impl();
    bool embed_impl(const Frame& f, const std::vector<Detection>& dets,
                    std::vector<Embedding>& out);

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace riposte
