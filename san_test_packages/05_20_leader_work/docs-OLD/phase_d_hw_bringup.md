# Phase D RK3588J HW Bring-up

본 문서는 Phase D field trial을 위한 RK3588J 보드 설정 절차다.

## Prerequisites

- RK3588J SoM + carrier board
- Ubuntu 22.04 LTS for ARM64 (FriendlyElec or vendor image)
- 32 GB eMMC + 64 GB SD card (rootfs + data)
- IMX678 camera (or AHD per P2-4 for Lab dev)
- Mid-360 LiDAR (Ethernet) or LD-19 (USB)
- u-blox F9P RTK GNSS module
- iptime AX2004M router (per P2-9)

## 1. OS Image Setup

```bash
# Flash Ubuntu 22.04 ARM64 to eMMC (vendor tool)
sudo dd if=ubuntu-22.04-arm64.img of=/dev/mmcblk0 bs=4M status=progress

# Boot and set hostname per robot
sudo hostnamectl set-hostname robot1   # robot2, robot3, ...

# /etc/hosts
192.168.42.10 robot1
192.168.42.11 robot2
192.168.42.12 robot3   # hub_ugv
```

## 2. ROS 2 Humble + CycloneDDS

```bash
sudo apt update && sudo apt install -y \
    ros-humble-ros-base \
    ros-humble-rmw-cyclonedds-cpp \
    linuxptp chrony

echo "source /opt/ros/humble/setup.bash"           >> ~/.bashrc
echo "export ROS_DOMAIN_ID=42"                     >> ~/.bashrc
echo "export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp" >> ~/.bashrc
```

## 3. PTP Time Sync (P1-15)

Enable PTP master on Hub UGV (best clock):

```bash
# Hub UGV
sudo systemctl enable --now ptp4l
sudo systemctl enable --now phc2sys

# Followers (slave-only)
sudo bash -c 'cat > /etc/ptp4l.conf' <<'EOF'
[global]
slaveOnly 1
network_transport L2
delay_mechanism E2E
EOF
sudo systemctl restart ptp4l

# Verify
chronyc tracking | grep "Last offset"
# Expected: < 1 ms
```

## 4. Calibration (P2-12)

Run all 4 calibrations per `doc/calibration_procedure.md`:

```bash
sudo python3 scripts/calibrate_imu.py
sudo python3 scripts/calibrate_camera.py --images-dir /tmp/calib_images/
sudo python3 scripts/calibrate_lidar_imu.py --bag-file /tmp/calib.bag
sudo python3 scripts/calibrate_rtk_base.py --duration 3600  # 1h survey-in
```

Verify all yaml files in `/etc/patrol/calibration/`.

## 5. Mission Brief (P2-7)

For each mission area:

```bash
sudo MESH_PSK="<wpa3_password>" bash scripts/iptime_provision.sh

bash scripts/mission_brief.sh \
    -m M_$(date +%Y%m%d)_test_a \
    -b "37.4,127.0,37.5,127.1"
```

This downloads OSM PBF + SRTM tiles + rasterizes + rsyncs to all robots.

## 6. Patrol Stack Service

```bash
sudo cp scripts/patrol.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable patrol.service

sudo systemctl start patrol.service
sudo journalctl -u patrol -f
```

Expected: `FSM=STREAMING` and the 17-process tree alive.

## 7. Health Verification

```bash
# WS telemetry alive
curl http://localhost:5001/health

# DDS topics
ros2 topic list | grep -E "leader_pose|follower_target|telemetry"

# Aggregate health
ros2 topic echo /sw/health --once
# overall: NORMAL expected
```

## 8. Field Trial Checklist

- [ ] All 5 robots powered + booted to STREAMING
- [ ] PTP offset < 1 ms across all robots
- [ ] All 4 calibrations valid (yaml files exist + plausible)
- [ ] Geofence config loaded for trial area
- [ ] Mission brief downloaded for trial location
- [ ] Battery > 60 % all robots
- [ ] WiFi6 mesh ping < 5 ms between all robots
- [ ] LTE backhaul tested (Hub UGV)
- [ ] Operator Android app paired with all robots
- [ ] Audit log collection verified (P1-16)
- [ ] OTA update path tested (P2-10)

## 9. Trial Scenarios (UC-1 ~ UC-11)

```bash
# UC-1 Boot + pairing -- naturally tested during step 6
# UC-2 Mission start (M1 V-shape) -- via Android app

# UC-6 Leader reconfiguration test
sim/scripts/inject_failure.sh leader_kill robot1
# Expected: < 10 s (KPP-4)

# UC-11 Leader rollback
sim/scripts/inject_failure.sh follower_drop robot3
sim/scripts/inject_failure.sh follower_drop robot4
# Expected: rollback initiated, retreat 30 s, replan
```

## 10. Post-trial Audit

```bash
for r in robot1 robot2 robot3 robot4 robot5; do
    rsync -avz "$r:/var/log/patrol/audit/" "/tmp/trial_audit/$r/"
done

python3 - <<'PYEOF'
from pathlib import Path
from core.audit_log import AuditLogger
for f in Path("/tmp/trial_audit").rglob("*.jsonl"):
    a = AuditLogger(log_dir=str(f.parent), robot_id="audit_check")
    valid, bad = a.verify_chain(f)
    status = "OK" if valid else f"FAIL@{bad}"
    print(f"{f.parent.name}: {status}")
PYEOF
```

## Troubleshooting

| Issue | Cause | Fix |
|---|---|---|
| FSM stuck BLE_ADV | WiFi mesh not reachable | Check iptime config + DHCP |
| PTP offset > 5 ms | GNSS PPS not locked | Verify u-blox antenna, sky view |
| RTK FLOAT only | NTRIP creds expired | Renew NTRIP subscription |
| Anomaly false positives | OSM stale | Run hybrid update (P2-5) |
| KPP-3 > 150 ms | WiFi6 channel congested | Switch to less crowded 5 GHz |
| Audit chain breaks | Disk full | Rotate logs more aggressively |
