#!/bin/sh
# Join the san-mesh-001 802.11s mesh.
# Usage: ./join_mesh.sh [mesh_id]  — defaults to san-mesh-001.
# Reads the SAE passphrase from /etc/san-mesh.key (must exist, 0600).

set -e

MESH_ID="${1:-san-mesh-001}"
KEY_FILE="/etc/san-mesh.key"

if [ ! -r "$KEY_FILE" ]; then
    echo "missing or unreadable $KEY_FILE — refusing to bring up mesh" >&2
    exit 1
fi

KEY="$(cat "$KEY_FILE")"

uci set wireless.mesh_5g.mesh_id="$MESH_ID"
uci set wireless.mesh_5g.key="$KEY"
uci set wireless.mesh_5g.disabled='0'
uci commit wireless

wifi reload
sleep 2

# Wait up to 15 s for bat0 to come up + see at least one peer.
i=0
while [ "$i" -lt 30 ]; do
    PEERS=$(batctl o -H 2>/dev/null | wc -l)
    if [ "$PEERS" -gt 0 ]; then
        echo "joined $MESH_ID, $PEERS peers visible"
        exit 0
    fi
    sleep 0.5
    i=$((i + 1))
done

echo "joined $MESH_ID but no peers visible after 15 s — orphaned?" >&2
exit 2
