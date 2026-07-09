#!/bin/sh
# Deploy the OpenWrt 24.10 tree to a router via SSH+rsync.
# Usage: deploy.sh <router_host> [--dry-run]
#
# Refuses to push if the working tree still contains the
# CHANGE_ME_PROD_KEY placeholders.

set -eu

if [ $# -lt 1 ]; then
    echo "usage: $0 <router_host> [--dry-run]" >&2
    exit 2
fi

ROUTER="$1"
DRY=""
if [ "${2-}" = "--dry-run" ]; then
    DRY="--dry-run -v"
fi

CONFIG_DIR="$(dirname "$0")/24.10"
if [ ! -d "$CONFIG_DIR" ]; then
    echo "config tree not found at $CONFIG_DIR" >&2
    exit 1
fi

# Guard: never push a tree with the placeholder secrets.
if grep -RIn 'CHANGE_ME_PROD' "$CONFIG_DIR" >/dev/null; then
    echo "refusing to deploy — CHANGE_ME_PROD placeholders still present" >&2
    echo "  (replace via envsubst from infra/openwrt/secrets/)" >&2
    exit 3
fi

# shellcheck disable=SC2086
rsync $DRY -av --rsh=ssh \
    "$CONFIG_DIR/etc/"     "root@${ROUTER}:/etc/"
# shellcheck disable=SC2086
rsync $DRY -av --rsh=ssh \
    "$CONFIG_DIR/scripts/" "root@${ROUTER}:/usr/local/sbin/"

if [ -z "$DRY" ]; then
    ssh "root@${ROUTER}" '
        uci commit
        /etc/init.d/network reload
        /etc/init.d/firewall reload
        /etc/init.d/sqm reload || true
        /usr/local/sbin/apply_dscp_marking.sh
    '
fi
echo "deploy to $ROUTER complete."
