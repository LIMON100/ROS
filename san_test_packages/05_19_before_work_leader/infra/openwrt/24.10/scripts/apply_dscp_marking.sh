#!/bin/sh
# DSCP marking for DDS multicast + GStreamer streams.
# Run once at boot via /etc/rc.local (idempotent: each rule replaces
# the previous one via nft `add` semantics — re-running is safe).
#
# DSCP class → SQM cake tin mapping (diffserv8):
#   EF   (46) → tin0 voice  — predictive follower-target
#   AF31 (26) → tin1 video  — robot heartbeat / status
#   AF21 (18) → tin2 video  — GStreamer SRT relay
#   BE   (0)  → tin3 best-effort

set -e

# /swarm/predict/follower_target (P0, real-time control, EF)
nft add rule inet fw4 forward udp dport 7400-7500 ip dscp set ef \
    comment '"swarm-predict EF"'

# RobotStatus / heartbeat (P1, AF31)
nft add rule inet fw4 forward udp dport 7501-7600 ip dscp set af31 \
    comment '"robot-status AF31"'

# GStreamer follower → Hub UDP (P3, AF21)
nft add rule inet fw4 forward udp dport 5000-5009 ip dscp set af21 \
    comment '"gst-follower AF21"'

# GStreamer Hub → tablet SRT (P3, AF21)
nft add rule inet fw4 forward tcp dport 8888-8897 ip dscp set af21 \
    comment '"gst-tablet AF21"'

echo "DSCP marking applied."
