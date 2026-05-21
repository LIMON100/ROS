// SAN v1.3 PHASE 5b - LTE link-quality grading + status-file parser.

#include <gtest/gtest.h>

#include "san_lte_redundancy/lte_link_quality_grader.hpp"
#include "san_lte_redundancy/lte_link_quality_node.hpp"

using san_lte_redundancy::LteLinkQualityGrader;
using san_lte_redundancy::LteLinkQualityNode;
using san_lte_redundancy::LteSignalRaw;
using Msg = combat_robot_msgs::msg::LteLinkQuality;

TEST(LinkQualityGrader, RsrpBucketsAtThreeGppBoundaries) {
    // 3GPP 36.214 bands: POOR < -110 <= FAIR < -100 <= GOOD < -85 <= EXCELLENT
    EXPECT_EQ(LteLinkQualityGrader::grade(-50),  Msg::LTE_GRADE_EXCELLENT);
    EXPECT_EQ(LteLinkQualityGrader::grade(-85),  Msg::LTE_GRADE_EXCELLENT);
    EXPECT_EQ(LteLinkQualityGrader::grade(-86),  Msg::LTE_GRADE_GOOD);
    EXPECT_EQ(LteLinkQualityGrader::grade(-100), Msg::LTE_GRADE_GOOD);
    EXPECT_EQ(LteLinkQualityGrader::grade(-101), Msg::LTE_GRADE_FAIR);
    EXPECT_EQ(LteLinkQualityGrader::grade(-110), Msg::LTE_GRADE_FAIR);
    EXPECT_EQ(LteLinkQualityGrader::grade(-111), Msg::LTE_GRADE_POOR);
    EXPECT_EQ(LteLinkQualityGrader::grade(-140), Msg::LTE_GRADE_POOR);
}

TEST(StatusBlobParser, AcceptsThreeKeysAnyOrder) {
    auto r = LteLinkQualityNode::parseStatusBlob(
        "rsrq_db=-10\nrsrp_dbm=-95\nsinr_db=14\n");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.rsrp_dbm, -95);
    EXPECT_EQ(r.rsrq_db, -10);
    EXPECT_EQ(r.sinr_db, 14);
}

TEST(StatusBlobParser, MissingRsrpIsInvalid) {
    auto r = LteLinkQualityNode::parseStatusBlob(
        "rsrq_db=-10\nsinr_db=14\n");
    EXPECT_FALSE(r.valid);
}

TEST(StatusBlobParser, IgnoresUnknownKeysAndComments) {
    auto r = LteLinkQualityNode::parseStatusBlob(
        "# auto-generated, do not edit\n"
        "modem=qmi\n"
        "rsrp_dbm=-72\n"
        "garbage line without equals\n");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.rsrp_dbm, -72);
}

TEST(StatusBlobParser, ClampsImplausibleValues) {
    auto r = LteLinkQualityNode::parseStatusBlob(
        "rsrp_dbm=-99999\nrsrq_db=99\nsinr_db=-9999\n");
    EXPECT_TRUE(r.valid);
    EXPECT_EQ(r.rsrp_dbm, -140);    // floor
    EXPECT_EQ(r.rsrq_db, 0);        // ceiling
    EXPECT_EQ(r.sinr_db, -30);      // floor
}

TEST(StatusBlobParser, NonNumericValueIsDropped) {
    auto r = LteLinkQualityNode::parseStatusBlob(
        "rsrp_dbm=excellent\n");
    EXPECT_FALSE(r.valid)
        << "non-numeric rsrp_dbm must NOT set valid=true";
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
