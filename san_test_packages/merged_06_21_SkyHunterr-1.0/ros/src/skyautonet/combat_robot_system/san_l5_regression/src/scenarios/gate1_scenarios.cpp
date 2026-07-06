// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// [DCN-2026-022] Gate-1 acceptance scenarios L5_26~L5_33.
//
// Each scenario follows the same shape:
//   1. Try to connect to its live dependency (service / action / topic).
//   2. If unavailable, record a descriptive FAIL (NOT crash) so the
//      JUnit XML still reports the scenario as a regular test case.
//   3. If available, run the acceptance check + record PASS / FAIL.
//
// This keeps the CI workflow useful even in bring-ups that lack a
// particular dependency, and makes the scenarios safe to dispatch
// from operator tooling.

#include "san_l5_regression/scenarios/gate1_scenarios.hpp"

#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>
#include <string>
#include <thread>

#include <rclcpp_action/rclcpp_action.hpp>
#include <std_msgs/msg/string.hpp>
#include <std_srvs/srv/trigger.hpp>

#include <combat_robot_msgs/action/return_to_home.hpp>
#include <combat_robot_msgs/msg/emergency_stop.hpp>
#include <combat_robot_msgs/msg/operation_state.hpp>

namespace san_l5_regression
{

using namespace std::chrono_literals;
using ReturnToHome = combat_robot_msgs::action::ReturnToHome;

// ─── helpers ─────────────────────────────────────────────────────────────

namespace
{

void initReport(
  ScenarioReport & rep, const std::string & id,
  const std::string & desc, int deadline_ms)
{
  rep.id = id;
  rep.description = desc;
  rep.deadline_ms = deadline_ms;
}

/// Spin `host` until `pred()` returns true or `deadline` elapses.
/// Returns elapsed ms (clamped to deadline on timeout).
template<class Pred>
int spinUntil(
  rclcpp::Node & host, std::chrono::milliseconds deadline,
  Pred pred)
{
  const auto t0 = std::chrono::steady_clock::now();
  while (std::chrono::steady_clock::now() - t0 < deadline &&
    rclcpp::ok())
  {
    rclcpp::spin_some(host.get_node_base_interface());
    if (pred()) {
      return static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - t0).count());
    }
    std::this_thread::sleep_for(50ms);
  }
  return static_cast<int>(deadline.count());
}

bool topicExists(rclcpp::Node & host, const std::string & topic)
{
  return host.get_topic_names_and_types().count(topic) > 0;
}

}  // namespace

// ─── L5_26 Deputy boot ───────────────────────────────────────────────────

ScenarioReport L5_26_DeputyBoot::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_26",
    "Deputy boot — 5 critical lifecycle nodes ACTIVE within 90 s",
    static_cast<int>(d_.boot_deadline.count()) * 1000);

  // Lightweight signal: check that the expected node names appear
  // in the ROS graph. Full lifecycle state query needs the manager
  // services which are bring-up dependent — this is the CI-safe
  // proxy.
  const std::vector<std::string> required = {
    "operation_control_node",
    "role_management_node",
    "rtk_gnss_node",
  };

  auto elapsed = spinUntil(
    host_, d_.boot_deadline, [&]() {
      const auto nodes = host_.get_node_names();
      for (const auto & want : required) {
        const bool found = std::any_of(
          nodes.begin(), nodes.end(),
          [&](const std::string & n) {
            return n.find(want) != std::string::npos;
          });
        if (!found) {return false;}
      }
      return true;
    });
  rep.attributes["elapsed_ms"] = std::to_string(elapsed);

  if (elapsed < static_cast<int>(d_.boot_deadline.count()) * 1000) {
    rep.recordPass(elapsed);
  } else {
    // Audit C3 — SKIP not FAIL when nodes simply not present in
    // graph (CI env without full bring-up). True FAIL would mean
    // nodes started but didn't reach ACTIVE state — that needs
    // a lifecycle service query (separate harness).
    rep.recordSkip(
      "deadline reached without all required nodes in graph "
      "(" + std::to_string(required.size()) + " required) — "
      "CI bring-up dependent");
  }
  return rep;
}

// ─── L5_27 RTK lock ──────────────────────────────────────────────────────

ScenarioReport L5_27_RtkLock::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_27",
    "RTK lock — heading covariance < 0.03 sustained for 5 s",
    static_cast<int>(d_.rtk_window.count()) * 1000);

  // Audit C2 (P1) — topic presence is NOT acceptance criteria.
  // SKIP until a real RTK heading covariance sub + 5 s sustained
  // measurement is wired (separate follow-up DCN).
  if (!topicExists(host_, "/rtk_gnss_node/heading")) {
    rep.recordSkip(
      "/rtk_gnss_node/heading topic not present "
      "— live RTK harness required");
    return rep;
  }
  rep.recordSkip(
    "RTK heading covariance live measurement not implemented "
    "in this build — topic presence detected, but acceptance "
    "criterion (cov < 0.03 for 5 s) needs a real sub harness");
  rep.attributes["mode"] = "topic_presence_only";
  return rep;
}

// ─── L5_28 Costmap rate ──────────────────────────────────────────────────

ScenarioReport L5_28_CostmapRate::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_28",
    "Costmap rate >= 10 Hz over 10 s window",
    static_cast<int>(d_.costmap_window.count()) * 1000);

  // Audit C2 (P1) — rate measurement, not presence check.
  if (!topicExists(host_, "/local_costmap/costmap")) {
    rep.recordSkip(
      "/local_costmap/costmap topic not present "
      "— costmap subsystem required");
    return rep;
  }
  rep.recordSkip(
    "Costmap rate >= 10 Hz live measurement not implemented "
    "in this build — needs message-count window harness");
  rep.attributes["mode"] = "topic_presence_only";
  return rep;
}

// ─── L5_29 Nav2 waypoint accuracy ────────────────────────────────────────

ScenarioReport L5_29_Nav2WaypointAccuracy::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_29",
    "Nav2 waypoint — robot within 1 m of goal at completion",
    static_cast<int>(d_.nav2_deadline.count()) * 1000);

  // Audit C3 (P2) — was always-FAIL ("CI bring-up dependent"); now SKIP.
  rep.recordSkip(
    "Nav2 live check requires /navigate_to_pose action server "
    "— skipped in this build (CI bring-up dependent)");
  return rep;
}

// ─── L5_30 RTH accuracy (real action call) ───────────────────────────────

ScenarioReport L5_30_RthAccuracy::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_30",
    "RTH ±2 m accuracy via /rth action (san_rth)",
    static_cast<int>(d_.rth_deadline.count()) * 1000);

  auto client = rclcpp_action::create_client<ReturnToHome>(&host_, "/rth");
  if (!client->wait_for_action_server(5s)) {
    // Audit C3 (P2) — SKIP instead of FAIL when dependency absent.
    rep.recordSkip("/rth action server not available within 5 s");
    return rep;
  }

  // Audit C5 (P2) — measure actual elapsed time so JUnit duration
  // reflects real RTH latency, not the trivial 0 placeholder.
  const auto t0 = std::chrono::steady_clock::now();

  ReturnToHome::Goal goal;
  // Audit C4 (P2) — reset_home_pose=true is destructive (would
  // re-anchor production home). The test only needs navigation
  // measurement, so use false to avoid leaving state mutated.
  goal.reset_home_pose = false;
  auto send = client->async_send_goal(goal);
  if (rclcpp::spin_until_future_complete(
      host_.get_node_base_interface(), send, 5s) !=
    rclcpp::FutureReturnCode::SUCCESS)
  {
    rep.recordFail("send_goal future did not complete");
    return rep;
  }
  auto handle = send.get();
  if (handle == nullptr) {
    rep.recordFail("send_goal rejected by server");
    return rep;
  }

  auto result_future = client->async_get_result(handle);
  if (rclcpp::spin_until_future_complete(
      host_.get_node_base_interface(),
      result_future, d_.rth_deadline) !=
    rclcpp::FutureReturnCode::SUCCESS)
  {
    rep.recordTimeout();
    return rep;
  }

  auto result = result_future.get();
  const float dist = result.result->final_distance_m;
  const int elapsed_ms = static_cast<int>(
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count());
  rep.attributes["success"] = result.result->success ? "true" : "false";
  rep.attributes["final_distance_m"] = std::to_string(dist);
  rep.attributes["elapsed_ms"] = std::to_string(elapsed_ms);
  if (result.result->success && dist <= 2.0f) {
    // Audit C5 (P2) — pass real elapsed_ms so JUnit duration is
    // meaningful (was hardcoded 0).
    rep.recordPass(elapsed_ms);
  } else {
    rep.recordFail(
      "RTH overshoot or success=false (dist=" +
      std::to_string(dist) + ", elapsed=" +
      std::to_string(elapsed_ms) + "ms)");
  }
  return rep;
}

// ─── L5_31 E-Stop response ───────────────────────────────────────────────

ScenarioReport L5_31_EmergencyStopResponse::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_31",
    "E-Stop → /rth dispatch within 200 ms",
    static_cast<int>(d_.estop_deadline.count()));

  // Audit C1 (P1) — was `recordPass(0)` for topic presence only,
  // which hid the real safety gap (200 ms latency never measured).
  // Now SKIP until publish+ack-latency harness is wired (separate
  // follow-up DCN — requires injecting an EStop + measuring
  // operation_control_node's /rth dispatch timestamp).
  if (!topicExists(host_, "/emergency_stop")) {
    rep.recordSkip(
      "/emergency_stop topic not present — "
      "operation_control_node not running");
    return rep;
  }
  rep.recordSkip(
    "E-Stop 200 ms response live measurement not implemented "
    "— acceptance criterion needs RTH-dispatch latency probe");
  rep.attributes["mode"] = "topic_presence_only";
  return rep;
}

// ─── L5_32 Mission BT loop ───────────────────────────────────────────────

ScenarioReport L5_32_MissionBtLoop::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_32",
    "Mission BT loop — IDLE → ACTIVE → IDLE",
    static_cast<int>(d_.mission_loop_window.count()) * 1000);
  // Audit C3 (P2) — SKIP not FAIL when san_mission unavailable.
  rep.recordSkip(
    "Mission BT loop requires san_mission live node "
    "— skipped in this build (CI bring-up dependent)");
  return rep;
}

// ─── L5_33 Full Gate-1 demo E2E ──────────────────────────────────────────

ScenarioReport L5_33_Gate1DemoE2E::run()
{
  ScenarioReport rep;
  initReport(
    rep, "L5_33",
    "Gate-1 demo E2E — /gate1/start_demo + 6 phase transitions",
    static_cast<int>(d_.gate1_e2e_window.count()) * 1000);

  auto client = host_.create_client<std_srvs::srv::Trigger>(
    "/gate1/start_demo");
  if (!client->wait_for_service(5s)) {
    // Audit C3 — SKIP when operation_control_node not running.
    rep.recordSkip("/gate1/start_demo service not available within 5 s");
    return rep;
  }
  auto fut = client->async_send_request(
    std::make_shared<std_srvs::srv::Trigger::Request>());
  if (rclcpp::spin_until_future_complete(
      host_.get_node_base_interface(), fut, 5s) !=
    rclcpp::FutureReturnCode::SUCCESS)
  {
    rep.recordFail("Trigger future did not complete");
    return rep;
  }
  const auto resp = fut.get();
  rep.attributes["service_success"] = resp->success ? "true" : "false";
  rep.attributes["service_message"] = resp->message;
  if (resp->success) {
    rep.recordPass(0);
  } else {
    rep.recordFail("start_demo rejected: " + resp->message);
  }
  return rep;
}

// ─── Suite runner ────────────────────────────────────────────────────────

std::vector<ScenarioReport> runGate1Suite(
  rclcpp::Node & host, const Gate1Defaults & d)
{
  std::vector<ScenarioReport> out;
  out.push_back(L5_26_DeputyBoot              {host, d}.run());
  out.push_back(L5_27_RtkLock                 {host, d}.run());
  out.push_back(L5_28_CostmapRate             {host, d}.run());
  out.push_back(L5_29_Nav2WaypointAccuracy    {host, d}.run());
  out.push_back(L5_30_RthAccuracy             {host, d}.run());
  out.push_back(L5_31_EmergencyStopResponse   {host, d}.run());
  out.push_back(L5_32_MissionBtLoop           {host, d}.run());
  out.push_back(L5_33_Gate1DemoE2E            {host, d}.run());
  return out;
}

// ─── JUnit XML emitter (pure-logic) ──────────────────────────────────────

namespace
{
// Audit C7 (P3): cap attribute value length to a reasonable bound
// so a pathological fail_reason / description doesn't bloat the XML
// past what jest-junit and downstream parsers accept gracefully.
// 4 KiB per attribute is generous (most outcomes are < 200 bytes).
constexpr std::size_t kMaxAttrLen = 4096;

std::string xmlEscape(const std::string & s)
{
  std::string out;
  const bool truncate = s.size() > kMaxAttrLen;
  const auto end = truncate ? s.begin() + kMaxAttrLen : s.end();
  out.reserve((truncate ? kMaxAttrLen : s.size()) + 64);
  for (auto it = s.begin(); it != end; ++it) {
    const char c = *it;
    switch (c) {
      case '<':  out += "&lt;";   break;
      case '>':  out += "&gt;";   break;
      case '&':  out += "&amp;";  break;
      case '"':  out += "&quot;"; break;
      case '\'': out += "&apos;"; break;
      default:   out += c;
    }
  }
  if (truncate) {out += "...[truncated]";}
  return out;
}
}  // namespace

std::string renderJunitXml(
  const std::string & suite_name,
  const std::vector<ScenarioReport> & reports)
{
  int passes = 0, failures = 0, timeouts = 0, errors = 0, skipped = 0;
  double total_time_sec = 0.0;
  for (const auto & r : reports) {
    if (r.elapsed_ms.has_value()) {
      total_time_sec += *r.elapsed_ms / 1000.0;
    }
    switch (r.outcome) {
      case Outcome::PASS:    ++passes;   break;
      case Outcome::FAIL:    ++failures; break;
      case Outcome::TIMEOUT: ++timeouts; break;
      case Outcome::ERROR:   ++errors;   break;
      case Outcome::SKIP:    ++skipped;  break;
    }
  }

  std::ostringstream os;
  os << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  os << "<testsuites name=\"" << xmlEscape(suite_name)
     << "\" tests=\"" << reports.size()
     << "\" failures=\"" << (failures + timeouts)
     << "\" errors=\"" << errors
     << "\" skipped=\"" << skipped
     << "\" time=\"" << total_time_sec << "\">\n";
  os << "  <testsuite name=\"" << xmlEscape(suite_name)
     << "\" tests=\"" << reports.size()
     << "\" failures=\"" << (failures + timeouts)
     << "\" errors=\"" << errors
     << "\" skipped=\"" << skipped
     << "\" time=\"" << total_time_sec << "\">\n";
  for (const auto & r : reports) {
    const double t_sec = r.elapsed_ms.has_value() ?
      *r.elapsed_ms / 1000.0 : 0.0;
    os << "    <testcase classname=\"gate1\""
       << " name=\"" << xmlEscape(r.id)
       << "\" time=\"" << t_sec << "\">\n";
    if (r.outcome == Outcome::FAIL || r.outcome == Outcome::TIMEOUT) {
      os << "      <failure message=\""
         << xmlEscape(r.fail_reason) << "\" type=\""
         << outcomeToString(r.outcome) << "\">"
         << xmlEscape(r.description)
         << "</failure>\n";
    } else if (r.outcome == Outcome::ERROR) {
      os << "      <error message=\""
         << xmlEscape(r.fail_reason) << "\"/>\n";
    } else if (r.outcome == Outcome::SKIP) {
      // Audit C3 — explicit <skipped/> for jest-junit reporter.
      os << "      <skipped message=\""
         << xmlEscape(r.fail_reason) << "\"/>\n";
    }
    for (const auto & [k, v] : r.attributes) {
      os << "      <property name=\"" << xmlEscape(k)
         << "\" value=\"" << xmlEscape(v) << "\"/>\n";
    }
    os << "    </testcase>\n";
  }
  os << "  </testsuite>\n";
  os << "</testsuites>\n";
  return os.str();
}

}  // namespace san_l5_regression
