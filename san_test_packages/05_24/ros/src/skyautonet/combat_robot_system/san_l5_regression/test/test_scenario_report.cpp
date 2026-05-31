// SAN v1.4 L5 regression - ScenarioReport / writer formatting test.

#include <gtest/gtest.h>

#include "san_l5_regression/scenario_report.hpp"

using namespace san_l5_regression;

TEST(ScenarioReport, RecordPassClearsFailReason) {
    ScenarioReport r;
    r.recordFail("transient probe");
    r.recordPass(1234);
    EXPECT_EQ(r.outcome, Outcome::PASS);
    ASSERT_TRUE(r.elapsed_ms.has_value());
    EXPECT_EQ(*r.elapsed_ms, 1234);
    EXPECT_TRUE(r.fail_reason.empty());
}

TEST(ScenarioReport, RecordTimeoutDropsElapsed) {
    ScenarioReport r;
    r.recordPass(900);     // 시작 시 pass
    r.recordTimeout();
    EXPECT_EQ(r.outcome, Outcome::TIMEOUT);
    EXPECT_FALSE(r.elapsed_ms.has_value());
    EXPECT_EQ(r.fail_reason, "deadline exceeded");
}

TEST(ReportWriter, AllPassedRequiresEveryReportPass) {
    ScenarioReportWriter w;
    ScenarioReport a; a.id = "S18-1"; a.recordPass(100);
    ScenarioReport b; b.id = "S18-2"; b.recordPass(200);
    w.add(a);
    w.add(b);
    EXPECT_TRUE(w.allPassed());
    EXPECT_EQ(w.countPass(), 2);
    EXPECT_EQ(w.countFail(), 0);
}

TEST(ReportWriter, AllPassedFalseWhenAnyTimeout) {
    ScenarioReportWriter w;
    ScenarioReport a; a.id = "S18-1"; a.recordPass(100);
    ScenarioReport b; b.id = "S18-2"; b.recordTimeout();
    w.add(a);
    w.add(b);
    EXPECT_FALSE(w.allPassed());
    EXPECT_EQ(w.countTimeout(), 1);
}

TEST(ReportWriter, JsonContainsScenarioFields) {
    ScenarioReportWriter w;
    ScenarioReport a;
    a.id = "S18-1";
    a.description = "Leader → Deputy 승계";
    a.deadline_ms = 5000;
    a.recordPass(842);
    a.attributes["promoted_robot_id"] = "3";
    a.attributes["succession_priority"] = "DEPUTY";
    w.add(a);

    const std::string json = w.renderJson();
    EXPECT_NE(json.find("\"id\": \"S18-1\""), std::string::npos);
    EXPECT_NE(json.find("\"outcome\": \"PASS\""), std::string::npos);
    EXPECT_NE(json.find("\"elapsed_ms\": 842"), std::string::npos);
    EXPECT_NE(json.find("\"deadline_ms\": 5000"), std::string::npos);
    EXPECT_NE(json.find("promoted_robot_id"), std::string::npos);
    EXPECT_NE(json.find("\"summary\""), std::string::npos);
    EXPECT_NE(json.find("\"pass\": 1"), std::string::npos);
}

TEST(ReportWriter, MarkdownLinesPerScenario) {
    ScenarioReportWriter w;
    ScenarioReport a;
    a.id = "S18-3";
    a.description = "Hub → Deputy 인수";
    a.deadline_ms = 7000;
    a.recordTimeout();
    w.add(a);

    const std::string md = w.renderMarkdown();
    EXPECT_NE(md.find("S18-3"), std::string::npos);
    EXPECT_NE(md.find("TIMEOUT"), std::string::npos);
    EXPECT_NE(md.find("deadline exceeded"), std::string::npos);
    EXPECT_NE(md.find("7000 ms"), std::string::npos);
}

TEST(ReportWriter, JsonEscapesQuotesAndNewlines) {
    ScenarioReportWriter w;
    ScenarioReport a;
    a.id = "S18-x";
    a.recordFail("got \"unexpected\"\nrole");
    w.add(a);

    const std::string json = w.renderJson();
    EXPECT_NE(json.find("\\\"unexpected\\\""), std::string::npos);
    EXPECT_NE(json.find("\\n"), std::string::npos);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
