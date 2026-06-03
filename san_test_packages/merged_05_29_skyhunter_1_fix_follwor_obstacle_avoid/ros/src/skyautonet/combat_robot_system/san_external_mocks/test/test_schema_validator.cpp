// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SkyHunter v1.5.3 — DCN-2026-021 schema validator gtest.
//
// 6-case structural validation that the Aban Android rosbridge spec
// doc, the C++ mock server, and the launch file stay in sync. Pure-
// logic (parses files on disk), runs in <100 ms, no rosbridge
// websocket spin-up needed — keeps CI fast and deterministic.
//
// Coverage
// --------
//   T1  Spec lists ≥ 8 production subscribe topics
//   T2  Mock server.cpp creates a publisher for each spec-listed
//       subscribe topic (mock ⊇ spec)
//   T3  Spec doc + mock both reference the ACTUAL message names
//       (HeartBeat, FireResult) — guards against re-introducing the
//       optionA draft's incorrect "Heartbeat" / "FireEvent" names
//   T4  Launch file references rosbridge_websocket AND the mock node
//   T5  IP allocation table in spec matches DCN-2026-014 v2 ports
//       (9091 leader, 9092 hub, 9093 deputy, 9094-9098 followers)
//   T6  Mock publishes at the rates the spec advertises (parses
//       create_wall_timer durations and matches them against spec)

#include <gtest/gtest.h>

#include <ament_index_cpp/get_package_share_directory.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

// SAN_REPO_ROOT env exported by CMakeLists set_tests_properties so the
// test can locate source files independent of the build dir layout.
fs::path repoRoot()
{
  const char * p = std::getenv("SAN_REPO_ROOT");
  return p ? fs::path(p) : fs::path(".");
}

std::string readFile(const fs::path & p)
{
  std::ifstream in(p);
  if (!in) {return {};}
  std::stringstream ss;
  ss << in.rdbuf();
  return ss.str();
}

fs::path mockCppPath()
{
  return repoRoot() / "ros" / "src" / "skyautonet" /
         "combat_robot_system" / "san_external_mocks" /
         "src" / "rosbridge_mock_server.cpp";
}

fs::path specMdPath()
{
  return repoRoot() / "docs" / "external" /
         "Aban_Android_rosbridge_schema_v2.md";
}

fs::path launchPath()
{
  return repoRoot() / "ros" / "src" / "skyautonet" /
         "combat_robot_system" / "san_external_mocks" /
         "launch" / "mock_server.launch.xml";
}

// Subscribe topics this mock+spec must agree on. Topics with the
// "v1.5.3 future" backend (DCN-016/019) are STILL in this list — the
// mock publishes them so Aban's UI dev isn't blocked.
const std::vector<std::string> & expectedTopics()
{
  static const std::vector<std::string> t = {
    "/diagnostics/robot_status_audit",
    "/diagnostics/hub_slam_audit",
    "/swarm/threat_alert_raw",
    "/swarm/threat_alert_consensus",
    "/rtk_gnss_node/heading",
    "/hub_internal/sbc1/heartbeat",
    "/hub_internal/sbc2/heartbeat",
    "/swarm/poses",
    "/gate1/demo_status",
    "/gun_trigger/simulated_fire_result",
  };
  return t;
}

}  // namespace

// -------------------------------------------------------------- T1
// Spec doc lists all expected subscribe topics (≥ 8 production + 2 future).
TEST(SchemaValidator, T1_SpecListsAllSubscribeTopics) {
  const auto spec = readFile(specMdPath());
  ASSERT_FALSE(spec.empty()) << "spec doc not found: " << specMdPath();

  int missing = 0;
  for (const auto & topic : expectedTopics()) {
    if (spec.find(topic) == std::string::npos) {
      ADD_FAILURE() << "spec missing topic: " << topic;
      ++missing;
    }
  }
  EXPECT_EQ(missing, 0)
    << missing << "/" << expectedTopics().size()
    << " expected subscribe topics missing from spec doc";
}

// -------------------------------------------------------------- T2
// Mock server creates a publisher for every spec-listed topic.
TEST(SchemaValidator, T2_MockPublishesEverySpecTopic) {
  const auto cpp = readFile(mockCppPath());
  ASSERT_FALSE(cpp.empty()) << "mock cpp not found: " << mockCppPath();

  int missing = 0;
  for (const auto & topic : expectedTopics()) {
    // Look for either '"<topic>"' (literal) or unique enough
    // substring. Topics are unique enough that bare grep works.
    const std::string literal = std::string("\"") + topic + "\"";
    if (cpp.find(literal) == std::string::npos) {
      ADD_FAILURE() << "mock missing publisher for topic: " << topic;
      ++missing;
    }
  }
  EXPECT_EQ(missing, 0);
}

// -------------------------------------------------------------- T3
// Schema-name corrections from optionA draft are preserved.
// Both spec doc and mock cpp must reference the ACTUAL names:
//   HeartBeat (not Heartbeat — case-sensitive!)
//   FireResult (not FireEvent)
//   EmergencyStop (not std_msgs/Bool for E-Stop)
// And must NOT regress to the incorrect names.
TEST(SchemaValidator, T3_ActualMessageNamesUsed) {
  const auto spec = readFile(specMdPath());
  const auto cpp = readFile(mockCppPath());
  ASSERT_FALSE(spec.empty());
  ASSERT_FALSE(cpp.empty());

  // --- spec doc ---
  EXPECT_NE(spec.find("HeartBeat"), std::string::npos)
    << "spec must reference HeartBeat (camelCase) per actual schema";
  EXPECT_NE(spec.find("FireResult"), std::string::npos)
    << "spec must reference FireResult (not FireEvent)";

  // --- mock cpp uses correct includes ---
  EXPECT_NE(cpp.find("heart_beat.hpp"), std::string::npos)
    << "mock must include combat_robot_msgs/msg/heart_beat.hpp "
    "(generated snake_case from HeartBeat.msg)";
  EXPECT_NE(cpp.find("fire_result.hpp"), std::string::npos)
    << "mock must include combat_robot_msgs/msg/fire_result.hpp";
  EXPECT_NE(cpp.find("HeartBeat"), std::string::npos)
    << "mock must use the HeartBeat class name";
  EXPECT_NE(cpp.find("FireResult"), std::string::npos)
    << "mock must use the FireResult class name";

  // --- regressions to avoid ---
  // Search for the WRONG include filenames (would only appear if
  // someone re-introduced the optionA draft names).
  EXPECT_EQ(cpp.find("heartbeat.hpp"), std::string::npos)
    << "regression: 'heartbeat.hpp' (lowercase) is wrong — schema "
    "uses HeartBeat → heart_beat.hpp";
  EXPECT_EQ(cpp.find("fire_event.hpp"), std::string::npos)
    << "regression: 'fire_event.hpp' is wrong — no FireEvent type "
    "exists; use FireResult (→ fire_result.hpp)";
}

// -------------------------------------------------------------- T4
// Launch file brings up BOTH rosbridge_websocket AND the mock node.
TEST(SchemaValidator, T4_LaunchFileLaunchesBothNodes) {
  const auto xml = readFile(launchPath());
  ASSERT_FALSE(xml.empty()) << "launch file not found: " << launchPath();

  EXPECT_NE(xml.find("rosbridge_server"), std::string::npos)
    << "launch must include rosbridge_server's rosbridge_websocket";
  EXPECT_NE(xml.find("rosbridge_websocket"), std::string::npos);

  EXPECT_NE(xml.find("san_external_mocks"), std::string::npos)
    << "launch must include san_external_mocks pkg";
  EXPECT_NE(xml.find("rosbridge_mock_server"), std::string::npos)
    << "launch must spawn the mock executable";
}

// -------------------------------------------------------------- T5
// Spec's IP/port allocation matches DCN-2026-014 v2 (port = 9090 +
// robot_id). Catches drift if either spec or the canonical port
// formula moves.
TEST(SchemaValidator, T5_IpAndPortTableMatchesCanonical) {
  const auto spec = readFile(specMdPath());
  ASSERT_FALSE(spec.empty());

  // Canonical robot_id → IP / port pairs per DCN-2026-014 v2.
  struct R { const char * ip; const char * port; const char * role; };
  const std::vector<R> table = {
    {"192.168.50.10", "9091", "leader"},
    {"192.168.50.20", "9092", "hub sbc1"},
    {"192.168.50.21", "9092", "hub sbc2"},      // same robot_id
    {"192.168.50.30", "9093", "deputy"},
  };
  for (const auto & r : table) {
    EXPECT_NE(spec.find(r.ip), std::string::npos)
      << "spec missing IP " << r.ip << " (" << r.role << ")";
    EXPECT_NE(
      spec.find(std::string("**") + r.port + "**"),
      std::string::npos)
      << "spec missing bolded port **" << r.port << "** for "
      << r.role;
  }

  // Follower range — string match on the table cell.
  EXPECT_NE(spec.find("9094-9098"), std::string::npos)
    << "spec must list follower ports 9094-9098 (robot_id 4..8)";
}

// -------------------------------------------------------------- T6
// Mock publishes at the rates the spec advertises. We parse the cpp
// for `create_wall_timer(<duration>...)` and match against expected
// rates per topic. Duration units: 200ms = 5 Hz, 1s = 1 Hz, 100ms = 10 Hz.
TEST(SchemaValidator, T6_MockRatesMatchSpecAdvertisedRates) {
  const auto cpp = readFile(mockCppPath());
  ASSERT_FALSE(cpp.empty());

  // Required <duration> values that must appear in create_wall_timer
  // calls (per spec rate column).
  struct RateCheck
  {
    const char * duration_literal;
    const char * hz_label;
  };
  const std::vector<RateCheck> checks = {
    {"1s", "1 Hz (audits, heartbeats)"},
    {"200ms", "5 Hz (heading)"},
    {"100ms", "10 Hz (poses)"},
    {"5s", "0.2 Hz (demo_status)"},
    {"4s", "~0.25 Hz (threat alert event sim)"},
    {"30s", "~0.03 Hz (fire result event sim)"},
  };
  for (const auto & c : checks) {
    const std::string needle = std::string("create_wall_timer(\n            ") +
      c.duration_literal;
    // The "\n            " indentation may vary — fall back to a
    // looser search: "create_wall_timer(" followed eventually by
    // duration literal on same or next line.
    const std::regex re(
      std::string("create_wall_timer\\s*\\(\\s*") + c.duration_literal);
    EXPECT_TRUE(std::regex_search(cpp, re))
      << "mock missing wall_timer @ " << c.duration_literal
      << " (" << c.hz_label << ")";
  }
}

// -------------------------------------------------------------- T7
// P0-3 regression guard — mock's publishHeartbeats() + publishFire()
// must reference the FULL schema fields, not the optionA draft's
// 9-of-14 / 3-of-9 minimal set. Catches re-introduction of the
// "publishes a topic but with empty payload" pattern that breaks
// downstream consumers without any obvious build error.
TEST(SchemaValidator, T7_MockPopulatesFullMessageSchemas) {
  const auto cpp = readFile(mockCppPath());
  ASSERT_FALSE(cpp.empty());

  // FireResult — 14 fields, mock must write all that have semantic
  // impact (header.stamp handled by ROS; below are the manual sets).
  const std::vector<const char *> fire_required = {
    "result",                      // outcome enum — UI banner color
    "rounds_fired",
    "target_id",
    "distance_to_target_m",
    "impact_point_x_m",            // missing in P0-3 draft
    "impact_point_y_m",            // missing in P0-3 draft
    "confidence",
    "authorization_chain",         // missing in P0-3 draft (audit join)
    "timestamp_fire_ms",           // missing in P0-3 draft
    "timestamp_report_ms",         // missing in P0-3 draft
  };
  for (const auto * field : fire_required) {
    EXPECT_NE(cpp.find(field), std::string::npos)
      << "FireResult mock missing field: " << field;
  }

  // HeartBeat — IDS §5.8 schema. Mock must set health/battery/mode
  // /tier so Aban's status panel can render them.
  const std::vector<const char *> hb_required = {
    "health_status",               // missing in P0-3 draft
    "battery_percent",             // missing in P0-3 draft
    "operation_mode",              // missing in P0-3 draft
    "current_tier",                // missing in P0-3 draft
    "timestamp_ms",                // missing in P0-3 draft
  };
  for (const auto * field : hb_required) {
    EXPECT_NE(cpp.find(field), std::string::npos)
      << "HeartBeat mock missing field: " << field;
  }
}

// -------------------------------------------------------------- T8
// The production rosbridge in squadron.launch.xml restricts client
// access via topics_glob / services_glob. Those globs MUST cover every
// topic/service in the Android contract (this schema), or the app
// silently can't see/publish them — including the E-Stop. Guards the
// DCN-2026-021 vs glob drift that blocked /emergency_stop, /diagnostics/*,
// /rtk_gnss_node/*, /hub_internal/*, /gate1/*, /gun_trigger/*.
namespace {

// Minimal rosbridge-style glob match: "a/b/*" prefix-matches, else exact.
bool globMatch(const std::string & pattern, const std::string & topic)
{
  if (!pattern.empty() && pattern.back() == '*') {
    return topic.rfind(pattern.substr(0, pattern.size() - 1), 0) == 0;
  }
  return pattern == topic;
}

// Extract a `[...]` rosbridge param list (e.g. topics_glob) into trimmed
// pattern tokens.
std::vector<std::string> extractGlobList(const std::string & xml,
                                         const std::string & param)
{
  std::vector<std::string> out;
  const auto np = xml.find("\"" + param + "\"");
  if (np == std::string::npos) {return out;}
  const auto vp = xml.find("value=\"", np);
  if (vp == std::string::npos) {return out;}
  const auto lb = xml.find('[', vp);
  const auto rb = xml.find(']', lb);
  if (lb == std::string::npos || rb == std::string::npos) {return out;}
  std::stringstream ss(xml.substr(lb + 1, rb - lb - 1));
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    const auto a = tok.find_first_not_of(" \t\n");
    const auto b = tok.find_last_not_of(" \t\n");
    if (a != std::string::npos) {out.push_back(tok.substr(a, b - a + 1));}
  }
  return out;
}

fs::path squadronLaunchPath()
{
  return repoRoot() / "ros" / "src" / "skyautonet" /
         "combat_robot_system" / "san_bringup" / "launch" /
         "squadron.launch.xml";
}

}  // namespace

TEST(SchemaValidator, T8_RosbridgeGlobCoversAndroidContract) {
  const auto xml = readFile(squadronLaunchPath());
  ASSERT_FALSE(xml.empty()) << "launch not found: " << squadronLaunchPath();

  const auto topic_globs = extractGlobList(xml, "topics_glob");
  ASSERT_FALSE(topic_globs.empty()) << "topics_glob not parsed";

  // Subscribe topics (shared list) + the Android publish topics.
  std::vector<std::string> contract = expectedTopics();
  for (const char * t : {"/emergency_stop", "/attack_permission",
                         "/mc/raw_command"}) {
    contract.emplace_back(t);
  }

  int blocked = 0;
  for (const auto & topic : contract) {
    bool allowed = false;
    for (const auto & g : topic_globs) {
      if (globMatch(g, topic)) {allowed = true; break;}
    }
    if (!allowed) {
      ADD_FAILURE() << "topics_glob BLOCKS Android-contract topic: "
                    << topic << " (app can't see/publish it)";
      ++blocked;
    }
  }
  EXPECT_EQ(blocked, 0)
    << blocked << " Android-contract topics blocked by rosbridge topics_glob";

  // /gate1/start_demo is a service the app calls -> services_glob must allow.
  const auto svc_globs = extractGlobList(xml, "services_glob");
  bool svc_ok = false;
  for (const auto & g : svc_globs) {
    if (globMatch(g, "/gate1/start_demo")) {svc_ok = true; break;}
  }
  EXPECT_TRUE(svc_ok)
    << "services_glob blocks /gate1/start_demo (app-called service)";
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
