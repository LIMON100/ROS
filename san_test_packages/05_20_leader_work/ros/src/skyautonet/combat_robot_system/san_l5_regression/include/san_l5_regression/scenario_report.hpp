// SAN v1.4 L5 regression - per-scenario timing + outcome report.
//
// Each scenario produces one ScenarioReport with: id (e.g. "S18-1"),
// description, deadline, elapsed_ms (or null if timeout), pass/fail,
// and any captured fields the runner wants to attach (e.g. promoted
// robot_id, succession_priority).

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace san_l5_regression {

enum class Outcome : uint8_t {
    PASS    = 0,
    FAIL    = 1,
    TIMEOUT = 2,
    ERROR   = 3,
};

const char* outcomeToString(Outcome o);

struct ScenarioReport {
    std::string id;                       // "S18-1"
    std::string description;
    int deadline_ms = 0;
    std::optional<int> elapsed_ms;        // null = timeout
    Outcome outcome = Outcome::ERROR;
    std::string fail_reason;
    std::map<std::string, std::string> attributes;   // free-form key/value

    void recordPass(int elapsed) {
        outcome = Outcome::PASS;
        elapsed_ms = elapsed;
        fail_reason.clear();
    }
    void recordTimeout() {
        outcome = Outcome::TIMEOUT;
        elapsed_ms.reset();
        if (fail_reason.empty()) fail_reason = "deadline exceeded";
    }
    void recordFail(const std::string& reason) {
        outcome = Outcome::FAIL;
        fail_reason = reason;
    }
};

class ScenarioReportWriter {
public:
    void add(const ScenarioReport& r) { reports_.push_back(r); }
    const std::vector<ScenarioReport>& reports() const { return reports_; }

    // Aggregate counts. Useful for the regression_main exit code.
    int countPass() const;
    int countFail() const;
    int countTimeout() const;
    bool allPassed() const;

    std::string renderJson() const;
    std::string renderMarkdown() const;

private:
    std::vector<ScenarioReport> reports_;
};

}  // namespace san_l5_regression
