#!/bin/sh
# Router health snapshot — printed in a stable key=value format the
# Hub's MeshMonitor parses. Exit code = number of degraded subsystems
# (0 = all healthy). Designed to be invoked via SSH from the Hub.

set -u

DEGRADED=0

# 1. batman-adv mesh — at least one peer reachable.
PEERS=$(batctl o -H 2>/dev/null | wc -l)
echo "mesh_peers=$PEERS"
if [ "$PEERS" -lt 1 ]; then
    DEGRADED=$((DEGRADED + 1))
fi

# 2. mwan3 — primary WAN tracking status.
WAN=$(mwan3 status 2>/dev/null \
        | awk '/^[[:space:]]*interface wan / {print $4}')
echo "wan_status=${WAN:-unknown}"
if [ "$WAN" != "online" ]; then
    DEGRADED=$((DEGRADED + 1))
fi

# 3. LTE fallback.
LTE=$(mwan3 status 2>/dev/null \
        | awk '/^[[:space:]]*interface wan_lte / {print $4}')
echo "wan_lte_status=${LTE:-unknown}"

# 4. SQM cake on WAN.
if tc qdisc show dev "$(uci get network.wan.device 2>/dev/null \
                            || echo eth1)" 2>/dev/null \
        | grep -q '^qdisc cake'; then
    echo "sqm_cake=on"
else
    echo "sqm_cake=off"
    DEGRADED=$((DEGRADED + 1))
fi

echo "degraded_subsystems=$DEGRADED"
exit $DEGRADED
