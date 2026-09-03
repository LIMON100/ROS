#!/usr/bin/env bash
# Sanitizer gate (G11.3 / G16.4) — build & ctest under ASan+UBSan and TSan.
# Run before merging any pointer/lifetime or multithreaded change.
#
#   test/run_sanitizers.sh [build-root]
set -eu
SRC=$(cd "$(dirname "$0")/.." && pwd)
BUILD_ROOT=${1:-$(mktemp -d /tmp/riposte-san.XXXXXX)}
echo "source: $SRC"
echo "builds: $BUILD_ROOT"

# ASan + UBSan --------------------------------------------------------------
ASAN_DIR=$BUILD_ROOT/asan
cmake -S "$SRC" -B "$ASAN_DIR" -DCMAKE_BUILD_TYPE=Debug \
    -DRIPOSTE_SANITIZE=address,undefined >/dev/null
cmake --build "$ASAN_DIR" -j >/dev/null
echo "== ASan+UBSan ctest =="
( cd "$ASAN_DIR" && \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --output-on-failure )

# TSan ----------------------------------------------------------------------
TSAN_DIR=$BUILD_ROOT/tsan
cmake -S "$SRC" -B "$TSAN_DIR" -DCMAKE_BUILD_TYPE=Debug \
    -DRIPOSTE_SANITIZE=thread >/dev/null
cmake --build "$TSAN_DIR" -j >/dev/null
echo "== TSan ctest =="
# setarch -R disables ASLR: some kernels (incl. WSL2) otherwise abort TSan with
# "unexpected memory mapping". NO suppressions (COMMON-SDD C-5): the SeqSlot
# payload is atomic words, so every TSan report is a real finding.
( cd "$TSAN_DIR" && \
  TSAN_OPTIONS="halt_on_error=1" \
  setarch -R ctest --output-on-failure )

echo "SANITIZERS_CLEAN"
