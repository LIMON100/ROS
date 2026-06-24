// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 0 PR-B — StubSerial regression test.
//
// Pre-patch StubSerial::write returned `bytes.size()` ("pretend
// success"), which made RtkGnssNode::onRtcm increment
// rtcm_inject_count_ and report RTCM corrections flowing even though
// no receiver was present. The operations dashboard then showed
// "RTK healthy" forever.
//
// Post-patch StubSerial::write returns 0 so the node sees a short
// write and logs a WARN.

#include "san_rtk_gnss/serial_interface.hpp"

#include <gtest/gtest.h>

namespace san_rtk_gnss
{
namespace
{

TEST(StubSerial, WriteReturnsZeroNotByteCount) {
  auto serial = makeRealSerial();  // production code path: this is a stub
  ASSERT_NE(serial, nullptr);

  // open() is expected to fail on stub.
  EXPECT_FALSE(serial->open("/dev/null_test", 115200));
  EXPECT_FALSE(serial->isOpen());

  // write() must return 0, NOT bytes.size() — that was the bug.
  const std::vector<uint8_t> rtcm_payload(128, 0xCC);
  const size_t wrote = serial->write(rtcm_payload);
  EXPECT_EQ(wrote, 0u)
    << "StubSerial::write must surface real failure (return 0), "
    << "not pretend success — otherwise the dashboard falsely "
    << "reports RTCM flowing.";
}

}  // namespace
}  // namespace san_rtk_gnss
