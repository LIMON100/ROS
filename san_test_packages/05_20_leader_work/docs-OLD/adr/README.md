# Architecture Decision Records (v1.1)

This directory captures the non-obvious architectural choices made during the
v1.1 rollout. Each record follows the standard **Context / Decision /
Consequences** template; supersession is tracked in the `Status` line at the
top of each file.

| # | Title | Status |
|---|---|---|
| [ADR-001](ADR-001-hub-dual-sbc.md) | Hub UGV dual-SBC architecture | Accepted (v1.1) |
| [ADR-002](ADR-002-slam-aggregation-period.md) | SLAM aggregation period 30–60s | Accepted (v1.1) |
| [ADR-003](ADR-003-gstreamer-srt-vs-webrtc.md) | GStreamer SRT for video (vs WebRTC/RTSP) | Accepted (v1.1) |
| [ADR-004](ADR-004-openwrt-vs-stock-firmware.md) | OpenWrt 24.10 over vendor stock firmware | Accepted (v1.1) |
| [ADR-005](ADR-005-robosense-e1-vs-mid360.md) | Robosense E1 over Livox Mid360 (follower LiDAR) | Accepted (v1.1) |

New ADRs are numbered sequentially. Once an ADR is **Accepted**, it should be
amended only via a follow-up ADR that supersedes it — do not rewrite history.
