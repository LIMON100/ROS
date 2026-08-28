#!/usr/bin/env bash
# Stage 5 — 회피 기동 (S-G3). Delegates to the canonical evasive harness:
# mid-tracking the target is commanded a >120° turn, and the run passes on
# either reconvergence-to-contact or a safe-side SM auto-disengage.
#
# Thin wrapper; authoritative logic in test/gazebo/run_gazebo_evasive.sh.
set -u
GZ_DIR=$(cd "$(dirname "$0")/.." && pwd)
if bash "$GZ_DIR/run_gazebo_evasive.sh"; then
    echo "STAGE5_EVASIVE_PASS"
else
    rc=$?; echo "STAGE5_EVASIVE_FAIL (evasive rc=$rc)"; exit "$rc"
fi
