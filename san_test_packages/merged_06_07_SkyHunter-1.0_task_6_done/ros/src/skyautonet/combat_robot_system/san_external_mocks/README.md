# san_external_mocks

External vendor mock services (Tier 3 per [[ADR-008]]).

## Purpose

Lets external vendors (currently Aban Co. for the Android operator app)
develop their integrations against the SkyHunter v1.5.3 rosbridge schema
**without robot hardware**. The mock publishes synthetic data on every
topic Aban's app subscribes to, so UI can be built/iterated in parallel
with backend implementation.

## Spec source of truth

```
docs/external/Aban_Android_rosbridge_schema_v2.md
```

The mock implements that document. If you change a topic name / type /
rate in the spec, update the mock AND the spec; the gtest
(`test_schema_validator`) catches drift between them.

## Quick start (Aban developer)

```bash
# 1. Source ROS 2 Humble + workspace
source /opt/ros/humble/setup.bash
source install/setup.bash

# 2. Launch mock + rosbridge
ros2 launch san_external_mocks mock_server.launch.xml

# 3. From the Android app, connect to:
#   ws://localhost:9091     (dev box)
#   ws://192.168.50.20:9091 (Hub UGV SBC #1 in tactical field — production)
```

## Topics published by this mock

10 topics, all listed in the v2 schema. The mock publishes synthetic
data at the documented rates. See the spec doc for the **schema**
(type, fields, frequency, QoS) and **production status** (ready /
v1.5.3 future) per topic.

## CI

```bash
colcon test --packages-select san_external_mocks --ctest-args -R test_schema_validator
# Expected: 6/6 PASS
```

The validator checks structural consistency between spec, mock source,
and launch file. Runs in milliseconds — pure-logic.

## Refs

- DCN-2026-021 — this DCN
- [[ADR-008]] — Tier 3 (test/tool, C++ acceptable)
- DCN-2026-014 v2 — rosbridge port allocation = 9090 + robot_id
- DCN-2026-008 v2 D-WIFI-002 — operator link is WiFi (rosbridge_server) only
