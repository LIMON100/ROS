#!/usr/bin/env bash
# Aggregate quality gate (G16.5/G16.7) — the single command CI and developers
# both run. Bundles every enforceable gate from the coding standard so nothing
# depends on human memory. Exits non-zero on the first failing gate.
#
#   ci/run_gates.sh [build-root]
#
# build-root defaults to a space-free temp dir (sanitizer suppressions need a
# space-free path). Set SKIP_SANITIZERS=1 for a fast lint/build/test-only pass.
# If clang-format / clang-tidy are absent those gates are skipped with a notice
# (CI installs them; see .github/workflows/ci.yml).
set -euo pipefail
SRC=$(cd "$(dirname "$0")/.." && pwd)
BUILD_ROOT=${1:-$(mktemp -d /tmp/riposte-gates.XXXXXX)}
HOST_DIR=$BUILD_ROOT/host
mapfile -t SOURCES < <(cd "$SRC" && find . \( -name '*.cpp' -o -name '*.h' \) -not -path './build*')
echo "source: $SRC"
echo "builds: $BUILD_ROOT"

have() { command -v "$1" >/dev/null 2>&1; }
step() { echo; echo "==== $* ===="; }

# --- Gate 1: clang-format (G16.1) --------------------------------------------
step "clang-format --dry-run --Werror"
if have clang-format; then
    ( cd "$SRC" && clang-format --dry-run --Werror "${SOURCES[@]}" )
    echo "format OK"
else
    echo "SKIP: clang-format not installed"
fi

# --- Gate 2: host build (-Werror) + ctest (G2.4/G2.3) -------------------------
step "host build (RIPOSTE_WITH_* OFF) + ctest"
cmake -S "$SRC" -B "$HOST_DIR" -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null
cmake --build "$HOST_DIR" -j
( cd "$HOST_DIR" && ctest --output-on-failure )

# --- Gate 3: clang-tidy TU sweep (G16.2) -------------------------------------
step "clang-tidy (compile_commands TU sweep)"
if have clang-tidy; then
    TUS=$(python3 -c "import json,sys; print('\n'.join(sorted({e['file'] for e in json.load(open(sys.argv[1]))})))" \
          "$HOST_DIR/compile_commands.json")
    REPORT=$BUILD_ROOT/tidy.txt
    ERRLOG=$BUILD_ROOT/tidy-err.txt
    # A nonzero exit can mean findings (WarningsAsErrors in .clang-tidy) OR a
    # tool failure — a crash, a bad compile database, a missing header. Both
    # must fail the gate. Discarding the status with `|| true` and judging only
    # by grepping stdout meant a clang-tidy that never ran read as a clean pass
    # (review CR-09); stderr went to /dev/null, so there was not even a trace.
    # This mirrors what the CI workflow already does.
    rc=0
    echo "$TUS" | xargs -d '\n' -P "$(nproc)" -n 1 clang-tidy -p "$HOST_DIR" --quiet \
        > "$REPORT" 2> "$ERRLOG" || rc=$?
    N=$(grep -cE "(warning|error):" "$REPORT" || true)
    if [ "$N" -ne 0 ]; then
        echo "clang-tidy findings: $N"
        grep -E "(warning|error):" "$REPORT" | sed "s|$SRC/||" | sort -u
        exit 1
    fi
    if [ "$rc" -ne 0 ]; then
        echo "clang-tidy TOOL FAILURE (exit $rc) — not a clean pass:"
        sed "s|$SRC/||" "$ERRLOG" | head -40
        exit 1
    fi
    echo "tidy OK (0 findings)"
else
    echo "SKIP: clang-tidy not installed"
fi

# --- Gate 4: sanitizers (G11.3) ----------------------------------------------
if [ "${SKIP_SANITIZERS:-0}" = "1" ]; then
    step "sanitizers SKIPPED (SKIP_SANITIZERS=1)"
else
    step "sanitizers (ASan+UBSan / TSan)"
    "$SRC/test/run_sanitizers.sh" "$BUILD_ROOT/san"
fi

# --- Gate 5: vendor-path syntax (stubs; no SDK needed) -----------------------
# The Hailo/RKNN TUs compile only with the vendor SDK, which CI does not have,
# so they used to reach hardware bring-up unverified (CR-05).
step "vendor-path syntax (stubs)"
bash "$SRC/ci/vendor_syntax.sh"

# --- Optional: MAVSDK ON build (syntax/link check, G2.2) ---------------------
step "MAVSDK ON build (optional)"
if [ -n "${MAVSDK_PREFIX:-}" ] || cmake -S "$SRC" -B "$BUILD_ROOT/mavsdk-probe" \
        -DRIPOSTE_WITH_MAVSDK=ON \
        ${MAVSDK_PREFIX:+-DCMAKE_PREFIX_PATH=$MAVSDK_PREFIX} >/dev/null 2>&1; then
    cmake -S "$SRC" -B "$BUILD_ROOT/mavsdk" -DRIPOSTE_WITH_MAVSDK=ON \
        ${MAVSDK_PREFIX:+-DCMAKE_PREFIX_PATH=$MAVSDK_PREFIX} >/dev/null
    cmake --build "$BUILD_ROOT/mavsdk" -j
    echo "MAVSDK build OK"
else
    echo "SKIP: MAVSDK not found (set MAVSDK_PREFIX to enable)"
fi

echo
echo "ALL_GATES_PASS"
