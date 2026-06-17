# iptime Mesh Router Setup (Manual)

If `scripts/iptime_provision.sh` fails (e.g. firmware update changed the form
selectors), follow this manual procedure.

## Models

- iptime AX2004M (Wi-Fi 6, dual-band)
- iptime AX5000M (Wi-Fi 6, tri-band)

## Setup Steps

1. **Connect**: PC → router LAN port via cable
2. **Browser**: http://192.168.0.1
3. **Login**: admin / admin (change immediately after first login)
4. **Wireless → Basic**:
   - SSID: `SAN-MESH-OPS-2026`
   - Security: WPA3-SAE (or WPA2-PSK fallback)
   - Password: see secure note
   - Channel: Auto (5 GHz preferred for ops)
5. **Mesh → EasyMesh**:
   - Enable: ✓
   - Role: Controller (first router) or Agent (additional)
6. **LAN → IGMP Snooping**:
   - Enable: ✓
   - Version: v2/v3
7. **DHCP Server**:
   - Range: 192.168.42.10 – 192.168.42.50
   - Lease: 86400 s
8. **System → Settings**:
   - Change admin password
   - Enable remote management (only for trusted IPs)
9. **Verify**:
   - Connect a robot via Wi-Fi
   - Verify DHCP lease assigned in 192.168.42.x range
   - Test multicast: `iperf -c 224.0.0.1 -u`

## Troubleshooting

- **DDS Liveliness intermittent** — IGMP snooping likely disabled
- **2.4 GHz interference** — switch to 5 GHz only
- **WPA3 unsupported by Android < 10** — fallback to WPA2-PSK
- **Mesh agent won't pair** — confirm controller is online and on the same
  band; reboot agent
