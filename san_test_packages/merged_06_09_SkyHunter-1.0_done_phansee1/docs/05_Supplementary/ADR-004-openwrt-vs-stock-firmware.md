# ADR-004 — OpenWrt 24.10 over vendor stock firmware

- **Status**: Accepted (v1.1, 2026-05-11)
- **Spec**: SAN-SDD-CMD-001 v1.1 §6.4 (mesh/WAN)
- **Implementation**: `infra/openwrt/24.10/`, `comm/mesh_monitor.py`,
  `tests/test_mesh_monitor.py`

## Context

v1.0 used the Wi-Fi router's stock firmware for mesh. That firmware:

- Exposed only a vendor-specific web UI; no programmatic control.
- Did not expose batman-adv link-quality metrics — we could see "connected"
  / "disconnected" but not the underlying link cost.
- Did not support DAWN (decentralized Wi-Fi roaming) — followers stuck to
  the first AP they associated with and did not re-balance.
- Had no SQM / cake configuration — bufferbloat under SRT video load was
  observable as latency spikes when the mesh backhaul saturated.
- Was not auditable for security — closed binary blobs, no community CVE
  tracking.

PHASE 6 needed all of: programmable mesh config, DAWN roaming, SQM,
peer-level link-quality polling, and a way to script `batctl` and `mwan3`
status into the existing safety/comm layer.

## Decision

Standardize on **OpenWrt 24.10** with the following package stack:

- **batman-adv** — layer-2 mesh routing across all robot nodes + the Hub.
- **mwan3** — multi-WAN with priority: Wi-Fi mesh primary, LTE secondary.
- **DAWN** — 802.11k/v/r roaming hints; followers re-associate as they move
  between APs without dropping the mesh.
- **SQM (cake)** — per-interface fair queuing, kills bufferbloat under SRT
  video saturation.
- A `MeshMonitor` Python adapter polls `batctl meshif bat0 originators` and
  `mwan3 status` via an injectable `cmd_runner` (so the unit tests can
  fake the shell layer).

Supported boards in `infra/openwrt/24.10/`:

- **GL.iNet Flint 2 (GL-MT6000)** — PoC. Off-the-shelf, internal antennas,
  OpenWrt 24.10 stock-flashable.
- **Compex WPJ563** — production target. IPQ6000, external antennas, IP67
  enclosure compatible.

## Consequences

**Positive**

- Mesh peer health, link quality, and WAN failover state are now visible to
  the swarm's safety/comm layer via `MeshStatus` (`comm/mesh_monitor.py`).
- DAWN measurably reduces follower handoff time during area_sweep missions
  (sub-200ms re-association vs ~3s with stock firmware's sticky-AP behavior).
- SQM cake eliminates the SRT video latency spikes that v1.0 saw whenever
  the mesh backhaul approached saturation.
- Common build toolchain — both PoC and production boards share the same
  config tree, so feature flags don't fork.

**Negative**

- We own the firmware build. Vendor security updates no longer arrive
  automatically; we subscribe to OpenWrt 24.10 stable backports and
  re-flash on a quarterly cadence.
- Initial bring-up requires a TFTP/recovery flash on the WPJ563 — there is
  no factory OpenWrt option. Document is in `infra/openwrt/24.10/README.md`.
- Loss of vendor support / RMA path on the Flint 2 if the unit is reflashed.
  This is acceptable for PoC stock.

**Out of scope**

- 5G modem integration — LTE is the v1.1 secondary WAN. A 5G upgrade is a
  v1.2 candidate and will be its own ADR.
