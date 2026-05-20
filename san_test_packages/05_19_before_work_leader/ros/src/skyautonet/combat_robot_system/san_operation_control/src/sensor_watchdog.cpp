#include "san_operation_control/sensor_watchdog.hpp"

#include <vector>

namespace san_operation_control {

SensorWatchdog::SensorWatchdog() = default;

void SensorWatchdog::configure(DeploymentMode mode, bool yaml_enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (watchdogIsForceEnabled(mode)) {
        // Yaml override IGNORED in non-development modes.
        enabled_ = true;
    } else {
        enabled_ = yaml_enabled;
    }
}

void SensorWatchdog::update(const std::string& sensor_name,
                            uint64_t now_ms)
{
    std::lock_guard<std::mutex> lock(mutex_);
    last_ms_[sensor_name] = now_ms;
}

bool SensorWatchdog::checkSensorState(uint64_t now_ms) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!enabled_) return true;
    const uint64_t threshold_ms =
        static_cast<uint64_t>(stale_threshold_sec_ * 1000.0);
    for (const auto& [name, last] : last_ms_) {
        // An unseen sensor (last == 0) is treated as fresh until its
        // first update lands - otherwise the watchdog would trip
        // before any data ever arrived.
        if (last == 0) continue;
        if (now_ms < last) continue;
        if ((now_ms - last) > threshold_ms) return false;
    }
    return true;
}

std::vector<std::string> SensorWatchdog::staleSensors(uint64_t now_ms) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::string> stale;
    if (!enabled_) return stale;
    const uint64_t threshold_ms =
        static_cast<uint64_t>(stale_threshold_sec_ * 1000.0);
    for (const auto& [name, last] : last_ms_) {
        if (last == 0) continue;
        if (now_ms < last) continue;
        if ((now_ms - last) > threshold_ms) stale.push_back(name);
    }
    return stale;
}

}  // namespace san_operation_control
