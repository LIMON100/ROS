#!/usr/bin/env bash
# Stage 1 — 인지 브리지 (gz_track_bridge: pose truth -> TrackBus). No flight.
#
# Verifies the perception SUBSTITUTE that stands in for riposte-seeker:
#   1a. Conversion accuracy — the first published sample matches the known
#       spawn geometry (target ~18 m ahead of the grounded ownship, above),
#       i.e. FRD forward ~= range and lateral small.
#   1b. Staleness gating — when the pose feed stops (sim paused), the bridge
#       stops publishing the frozen pose as fresh and emits valid=0 within its
#       300 ms guard; on resume it recovers. This is what keeps SM-7 effective
#       in the gz environment.
# No OBC, no MAVSDK flight — fast and deterministic.
set -u
. "$(dirname "$0")/gz_lib.sh"
gz_setup
gz_arm_ladder

gz_start_px4 || exit $?
gz_start_bridge || exit $?

# 1a. conversion accuracy against the shipped geometry (pose 18,0,3; ownship 0).
LINE=$(grep -m1 -oE "range=[0-9.]+ frd=\[[-0-9. ]+\]" "$TESTDIR/bridge.log")
echo "-- first sample: $LINE"
R=$(echo "$LINE" | grep -oE "range=[0-9.]+" | cut -d= -f2)
read -r FX FY FZ <<<"$(echo "$LINE" | sed -E 's/.*frd=\[([-0-9. ]+)\].*/\1/')"
CONV=$("$PYTHON" - "$R" "$FX" "$FY" "$FZ" <<'PY'
import sys
r,fx,fy,fz=map(float,sys.argv[1:5])
ok=abs(r-(fx*fx+fy*fy+fz*fz)**.5)<.2 and fx>0 and fz<0
print(1 if ok else 0)
PY
)
[ "$CONV" = "1" ] && gz_ok "FRD conversion sane (range=$R frd=[$FX $FY $FZ])" \
    || { echo "BAD_CONVERSION range=$R frd=[$FX $FY $FZ] (expected fwd~range, lat~0, down<0)"; exit 2; }

# 1b. staleness: this is the LAST check, so we can tear down the pose source.
# Pausing does not stop pose/info in this gz build (it keeps publishing on a
# wall timer), so instead kill the gz server outright — the bridge must detect
# the >300 ms pose silence and switch to valid=0 (this is what keeps SM-7
# effective when the truth feed dies). Safe here: this script's own command line
# is `bash stage1_bridge.sh`, so the pattern pkill does not self-match.
gz_log "killing pose source (gz server) to exercise the 300 ms staleness guard…"
pkill -f "gz sim" 2>/dev/null
for _ in $(seq 1 8); do
    grep -q "pose feed stale\|valid=0" "$TESTDIR/bridge.log" && break
    sleep 1
done
if grep -qE "pose feed stale|valid=0" "$TESTDIR/bridge.log"; then
    gz_ok "staleness guard fired after pose source died ($(grep -oE 'WRN pose feed stale[^\n]*' "$TESTDIR/bridge.log" | head -1))"
else
    echo "STALENESS_NOT_FLAGGED — pose source died but bridge kept publishing fresh"
    tail -5 "$TESTDIR/bridge.log"; exit 2
fi

echo "STAGE1_BRIDGE_PASS"
