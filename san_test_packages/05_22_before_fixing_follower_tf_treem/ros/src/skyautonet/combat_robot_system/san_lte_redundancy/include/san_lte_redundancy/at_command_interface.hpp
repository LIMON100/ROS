// SAN v1.5 Phase 2-E Turn 3 — AT command interface abstraction.
//
// Same SDK abstraction pattern as Turn 2 (Go2SdkInterface):
//   * Production: RealAtCommand opens /dev/ttyUSBn, writes AT, reads lines
//   * Build-time fallback: StubAtCommand (always returns ERROR)
//   * Test: MockAtCommand (in-memory canned responses)
//
// Per DCN-2026-002 D-008, this is a Tier 1 C++ HW driver (replaces
// adapters/lte_modem.py).
//
// 권원:
//   * SDD-SWARM v1.5 §3.1.4 (LTE 모뎀 HW)
//   * ADR-006 §5.1 패턴 1 (Adapter → Node)

#ifndef SAN_LTE_REDUNDANCY__AT_COMMAND_INTERFACE_HPP_
#define SAN_LTE_REDUNDANCY__AT_COMMAND_INTERFACE_HPP_

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace san_lte_redundancy {

/// Abstract serial AT-command interface. The implementation is
/// responsible for line framing (\r\n terminated) and timeout.
class AtCommandInterface {
public:
  virtual ~AtCommandInterface() = default;

  /// Open the modem control port. Returns false on any failure
  /// (file not found, ioctl error, permission). Idempotent across
  /// repeated calls with the same arguments.
  virtual bool open(const std::string& device, int baud) = 0;

  /// Close the port. Always succeeds.
  virtual void close() = 0;

  /// Send `cmd` (no trailing CRLF — implementation appends), read
  /// response lines until "OK" / "ERROR" / "+CME ERROR:..." or
  /// `timeout`. Each returned string is one stripped line (no
  /// trailing CRLF, possibly empty lines suppressed).
  virtual std::vector<std::string> send(
      const std::string& cmd,
      std::chrono::milliseconds timeout = std::chrono::milliseconds(500)) = 0;

  /// True iff the underlying port is currently open.
  virtual bool isOpen() const = 0;
};

/// Factory — returns the real serial-backed implementation. CMake
/// links this to either real_at_command.cpp (production) or
/// stub_at_command.cpp (no serial deps for CI/dev).
std::unique_ptr<AtCommandInterface> makeRealAtCommand();

}  // namespace san_lte_redundancy

#endif  // SAN_LTE_REDUNDANCY__AT_COMMAND_INTERFACE_HPP_
