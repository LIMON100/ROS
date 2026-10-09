# Tablet-Backend Protocol V1

## Purpose

This document captures the currently agreed protocol direction between the tablet app and the backend test/robot server.

It is intended to guide:

- backend packet and state design
- Android app integration
- future documentation updates in `app_interface_spec.md` and `app_dev_spec_en.md`

## Scope

This protocol direction applies to the leader-centric control model.

- The tablet sends commands to the leader robot.
- The leader coordinates follower robots internally.
- Follower robots are not directly mode-commanded by the tablet.

## Core Control Model

### Leader-Centric Commanding

- The tablet changes the leader robot mode.
- Follower robots do not receive independent high-level mode commands from the tablet.
- Follower behavior is derived from leader state plus swarm movement assignment.

### Robot Selection

- The tablet sends a `robot_id[]` array for the selected robots.
- Only selected robots execute the requested operation.
- Robots not included in the selection remain in `HOLD`.
- The leader robot is also part of the selectable robot set.

### Follower Mode Derivation

Follower display mode is derived from movement assignment, not from a separate per-follower mode command.

- `ESTOP` has highest priority
- `RETURN_HOME` has priority over `FOLLOW_LEADER`
- `FOLLOW_LEADER` means the follower shows the same mode as the leader
- `HOLD` means the follower displays `IDLE`

Priority order:

1. `ESTOP`
2. `RETURN_HOME`
3. `FOLLOW_LEADER`
4. `HOLD`

## Mode IDs

The backend will use the following app-facing mode IDs.

- `0`: `IDLE`
- `1`: `RECON`
- `2`: `PROTECT_GENERAL`
- `3`: `PROTECT_DRONE`
- `4`: `ASSAULT`
- `5`: `RETURN_HOME`
- `6`: `ESTOP`

Legacy `6/7/8` command compatibility is no longer required.

## E-Stop Policy

`E-Stop` is not treated as a mode replacement in the tablet UI.

- The backend keeps the existing operation mode context.
- `estop_active` is delivered as a separate overlay state.
- The app should display `E-Stop` on top of the current mode, not instead of it.

Example:

- `RECON + estop_active=1`
- `ASSAULT + estop_active=1`

## Formation and Grouping

Formation and grouping are app-driven settings.

- The tablet sends formation/grouping settings to the backend.
- The backend validates and applies them.

### Formation Model

Formation is defined as:

- `mode`
- `preset(1..4)`

Valid formation presets:

- `RECON` preset `1..4`
- `PROTECT` preset `1..4`
- `ASSAULT` preset `1..4`

### Grouping Model

Grouping is app-provided as a grouping index.

Current decision:

- grouping is sent from the app
- the backend stores and applies the grouping assignment

Open detail to finalize later:

- exact semantic table for each grouping index

## Camera and Zoom Policy

Zoom level is tied to the currently selected robot camera, not the leader only.

- The tablet requests streaming through the leader.
- The backend reports `active_stream_robot_id`.
- `current_zoom_level` must refer to the zoom level of the currently selected stream robot.
- Follower robots must also provide their own zoom level in per-robot status.

When stream target changes:

- the app should receive the selected robot's current zoom state
- zoom must not be interpreted as leader-global camera zoom

## Attack Approval Flow

Attack approval is separate from log messages.

### Important Separation

`TARGET_DETECTED` log entries are for monitoring/log display only.

They are not used as the trigger for attack approval UI.

### Approval Request Trigger

Objects that require operator approval must be sent in a dedicated approval-request state, not through `RobotLogEntry`.

### Approval Response Enum

`attack_permission` uses a 3-state enum:

- `0`: `NONE`
- `1`: `APPROVE`
- `2`: `DENY`

Meaning:

- `NONE`: no response yet / hold
- `APPROVE`: operator approved
- `DENY`: operator denied

### Approval Flow

1. Approval-required detection occurs.
2. Backend sends dedicated approval-request state to the app.
3. App shows approval UI.
4. If the operator approves, the app sends `APPROVE`.
5. If the operator denies, the app sends `DENY`.
6. If the operator holds, the app sends nothing or keeps `NONE`.

### Approval Request Scope

Approval request state is integrated at system level, not tied to a single robot card as the primary UI trigger.

This means:

- the app receives a unified approval-request state
- robot-specific context can still be included if needed
- the approval UI is not triggered from monitoring log messages

## Status Delivery Model

The backend should provide:

- leader status block
- per-robot aggregate status
- recent monitoring logs
- dedicated approval-request state

### Per-Robot Status Expectations

Each robot status entry should contain at least:

- `robot_id`
- `movement_type`
- derived `active_mode_id`
- `estop_active`
- `battery`
- `link_status`
- `comm_quality_level`
- `gps position`
- `formation mode`
- `formation preset`
- `grouping index`
- `zoom_level`

## Proposed App -> Backend Command Direction

The tablet command model should include:

- `command_id`
- `selected_robot_ids[]`
- `formation_mode`
- `formation_preset`
- `grouping_index`
- `stream_target_robot_id`
- `attack_permission`

## Proposed Backend -> App Status Direction

The backend status model should include:

- `active_mode_id`
- `estop_active`
- `active_stream_robot_id`
- `current_zoom_level`
- swarm-wide per-robot status
- recent logs
- approval-request state

## Explicit Non-Goals

The following are not part of the tablet-triggered high-level control model:

- direct per-follower high-level mode command from the app
- using `TARGET_DETECTED` log entries as approval triggers
- interpreting zoom as leader-only camera state
- replacing the current mode with `ESTOP` in the UI

## Implementation Notes

Current backend direction:

- selected robots execute commands
- unselected robots remain `HOLD`
- followers in `FOLLOW_LEADER` mirror leader mode
- followers in `RETURN_HOME` display `RETURN_HOME`
- `attack_permission` is `NONE / APPROVE / DENY`

Pending backend work:

- define dedicated approval-request status structure
- add grouping index to app/server command and status contract
- add per-robot zoom reporting for follower robots
- remove legacy assumptions still present in older test code or documentation
