#include "san_operation_control/deployment_mode.hpp"

#include <algorithm>
#include <cctype>

namespace san_operation_control {

namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return s;
}

}  // namespace

DeploymentMode fromString(const std::string& s) {
    const std::string n = toLower(s);
    if (n == "development" || n == "dev")       return DeploymentMode::DEVELOPMENT;
    if (n == "bench")                           return DeploymentMode::BENCH;
    if (n == "lab_test" || n == "lab")          return DeploymentMode::LAB_TEST;
    if (n == "demo")                            return DeploymentMode::DEMO;
    if (n == "production" || n == "prod")       return DeploymentMode::PRODUCTION;
    // Conservative default: PRODUCTION (watchdog forced on, no DEMO).
    return DeploymentMode::PRODUCTION;
}

std::string toString(DeploymentMode m) {
    switch (m) {
        case DeploymentMode::DEVELOPMENT: return "development";
        case DeploymentMode::BENCH:       return "bench";
        case DeploymentMode::LAB_TEST:    return "lab_test";
        case DeploymentMode::DEMO:        return "demo";
        case DeploymentMode::PRODUCTION:  return "production";
    }
    return "production";
}

bool demoSequencerEnabled(DeploymentMode m) {
    return m == DeploymentMode::DEMO || m == DeploymentMode::LAB_TEST;
}

bool watchdogIsForceEnabled(DeploymentMode m) {
    return m != DeploymentMode::DEVELOPMENT;
}

}  // namespace san_operation_control
