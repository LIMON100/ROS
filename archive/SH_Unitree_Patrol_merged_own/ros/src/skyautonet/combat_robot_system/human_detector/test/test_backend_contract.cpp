// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - InferenceBackend interface contract test.
//
// Each backend type must agree on: name shape (lowercase ASCII),
// non-negative latency reporting, repeatable output for the same
// input (deterministic - empty stub returns deterministic empty).

#include <gtest/gtest.h>

#include <algorithm>
#include <opencv2/opencv.hpp>

#include "human_detector/inference_backend.hpp"
#include "human_detector/rk3588_npu_backend.hpp"
#include "human_detector/hailo8_backend.hpp"
#include "human_detector/stub_backend.hpp"

using namespace human_detector;

namespace {

bool isLowerAscii(const std::string& s) {
    return std::all_of(s.begin(), s.end(),
        [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
        }) && !s.empty();
}

}  // namespace

TEST(BackendContract, NamesAreStableLowercaseAscii) {
    RK3588NPUBackend rk;
    Hailo8Backend hl;
    StubBackend st;
    EXPECT_TRUE(isLowerAscii(rk.getName()));
    EXPECT_TRUE(isLowerAscii(hl.getName()));
    EXPECT_TRUE(isLowerAscii(st.getName()));
    EXPECT_NE(rk.getName(), hl.getName());
    EXPECT_NE(rk.getName(), st.getName());
}

TEST(BackendContract, UninitializedInferReturnsEmpty) {
    RK3588NPUBackend rk;
    Hailo8Backend hl;
    cv::Mat frame = cv::Mat::zeros(640, 640, CV_8UC3);
    EXPECT_TRUE(rk.infer(frame).empty());
    EXPECT_TRUE(hl.infer(frame).empty());
}

TEST(BackendContract, StubReturnsConsistentOutputForSameInput) {
    StubBackend st;
    ASSERT_TRUE(st.initialize(""));
    cv::Mat frame = cv::Mat::zeros(640, 640, CV_8UC3);
    auto out1 = st.infer(frame);
    auto out2 = st.infer(frame);
    EXPECT_EQ(out1.size(), out2.size());
    EXPECT_GT(st.getInferenceLatencyMs(), 0.0);
}

TEST(BackendContract, LatencyEmaIsNonNegative) {
    RK3588NPUBackend rk;
    rk.setLatencyForTest(0.0);
    EXPECT_GE(rk.getInferenceLatencyMs(), 0.0);
    rk.setLatencyForTest(15.5);
    EXPECT_NEAR(rk.getInferenceLatencyMs(), 15.5, 1e-6);
}

TEST(BackendContract, DetectionStructBboxOrdering) {
    Detection d{"person", 0.95f, 0, 10.f, 20.f, 30.f, 40.f};
    EXPECT_EQ(d.bbox[0], 10.f);
    EXPECT_EQ(d.bbox[1], 20.f);
    EXPECT_EQ(d.bbox[2], 30.f);
    EXPECT_EQ(d.bbox[3], 40.f);
    EXPECT_GT(d.bbox[2], d.bbox[0]);   // x2 > x1
    EXPECT_GT(d.bbox[3], d.bbox[1]);   // y2 > y1
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
