// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.3 PHASE 2 v2 - mwan3 UCI configuration via libuci C API.
//
// Replaces the v1 bash `uci set ... && uci commit && /etc/init.d/mwan3 reload`
// pattern. All UCI mutations go through this class; no shell calls.
//
// Thread-safety: every public method takes uci_mutex_, so a single
// Mwan3UciController instance is safe to share across nodes (e.g. the
// init node and the role manager both write to mwan3.wan_lte.weight).
//
// References:
//   * OpenWrt libuci API: https://openwrt.org/docs/techref/uci
//   * SAN-SDD-SWARM-001_v1.3 §5.5

#pragma once

#include <rclcpp/logger.hpp>
#include <mutex>
#include <string>

// Forward-declare uci_context (defined in <uci.h>) so this header
// compiles on non-OpenWRT environments where libuci-dev is absent.
// The real definition lands in src/mwan3_uci_controller.cpp which
// only builds when libuci/libubox/libubus are detected by pkg-config.
// On a stub build the same symbols are provided by src/stub_mwan3_uci_controller.cpp
// with no-op bodies so downstream linkers stay happy.
extern "C" {
struct uci_context;
}

namespace san_lte_redundancy
{

class Mwan3UciController
{
public:
  explicit Mwan3UciController(rclcpp::Logger logger);
  ~Mwan3UciController();

  Mwan3UciController(const Mwan3UciController &) = delete;
  Mwan3UciController & operator=(const Mwan3UciController &) = delete;

  // Convenience: set mwan3.wan_lte.weight and commit in one call.
  // Returns true on success, false on any UCI error (caller logs).
  bool setLteWeight(int weight);

  // Generic UCI mutators. `key` uses dotted path notation, e.g.
  // "mwan3.wan_lte.weight". For createSection, pass "mwan3.wan_lte"
  // as section_path and "interface" as type.
  bool setOption(const std::string & key, const std::string & value);
  bool createSection(
    const std::string & section_path,
    const std::string & type);
  bool deleteSection(const std::string & section_path);

  // Commit pending changes to /etc/config/<package>. Caller must
  // commit after a batch of setOption/createSection calls.
  bool commit(const std::string & package = "mwan3");

  // Override the UCI confdir (defaults to /etc/config). Used by
  // unit tests to redirect writes into a temporary directory.
  void setConfDir(const std::string & confdir);
  void setSaveDir(const std::string & savedir);

private:
  rclcpp::Logger logger_;
  struct uci_context * uci_ctx_;
  std::mutex uci_mutex_;
};

}  // namespace san_lte_redundancy
