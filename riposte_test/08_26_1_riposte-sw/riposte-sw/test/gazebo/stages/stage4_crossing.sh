#!/usr/bin/env bash
# Stage 4 — 이동 대상 추종 (S-G2). Delegates to the canonical, already-validated
# closure harness: constant-velocity crossing target, PN lead tracking, SM-4
# in-flight clamp, contact + safe egress.
#
# Kept as a thin wrapper so the ladder runs a single family of scripts; the
# authoritative logic lives in test/gazebo/run_gazebo_closure.sh. Pass marker
# GAZEBO_CLOSURE_PASS is re-emitted as STAGE4_CROSSING_PASS.
set -u
GZ_DIR=$(cd "$(dirname "$0")/.." && pwd)
if bash "$GZ_DIR/run_gazebo_closure.sh"; then
    echo "STAGE4_CROSSING_PASS"
else
    rc=$?; echo "STAGE4_CROSSING_FAIL (closure rc=$rc)"; exit "$rc"
fi
