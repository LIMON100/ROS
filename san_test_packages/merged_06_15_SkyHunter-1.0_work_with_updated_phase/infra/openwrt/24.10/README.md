# OpenWrt 24.10 Wi-Fi 6 mesh config

SAN-SDD-SWARM-001 v1.1 §5. Target image: OpenWrt 24.10.x on a Wi-Fi 6
router (mwlwifi or mt76 driver; ath11k-based units also work but are
less tested for the `mesh_fwding=0` + `batman_v` combination used
here).

## Layout

```
24.10/
├── etc/
│   ├── config/
│   │   ├── network         # mesh / LAN / wan / wan_lte
│   │   ├── wireless        # 5 GHz mesh radio + 2.4 GHz tablet AP
│   │   ├── batman-adv      # BATMAN V tuning
│   │   ├── mwan3           # primary / LTE failover policy
│   │   ├── firewall        # default-deny WAN, accept LAN/mesh
│   │   └── system          # hostname + NTP
│   ├── dawn/dawn.json      # AP steering (tablet only)
│   └── sqm/cake.conf       # diffserv8 cake QoS
└── scripts/
    ├── apply_dscp_marking.sh   # DDS + GStreamer → DSCP classes
    ├── join_mesh.sh            # wifi reload + wait for bat0 peers
    └── health_check.sh         # batctl + mwan3 → key=value snapshot
```

## Deploy

```bash
infra/openwrt/deploy.sh 192.168.42.1            # primary router
infra/openwrt/deploy.sh hub-comm.local --dry-run
```

`deploy.sh` SCPs the tree into `/etc/` then runs `reload_config` and
`/etc/init.d/{network,firewall,sqm}` restarts. Use `--dry-run` to
print the rsync command without executing.

## Verify

```bash
infra/openwrt/verify.sh 192.168.42.1
```

Expected output (all `OK`):

```
batctl_originators OK 5 peers
mwan3_wan OK online
mwan3_wan_lte OK online (standby)
dscp_marking OK 4 rules present
sqm_cake OK qdisc cake on eth1
```

## DSCP class map

| Topic              | Port range | DSCP class | SQM cake tin |
|--------------------|-----------:|------------|--------------|
| follower_target P0 | 7400–7500  | EF (46)    | tin0 voice   |
| robot_status P1    | 7501–7600  | AF31 (26)  | tin1 video   |
| gst follower UDP   | 5000–5009  | AF21 (18)  | tin2 video   |
| gst tablet SRT     | 8888–8897  | AF21 (18)  | tin2 video   |

The KPP-3 (≤150 ms swarm control latency) gate depends on EF/AF31
markings reaching the upstream APs. If the mesh has a non-OpenWrt
hop, that hop must preserve DSCP (most enterprise APs do; consumer
hardware often strips it).

## Secrets

`wireless.mesh_5g.key` and `dawn.network.shared_key` are
`CHANGE_ME_PROD_KEY` placeholders. Production deploy MUST replace
them before applying — `deploy.sh` refuses to push a tree that still
contains the placeholder. Store the real keys in
`infra/openwrt/secrets/` (gitignored) and merge via envsubst at
deploy time.

## Simulation (§6.4)

Gazebo network-namespace mesh simulation lives in
`sim/docker-compose.gazebo.yml`; it does NOT consume this OpenWrt
tree (the simulated mesh uses `netem` on virtual interfaces rather
than real radios). The DSCP class table is the only piece that
crosses the sim/prod boundary — `apply_dscp_marking.sh` is mirrored
in the sim's iptables setup.
