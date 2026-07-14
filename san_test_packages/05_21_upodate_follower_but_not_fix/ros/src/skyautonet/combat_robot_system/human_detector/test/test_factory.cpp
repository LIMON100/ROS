// SAN v1.5.1 (was v1.3 PHASE 6 — see DCN-2026-004 D-011) - createBackend factory unit test.
//
// Verifies the factory dispatch is name-based, case-insensitive, and
// accepts the documented aliases.

#include <gtest/gtest.h>

#include "human_detector/inference_backend.hpp"

using human_detector::createBackend;

TEST(InferenceBackendFactory, KnownNamesProduceBackend) {
    EXPECT_EQ(createBackend("rk3588")->getName(), "rk3588");
    EXPECT_EQ(createBackend("hailo8")->getName(), "hailo8");
    EXPECT_EQ(createBackend("stub")->getName(), "stub");
}

TEST(InferenceBackendFactory, AliasesResolveToCanonicalName) {
    EXPECT_EQ(createBackend("RK3588")->getName(), "rk3588");
    EXPECT_EQ(createBackend("rknn")->getName(), "rk3588");
    EXPECT_EQ(createBackend("Hailo")->getName(), "hailo8");
    EXPECT_EQ(createBackend("hailort")->getName(), "hailo8");
    EXPECT_EQ(createBackend("none")->getName(), "stub");
    EXPECT_EQ(createBackend("CPU")->getName(), "stub");
}

TEST(InferenceBackendFactory, UnknownNameReturnsNullptr) {
    EXPECT_EQ(createBackend("foo"), nullptr);
    EXPECT_EQ(createBackend(""), nullptr);
}

TEST(InferenceBackendFactory, StubInitializeAlwaysSucceeds) {
    auto stub = createBackend("stub");
    ASSERT_NE(stub, nullptr);
    EXPECT_TRUE(stub->initialize("/dev/null"));
    EXPECT_TRUE(stub->isReady());
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
