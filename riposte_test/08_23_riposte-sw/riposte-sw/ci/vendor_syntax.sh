#!/usr/bin/env bash
# Vendor-path syntax gate (review CR-05 recommendation 4).
#
# The RIPOSTE_WITH_HAILO / RIPOSTE_WITH_RKNN translation units are the device
# boundary: they are compiled only when the vendor SDK is present, which it is
# not in CI nor on most dev machines. Until now that meant a change to those
# files was never compiled by anyone until hardware bring-up, so a typo, a
# renamed member or a changed signature waited weeks to surface — on the one
# platform where debugging is most expensive.
#
# This compiles them with -fsyntax-only against the stubs in test/vendor_stubs.
# It proves the code PARSES and TYPE-CHECKS. It proves nothing about behaviour
# against the real runtime: the stubs have no implementation, and the bring-up
# checklist (BRINGUP-001 §3/§4) is still what closes those paths.
set -eu
SRC=$(cd "$(dirname "$0")/.." && pwd)
STUBS=$SRC/test/vendor_stubs
CXX=${CXX:-g++}

fail=0
check() { # <label> <define> <stub-include> <file...>
    label=$1
    define=$2
    inc=$3
    shift 3
    for f in "$@"; do
        [ -f "$f" ] || continue
        if "$CXX" -std=c++17 -fsyntax-only "-D$define" -I"$inc" \
            -I"$SRC/seeker/src" -I"$SRC/obc/src" -I"$SRC/common/include" "$f"; then
            echo "  ok   $label $(basename "$f")"
        else
            echo "  FAIL $label $(basename "$f")"
            fail=1
        fi
    done
}

echo "==== vendor-path syntax check (stubs, no SDK) ===="
# HailoDetector includes <hailo/hailort.hpp>, so the stub ROOT is the include
# path; RknnEmbedder includes <rknn_api.h> directly from its own stub dir.
check hailo RIPOSTE_WITH_HAILO "$STUBS" "$SRC/seeker/src/HailoDetector.cpp"
check rknn RIPOSTE_WITH_RKNN "$STUBS/rknn" "$SRC/seeker/src/RknnEmbedder.cpp"

if [ "$fail" -ne 0 ]; then
    echo "VENDOR_SYNTAX_FAIL"
    exit 1
fi
echo "VENDOR_SYNTAX_OK"
