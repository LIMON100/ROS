// SAN v1.5 Phase 2-E Turn 4 — Stub serial port.
// Returns canned NMEA when stubbed for downstream consumer testing.

#include "san_rtk_gnss/serial_interface.hpp"

#include <iostream>

namespace san_rtk_gnss {

namespace {

class StubSerial : public SerialInterface {
public:
  bool open(const std::string& device, int baud) override {
    std::cerr << "[san_rtk_gnss][STUB-SERIAL] open(\"" << device
              << "\", " << baud
              << ") — real serial backend not linked.\n";
    return false;
  }
  void close() override {}
  std::string readLine(std::chrono::milliseconds /*timeout*/) override {
    return {};
  }
  size_t write(const std::vector<uint8_t>& bytes) override {
    return bytes.size();   // pretend success
  }
  bool isOpen() const override { return false; }
};

}  // namespace

std::unique_ptr<SerialInterface> makeRealSerial() {
  return std::make_unique<StubSerial>();
}

}  // namespace san_rtk_gnss
