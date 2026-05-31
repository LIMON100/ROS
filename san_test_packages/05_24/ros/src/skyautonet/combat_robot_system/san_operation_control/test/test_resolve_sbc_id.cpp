// SAN v1.5.2 — DCN-2026-011 D-032 sbc_id resolver tests.
//
// Coverage:
//   T1 ParameterOverrideWinsOverFile
//   T2 FileReadWhenParamUnset
//   T3 InvalidFileContentFallsToZero
//   T4 MissingFileFallsToZero
//   T5 InvalidParamFallsToFile

#include <gtest/gtest.h>
#include <rclcpp/rclcpp.hpp>

#include <filesystem>
#include <fstream>
#include <string>

#include "san_operation_control/operation_control_node.hpp"

namespace san_operation_control {
namespace {

rclcpp::NodeOptions makeOpts(int sbc_id_param) {
    rclcpp::NodeOptions opts;
    opts.parameter_overrides({
        {"robot_id", 2},
        {"deployment_mode", std::string("bench")},
        {"sbc_id", sbc_id_param},
        // tick_timer would otherwise drive publishStatus 1 Hz during
        // the test; bench mode + a short ctor scope keeps things quiet.
    });
    return opts;
}

class ResolveSbcIdTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!rclcpp::ok()) rclcpp::init(0, nullptr);
        tmp_path_ = std::filesystem::temp_directory_path() /
            ("skyautonet_sbc_id_" +
             std::string(::testing::UnitTest::GetInstance()
                         ->current_test_info()->name()));
    }
    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove(tmp_path_, ec);   // best-effort cleanup
    }

    void writeFile(const std::string& content) const {
        std::ofstream f(tmp_path_);
        f << content;
    }

    std::filesystem::path tmp_path_;
};

}  // namespace

// ─── T1: launch param wins over file ──────────────────────────────────
TEST_F(ResolveSbcIdTest, ParameterOverrideWinsOverFile) {
    auto node = std::make_shared<OperationControlNode>(makeOpts(1));
    writeFile("2");
    EXPECT_EQ(node->resolveSbcIdForTest(tmp_path_.string()), 1u)
        << "param=1 must override file content '2'";
}

// ─── T2: file read when param is the default sentinel ────────────────
TEST_F(ResolveSbcIdTest, FileReadWhenParamUnset) {
    // -1 is the documented "auto-resolve" sentinel; the resolver should
    // fall through to the file.
    auto node = std::make_shared<OperationControlNode>(makeOpts(-1));
    writeFile("1");
    EXPECT_EQ(node->resolveSbcIdForTest(tmp_path_.string()), 1u);

    writeFile("2\n");   // trailing newline must be tolerated
    EXPECT_EQ(node->resolveSbcIdForTest(tmp_path_.string()), 2u);
}

// ─── T3: malformed file falls back to 0 ──────────────────────────────
TEST_F(ResolveSbcIdTest, InvalidFileContentFallsToZero) {
    auto node = std::make_shared<OperationControlNode>(makeOpts(-1));
    writeFile("x");
    EXPECT_EQ(node->resolveSbcIdForTest(tmp_path_.string()), 0u);

    writeFile("9");   // digit but out of [0,2]
    EXPECT_EQ(node->resolveSbcIdForTest(tmp_path_.string()), 0u);

    writeFile("");    // empty file
    EXPECT_EQ(node->resolveSbcIdForTest(tmp_path_.string()), 0u);
}

// ─── T4: missing file falls back to 0 ────────────────────────────────
TEST_F(ResolveSbcIdTest, MissingFileFallsToZero) {
    auto node = std::make_shared<OperationControlNode>(makeOpts(-1));
    EXPECT_EQ(node->resolveSbcIdForTest("/non/existent/path"), 0u);
}

// ─── T5: invalid launch param value falls through to the file ───────
TEST_F(ResolveSbcIdTest, InvalidParamFallsToFile) {
    // Out-of-range positive (7) is treated as "unset" → file lookup.
    auto node = std::make_shared<OperationControlNode>(makeOpts(7));
    writeFile("2");
    EXPECT_EQ(node->resolveSbcIdForTest(tmp_path_.string()), 2u);

    // Out-of-range negative (-99) also rejected → file lookup.
    auto node2 = std::make_shared<OperationControlNode>(makeOpts(-99));
    EXPECT_EQ(node2->resolveSbcIdForTest(tmp_path_.string()), 2u);
}

}  // namespace san_operation_control
