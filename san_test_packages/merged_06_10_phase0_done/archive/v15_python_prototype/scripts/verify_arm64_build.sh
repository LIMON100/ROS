#!/bin/bash
# ───────────────────────────────────────────────────────────────────────────
#  Pre-procurement verification: CycloneDDS 0.10.2 + cyclonedds-cxx 0.10.2
#  + unitree_sdk2 link compatibility on aarch64 (RK3588 target)
# ───────────────────────────────────────────────────────────────────────────
# Run on any x86_64 Linux host with the aarch64 cross-toolchain installed.
# Verifies that the exact same build sequence we'll run natively on the
# RK3588 will succeed before we commit to procurement.
#
# What this proves:
#   • CycloneDDS 0.10.2 source compiles clean for aarch64
#   • The known issue #2257 (OpenSSL header collision) is dodged by
#     ENABLE_SECURITY=OFF
#   • cyclonedds-cxx 0.10.2 builds against our CycloneDDS 0.10.2
#   • unitree_sdk2's pre-compiled aarch64 .a links cleanly with our
#     CycloneDDS build (i.e. ABI matches — not just any 0.10.2)
#
# What this does NOT prove (still requires real hardware):
#   • Runtime behavior of CycloneDDS on RK3588 (cache coherency, scheduling)
#   • DDS handshake with Go2 EDU's onboard publisher
#   • Sport Mode release sequence
#   • Real-time latency under load
#
# Setup (Ubuntu 22.04 / 24.04 host):
#     sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
#         cmake git
#     sudo dpkg --add-architecture arm64        # only if the libc6:arm64
#     sudo apt-get update                        # ... package is needed
#     sudo apt-get install -y libc6-dev:arm64
#
# Usage:
#     ./verify_arm64_build.sh
#     ./verify_arm64_build.sh --keep            # don't delete WORK on exit

set -euo pipefail

WORK=${WORK:-$(mktemp -d -t cdds-verify-XXXXXX)}
KEEP=0
[[ "${1:-}" == "--keep" ]] && KEEP=1

cleanup() {
    if [[ $KEEP == 0 ]]; then
        rm -rf "$WORK"
    else
        echo "Kept work dir: $WORK"
    fi
}
trap cleanup EXIT

echo "═══ aarch64 cross-build verification ═══"
echo "WORK: $WORK"
cd "$WORK"

# Toolchain file targeting RK3588 (Cortex-A76/A55 big.LITTLE)
cat > toolchain-arm64.cmake << 'EOF'
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_C_FLAGS_INIT   "-march=armv8-a -mtune=cortex-a76.cortex-a55")
set(CMAKE_CXX_FLAGS_INIT "-march=armv8-a -mtune=cortex-a76.cortex-a55")
set(CMAKE_FIND_ROOT_PATH /usr/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
EOF

# ── 1. CycloneDDS 0.10.2 ──
echo ""
echo "═══ [1/3] CycloneDDS 0.10.2 cross-build ═══"
git clone --depth 1 --branch 0.10.2 \
    https://github.com/eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$WORK/toolchain-arm64.cmake" \
    -DCMAKE_INSTALL_PREFIX="$WORK/install" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_IDLC=OFF \
    -DBUILD_TESTING=OFF \
    -DBUILD_DOCS=OFF \
    -DENABLE_SECURITY=OFF \
    -DENABLE_LTO=OFF
cmake --build . -j"$(nproc)" 2>&1 | tail -3
cmake --install . > /dev/null
file "$WORK/install/lib/libddsc.so.0.10.2" | grep -q "ARM aarch64" \
    || { echo "✗ libddsc not aarch64"; exit 1; }
echo "  ✓ libddsc.so.0.10.2 ($(stat -c %s $WORK/install/lib/libddsc.so.0.10.2) bytes, ARM aarch64)"
cd "$WORK"

# ── 2. cyclonedds-cxx 0.10.2 ──
echo ""
echo "═══ [2/3] cyclonedds-cxx 0.10.2 cross-build ═══"
git clone --depth 1 --branch 0.10.2 \
    https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git
cd cyclonedds-cxx
mkdir build && cd build
cmake .. \
    -DCMAKE_TOOLCHAIN_FILE="$WORK/toolchain-arm64.cmake" \
    -DCMAKE_INSTALL_PREFIX="$WORK/install" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTING=OFF \
    -DBUILD_DOCS=OFF \
    -DBUILD_IDLLIB=OFF \
    -DCycloneDDS_DIR="$WORK/install/lib/cmake/CycloneDDS"
cmake --build . -j"$(nproc)" 2>&1 | tail -3
cmake --install . > /dev/null
file "$WORK/install/lib/libddscxx.so.0.10.2" | grep -q "ARM aarch64" \
    || { echo "✗ libddscxx not aarch64"; exit 1; }
echo "  ✓ libddscxx.so.0.10.2 ($(stat -c %s $WORK/install/lib/libddscxx.so.0.10.2) bytes, ARM aarch64)"
cd "$WORK"

# ── 3. unitree_sdk2 link smoke test ──
echo ""
echo "═══ [3/3] unitree_sdk2 link compatibility ═══"
git clone --depth 1 https://github.com/unitreerobotics/unitree_sdk2.git

cat > test_link.cc << 'EOF'
#include <unitree/robot/channel/channel_factory.hpp>
int main() {
    unitree::robot::ChannelFactory::Instance()->Init(0);
    return 0;
}
EOF

aarch64-linux-gnu-g++ -std=c++17 \
    -I"$WORK/unitree_sdk2/include" \
    -I"$WORK/install/include" \
    -I"$WORK/install/include/ddscxx" \
    test_link.cc \
    -L"$WORK/unitree_sdk2/lib/aarch64" \
    -L"$WORK/install/lib" \
    -lunitree_sdk2 -lddscxx -lddsc \
    -lpthread -ldl -lrt \
    -o test_link 2>&1 | tail -5

file test_link | grep -q "ARM aarch64" \
    || { echo "✗ test_link not aarch64"; exit 1; }
echo "  ✓ test_link ($(stat -c %s test_link) bytes, ARM aarch64)"

echo ""
echo "═══ ALL CHECKS PASSED ═══"
echo "CycloneDDS 0.10.2 + cyclonedds-cxx 0.10.2 + unitree_sdk2 are aarch64-build-compatible."
echo "Run scripts/build_unitree_stack_rk3588.sh on the RK3588 once hardware arrives."
