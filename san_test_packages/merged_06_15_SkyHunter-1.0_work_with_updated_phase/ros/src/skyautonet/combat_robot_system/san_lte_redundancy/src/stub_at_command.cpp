// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 3 — Stub AT command implementation.
//
// Used when no real serial port is available (CI / dev laptop). The
// node's stub_on_no_modem parameter (default true) then activates the
// STUB publish mode which emits canned LteModemStatus values for
// downstream debugging.
//
// A real implementation (real_at_command.cpp) using termios + non-blocking
// reads will be added when production deployment requires it. The
// abstraction is in place so swapping is a one-file change.

#include "san_lte_redundancy/at_command_interface.hpp"

#include <iostream>

namespace san_lte_redundancy
{

namespace
{

class StubAtCommand : public AtCommandInterface
{
public:
  bool open(const std::string & device, int baud) override
  {
    std::cerr << "[san_lte_redundancy][STUB-AT] open(\"" << device
              << "\", " << baud
              << ") — real serial backend not linked. Node will run "
              << "in STUB publish mode.\n";
    return false;     // signal "no modem" to the node
  }

  void close() override {}

  std::vector<std::string> send(
    const std::string & /*cmd*/,
    std::chrono::milliseconds /*timeout*/) override
  {
    // Always return ERROR; the node should be in STUB publish mode
    // anyway since open() returned false.
    return {"ERROR"};
  }

  bool isOpen() const override {return false;}
};

}  // namespace

std::unique_ptr<AtCommandInterface> makeRealAtCommand()
{
  // When real_at_command.cpp lands (production), this returns a
  // termios-backed implementation. For now (stub-only build), return
  // the stub so the build succeeds.
  return std::make_unique<StubAtCommand>();
}

}  // namespace san_lte_redundancy
