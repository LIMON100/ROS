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

namespace {

std::string flagValue(int argc, char** argv,
                      const std::string& flag,
                      const std::string& fallback)
{
    for (int i = 1; i + 1 < argc; ++i) {
        if (argv[i] == flag) return argv[i + 1];
    }
    return fallback;
}

void writeFile(const std::string& path, const std::string& contents) {
    std::ofstream f(path, std::ios::trunc);
    if (!f.is_open()) {
        std::fprintf(stderr, "regression_main: failed to open %s\n",
                     path.c_str());
        return;
    }
    f << contents;
}

}  // namespace

int main(int argc, char** argv) {
    rclcpp::init(argc, argv);

    const std::string report_dir =
        flagValue(argc, argv, "--report-dir", "/var/log/san/regression");

    san_l5_regression::RunnerConfig cfg;
    auto runner = std::make_shared<san_l5_regression::ScenarioRunner>(cfg);

    rclcpp::executors::MultiThreadedExecutor exec;
    exec.add_node(runner);
    std::thread spin_thread([&] { exec.spin(); });

    // Allow subscriptions to discover publishers before we start.
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto writer = runner->runAll();

    exec.cancel();
    if (spin_thread.joinable()) spin_thread.join();

    const std::string json_path = report_dir + "/s18_report.json";
    const std::string md_path   = report_dir + "/s18_report.md";
    writeFile(json_path, writer.renderJson());
    writeFile(md_path,   writer.renderMarkdown());

    std::printf("\n%s\n", writer.renderMarkdown().c_str());
    std::printf("\nReports written to:\n  %s\n  %s\n",
                json_path.c_str(), md_path.c_str());

    const bool all_passed = writer.allPassed();
    rclcpp::shutdown();
    return all_passed ? 0 : 1;
}
