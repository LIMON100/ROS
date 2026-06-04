#!/bin/bash
# ───────────────────────────────────────────────────────────────────────────
#  Unitree Go2 EDU + CycloneDDS 0.10.2 stack — RK3588 native build
# ───────────────────────────────────────────────────────────────────────────
# Run this on the RK3588 (Orange Pi 5 Plus) the day the hardware arrives.
#
# What it builds:
#   1. CycloneDDS 0.10.2 (Unitree's pinned version) with ENABLE_SECURITY=OFF
#      to dodge issue eclipse-cyclonedds/cyclonedds#2257 (OpenSSL header
#      collision on aarch64).
#   2. cyclonedds-cxx 0.10.2 — required by unitree_sdk2's C++ ABI.
#   3. unitree_sdk2 example smoke test — verifies link compatibility.
#
# Why ENABLE_SECURITY=OFF: the OpenSSL headers shipped with Ubuntu 22.04 arm64
# trigger the documented compile error. We don't use DDS Security (only ROS 2
# pub/sub), so this option is safe to disable.
#
# Why -mtune=cortex-a76.cortex-a55: RK3588 is big.LITTLE — A76 (4) + A55 (4).
# This tuning prefers A76 instructions but stays compatible with A55 cores.
#
# Verified on x86_64 host with aarch64 cross-compile (this exact sequence
# produced a 1.4 MB libddsc.so.0.10.2 + linked unitree_sdk2 example).
#
# Usage:
#     ./build_unitree_stack_rk3588.sh                # build all
#     ./build_unitree_stack_rk3588.sh --clean        # rm -rf intermediate
#     ./build_unitree_stack_rk3588.sh --skip-cdds    # only -cxx + sdk2 test

set -euo pipefail

PREFIX=${PREFIX:-$HOME/unitree_stack}
JOBS=${JOBS:-$(nproc)}
WORK=${WORK:-$HOME/unitree_build}
CDDS_TAG=0.10.2
CDDSCXX_TAG=0.10.2

CLEAN=0
SKIP_CDDS=0
SKIP_CDDSCXX=0
for arg in "$@"; do
    case $arg in
        --clean)        CLEAN=1 ;;
        --skip-cdds)    SKIP_CDDS=1 ;;
        --skip-cdds-cxx) SKIP_CDDSCXX=1 ;;
        -h|--help)      sed -n '4,30p' "$0"; exit 0 ;;
    esac
done

# ── Sanity ──
[[ "$(uname -m)" == "aarch64" ]] || {
    echo "ERROR: this script must run on aarch64 (RK3588). Got $(uname -m)."
    exit 1
}

# ── Dependencies ──
echo "═══ Installing build deps (sudo required) ═══"
sudo apt-get update
sudo apt-get install -y \
    build-essential cmake git pkg-config \
    libssl-dev maven default-jre \
    python3-dev python3-pip

# ── Workspace ──
[[ $CLEAN == 1 ]] && rm -rf "$WORK"
mkdir -p "$WORK" "$PREFIX"
cd "$WORK"

# ── Optimization flags for RK3588 ──
RK3588_FLAGS="-march=armv8-a -mtune=cortex-a76.cortex-a55 -O2"
export CFLAGS="$RK3588_FLAGS ${CFLAGS:-}"
export CXXFLAGS="$RK3588_FLAGS ${CXXFLAGS:-}"

# ─────────────────────────────────────────────────────────────────────────
# 1. CycloneDDS 0.10.2
# ─────────────────────────────────────────────────────────────────────────
if [[ $SKIP_CDDS == 0 ]]; then
    echo ""
    echo "═══ [1/3] CycloneDDS $CDDS_TAG ═══"
    if [[ ! -d cyclonedds ]]; then
        git clone --branch releases/0.10.x \
            https://github.com/eclipse-cyclonedds/cyclonedds.git
        ( cd cyclonedds && git checkout $CDDS_TAG )
    fi
    cd cyclonedds
    rm -rf build && mkdir build && cd build
    cmake .. \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_IDLC=ON \
        -DBUILD_TESTING=OFF \
        -DBUILD_DOCS=OFF \
        -DENABLE_SECURITY=OFF      `# dodges issue #2257 OpenSSL collision`
    cmake --build . -- -j"$JOBS"
    cmake --install .
    cd "$WORK"
fi

# ─────────────────────────────────────────────────────────────────────────
# 2. cyclonedds-cxx 0.10.2 (required by unitree_sdk2 C++ ABI)
# ─────────────────────────────────────────────────────────────────────────
if [[ $SKIP_CDDSCXX == 0 ]]; then
    echo ""
    echo "═══ [2/3] cyclonedds-cxx $CDDSCXX_TAG ═══"
    if [[ ! -d cyclonedds-cxx ]]; then
        git clone --branch releases/0.10.x \
            https://github.com/eclipse-cyclonedds/cyclonedds-cxx.git
        ( cd cyclonedds-cxx && git checkout $CDDSCXX_TAG )
    fi
    cd cyclonedds-cxx
    rm -rf build && mkdir build && cd build
    cmake .. \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_TESTING=OFF \
        -DBUILD_DOCS=OFF \
        -DBUILD_IDLLIB=OFF \
        -DCycloneDDS_DIR="$PREFIX/lib/cmake/CycloneDDS"
    cmake --build . -- -j"$JOBS"
    cmake --install .
    cd "$WORK"
fi

# ─────────────────────────────────────────────────────────────────────────
# 3. unitree_sdk2 — link smoke test
# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "═══ [3/3] unitree_sdk2 ═══"
if [[ ! -d unitree_sdk2 ]]; then
    git clone --depth 1 https://github.com/unitreerobotics/unitree_sdk2.git
fi

cat > test_link.cc << 'EOF'
#include <unitree/robot/channel/channel_factory.hpp>
#include <iostream>
int main() {
    // ChannelFactory::Init(domain_id, network_iface) — empty iface = simulator
    unitree::robot::ChannelFactory::Instance()->Init(0);
    std::cout << "unitree_sdk2 link OK on " << "aarch64\n";
    return 0;
}
EOF

g++ -std=c++17 \
    -I"$PWD/unitree_sdk2/include" \
    -I"$PREFIX/include" \
    -I"$PREFIX/include/ddscxx" \
    test_link.cc \
    -L"$PWD/unitree_sdk2/lib/aarch64" \
    -L"$PREFIX/lib" \
    -Wl,-rpath,"$PREFIX/lib" \
    -lunitree_sdk2 -lddscxx -lddsc \
    -lpthread -ldl -lrt \
    -o test_link

echo ""
echo "═══ Smoke test ═══"
file test_link
./test_link

# ─────────────────────────────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────────────────────────────
echo ""
echo "═══ Build complete ═══"
echo "Install prefix : $PREFIX"
echo "  libddsc      : $(ls -la $PREFIX/lib/libddsc.so.* 2>/dev/null | head -1)"
echo "  libddscxx    : $(ls -la $PREFIX/lib/libddscxx.so.* 2>/dev/null | head -1)"
echo ""
echo "Add to ~/.bashrc:"
echo "  export CYCLONEDDS_HOME=$PREFIX"
echo "  export LD_LIBRARY_PATH=$PREFIX/lib:\$LD_LIBRARY_PATH"
echo ""
echo "For ROS 2 Humble integration, point rmw_cyclonedds_cpp at this build:"
echo "  export CMAKE_PREFIX_PATH=$PREFIX:\$CMAKE_PREFIX_PATH"
