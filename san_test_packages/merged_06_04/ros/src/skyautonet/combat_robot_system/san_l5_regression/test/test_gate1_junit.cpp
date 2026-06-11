// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-022] renderJunitXml pure-logic tests.
//
// Verifies XML schema (testsuites > testsuite > testcase + failure /
// property), counter aggregation, and XML-escape correctness — all
// without spinning a node or running a live scenario.

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "san_l5_regression/scenarios/gate1_scenarios.hpp"

namespace san_l5_regression
{
namespace
{

ScenarioReport makePass(const std::string & id, int elapsed_ms = 100)
{
  ScenarioReport r;
  r.id = id;
  r.description = id + " desc";
  r.recordPass(elapsed_ms);
  r.attributes["mode"] = "live";
  return r;
}

ScenarioReport makeFail(const std::string & id, const std::string & why)
{
  ScenarioReport r;
  r.id = id;
  r.description = id + " desc";
  r.recordFail(why);
  return r;
}

// ─── J1 empty suite → valid skeleton ─────────────────────────────────────
TEST(Gate1Junit, J1_EmptySuiteEmitsSkeleton) {
  auto xml = renderJunitXml("Gate-1", {});
  EXPECT_NE(xml.find("<?xml"), std::string::npos);
  EXPECT_NE(xml.find("<testsuites"), std::string::npos);
  EXPECT_NE(xml.find("tests=\"0\""), std::string::npos);
  EXPECT_NE(xml.find("failures=\"0\""), std::string::npos);
}

// ─── J2 all-pass suite → no <failure> elements ───────────────────────────
TEST(Gate1Junit, J2_AllPassNoFailureElements) {
  std::vector<ScenarioReport> rs = {
    makePass("L5_26"), makePass("L5_27"), makePass("L5_30")
  };
  auto xml = renderJunitXml("Gate-1", rs);
  EXPECT_NE(xml.find("tests=\"3\""), std::string::npos);
  EXPECT_NE(xml.find("failures=\"0\""), std::string::npos);
  EXPECT_EQ(xml.find("<failure"), std::string::npos);
  EXPECT_NE(xml.find("name=\"L5_26\""), std::string::npos);
  EXPECT_NE(xml.find("name=\"L5_30\""), std::string::npos);
}

// ─── J3 mixed → failure counted + message present ────────────────────────
TEST(Gate1Junit, J3_MixedFailureSurfaced) {
  std::vector<ScenarioReport> rs = {
    makePass("L5_26"),
    makeFail("L5_30", "/rth action server not available"),
    makePass("L5_33"),
  };
  auto xml = renderJunitXml("Gate-1", rs);
  EXPECT_NE(xml.find("tests=\"3\""), std::string::npos);
  EXPECT_NE(xml.find("failures=\"1\""), std::string::npos);
  EXPECT_NE(xml.find("<failure"), std::string::npos);
  EXPECT_NE(
    xml.find("/rth action server not available"),
    std::string::npos);
}

// ─── J4 XML escape (<, &, ") ─────────────────────────────────────────────
TEST(Gate1Junit, J4_XmlEscape) {
  ScenarioReport r;
  r.id = "L5_99";
  r.description = "tricky <test> & \"quotes\"";
  r.recordFail("payload: <bad> & \"escaped\"");
  auto xml = renderJunitXml("Gate-1", {r});
  EXPECT_EQ(xml.find("<bad>"), std::string::npos)
    << "raw '<bad>' must be escaped to '&lt;bad&gt;'";
  EXPECT_NE(xml.find("&lt;bad&gt;"), std::string::npos);
  EXPECT_NE(xml.find("&amp;"), std::string::npos);
  EXPECT_NE(xml.find("&quot;"), std::string::npos);
}

// ─── J5 attributes surface as <property> children ────────────────────────
TEST(Gate1Junit, J5_AttributesEmittedAsProperty) {
  ScenarioReport r;
  r.id = "L5_30";
  r.recordPass(150);
  r.attributes["final_distance_m"] = "1.42";
  r.attributes["success"] = "true";
  auto xml = renderJunitXml("Gate-1", {r});
  EXPECT_NE(
    xml.find("<property name=\"final_distance_m\""),
    std::string::npos);
  EXPECT_NE(xml.find("value=\"1.42\""), std::string::npos);
  EXPECT_NE(
    xml.find("<property name=\"success\""),
    std::string::npos);
}

// ─── J6 TIMEOUT counts as failure ────────────────────────────────────────
TEST(Gate1Junit, J6_TimeoutCountedInFailures) {
  ScenarioReport r;
  r.id = "L5_30";
  r.recordTimeout();
  auto xml = renderJunitXml("Gate-1", {r});
  EXPECT_NE(xml.find("failures=\"1\""), std::string::npos);
  EXPECT_NE(xml.find("type=\"TIMEOUT\""), std::string::npos);
}

// ─── J7 (audit C3 P2): SKIP outcome renders <skipped/> and counts
// in skipped attribute, NOT in failures.
TEST(Gate1Junit, J7_SkipRendersAsSkippedElement) {
  ScenarioReport r;
  r.id = "L5_27";
  r.description = "RTK lock — sustained 5 s";
  r.recordSkip("RTK live harness not implemented");

  auto xml = renderJunitXml("Gate-1", {r});
  EXPECT_NE(xml.find("skipped=\"1\""), std::string::npos)
    << "testsuites must carry skipped count";
  EXPECT_NE(xml.find("<skipped message=\""), std::string::npos);
  EXPECT_EQ(xml.find("<failure"), std::string::npos)
    << "SKIP must NOT render as failure";
  EXPECT_NE(
    xml.find("RTK live harness not implemented"),
    std::string::npos);
}

}  // namespace
}  // namespace san_l5_regression

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
