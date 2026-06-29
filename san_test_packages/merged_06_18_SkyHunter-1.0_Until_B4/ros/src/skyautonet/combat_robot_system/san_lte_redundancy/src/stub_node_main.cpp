// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 — No-op ROS node entry for stub-mode executables.
//
// When san_lte_redundancy builds in STUB mode (libuci/libubox/libubus
// not present, see CMakeLists.txt), the production executables
// (lte_node, lte_link_quality_node, lte_modem_node) are replaced with
// this single stub that:
//   1. Creates a ROS node named via argv[0]'s basename,
//   2. Spins on a single-threaded executor until shutdown,
//   3. Logs once at startup so the operator/CI can see it's a stub.
//
// The squadron launch file references each executable by name; on CI
// (and any non-OpenWRT host) we still need a real binary at the
// expected install path so launch_ros can spawn it. Without this
// stub, launch tests TST S20-1 / S20-2 fail with
//   "libexec directory '.../lib/san_lte_redundancy' does not exist".
// The runtime behaviour mirrors stub_mwan3_uci_controller.cpp — calls
// noop, link quality stays unknown, role manager never promotes.

#include <filesystem>
#include <string>

#include <rclcpp/rclcpp.hpp>

namespace
{

std::string nodeNameFromArg0(const char * arg0)
{
  if (arg0 == nullptr) {return "san_lte_stub_node";}
  std::filesystem::path p(arg0);
  auto stem = p.stem().string();
  return stem.empty() ? std::string("san_lte_stub_node") : stem;
}

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  const std::string name =
    nodeNameFromArg0(argc > 0 ? argv[0] : nullptr);
  auto node = std::make_shared<rclcpp::Node>(name);
  // Match the "<name> UP" log signature so launch-test scenarios
  // that wait on stdout (test_s20_1 CRITICAL_NODES) recognise the
  // stub as a live node. Production nodes use the same prefix.
  RCLCPP_INFO(
    node->get_logger(),
    "%s UP: backend=STUB reason=openwrt_deps_absent "
    "(install libuci/libubox/libubus for production behaviour)",
    name.c_str());
  rclcpp::spin(node);
  rclcpp::shutdown();
  return 0;
}
