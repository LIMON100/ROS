// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — Frame metadata tests.
//
// Pure-logic standalone gtest. Covers the encoding ↔ layout
// invariants both camera nodes rely on.
//
// Coverage:
//   F1  bytesPerPixel for raw encodings
//   F2  bytesPerPixel returns 0 for compressed
//   F3  bytesPerPixel returns nullopt for unknown
//   F4  isCompressed truth table
//   F5  computeRowStep for raw + compressed + unknown
//   F6  isPlausibleBuffer for raw exact-match
//   F7  isPlausibleBuffer rejects raw size mismatch
//   F8  isPlausibleBuffer accepts compressed with >=8 bytes
//   F9  isPlausibleBuffer rejects compressed 0-byte payload
//   F10 IMX678 4K H.265 typical case
//   F11 Thermal 640×512 mono16 typical case

#include "san_cameras/frame_metadata.hpp"

#include <gtest/gtest.h>

namespace san_cameras
{
namespace
{

TEST(FrameMetadata, F1_BytesPerPixelRawEncodings) {
  EXPECT_EQ(bytesPerPixel("mono8"), std::optional<size_t>(1));
  EXPECT_EQ(bytesPerPixel("mono16"), std::optional<size_t>(2));
  EXPECT_EQ(bytesPerPixel("rgb8"), std::optional<size_t>(3));
  EXPECT_EQ(bytesPerPixel("bgr8"), std::optional<size_t>(3));
  EXPECT_EQ(bytesPerPixel("rgba8"), std::optional<size_t>(4));
  EXPECT_EQ(bytesPerPixel("yuv422"), std::optional<size_t>(2));
  EXPECT_EQ(bytesPerPixel("bayer_rggb8"), std::optional<size_t>(1));
  EXPECT_EQ(bytesPerPixel("bayer_rggb16"), std::optional<size_t>(2));
}

TEST(FrameMetadata, F2_BytesPerPixelCompressedZero) {
  EXPECT_EQ(bytesPerPixel("h264"), std::optional<size_t>(0));
  EXPECT_EQ(bytesPerPixel("h265"), std::optional<size_t>(0));
  EXPECT_EQ(bytesPerPixel("hevc"), std::optional<size_t>(0));
  EXPECT_EQ(bytesPerPixel("mjpeg"), std::optional<size_t>(0));
  EXPECT_EQ(bytesPerPixel("jpeg"), std::optional<size_t>(0));
}

TEST(FrameMetadata, F3_BytesPerPixelUnknownNullopt) {
  EXPECT_FALSE(bytesPerPixel("").has_value());
  EXPECT_FALSE(bytesPerPixel("rgb32").has_value());
  EXPECT_FALSE(bytesPerPixel("garbage").has_value());
}

TEST(FrameMetadata, F4_IsCompressedTruthTable) {
  EXPECT_TRUE(isCompressed("h264"));
  EXPECT_TRUE(isCompressed("h265"));
  EXPECT_TRUE(isCompressed("hevc"));
  EXPECT_TRUE(isCompressed("mjpeg"));
  EXPECT_TRUE(isCompressed("jpeg"));
  EXPECT_FALSE(isCompressed("rgb8"));
  EXPECT_FALSE(isCompressed("mono16"));
  EXPECT_FALSE(isCompressed("yuv422"));
  EXPECT_FALSE(isCompressed(""));
}

TEST(FrameMetadata, F5_ComputeRowStep) {
  // raw
  EXPECT_EQ(computeRowStep("rgb8", 1920), std::optional<size_t>(5760));
  EXPECT_EQ(computeRowStep("mono16", 640), std::optional<size_t>(1280));
  // compressed → 0
  EXPECT_EQ(computeRowStep("h265", 3840), std::optional<size_t>(0));
  // unknown
  EXPECT_FALSE(computeRowStep("garbage", 100).has_value());
}

TEST(FrameMetadata, F6_PlausibleBufferRawExactMatch) {
  // 100 × 50 × rgb8 = 15000 bytes
  EXPECT_TRUE(isPlausibleBuffer("rgb8", 100, 50, 15000));
  // 640 × 512 × mono16 = 655360 bytes
  EXPECT_TRUE(isPlausibleBuffer("mono16", 640, 512, 655360));
}

TEST(FrameMetadata, F7_PlausibleBufferRawSizeMismatch) {
  EXPECT_FALSE(isPlausibleBuffer("rgb8", 100, 50, 14999));
  EXPECT_FALSE(isPlausibleBuffer("rgb8", 100, 50, 0));
  EXPECT_FALSE(isPlausibleBuffer("mono16", 640, 512, 1000));
}

TEST(FrameMetadata, F8_PlausibleBufferCompressedMinSize) {
  // H.265 frame — any size >= 8 bytes is plausible (just NAL header
  // sanity check)
  EXPECT_TRUE(isPlausibleBuffer("h265", 3840, 2160, 8));
  EXPECT_TRUE(isPlausibleBuffer("h265", 3840, 2160, 100000));
  EXPECT_TRUE(isPlausibleBuffer("mjpeg", 1920, 1080, 50000));
}

TEST(FrameMetadata, F9_PlausibleBufferCompressedTooSmall) {
  EXPECT_FALSE(isPlausibleBuffer("h265", 3840, 2160, 0));
  EXPECT_FALSE(isPlausibleBuffer("h265", 3840, 2160, 7));
  EXPECT_FALSE(isPlausibleBuffer("jpeg", 100, 100, 0));
}

TEST(FrameMetadata, F10_Imx678_4K_H265) {
  // 3840 × 2160 H.265 — compressed, any size >= 8 OK
  const uint32_t W = 3840, H = 2160;
  EXPECT_EQ(bytesPerPixel("h265"), std::optional<size_t>(0));
  EXPECT_TRUE(isCompressed("h265"));
  EXPECT_EQ(computeRowStep("h265", W), std::optional<size_t>(0));
  EXPECT_TRUE(isPlausibleBuffer("h265", W, H, 250'000));
}

TEST(FrameMetadata, F11_Thermal_640x512_Mono16) {
  // 640 × 512 mono16 = 655360 bytes exact
  const uint32_t W = 640, H = 512;
  EXPECT_EQ(bytesPerPixel("mono16"), std::optional<size_t>(2));
  EXPECT_FALSE(isCompressed("mono16"));
  EXPECT_EQ(computeRowStep("mono16", W), std::optional<size_t>(1280));
  EXPECT_TRUE(isPlausibleBuffer("mono16", W, H, 655360));
  EXPECT_FALSE(isPlausibleBuffer("mono16", W, H, 655359));
}

// ─── F12: Degenerate raw frame — width × 0 × bpp = 0 bytes ──────────
//
// A camera initialising might send a "no-frame" announcement with
// height=0. The plausibility check must accept it (0 bytes is the
// arithmetically correct expected size), not reject it as compressed-
// too-small. This guards against the off-by-one bug where buf.size()=0
// would otherwise hit the compressed min-size branch.
TEST(FrameMetadata, F12_DegenerateZeroHeightRawIsValid) {
  // For raw encodings, zero height is a degenerate but mathematically
  // valid frame — expected size is 0.
  EXPECT_TRUE(isPlausibleBuffer("rgb8", 1920, 0, 0));
  EXPECT_TRUE(isPlausibleBuffer("mono16", 640, 0, 0));
  // But buf with extra bytes for a 0-height frame is NOT plausible
  // (would indicate the producer ignored the height field).
  EXPECT_FALSE(isPlausibleBuffer("rgb8", 1920, 0, 1));
}

// ─── F13: Large compressed frame — 1 MB H.265 4K I-frame ─────────────
//
// 4K H.265 I-frames at high bitrate (e.g. CQP 18, 4K-60) can hit
// ~1 MB. The plausibility check must accept arbitrarily large
// compressed payloads (only the >=8 lower bound matters; no upper).
TEST(FrameMetadata, F13_LargeCompressedFrameNearMb) {
  const size_t one_mb = 1024 * 1024;
  EXPECT_TRUE(isPlausibleBuffer("h265", 3840, 2160, one_mb));
  EXPECT_TRUE(isPlausibleBuffer("h265", 3840, 2160, one_mb + 1));
  EXPECT_TRUE(isPlausibleBuffer("h264", 3840, 2160, 10 * one_mb));
  EXPECT_TRUE(isPlausibleBuffer("mjpeg", 1920, 1080, one_mb));
}

}  // namespace
}  // namespace san_cameras
