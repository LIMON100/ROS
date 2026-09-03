#!/usr/bin/env bash
# SM-1~9 violation-injection tests against PX4 SITL (SIH) — SAD-001 §12-2.
#
# Each scenario: fresh riposte-obc with a scenario-specific config -> engage ->
# inject the fault -> assert the expected SafetyBit in the violation mask ->
# assert automatic DISENGAGING -> READY.
#
#   SM-8  engage timebox     (timebox 6 s, hover — waits it out)
#   SM-3  soft geofence      (constant 2 m/s north, geofence 10 m)
#   SM-7  track stale        (guidance + synthetic seeker, seeker killed mid-engage)
#   SM-2  mode override      (GCS switches PX4 to Hold mid-engage)
#   SM-5  loop jitter        (SIGSTOP/SIGCONT bursts on the OBC process)
#   SM-1  telemetry stale    (SIGSTOP on PX4 for 0.8 s)
#   SM-9  low battery        (SIM_BAT_MIN_PCT drop; also checks the engage-deny
#                             half, then restores the battery — before SM-6)
#   SM-6  in-flight disarm   (forced disarm — vehicle falls; LAST scenario)
#   SM-4  clamp: no runtime injection — enforced pre-send, covered by unit test
#
# Environment: same as run_sitl_test.sh (PX4_BUILD, RIPOSTE_BUILD, PYTHON,
# optional MAVSDK_LIB). The vehicle is armed+airborne once and reused; SM-6
# runs last because it crashes the airframe.
set -u
: "${PX4_BUILD:?set PX4_BUILD to the px4_sitl_default build dir}"
: "${RIPOSTE_BUILD:?set RIPOSTE_BUILD to a RIPOSTE_WITH_MAVSDK=ON build dir}"
PYTHON=${PYTHON:-python3}
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
TESTDIR=${TESTDIR:-$(mktemp -d /tmp/riposte-sm.XXXXXX)}
mkdir -p "$TESTDIR"
[ -n "${MAVSDK_LIB:-}" ] && export LD_LIBRARY_PATH=$MAVSDK_LIB:${LD_LIBRARY_PATH:-}
echo "logs: $TESTDIR"

PASS=0
FAIL=0
OBC_PID=""
SEEKER_PID=""
PX4_PID=""

cleanup() {
    # Kill the PIDs this run started first so a concurrent unrelated session's
    # processes are spared as far as practical; the pattern pkill is the
    # fallback for leftovers.
    [ -n "$OBC_PID" ] && kill "$OBC_PID" 2>/dev/null
    [ -n "$SEEKER_PID" ] && kill "$SEEKER_PID" 2>/dev/null
    [ -n "$PX4_PID" ] && kill "$PX4_PID" 2>/dev/null
    pkill -f "bin/px4" 2>/dev/null
    rm -f /tmp/riposte-obc.sock
}
trap cleanup EXIT

write_ini() { # $1=path $2=source $3=geofence_r $4=timebox_s [$5=bat_engage $6=bat_land]
    # SM-9 gates default to the product values; scenarios that must be immune to
    # battery state (e.g. SM-6 after the battery scenario) pass 0 0 to disable.
    cat > "$1" <<EOF
[obc]
connection_url = udpin://0.0.0.0:14540
source = $2
operator_token = sitl-test-token
cmd_socket = /tmp/riposte-obc.sock
rt_priority = 0
cpu_affinity = -1
[safety]
vmax_h = 5.0
vmax_v = 2.0
geofence_r = $3
alt_min = 1.0
alt_max = 80.0
engage_timebox_s = $4
bat_engage_min_frac = ${5:-0.30}
bat_land_frac = ${6:-0.20}
EOF
}

start_obc() { # $1=ini $2=log
    rm -f /tmp/riposte-obc.sock
    "$RIPOSTE_BUILD/riposte-obc" "$1" > "$2" 2>&1 &
    OBC_PID=$!
    for _ in $(seq 1 30); do
        grep -q "state=READY" "$2" && return 0
        kill -0 "$OBC_PID" 2>/dev/null || return 1
        sleep 1
    done
    return 1
}

stop_obc() {
    [ -n "$OBC_PID" ] && kill "$OBC_PID" 2>/dev/null && wait "$OBC_PID" 2>/dev/null
    OBC_PID=""
}

engage_and_wait_active() { # $1=log
    RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
        "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token > /dev/null
    for _ in $(seq 1 15); do
        grep -q "OFFBOARD_ACTIVE" "$1" && return 0
        sleep 1
    done
    return 1
}

wait_mask_bit() { # $1=log $2=bit(decimal) $3=timeout_s
    for _ in $(seq 1 "$3"); do
        for m in $(grep -oP 'violation mask=0x\K[0-9a-fA-F]+' "$1" | sort -u); do
            if (((16#$m) & $2)); then return 0; fi
        done
        sleep 1
    done
    return 1
}

wait_back_to_ready() { # $1=log $2=timeout_s
    for _ in $(seq 1 "$2"); do
        grep -q "DISENGAGING -> READY" "$1" && return 0
        sleep 1
    done
    return 1
}

verdict() { # $1=name $2=0/1(pass)
    if [ "$2" -eq 0 ]; then echo "PASS  $1"; PASS=$((PASS + 1));
    else echo "FAIL  $1"; FAIL=$((FAIL + 1)); fi
}

# ---- bring-up: PX4 + one-time arm/takeoff -----------------------------------
# Startup sweep (documented single-instance assumption: this test owns the
# host's PX4/riposte processes and clears any stale leftovers).
cleanup; sleep 1
cd "$PX4_BUILD" || exit 2
rm -rf eeprom parameters*.bts log
PX4_SIM_MODEL=sihsim_quadx PX4_SIMULATOR=sihsim \
    ./bin/px4 -d -s etc/init.d-posix/rcS > "$TESTDIR/px4.log" 2>&1 &
PX4_PID=$!
for _ in $(seq 1 30); do
    grep -q "Startup script returned successfully" "$TESTDIR/px4.log" && break
    sleep 1
done
grep -q "Startup script returned successfully" "$TESTDIR/px4.log" \
    || { echo "PX4 start failed"; exit 2; }
echo "PX4_UP (pid=$PX4_PID)"

"$PYTHON" "$SCRIPT_DIR/arm_takeoff.py" > "$TESTDIR/takeoff.log" 2>&1
grep -q "READY_FOR_ENGAGE" "$TESTDIR/takeoff.log" \
    || { echo "takeoff failed"; tail -10 "$TESTDIR/takeoff.log"; exit 2; }
echo "AIRBORNE"

# ---- SM-8: engage timebox ----------------------------------------------------
L=$TESTDIR/sm8.log; write_ini "$TESTDIR/sm8.ini" hover 200.0 6.0
ok=1
if start_obc "$TESTDIR/sm8.ini" "$L" && engage_and_wait_active "$L" \
   && wait_mask_bit "$L" $((0x80)) 15 && wait_back_to_ready "$L" 10; then ok=0; fi
verdict "SM-8 engage timebox (0x80)" $ok; stop_obc

# ---- SM-3: soft geofence -------------------------------------------------------
L=$TESTDIR/sm3.log; write_ini "$TESTDIR/sm3.ini" constant 10.0 30.0
ok=1
if start_obc "$TESTDIR/sm3.ini" "$L" && engage_and_wait_active "$L" \
   && wait_mask_bit "$L" $((0x04)) 20 && wait_back_to_ready "$L" 10; then ok=0; fi
verdict "SM-3 soft geofence (0x04)" $ok; stop_obc

# ---- SM-7: track stale (kill seeker mid-engage) --------------------------------
L=$TESTDIR/sm7.log; write_ini "$TESTDIR/sm7.ini" guidance 200.0 30.0
# The seeker needs an EXPLICIT SIL profile: seeker.synthetic defaults to
# false (production) so a deployment can never fall back to synthetic
# detections (P0-01). Without it the seeker refuses to start, and this
# scenario would "pass" for the wrong reason — SM-7 firing because a track
# never existed rather than because a live one went stale.
{
    echo "[seeker]"
    echo "synthetic = true"
    echo "record    = false"
} > "$TESTDIR/sm7-seeker.ini"
"$RIPOSTE_BUILD/riposte-seeker" "$TESTDIR/sm7-seeker.ini" > "$TESTDIR/sm7-seeker.log" 2>&1 &
SEEKER_PID=$!
ok=1
# Require a REAL published track before injecting: otherwise the staleness
# the test asserts is not the one it means to inject.
for _ in $(seq 1 15); do
    grep -q "track=yes" "$TESTDIR/sm7-seeker.log" && break
    sleep 1
done
if ! grep -q "track=yes" "$TESTDIR/sm7-seeker.log"; then
    echo "  (seeker never published a track — SM-7 cannot be injected)"
elif start_obc "$TESTDIR/sm7.ini" "$L" && engage_and_wait_active "$L"; then
    sleep 2 # track the synthetic target briefly
    kill "$SEEKER_PID" 2>/dev/null && wait "$SEEKER_PID" 2>/dev/null
    SEEKER_PID=""
    if wait_mask_bit "$L" $((0x40)) 10 && wait_back_to_ready "$L" 10; then ok=0; fi
fi
verdict "SM-7 track stale (0x40)" $ok; stop_obc
[ -n "$SEEKER_PID" ] && kill "$SEEKER_PID" 2>/dev/null && SEEKER_PID=""

# ---- SM-2: external mode override ----------------------------------------------
L=$TESTDIR/sm2.log; write_ini "$TESTDIR/sm2.ini" hover 200.0 30.0
ok=1
if start_obc "$TESTDIR/sm2.ini" "$L" && engage_and_wait_active "$L"; then
    "$PYTHON" "$SCRIPT_DIR/inject.py" hold > "$TESTDIR/sm2-inject.log" 2>&1
    if wait_mask_bit "$L" $((0x02)) 10 && wait_back_to_ready "$L" 10 \
       && grep -q "reentry blocked" "$L"; then ok=0; fi
fi
verdict "SM-2 mode override (0x02, reentry blocked)" $ok; stop_obc

# ---- SM-5: control-loop jitter (SIGSTOP bursts on the OBC) ----------------------
L=$TESTDIR/sm5.log; write_ini "$TESTDIR/sm5.ini" hover 200.0 30.0
ok=1
for attempt in 1 2 3; do
    : > "$L"
    if start_obc "$TESTDIR/sm5.ini" "$L" && engage_and_wait_active "$L"; then
        # Six >130 ms stalls with ~10 ms of run time between them = at least
        # three consecutive over-budget periods (budget: 50 ms ± 20 %). Python
        # keeps the STOP/CONT cadence tight; shell kill/sleep spawn latency lets
        # clean ticks slip in between stalls and reset the consecutive counter.
        "$PYTHON" - "$OBC_PID" <<'PYEOF'
import os, signal, sys, time
pid = int(sys.argv[1])
for _ in range(6):
    os.kill(pid, signal.SIGSTOP)
    time.sleep(0.13)
    os.kill(pid, signal.SIGCONT)
    time.sleep(0.01)
PYEOF
        if wait_mask_bit "$L" $((0x10)) 5 && wait_back_to_ready "$L" 10; then
            ok=0; stop_obc; break
        fi
    fi
    stop_obc
    echo "  (SM-5 attempt $attempt did not fire, retrying)"
done
verdict "SM-5 loop jitter (0x10)" $ok

# ---- SM-1: telemetry stale (freeze PX4 0.8 s) ------------------------------------
L=$TESTDIR/sm1.log; write_ini "$TESTDIR/sm1.ini" hover 200.0 30.0
ok=1
if start_obc "$TESTDIR/sm1.ini" "$L" && engage_and_wait_active "$L"; then
    kill -STOP "$PX4_PID"
    sleep 0.8
    kill -CONT "$PX4_PID"
    if wait_mask_bit "$L" $((0x01)) 10 && wait_back_to_ready "$L" 15; then ok=0; fi
fi
verdict "SM-1 telemetry stale (0x01)" $ok; stop_obc

# ---- SM-9: low battery (SIM_BAT_MIN_PCT drop) ------------------------------------
# Two halves. In flight: a KNOWN-low battery must set SB_BATTERY (0x100) and
# disengage. Then, while the battery is still low, a re-engage must be DENIED
# by the READY-side gate (log line, no second OFFBOARD_ACTIVE). Runs before
# SM-6 because it needs a flying vehicle; the battery is restored afterwards.
L=$TESTDIR/sm9.log; write_ini "$TESTDIR/sm9.ini" hover 200.0 120.0
ok=1
if start_obc "$TESTDIR/sm9.ini" "$L" && engage_and_wait_active "$L"; then
    "$PYTHON" "$SCRIPT_DIR/inject.py" battery_low > "$TESTDIR/sm9-inject.log" 2>&1
    if wait_mask_bit "$L" $((0x100)) 90 && wait_back_to_ready "$L" 10; then ok=0; fi
fi
verdict "SM-9 low battery (0x100)" $ok
ok=1
if [ -n "$OBC_PID" ] && kill -0 "$OBC_PID" 2>/dev/null; then
    RIPOSTE_OBC_SOCKET=/tmp/riposte-obc.sock \
        "$RIPOSTE_BUILD/test/riposte-engage" engage sitl-test-token > /dev/null
    for _ in $(seq 1 10); do
        grep -q "engage ignored: battery" "$L" && { ok=0; break; }
        sleep 1
    done
fi
verdict "SM-9 engage denied on low battery" $ok; stop_obc
"$PYTHON" "$SCRIPT_DIR/inject.py" battery_restore > "$TESTDIR/sm9-restore.log" 2>&1 \
    || echo "  (battery restore not confirmed — SM-6 runs with its gate disabled)"

# ---- SM-6: forced disarm in flight (vehicle falls — last) ------------------------
# Battery gates off: this scenario asserts the disarm bit and must not depend
# on how far the battery recovered after SM-9.
L=$TESTDIR/sm6.log; write_ini "$TESTDIR/sm6.ini" hover 200.0 30.0 0 0
ok=1
if start_obc "$TESTDIR/sm6.ini" "$L" && engage_and_wait_active "$L"; then
    "$PYTHON" "$SCRIPT_DIR/inject.py" disarm_force > "$TESTDIR/sm6-inject.log" 2>&1
    if wait_mask_bit "$L" $((0x20)) 10 && wait_back_to_ready "$L" 15; then ok=0; fi
fi
verdict "SM-6 in-flight disarm (0x20)" $ok; stop_obc

echo "----------------------------------------"
echo "SM injection: $PASS passed, $FAIL failed (SM-4 clamp: unit-tested; SM-1..3,5..9 injected)"
[ "$FAIL" -eq 0 ] && echo "SM_INJECTION_PASS"
exit "$FAIL"
