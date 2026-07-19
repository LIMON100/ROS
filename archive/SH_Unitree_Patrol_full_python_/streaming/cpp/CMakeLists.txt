# ============================================================================
#  patrol-streaming — AIRYS stream_ch2 + GstStreamer C++ wrapper for RK3588
# ============================================================================
# Builds:
#   • libpatrol_stream_ch2.so   — Kong's TP2855_SRT C library (byte-identical)
#   • libpatrol_stream.so       — AIRYS C++ wrapper (RAII over Kong's C API)
#   • libpatrol_stream_dmabuf   — board variant with zero-copy GstDmaBufAllocator
#                                 (only when GSTREAMER_ALLOCATORS is found)
#
# RK3588 native build (the target deployment):
#   cmake -B build -DCMAKE_BUILD_TYPE=Release \
#         -DCMAKE_C_FLAGS="-march=armv8-a -mtune=cortex-a76.cortex-a55"
#   cmake --build build -j
#
# x86 host cross-compile verification (pre-flight before deploying to board):
#   cmake -B build-arm \
#         -DCMAKE_TOOLCHAIN_FILE=../../scripts/aarch64-toolchain.cmake \
#         -DSTREAM_BUILD_GSTREAMER=OFF        # gstreamer:arm64 not in apt
#   cmake --build build-arm
#   → produces ARM aarch64 ELF .so files; verifies the source tree is
#     toolchain-clean.
#
# When GStreamer is unavailable (CI / Mac dev), the build falls back to
# the stub TU which compiles into a no-op stream_ch2_*.
# ============================================================================
cmake_minimum_required(VERSION 3.18)
project(patrol_streaming C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ────────────────────────────────────────────────────────────
# Sanitizer build (host CI only — never ship)
#   cmake -B build-san -DSTREAM_SANITIZE=ON
# ────────────────────────────────────────────────────────────
option(STREAM_SANITIZE "Build with AddressSanitizer + UBSan" OFF)
if(STREAM_SANITIZE)
    set(_san_flags -fsanitize=address,undefined -fno-omit-frame-pointer -O1 -g)
    add_compile_options(${_san_flags})
    add_link_options(${_san_flags})
    message(STATUS "patrol_streaming: ASan + UBSan enabled")
endif()

# Find GStreamer — optional, falls back to stub when missing
find_package(PkgConfig)
option(STREAM_BUILD_GSTREAMER "Link real GStreamer (otherwise use stub)" ON)

if(STREAM_BUILD_GSTREAMER AND PkgConfig_FOUND)
    pkg_check_modules(GSTREAMER       IMPORTED_TARGET gstreamer-1.0)
    pkg_check_modules(GSTREAMER_APP   IMPORTED_TARGET gstreamer-app-1.0)
    pkg_check_modules(GSTREAMER_ALLOC IMPORTED_TARGET gstreamer-allocators-1.0)
endif()

set(_HAVE_GST FALSE)
if(STREAM_BUILD_GSTREAMER AND GSTREAMER_FOUND AND GSTREAMER_APP_FOUND)
    set(_HAVE_GST TRUE)
    message(STATUS "patrol_streaming: building with real GStreamer "
                   "(version ${GSTREAMER_VERSION})")
else()
    message(STATUS "patrol_streaming: GStreamer not found — building stubs")
endif()

# ────────────────────────────────────────────────────────────
# 1. Kong's stream_ch2 C library (byte-identical from AIRYS_TP2855_SRT)
# ────────────────────────────────────────────────────────────
if(_HAVE_GST)
    add_library(patrol_stream_ch2 SHARED stream_ch2.c)
    target_link_libraries(patrol_stream_ch2 PUBLIC
        PkgConfig::GSTREAMER PkgConfig::GSTREAMER_APP)
else()
    # Compile the stub TU instead — same ABI, no real encoder
    add_library(patrol_stream_ch2 SHARED
        ${CMAKE_CURRENT_SOURCE_DIR}/../../streaming/cpp/stream_ch2_stub.c)
endif()
target_include_directories(patrol_stream_ch2 PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# ────────────────────────────────────────────────────────────
# 2. C++ wrapper (RAII GstStreamer)
# ────────────────────────────────────────────────────────────
add_library(patrol_stream SHARED gst_streamer.cpp)
target_link_libraries(patrol_stream PUBLIC patrol_stream_ch2)
target_include_directories(patrol_stream PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include)

# ────────────────────────────────────────────────────────────
# 3. Zero-copy DMA-buf board variant (RK3588 BSP only)
# ────────────────────────────────────────────────────────────
# This file overrides the weak symbol stream_ch2_push_dmabuf_nv12 with
# a real GstDmaBufAllocator implementation. Only meaningful with full
# GStreamer plus gstreamer-allocators-1.0 (RK3588 BSP / Yocto).
if(GSTREAMER_ALLOC_FOUND)
    add_library(patrol_stream_dmabuf SHARED stream_ch2_dmabuf_board.c)
    target_link_libraries(patrol_stream_dmabuf PUBLIC
        patrol_stream_ch2 PkgConfig::GSTREAMER_ALLOC)
    target_include_directories(patrol_stream_dmabuf PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/../include)
    message(STATUS "patrol_streaming: zero-copy DMA-buf path enabled")
else()
    message(STATUS "patrol_streaming: gstreamer-allocators-1.0 missing — "
                   "DMA-buf path stays stubbed (fallback to memcpy)")
endif()

# Install for the deploy script
install(TARGETS patrol_stream_ch2 patrol_stream
        LIBRARY DESTINATION lib)
if(GSTREAMER_ALLOC_FOUND)
    install(TARGETS patrol_stream_dmabuf LIBRARY DESTINATION lib)
endif()
