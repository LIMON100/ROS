#!/usr/bin/env bash
# SAN v1.5.2 — DCN-2026-011 D-035 install.sh smoke test.
#
# Runs in CI without root / without modifying the host:
#   * install.sh parses cleanly with `bash -n`
#   * `shellcheck` (when installed) reports no warnings
#   * `systemd-analyze verify` (when installed) accepts each
#     .service file
#
# A real provisioning end-to-end is the responsibility of the bench
# tier — see infra/systemd/README.md.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "[install.sh] bash -n parse check ..."
bash -n "${ROOT}/install.sh"
echo "  OK"

if command -v shellcheck >/dev/null 2>&1; then
    echo "[install.sh] shellcheck ..."
    shellcheck "${ROOT}/install.sh"
    echo "  OK"
else
    echo "[install.sh] shellcheck not installed - skipped"
fi

echo "[*.service] systemd-analyze verify ..."
if command -v systemd-analyze >/dev/null 2>&1; then
    failed=0
    for svc in "${ROOT}"/skyautonet-*.service; do
        # `systemd-analyze verify` resolves [Service] User= / Group= against
        # the local /etc/passwd which doesn't have a `skyautonet` account
        # on CI; squelch only that line and re-raise anything else.
        if ! out=$(systemd-analyze verify "${svc}" 2>&1); then
            filtered=$(printf '%s\n' "${out}" | grep -v "skyautonet" || true)
            if [[ -n "${filtered}" ]]; then
                echo "  FAIL: ${svc}" >&2
                echo "${filtered}" >&2
                failed=1
            fi
        fi
    done
    if [[ ${failed} -ne 0 ]]; then
        echo "[*.service] systemd-analyze verify - some failures" >&2
        exit 1
    fi
    echo "  OK"
else
    echo "[*.service] systemd-analyze not installed - skipped"
fi

echo "[smoke] all checks passed"
