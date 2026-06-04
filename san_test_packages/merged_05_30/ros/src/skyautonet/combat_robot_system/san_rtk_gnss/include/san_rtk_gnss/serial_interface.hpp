// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 4 — Serial port abstraction.
//
// Used by RtkGnssNode to talk to u-blox F9P (or any NMEA receiver).
// Same SDK abstraction pattern as Turn 2 (Go2SdkInterface) and
// Turn 3 (AtCommandInterface).
//
// Bi-directional:
//   - readLine() — pull next NMEA sentence
//   - write()    — push raw RTCM3 bytes (corrections inject)
// Both must be thread-safe (read on one thread, write on another).

#ifndef SAN_RTK_GNSS__SERIAL_INTERFACE_HPP_
#define SAN_RTK_GNSS__SERIAL_INTERFACE_HPP_

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace san_rtk_gnss
{

class SerialInterface
{
public:
  virtual ~SerialInterface() = default;

  /// Open the serial device. Returns false on any failure.
  virtual bool open(const std::string & device, int baud) = 0;

  /// Close the port.
  virtual void close() = 0;

  /// Read one complete line (terminated by '\n'). Returns empty
  /// string on timeout. Strips the trailing '\r\n'.
  /// Thread-safe with respect to write().
  virtual std::string readLine(std::chrono::milliseconds timeout) = 0;

  /// Write raw bytes (for RTCM injection). Returns the number of
  /// bytes written. Thread-safe with respect to readLine().
  virtual size_t write(const std::vector<uint8_t> & bytes) = 0;

  virtual bool isOpen() const = 0;
};

std::unique_ptr<SerialInterface> makeRealSerial();

}  // namespace san_rtk_gnss

#endif  // SAN_RTK_GNSS__SERIAL_INTERFACE_HPP_
