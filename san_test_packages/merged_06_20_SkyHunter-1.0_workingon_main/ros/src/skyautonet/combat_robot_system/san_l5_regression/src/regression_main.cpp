// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.4 L5 regression - main entry.
//
// Spins a multi-threaded executor so the watcher subscriptions stay
// responsive while the scenario logic blocks on predicate waits.
// Writes JSON + Markdown reports to the path given by --report-dir
// (default /var/log/san/regression). Exit code 0 only when every
// scenario reports PASS.

#include <rclcpp/rclcpp.hpp>

#include <chrono>
#include <cstdio>
#include <fstream>
#include <string>
#include <thread>

#include "san_l5_regression/scenario_runner.hpp"
// [DCN-2026-020] MC stress scenario — dispatched via --scenario flag.
#include "san_l5_regression/scenarios/mc_stress.hpp"
// [DCN-2026-022] Gate-1 acceptance scenarios L5_26~33 + JUnit emitter.
#include "san_l5_regression/scenarios/gate1_scenarios.hpp"

namespace
{

std::string flagValue(
  int argc, char ** argv,
  const std::string & flag,
  const std::string & fallback)
{
  for (int i = 1; i + 1 < argc; ++i) {
    if (argv[i] == flag) {return argv[i + 1];}
  }
  return fallback;
}

void writeFile(const std::string & path, const std::string & contents)
{
  std::ofstream f(path, std::ios::trunc);
  if (!f.is_open()) {
    std::fprintf(
      stderr, "regression_main: failed to open %s\n",
      path.c_str());
    return;
  }
  f << contents;
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  const std::string report_dir =
    flagValue(argc, argv, "--report-dir", "/var/log/san/regression");

  // [DCN-2026-020] --scenario flag dispatch. Default = S18 suite
  // (legacy behavior). "mc_stress" runs the DCN-019 wire-protocol
  // stress test instead and exits with that scenario's outcome.
  const std::string scenario =
    flagValue(argc, argv, "--scenario", "s18");
  if (scenario == "mc_stress") {
    san_l5_regression::McStressConfig mc_cfg;
    mc_cfg.duration_sec =
      std::atoi(
      flagValue(
        argc, argv, "--duration",
        std::to_string(mc_cfg.duration_sec)).c_str());
    mc_cfg.rate_hz =
      std::atoi(
      flagValue(
        argc, argv, "--rate",
        std::to_string(mc_cfg.rate_hz)).c_str());
    mc_cfg.csv_path =
      flagValue(argc, argv, "--csv", mc_cfg.csv_path);
    // Audit B15 — optional --seed for reproducible debug runs.
    mc_cfg.rng_seed =
      static_cast<uint32_t>(
      std::atol(flagValue(argc, argv, "--seed", "0").c_str()));

    auto host = std::make_shared<rclcpp::Node>("mc_stress_host");
    san_l5_regression::McStressScenario sc(*host, mc_cfg);
    const auto rep = sc.run();

    std::printf("\n=== DCN-2026-020 MC stress ===\n");
    std::printf(
      "  outcome: %s\n",
      san_l5_regression::outcomeToString(rep.outcome));
    if (rep.elapsed_ms.has_value()) {
      std::printf("  elapsed_ms: %d\n", *rep.elapsed_ms);
    }
    std::printf("  detail: %s\n", rep.fail_reason.c_str());
    for (const auto & [k, v] : rep.attributes) {
      std::printf("  %s: %s\n", k.c_str(), v.c_str());
    }
    std::printf("  csv: %s\n", mc_cfg.csv_path.c_str());

    const bool passed =
      (rep.outcome == san_l5_regression::Outcome::PASS);
    rclcpp::shutdown();
    return passed ? 0 : 1;
  }

  // [DCN-2026-022] Gate-1 suite + per-scenario dispatch.
  auto runOne = [&](auto && sc, const std::string & tag) {
      const auto rep = sc.run();
      std::printf("\n=== %s (%s) ===\n", tag.c_str(), rep.id.c_str());
      std::printf(
        "  outcome: %s\n",
        san_l5_regression::outcomeToString(rep.outcome));
      std::printf("  detail: %s\n", rep.fail_reason.c_str());
      for (const auto & [k, v] : rep.attributes) {
        std::printf("  %s: %s\n", k.c_str(), v.c_str());
      }
      const bool passed =
        (rep.outcome == san_l5_regression::Outcome::PASS);
      rclcpp::shutdown();
      std::exit(passed ? 0 : 1);
    };

  if (scenario.rfind("L5_", 0) == 0) {
    auto host = std::make_shared<rclcpp::Node>("gate1_single_host");
    san_l5_regression::Gate1Defaults d;
    if (scenario == "L5_26") {
      runOne(san_l5_regression::L5_26_DeputyBoot{*host, d}, "L5_26");
    } else if (scenario == "L5_27") {
      runOne(san_l5_regression::L5_27_RtkLock{*host, d}, "L5_27");
    } else if (scenario == "L5_28") {
      runOne(san_l5_regression::L5_28_CostmapRate{*host, d}, "L5_28");
    } else if (scenario == "L5_29") {
      runOne(san_l5_regression::L5_29_Nav2WaypointAccuracy{*host, d}, "L5_29");
    } else if (scenario == "L5_30") {
      runOne(san_l5_regression::L5_30_RthAccuracy{*host, d}, "L5_30");
    } else if (scenario == "L5_31") {
      runOne(san_l5_regression::L5_31_EmergencyStopResponse{*host, d}, "L5_31");
    } else if (scenario == "L5_32") {
      runOne(san_l5_regression::L5_32_MissionBtLoop{*host, d}, "L5_32");
    } else if (scenario == "L5_33") {
      runOne(san_l5_regression::L5_33_Gate1DemoE2E{*host, d}, "L5_33");
    } else {
      std::fprintf(stderr, "unknown L5 scenario: %s\n", scenario.c_str());
      rclcpp::shutdown();
      return 2;
    }
  }

  if (scenario == "gate1_suite") {
    const std::string xml_path =
      flagValue(
      argc, argv, "--junit-xml",
      "/tmp/gate1_results.xml");
    auto host = std::make_shared<rclcpp::Node>("gate1_suite_host");
    san_l5_regression::Gate1Defaults d;
    const auto reports = san_l5_regression::runGate1Suite(*host, d);

    writeFile(
      xml_path,
      san_l5_regression::renderJunitXml("Gate-1", reports));

    int passes = 0, skipped = 0, failures = 0;
    std::printf("\n=== Gate-1 suite ===\n");
    for (const auto & r : reports) {
      const char * oc = san_l5_regression::outcomeToString(r.outcome);
      std::printf(
        "  %-7s %-7s %s\n",
        r.id.c_str(), oc, r.fail_reason.c_str());
      switch (r.outcome) {
        case san_l5_regression::Outcome::PASS:  ++passes;   break;
        case san_l5_regression::Outcome::SKIP:  ++skipped;  break;
        default:                                 ++failures; break;
      }
    }
    std::printf(
      "\nPassed: %d  Skipped: %d  Failed: %d  /  %zu  →  %s\n",
      passes, skipped, failures, reports.size(), xml_path.c_str());

    // Audit C3 — SKIP is not failure for CI exit code. Suite is
    // "green" if no FAIL/TIMEOUT/ERROR; SKIP is acceptable when
    // a live dependency is bring-up dependent.
    rclcpp::shutdown();
    return (failures == 0) ? 0 : 1;
  }

  san_l5_regression::RunnerConfig cfg;
  auto runner = std::make_shared<san_l5_regression::ScenarioRunner>(cfg);

  rclcpp::executors::MultiThreadedExecutor exec;
  exec.add_node(runner);
  std::thread spin_thread([&] {exec.spin();});

  // Allow subscriptions to discover publishers before we start.
  std::this_thread::sleep_for(std::chrono::seconds(1));

  auto writer = runner->runAll();

  exec.cancel();
  if (spin_thread.joinable()) {spin_thread.join();}

  const std::string json_path = report_dir + "/s18_report.json";
  const std::string md_path = report_dir + "/s18_report.md";
  writeFile(json_path, writer.renderJson());
  writeFile(md_path, writer.renderMarkdown());

  std::printf("\n%s\n", writer.renderMarkdown().c_str());
  std::printf(
    "\nReports written to:\n  %s\n  %s\n",
    json_path.c_str(), md_path.c_str());

  const bool all_passed = writer.allPassed();
  rclcpp::shutdown();
  return all_passed ? 0 : 1;
}
