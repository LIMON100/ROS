#!/bin/sh
# Post-deploy verification — SSHs into the router and checks each
# subsystem the spec requires. Prints `<check> OK <detail>` or
# `<check> FAIL <reason>`; exits 0 iff every check is OK.

set -u

if [ $# -lt 1 ]; then
    echo "usage: $0 <router_host>" >&2
    exit 2
fi

ROUTER="$1"
FAIL=0

check() {
    NAME="$1"
    OUT="$2"
    OK="$3"     # 0 = OK, anything else = FAIL
    DETAIL="${4-}"
    if [ "$OK" -eq 0 ]; then
        echo "$NAME OK $DETAIL"
    else
        echo "$NAME FAIL $DETAIL"
        FAIL=$((FAIL + 1))
    fi
}

# 1. batman-adv mesh peers
PEERS=$(ssh "root@${ROUTER}" 'batctl o -H 2>/dev/null | wc -l' || echo 0)
if [ "$PEERS" -gt 0 ]; then
    check "batctl_originators" "" 0 "$PEERS peers"
else
    check "batctl_originators" "" 1 "no peers visible"
fi

# 2. mwan3 primary
WAN=$(ssh "root@${ROUTER}" \
    'mwan3 status 2>/dev/null | awk "/interface wan / {print \$4}"' \
    || echo unknown)
if [ "$WAN" = "online" ]; then
    check "mwan3_wan" "" 0 "online"
else
    check "mwan3_wan" "" 1 "$WAN"
fi

# 3. mwan3 LTE fallback
LTE=$(ssh "root@${ROUTER}" \
    'mwan3 status 2>/dev/null | awk "/interface wan_lte / {print \$4}"' \
    || echo unknown)
if [ "$LTE" = "online" ]; then
    check "mwan3_wan_lte" "" 0 "online (standby)"
else
    check "mwan3_wan_lte" "" 0 "$LTE (acceptable if no SIM)"
fi

# 4. DSCP marking rules present
RULES=$(ssh "root@${ROUTER}" \
    'nft list ruleset 2>/dev/null | grep -c "ip dscp set"' \
    || echo 0)
if [ "$RULES" -ge 4 ]; then
    check "dscp_marking" "" 0 "$RULES rules present"
else
    check "dscp_marking" "" 1 "only $RULES rules (need 4)"
fi

# 5. SQM cake on the WAN device
WAN_DEV=$(ssh "root@${ROUTER}" 'uci get network.wan.device 2>/dev/null' \
    || echo eth1)
if ssh "root@${ROUTER}" \
        "tc qdisc show dev $WAN_DEV 2>/dev/null | grep -q '^qdisc cake'"
then
    check "sqm_cake" "" 0 "qdisc cake on $WAN_DEV"
else
    check "sqm_cake" "" 1 "not active on $WAN_DEV"
fi

exit $FAIL
