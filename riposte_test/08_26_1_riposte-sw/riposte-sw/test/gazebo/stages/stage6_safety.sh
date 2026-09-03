#!/usr/bin/env bash
# Stage 6 — 안전 이탈 (SM-7 track-stale fault injection).
#
# Verifies the safe-side EGRESS path that the whole safety layer exists for:
# mid-control session the track source is killed (bridge process terminated), so the
# TrackBus stops updating. The OBC's own staleness gate (track older than
# TRACK_STALE_NS + coast) must fire SM-7 and auto-disengage to READY WITHOUT any
# operator command. This is distinct from the bridge's own valid=0 guard — here
# the writer dies outright, so only the consumer-side age check can catch it.
# If Stage 6 fails, the vehicle would keep flying a guidance command derived
# from a frozen last-known target.
set -u
. "$(dirname "$0")/gz_lib.sh"
gz_setup
gz_arm_ladder

gz_write_ini "$TESTDIR/obc.ini" guidance
gz_start_px4 || exit $?
# Pin the target as a gentle STATIC target so the tracking itself cannot trip a
# different rule (an aggressive moving-target chase can hit the SM-3 altitude
# clamp first, masking the SM-7 we want to isolate). With a static target the
# ONLY thing that can disengage after we kill the feed is track staleness.
gz_target_place 12 0 3
gz_start_bridge || exit $?
gz_start_obc "$TESTDIR/obc.ini" || exit $?
gz_arm_takeoff || exit $?
gz_target_place 12 0 3   # re-pin after takeoff drift
gz_engage || exit $?
gz_follow_camera "$OWNSHIP"   # GUI only (GUI_FOLLOW=1); no-op headless

# Kill the feed EARLY (before a long gentle approach can near contact), so the
# disengage cause is unambiguously the frozen track.
gz_log "tracking 3 s (gentle static approach), then killing the track source (bridge PID $BRIDGE_PID)…"
sleep 3
kill "$BRIDGE_PID" 2>/dev/null; BRIDGE_PID=""
pkill -f "gz_track_bridge" 2>/dev/null
gz_ok "track source killed — TrackBus now frozen"

for _ in $(seq 1 15); do
    grep -q "DISENGAGING -> READY" "$TESTDIR/obc.log" && break
    sleep 1
done
grep -q "OFFBOARD_ACTIVE -> DISENGAGING" "$TESTDIR/obc.log" \
    || { echo "NO_AUTO_DISENGAGE — SM-7 did not fire on a frozen track"; tail -20 "$TESTDIR/obc.log"; exit 6; }
grep -q "DISENGAGING -> READY" "$TESTDIR/obc.log" \
    || { echo "DID_NOT_REACH_READY after auto-disengage"; tail -20 "$TESTDIR/obc.log"; exit 6; }

# Assert SM-7 SPECIFICALLY drove it: the violation line immediately preceding the
# DISENGAGING transition must have the SB_TRACK_STALE (0x40) bit set.
DIS_MASK=$(grep -B1 "OFFBOARD_ACTIVE -> DISENGAGING" "$TESTDIR/obc.log" \
    | grep -oE "mask=0x[0-9a-f]+" | tail -1 | cut -d= -f2)
if [ -z "$DIS_MASK" ] || ! "$PYTHON" -c "import sys;sys.exit(0 if (int('$DIS_MASK',16) & 0x40) else 1)"; then
    echo "WRONG_DISENGAGE_CAUSE — expected SB_TRACK_STALE(0x40), got ${DIS_MASK:-none}"
    grep -E "safety violation|DISENGAGING" "$TESTDIR/obc.log" | tail -5
    exit 6
fi
gz_ok "auto-disengaged to READY, cause mask=$DIS_MASK has SB_TRACK_STALE(0x40) set"

echo "STAGE6_SAFETY_PASS"
