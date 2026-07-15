// SAN v1.5 Phase 2-E Turn 6 — Frame metadata utility.
//
// Pure-logic helpers shared by IMX678 and Thermal camera nodes:
//   * Map encoding string → bytes per pixel (or 0 for compressed)
//   * Compute row step (bytes per row)
//   * Validate buffer size matches advertised dimensions
//   * Compressed encoding detection
//
// No ROS, no V4L2 — fully standalone testable. Both camera nodes
// delegate frame-bookkeeping to this module before pushing the
// Image / CompressedImage message onto a publisher.

#ifndef SAN_CAMERAS__FRAME_METADATA_HPP_
#define SAN_CAMERAS__FRAME_METADATA_HPP_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace san_cameras {

/// Returns the bytes-per-pixel for the encoding, or 0 for
/// compressed/variable-rate formats (h264, h265, mjpeg).
/// Returns std::nullopt for unknown encodings.
std::optional<size_t> bytesPerPixel(const std::string& encoding);

/// Returns true if `encoding` is a compressed format that does NOT
/// follow the width*bpp*height layout (h264, h265, mjpeg, jpeg).
bool isCompressed(const std::string& encoding);

/// Compute row step = width * bytesPerPixel(encoding). Returns 0 for
/// compressed encodings. Returns std::nullopt on unknown encoding.
std::optional<size_t> computeRowStep(const std::string& encoding,
                                       uint32_t width);

/// For raw (uncompressed) encodings, verify the buffer length matches
/// width * bytesPerPixel * height. For compressed encodings, only a
/// minimum size sanity check is applied.
///   * returns true on plausible buffer
///   * returns false on encoding unknown, or size mismatch for raw
bool isPlausibleBuffer(const std::string& encoding,
                        uint32_t width,
                        uint32_t height,
                        size_t buffer_size);

}  // namespace san_cameras

#endif  // SAN_CAMERAS__FRAME_METADATA_HPP_
