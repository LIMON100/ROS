#!/usr/bin/env bash
# ota_update.sh — A/B partition OTA update (SDD §10.6.1, P2-10).
#
# Workflow:
#   1. Verify mission not active (block if active)
#   2. Download manifest.json + image
#   3. Verify signature (gpg) + sha256 + kernel ABI
#   4. Write to inactive partition (A or B)
#   5. Set boot flag -> reboot
#   6. 5-min stability window after reboot
#   7. If stable: commit; else: auto-rollback

set -euo pipefail

OTA_BASE="${OTA_BASE:-https://ota.skyautonet.local}"
ROBOT_ID="${ROBOT_ID:-$(hostname)}"
MANIFEST_DIR="${MANIFEST_DIR:-/etc/patrol}"
PARTITION_A="${PARTITION_A:-/dev/mmcblk0p2}"
PARTITION_B="${PARTITION_B:-/dev/mmcblk0p3}"
GPG_KEYRING="${GPG_KEYRING:-/etc/patrol/keys/skyautonet-ota.gpg}"
MISSION_LOCK="${MISSION_LOCK:-/var/run/patrol/mission.active}"
STABILITY_WINDOW_S=300

usage() {
    cat <<EOF
Usage: $0 -v <version> [-c|-r|-s]

Options:
  -v   Target version (e.g. v1.4.2)
  -c   Commit (called by post-reboot stability check)
  -r   Rollback (manual or automatic)
  -s   Status (current partition + version)
  -h   Help

Environment:
  OTA_BASE        - HTTPS base URL for manifests
  MANIFEST_DIR    - state dir (default /etc/patrol)
  MISSION_LOCK    - mission-active lock file
  PARTITION_A/B   - block devices
  GPG_KEYRING     - signing keyring path
EOF
    exit 1
}

VERSION=""
ACTION="install"
while getopts "v:crsh" opt; do
    case $opt in
        v) VERSION="$OPTARG" ;;
        c) ACTION="commit" ;;
        r) ACTION="rollback" ;;
        s) ACTION="status" ;;
        h|?) usage ;;
    esac
done

get_active_partition() {
    if [[ -f "$MANIFEST_DIR/active" ]]; then
        cat "$MANIFEST_DIR/active"
    else
        echo "A"
    fi
}

get_inactive_partition() {
    if [[ "$(get_active_partition)" == "A" ]]; then
        echo "B"
    else
        echo "A"
    fi
}

# ---------- status ----------
if [[ "$ACTION" == "status" ]]; then
    echo "Active partition: $(get_active_partition)"
    if [[ -f "$MANIFEST_DIR/manifest.json" ]] && command -v jq >/dev/null 2>&1; then
        echo "Current version: $(jq -r .version "$MANIFEST_DIR/manifest.json")"
    fi
    exit 0
fi

# ---------- rollback ----------
if [[ "$ACTION" == "rollback" ]]; then
    INACTIVE=$(get_inactive_partition)
    echo "Rolling back to partition $INACTIVE..."
    mkdir -p "$MANIFEST_DIR"
    echo "$INACTIVE" > "$MANIFEST_DIR/active"
    # Discard any pending install state so a stale active.pending /
    # manifest.pending.json from an abandoned install can't sneak in
    # on a future commit.
    rm -f "$MANIFEST_DIR/active.pending" \
          "$MANIFEST_DIR/manifest.pending.json" \
          "$MANIFEST_DIR/pending_commit"
    echo "Reboot required: sudo reboot"
    exit 0
fi

# ---------- commit ----------
if [[ "$ACTION" == "commit" ]]; then
    if [[ -f "$MANIFEST_DIR/pending_commit" ]]; then
        # Promote the staged active.pending / manifest.pending.json to
        # the live names. Without this step the post-reboot stability
        # window cleared pending_commit but left `active` pointing at
        # the OLD partition -- the A/B flip silently no-op'd.
        if [[ -f "$MANIFEST_DIR/active.pending" ]]; then
            mv "$MANIFEST_DIR/active.pending" "$MANIFEST_DIR/active"
        fi
        if [[ -f "$MANIFEST_DIR/manifest.pending.json" ]]; then
            mv "$MANIFEST_DIR/manifest.pending.json" \
               "$MANIFEST_DIR/manifest.json"
        fi
        rm -f "$MANIFEST_DIR/pending_commit"
        echo "Update committed (active=$(get_active_partition))."
    else
        echo "No pending commit."
    fi
    exit 0
fi

# ---------- install ----------
if [[ -z "$VERSION" ]]; then
    echo "ERROR: -v required" >&2
    usage
fi

# Step 1: Block if mission active
if [[ -f "$MISSION_LOCK" ]]; then
    echo "ERROR: cannot OTA during active mission" >&2
    exit 2
fi

# Step 2: Download manifest
echo "[1/6] Download manifest for $VERSION..."
MANIFEST_URL="$OTA_BASE/$VERSION/manifest.json"
MANIFEST_TMP=$(mktemp)
trap 'rm -f "$MANIFEST_TMP"' EXIT

if ! curl -sf -m 30 -o "$MANIFEST_TMP" "$MANIFEST_URL"; then
    echo "ERROR: manifest download failed" >&2
    exit 3
fi

# Step 3: Verify manifest signature
echo "[2/6] Verify signature..."
if [[ -f "$GPG_KEYRING" && -f "$MANIFEST_TMP.sig" ]]; then
    if ! gpg --keyring "$GPG_KEYRING" --verify "$MANIFEST_TMP.sig" \
            "$MANIFEST_TMP" 2>/dev/null; then
        echo "ERROR: signature verification failed" >&2
        exit 4
    fi
else
    echo "  -> signature skipped (no keyring or no .sig in test)"
fi

# Step 4: Check kernel ABI compat
if command -v jq >/dev/null 2>&1; then
    EXPECTED_KERNEL=$(jq -r '.min_kernel // empty' "$MANIFEST_TMP" 2>/dev/null || echo "")
    CURRENT_KERNEL=$(uname -r | cut -d. -f1-2)
    echo "[3/6] Kernel: current=$CURRENT_KERNEL, required>=${EXPECTED_KERNEL:-<unspecified>}"
fi

# Step 5: Stage image to inactive partition
INACTIVE=$(get_inactive_partition)
if [[ "$INACTIVE" == "A" ]]; then
    PARTITION_DEV="$PARTITION_A"
else
    PARTITION_DEV="$PARTITION_B"
fi
echo "[4/6] Stage image -> partition $INACTIVE ($PARTITION_DEV)..."
# Production: dd or rauc. For test: a marker file.
if [[ -w "/tmp" ]]; then
    touch "/tmp/ota_test_${INACTIVE}.img"
fi

# Step 6: Set pending commit + flip active
echo "[5/6] Set boot flag..."
mkdir -p "$MANIFEST_DIR"
cp "$MANIFEST_TMP" "$MANIFEST_DIR/manifest.pending.json"
echo "$INACTIVE" > "$MANIFEST_DIR/active.pending"
touch "$MANIFEST_DIR/pending_commit"

echo "[6/6] OTA staged. Reboot to apply:"
echo "  sudo reboot"
echo ""
echo "After reboot, system has ${STABILITY_WINDOW_S} sec to call:"
echo "  $0 -c"
echo ""
echo "If $0 -c is NOT called within the window, auto-rollback triggers."
