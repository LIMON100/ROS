# Combat Robot App Developer Guide

## 1. Purpose

This document extracts only the app-developer-facing requirements from the unified app integration specification.

## 1.1 Current Test-Server Snapshot

The current tablet test-server implementation follows `combatrobotcontroller/src/robot_server_rpi.cpp`.

- The app sends commands only to the selected leader robot.
- Follower robots are not controlled directly by the app.
- Robot IDs are numeric `uint32` values.
- `1..8` map to `S1..S8`.
- The leader robot is included inside `S1..S8`; it is not a separate ninth robot.
- In the current dummy telemetry, `S1` is the leader robot.
- The leader sends `SwarmStatusPacket` to the app, containing `leader_status`, `robots[8]`, and `logs[16]`.
- `leader_status` is a leader summary block, and the same leader (`S1`) is also included in `robots[8]`.
- `StatusPacket` includes `last_tablet_command_id`, so the app can verify the last raw tablet `command_id` even when there is no state-machine feedback.
- `active_mode_id` uses contiguous internal values `0..6`.
- `command_id` remains the app-facing input value set: `0,1,2,3,6,7,8`.
- Streaming uses `stream_target_robot_id` so the tablet can request which robot video should be relayed by the leader.
- Formation is simplified to `formation_type + formation_number`.
- `formation_type`: `0=None`, `1=Recon`, `2=Protect`, `3=Assault`
- `formation_number`: `0=None`, `1..4=one of the four presets defined for the selected formation type`
- Four presets exist for each production formation type: `Recon`, `Protect`, and `Assault`.

The app controls only the selected `leader robot`. Follower robots are never controlled directly by the app.

## 2. Core Rules

- All app commands must be sent to the `leader robot` only.
- Follower robots execute only the commands relayed by the leader robot.
- Follower status shown in the app must come from the leader robot's aggregated status.
- `Recon` is the app label for move/recon behavior.
- `Protect` is the app label for surveillance behavior.
- `Assault` is the only attack-related production mode exposed in the final UI.
- `Assault Tracking` is debug-only and should be hidden in production builds.
- `Assault Manual` must not be exposed.
- `Ready`, `Moving`, `Paused`, `Reached`, and `Error` are `Mission Status` values, not product modes.
- Video streaming uses a fixed RTSP pipeline. The app only sends `Stream Start` and `Stream Stop`.
- `Return to Home` is a dedicated post-mission mode used after `Recon` or `Assault`.

## 3. Product Modes

| App Label | Meaning | Internal Mode |
| --- | --- | --- |
| Idle | Standby state | `STOP` |
| Recon | Movement and reconnaissance | `MOVE_MODE` |
| Protect General | General surveillance | `SURVEILLANCE_MODE` |
| Protect Drone | Drone surveillance | `DRONE_SURVEILLANCE_MODE` |
| Assault | Path-based mission execution | `ASSAULT_MODE` |
| Return to Home | Return to initial start position | `RTH_MODE` |
| E-Stop | Emergency stop | `EMERGENCY_STOP` |

Debug-only items:

- `Assault Tracking`: hidden or demo-build only
- `Assault Manual`: removed

## 4. Mission Status

| missionStatus | Meaning |
| --- | --- |
| `NONE` | No detailed execution state |
| `READY` | Ready to execute |
| `MOVING` | Moving or executing the mission |
| `PAUSED` | Temporarily paused |
| `REACHED` | Goal reached or mission completed |
| `ERROR` | Execution error |

Examples:

- `activeMode = Recon`, `missionStatus = Moving`
- `activeMode = Assault`, `missionStatus = Ready`
- `activeMode = Return to Home`, `missionStatus = Reached`

## 5. Leader-Follower App Model

- The app directly controls only one selected leader robot.
- The leader robot generates route, formation, and mission commands for follower robots.
- The leader robot aggregates follower state and sends it to the app.
- Follower robots must not receive direct manual driving commands from the app.
- The swarm view should be leader-centered and show route, formation, and follower status.

## 6. Device Check Requirements

1. Show robots `S1` to `S8` as map markers.
2. Mark the leader robot with a star or leader badge.
3. Let the operator select the leader robot.
4. Use the selected leader robot as the main control target.
5. Send all movement, route, mission, and return commands only to that leader robot.
6. Display follower status based on the leader robot's aggregated data.

## 7. Main Screen Requirements

The initial and main screen should show:

- `activeMode`
- `missionStatus`
- RTSP status
- leader and follower connectivity
- GPS status
- battery status
- follower mode, mission status, battery, and error state reported by the leader robot

The main menu should expose:

- Recon
- Protect
- Assault
- Return to Home
- E-Stop

Additional UI policy:

- `Return to Home` is a post-mission return feature.
- `Assault Tracking` must not appear in the default production menu.

## 8. Streaming Rules

- Streaming is fixed-pipeline only.
- The app must not change video quality, bitrate, or resolution.
- The app only controls `Stream Start` and `Stream Stop`.
- RTSP playback must connect to the predefined endpoint.

Example RTSP URLs:

- `rtsp://<robot_ip>:8554/cam0`
- `rtsp://<robot_ip>:8554/cam1`

If video is lost:

- Show `No Signal`.
- Keep mission and status UI visible if the status channel is still alive.
- Retry with `Stream Stop -> Stream Start`.

## 9. Mission Flow Notes

### 9.1 Recon

- Uses waypoint-based route input.
- The app uploads the route to the leader robot.
- The app should confirm `Mission Status = Ready` before starting.
- During execution, the app should show waypoint progress, current path, and follower formation status.

### 9.2 Protect

- The app selects either `Protect General` or `Protect Drone`.
- The app should display detection events, crosshair state, and permission request state.
- Follower protect state is displayed from leader-aggregated data.

### 9.3 Assault

- Uses waypoint-based mission paths.
- The app uploads the mission path to the leader robot.
- The app should show progress, current waypoint, remaining distance, mission status, and follower status.

### 9.4 Return to Home

- Available only when a valid home position exists.
- Used after `Recon` or `Assault`.
- The app should display `activeMode = Return to Home` and the current `Mission Status`.

## 10. Network Channels

| Channel | Direction | Protocol | Port | Purpose |
| --- | --- | --- | --- | --- |
| Command | App -> Leader Robot | TCP | `65432` | Mode changes, gimbal, stream start/stop |
| Touch | App -> Leader Robot | UDP | `65433` | Touch coordinates |
| Driving | App -> Leader Robot | UDP | `65434` | Direct drive correction |
| Status | Leader Robot -> App | TCP | `65435` | Aggregated leader and follower status |
| Path | App -> Leader Robot | TCP | `65436` | Route and mission path upload |
| RTSP | Robot -> App | RTSP | `8554` | Fixed-pipeline video |

## 11. API / Packet Definitions

### 11.1 StateCommand

| Field | Type | Description |
| --- | --- | --- |
| `command_id` | `uint8` | 0=Idle, 1=Recon, 2=Protect General, 3=Protect Drone, 6=Assault, 7=Return to Home, 8=E-Stop |
| `e_stop_command` | `uint8` | Emergency-stop flag |
| `attack_permission` | `uint8` | Permission flag |
| `pan_speed` | `int8` | Pan speed |
| `tilt_speed` | `int8` | Tilt speed |
| `zoom_command` | `int8` | Zoom delta command |
| `lateral_wind_speed` | `float32` | Optional extension field |
| `stream_command` | `uint8` | 0=None, 1=Start, 2=Stop |
| `stream_target_robot_id` | `uint32` | Robot video source requested by the tablet, 0 means leader/default |
| `drone_target_lat` | `float64` | Protect Drone target latitude |
| `drone_target_lon` | `float64` | Protect Drone target longitude |
| `drone_target_valid` | `uint8` | Target validity flag |
### 11.2 DrivingCommand

| Field | Type | Description |
| --- | --- | --- |
| `move_speed` | `int8` | `-100 ~ 100` |
| `turn_angle` | `int8` | `-100 ~ 100` |

Policy:

- Direct drive is sent only to the leader robot.
- Follower robots do not receive direct drive commands from the app.

### 11.3 TouchCoordinate

| Field | Type | Description |
| --- | --- | --- |
| `x` | `float32` | Normalized X |
| `y` | `float32` | Normalized Y |

### 11.4 Path Control Header

Path transfer uses `AssaultCommandHeader + JSON Payload`.

| Field | Type | Description |
| --- | --- | --- |
| `command` | `uint8` | 0=None, 1=Start, 2=Stop, 3=Pause, 4=Resume, 5=LoadPath |
| `num_waypoints` | `uint16` | Number of waypoints |
| `data_length` | `uint32` | JSON payload length |

### 11.5 Assault Path Payload Example

```json
{
  "missionId": "mission-001",
  "frame": "wgs84",
  "waypoints": [
    { "seq": 1, "lat": 37.5665, "lon": 126.9780 },
    { "seq": 2, "lat": 37.5667, "lon": 126.9785 }
  ]
}
```

### 11.6 Recon Route Payload Example

```json
{
  "type": "recon.route",
  "leaderRobotId": "S1",
  "routeId": "recon-route-001",
  "waypoints": [
    { "seq": 1, "lat": 37.5665, "lon": 126.9780 },
    { "seq": 2, "lat": 37.5667, "lon": 126.9785 }
  ],
  "formation": {
    "type": "none|recon|protect|assault",
    "presetNumber": 1,
    "slotCount": 4
  }
}
```

Additional path rule:

- The home position must be recorded when a route starts.
- `Return to Home` depends on the saved home position.

### 11.7 StatusPacket

| Field | Type | Description |
| --- | --- | --- |
| `rtsp_server_status` | `uint8` | RTSP state |
| `active_mode_id` | `uint8` | Current internal mode (`0..6`) |
| `last_tablet_command_id` | `uint8` | Last raw `command_id` received from the tablet |
| `estop_active` | `uint8` | Emergency stop active flag |
| `permission_request_active` | `uint8` | Permission request flag |
| `crosshair_x` | `float32` | Crosshair X |
| `crosshair_y` | `float32` | Crosshair Y |
| `current_zoom_level` | `float32` | Current zoom value |
| `mission_status` | `uint8` | Detailed mission state |
| `swarm_role` | `uint8` | 0=Standalone, 1=Leader, 2=Follower |
| `formation_type` | `uint8` | 0=None, 1=Recon, 2=Protect, 3=Assault |
| `formation_number` | `uint8` | 0=None, 1..4=one of the four presets of the selected formation type |
| `slot_index` | `uint8` | Formation slot index |
| `robot_id` | `uint32` | Current robot ID |
| `leader_robot_id` | `uint32` | Leader robot ID |
| `active_stream_robot_id` | `uint32` | Robot whose video is currently being relayed to the app |
| `nav_state` | struct | Position, heading, and speed |
| `mission_state` | struct | Waypoint progress |
| `assault_status` | struct | Assault-specific internal state |
### 11.8 RobotAggregateStatus

| Field | Type | Description |
| --- | --- | --- |
| `robot_id` | `uint32` | Robot ID, where `1..8` map to `S1..S8` |
| `role` | `uint8` | 0=Standalone, 1=Leader, 2=Follower |
| `link_status` | `uint8` | 0=Disconnected, 1=Connecting, 2=Connected |
| `comm_quality_level` | `uint8` | 0=None, 1=Poor, 2=Fair, 3=Good, 4=Excellent |
| `battery_pct` | `uint8` | Battery level |
| `active_mode_id` | `uint8` | Current internal mode |
| `mission_status` | `uint8` | Current mission status |
| `estop_active` | `uint8` | Emergency stop active flag |
| `formation_type` | `uint8` | 0=None, 1=Recon, 2=Protect, 3=Assault |
| `formation_number` | `uint8` | 0=None, 1..4=one of the four presets of the selected formation type |
| `slot_index` | `uint8` | Formation slot index |
| `error_code` | `uint8` | Robot error code |
| `status_flags` | `uint16` | Status bit flags |
| `latitude` | `float64` | Current latitude |
| `longitude` | `float64` | Current longitude |
| `heading` | `float32` | Current heading |
| `speed_mps` | `float32` | Current speed |

### 11.8.1 RobotLogEntry

| Field | Type | Description |
| --- | --- | --- |
| `robot_id` | `uint32` | Robot that generated the log |
| `timestamp_sec` | `uint32` | Unix timestamp in seconds |
| `severity` | `uint8` | 0=Info, 1=Warn, 2=Error |
| `event_code` | `uint8` | Mode change, stream start/stop, target detected, swarm update, etc. |
| `message` | `char[64]` | Short UI-ready log message |
### 11.9 SwarmStatusPacket

| Field | Type | Description |
| --- | --- | --- |
| `leader_status` | `StatusPacket` | Leader robot state summary |
| `robot_count` | `uint8` | Number of valid entries in `robots[]` |
| `robots` | `RobotAggregateStatus[8]` | Aggregated robot list for `S1..S8`, including the leader |
| `log_count` | `uint8` | Number of valid entries in `logs[]` |
| `logs` | `RobotLogEntry[16]` | Recent robot/system logs |

Status handling rules:

- The app does not receive follower status directly from each follower.
- The leader robot sends one aggregated state frame to the app.
- If a follower link is lost, the last known state should still be delivered with `link_status = 0`.

## 12. UI Mapping Tables

### 12.1 activeMode Mapping

| Value | App Label |
| --- | --- |
| `0` | Idle |
| `1` | Recon |
| `2` | Protect General |
| `3` | Protect Drone |
| `4` | Assault |
| `5` | Return to Home |
| `6` | E-Stop |

### 12.2 missionStatus Mapping

| Value | App Label |
| --- | --- |
| `0` | None |
| `1` | Ready |
| `2` | Moving |
| `3` | Paused |
| `4` | Reached |
| `5` | Error |

## 13. Button Mapping

| Button | Result |
| --- | --- |
| Recon | `activeMode = Recon` |
| Protect General | `activeMode = Protect General` |
| Protect Drone | `activeMode = Protect Drone` |
| Load Path | `activeMode = Assault`, `missionStatus = Ready` |
| Start | `missionStatus = Moving` |
| Pause | `missionStatus = Paused` |
| Resume | `missionStatus = Moving` |
| Return to Home | `activeMode = Return to Home` |
| Stream Start | Mode unchanged, stream starts |
| Stream Stop | Mode unchanged, stream stops |
| Stop | `activeMode = Idle` |
| E-Stop | `activeMode = E-Stop` |

## 14. Error and Safety Rules

### 14.1 Network Loss

- If the status channel is disconnected, show `Disconnected`.
- Lock all controls except `E-Stop`.

### 14.2 No Home Position

- Disable `Return to Home`.
- Show `Home Position Not Available`.

### 14.3 Mission Error

- Show `missionStatus = Error`.
- Guide the operator to `Stop` or mission restart preparation.

### 14.4 Swarm Safety

- If the leader connection is lost, followers enter fail-safe stop behavior.
- `E-Stop` has priority over the entire swarm.
- If follower status times out, show that follower as `Disconnected` based on leader-aggregated state.

## 15. Implementation Summary

1. Select the leader robot in Device Check.
2. Send all app commands only to the leader robot.
3. Use waypoint-based flows for Recon and Assault.
4. Enable Return to Home only after a valid mission with a stored home position.
5. Always display execution progress through `Mission Status`.
6. Treat streaming as fixed-pipeline start/stop control only.
7. Use leader-aggregated follower data for all swarm UI.
8. Keep `E-Stop` globally accessible and prioritized.





