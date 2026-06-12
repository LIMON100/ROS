#!/bin/bash
# Attach to a running patrol process and dump thread state.
#
# Two backends — picks whichever is installed:
#   1. py-spy   (preferred — no GIL takeover, works on optimized builds)
#                 pip install py-spy
#   2. gdb      (fallback — slower, requires python3-dbg helpers)
#
# Usage:
#   scripts/attach.sh <process_name>          # one process by name
#   scripts/attach.sh --pid <pid>              # by PID
#   scripts/attach.sh --all                    # every patrol_* process
#   scripts/attach.sh <name> --output dumps/   # save instead of print
#
# Examples:
#   scripts/attach.sh Localization
#   scripts/attach.sh --all --output /var/log/patrol/dumps/
#   scripts/attach.sh --pid 12345 --native     # py-spy + native frames

set -euo pipefail

OUTPUT=""
NATIVE=""
TARGETS=()
MODE="single"

while [[ $# -gt 0 ]]; do
    case $1 in
        --pid)     TARGETS+=("$2"); MODE="pid"; shift 2 ;;
        --all)     MODE="all"; shift ;;
        --output)  OUTPUT="$2"; shift 2 ;;
        --native)  NATIVE="--native"; shift ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *)         TARGETS+=("$1"); shift ;;
    esac
done

# Detect backend
if command -v py-spy >/dev/null 2>&1; then
    BACKEND=py-spy
elif command -v gdb >/dev/null 2>&1; then
    BACKEND=gdb
else
    echo "ERROR: install py-spy (preferred):  pip install py-spy" >&2
    echo "       or have gdb available in PATH." >&2
    exit 2
fi

resolve_pid() {
    local target="$1"
    if [[ "$target" =~ ^[0-9]+$ ]]; then
        echo "$target"; return
    fi
    # Match by mp.Process.name (visible in /proc/*/comm).
    # On Linux, mp.Process sets the comm via prctl(PR_SET_NAME), so
    # /proc/<pid>/comm holds the process's name (truncated to 15 chars).
    pgrep -f "$target" | head -1 || echo ""
}

list_all_patrol_pids() {
    # Anything whose argv contains main.py from this checkout
    pgrep -f "patrol.*main\.py" 2>/dev/null || true
}

dump_one() {
    local pid="$1"
    local out_file="${2:-}"
    if ! kill -0 "$pid" 2>/dev/null; then
        echo "  ✗ pid $pid not running" >&2
        return 1
    fi
    local cmd
    case "$BACKEND" in
        py-spy)
            cmd="py-spy dump --pid $pid $NATIVE"
            ;;
        gdb)
            # `set logging` puts gdb commands in the script; py-bt requires
            # python3 debug symbols (`apt install python3-dbg` on Debian).
            cmd="gdb -batch -p $pid \
                -ex 'set pagination off' \
                -ex 'thread apply all py-bt' \
                -ex 'thread apply all bt' 2>&1"
            ;;
    esac

    echo "─── pid=$pid  backend=$BACKEND ───"
    if [[ -n "$out_file" ]]; then
        if eval "$cmd" >"$out_file"; then
            echo "  ✓ saved to $out_file"
        else
            echo "  ✗ failed (see $out_file for partial output)" >&2
            return 1
        fi
    else
        eval "$cmd"
    fi
}

# ── Resolve targets ──
PIDS=()
case "$MODE" in
    all)
        # shellcheck disable=SC2046
        PIDS=($(list_all_patrol_pids))
        if [[ ${#PIDS[@]} -eq 0 ]]; then
            echo "No patrol main.py processes found." >&2
            exit 1
        fi
        ;;
    pid|single)
        if [[ ${#TARGETS[@]} -eq 0 ]]; then
            echo "Usage: $0 <process_name>|--pid <pid>|--all" >&2
            exit 1
        fi
        for t in "${TARGETS[@]}"; do
            p=$(resolve_pid "$t")
            if [[ -z "$p" ]]; then
                echo "  ✗ couldn't find PID for '$t'" >&2
                continue
            fi
            PIDS+=("$p")
        done
        ;;
esac

# ── Make output dir if requested ──
if [[ -n "$OUTPUT" ]]; then
    mkdir -p "$OUTPUT"
fi

# ── Dump each target ──
TS=$(date +%Y%m%d_%H%M%S)
RC=0
for pid in "${PIDS[@]}"; do
    out_file=""
    if [[ -n "$OUTPUT" ]]; then
        out_file="${OUTPUT%/}/dump_${pid}_${TS}.txt"
    fi
    dump_one "$pid" "$out_file" || RC=$?
done

exit $RC
