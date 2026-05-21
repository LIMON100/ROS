// SAN v1.5 — Stub Mwan3UciController for non-OpenWRT build environments.
//
// On Ubuntu CI / dev boxes (no libuci/libubox/libubus), san_lte_redundancy
// goes into stub mode: no real UCI mutations, but downstream packages
// (san_role_management) still link against this library so their build +
// install stay clean. Methods log once and return false to make stub
// usage visible without spamming.

#include "san_lte_redundancy/mwan3_uci_controller.hpp"

#include <rclcpp/logging.hpp>

namespace san_lte_redundancy {

Mwan3UciController::Mwan3UciController(rclcpp::Logger logger)
    : logger_(logger), uci_ctx_(nullptr)
{
    RCLCPP_WARN(logger_,
        "Mwan3UciController: STUB build — libuci/libubox/libubus not "
        "found at CMake configure time. All UCI calls will return false. "
        "Install the OpenWRT cross-toolchain for full functionality.");
}

Mwan3UciController::~Mwan3UciController() = default;

bool Mwan3UciController::setLteWeight(int /*weight*/) {
    return false;
}

bool Mwan3UciController::setOption(const std::string& /*key*/,
                                    const std::string& /*value*/) {
    return false;
}

bool Mwan3UciController::createSection(const std::string& /*section_path*/,
                                        const std::string& /*type*/) {
    return false;
}

bool Mwan3UciController::deleteSection(const std::string& /*section_path*/) {
    return false;
}

bool Mwan3UciController::commit(const std::string& /*package*/) {
    return false;
}

void Mwan3UciController::setConfDir(const std::string& /*confdir*/) {}
void Mwan3UciController::setSaveDir(const std::string& /*savedir*/) {}

}  // namespace san_lte_redundancy
