// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 Phase 2-E Turn 6 — Frame metadata implementation.

#include "san_cameras/frame_metadata.hpp"

namespace san_cameras
{

std::optional<size_t> bytesPerPixel(const std::string & encoding)
{
  // Raw encodings — bytes per pixel known
  if (encoding == "mono8") {return 1;}
  if (encoding == "mono16") {return 2;}
  if (encoding == "rgb8") {return 3;}
  if (encoding == "bgr8") {return 3;}
  if (encoding == "rgba8") {return 4;}
  if (encoding == "bgra8") {return 4;}
  if (encoding == "yuv422") {return 2;}
  if (encoding == "yuyv") {return 2;}
  if (encoding == "bayer_rggb8") {return 1;}
  if (encoding == "bayer_grbg8") {return 1;}
  if (encoding == "bayer_gbrg8") {return 1;}
  if (encoding == "bayer_bggr8") {return 1;}
  // 16-bit Bayer (high-bit-depth raw)
  if (encoding == "bayer_rggb16") {return 2;}
  if (encoding == "bayer_grbg16") {return 2;}
  // Compressed — variable rate, no fixed bpp
  if (encoding == "h264" || encoding == "h265" ||
    encoding == "hevc" || encoding == "mjpeg" ||
    encoding == "jpeg")
  {
    return 0;
  }
  return std::nullopt;
}

bool isCompressed(const std::string & encoding)
{
  return encoding == "h264" || encoding == "h265" ||
         encoding == "hevc" || encoding == "mjpeg" ||
         encoding == "jpeg";
}

std::optional<size_t> computeRowStep(
  const std::string & encoding,
  uint32_t width)
{
  auto bpp = bytesPerPixel(encoding);
  if (!bpp) {return std::nullopt;}
  if (*bpp == 0) {
    return 0;                      // compressed
  }
  return *bpp * width;
}

bool isPlausibleBuffer(
  const std::string & encoding,
  uint32_t width,
  uint32_t height,
  size_t buffer_size)
{
  auto bpp = bytesPerPixel(encoding);
  if (!bpp) {
    return false;                // unknown encoding
  }
  if (*bpp == 0) {
    // Compressed: require at least a few NAL-unit-sized bytes;
    // H.264/H.265 frames are typically > 100 bytes even for I-frames.
    // Reject obviously bogus 0-byte or sub-header payloads.
    return buffer_size >= 8;
  }
  const size_t expected = static_cast<size_t>(*bpp) * width * height;
  return buffer_size == expected;
}

}  // namespace san_cameras
