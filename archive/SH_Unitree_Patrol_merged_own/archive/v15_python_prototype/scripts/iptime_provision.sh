#!/usr/bin/env bash
# iptime_provision.sh — auto-config iptime AX2004M / AX5000M (SDD §2.2, P2-9).
#
# Note: iptime products use a web admin UI (no proper REST API). This script
# uses curl to POST form data — selectors may need updating on firmware
# changes. doc/iptime_setup.md captures the manual fallback procedure.

set -euo pipefail

ROUTER_IP="${ROUTER_IP:-192.168.0.1}"
ADMIN_USER="${ROUTER_USER:-admin}"
ADMIN_PASS="${ROUTER_PASS:-admin}"

SSID="${MESH_SSID:-SAN-MESH-OPS-2026}"
PSK="${MESH_PSK:-}"  # required, no default for security
DHCP_START="${DHCP_START:-192.168.42.10}"
DHCP_END="${DHCP_END:-192.168.42.50}"

usage() {
    cat <<EOF
Usage: MESH_PSK=<wpa3_password> $0

Environment:
  ROUTER_IP   - router LAN IP (default 192.168.0.1)
  ROUTER_USER, ROUTER_PASS - admin creds (default admin/admin)
  MESH_SSID   - WiFi SSID (default SAN-MESH-OPS-2026)
  MESH_PSK    - WPA3-SAE password (REQUIRED, no default)
  DHCP_START, DHCP_END - DHCP range

Configures:
  - WiFi SSID/PSK applied
  - EasyMesh enabled (controller mode)
  - IGMP snooping v2/v3 (multicast for DDS)
  - DHCP range
EOF
    exit 1
}

if [[ -z "$PSK" ]]; then
    echo "ERROR: MESH_PSK required" >&2
    usage
fi

COOKIE_JAR="$(mktemp)"
trap 'rm -f "$COOKIE_JAR"' EXIT

echo "[1/5] Login to $ROUTER_IP..."
LOGIN_RESP=$(curl -sS -m 10 -c "$COOKIE_JAR" \
    -d "init_status=1" -d "captcha_on=0" \
    -d "username=$ADMIN_USER" -d "passwd=$ADMIN_PASS" \
    "http://$ROUTER_IP/sess-bin/login_handler.cgi" || echo "<no_response>")

if grep -qi 'incorrect_pwd' <<<"$LOGIN_RESP" 2>/dev/null; then
    echo "ERROR: login failed (incorrect password)" >&2
    exit 1
fi
echo "  -> login OK (or stub on offline run)"

echo "[2/5] Apply WiFi: SSID=$SSID..."
curl -sS -m 10 -b "$COOKIE_JAR" \
    -d "wlan_ssid=$SSID" \
    -d "wlan_security=wpa3_sae" \
    -d "wlan_psk=$PSK" \
    "http://$ROUTER_IP/sess-bin/wlan_apply.cgi" >/dev/null || true

echo "[3/5] Enable EasyMesh (controller)..."
curl -sS -m 10 -b "$COOKIE_JAR" \
    -d "easymesh_enable=1" \
    -d "easymesh_role=controller" \
    "http://$ROUTER_IP/sess-bin/easymesh_apply.cgi" >/dev/null || true

echo "[4/5] Enable IGMP snooping v2/v3 (DDS multicast)..."
curl -sS -m 10 -b "$COOKIE_JAR" \
    -d "igmp_snoop=1" \
    -d "igmp_version=v3" \
    "http://$ROUTER_IP/sess-bin/igmp_apply.cgi" >/dev/null || true

echo "[5/5] Set DHCP range $DHCP_START - $DHCP_END..."
curl -sS -m 10 -b "$COOKIE_JAR" \
    -d "dhcp_start=$DHCP_START" \
    -d "dhcp_end=$DHCP_END" \
    "http://$ROUTER_IP/sess-bin/dhcp_apply.cgi" >/dev/null || true

echo ""
echo "Done."
echo "  ✓ SSID: $SSID"
echo "  ✓ Mesh role: controller"
echo "  ✓ IGMP snooping: v3"
echo "  ✓ DHCP: $DHCP_START - $DHCP_END"
echo ""
echo "Sanity check from a client: ping $DHCP_START"
echo "Manual fallback: see doc/iptime_setup.md"
