#!/bin/bash
# RK3588 MIPI-CSI capture pipeline setup (board-specific TEMPLATE).
#
# On RK3588 the MIPI sensor is an I2C v4l2-subdev that reports CSI2-DPHY and binds
# to rkisp through the device-tree of_graph. On most BSPs the media pipeline is
# auto-linked and the ISP mainpath capture node (/dev/videoN) yields NV12 directly
# — in that case riposte-seeker needs NO setup, just seeker.device=/dev/videoN.
#
# Some BSPs require an explicit media-ctl link + format configuration before the
# capture node streams. This script is a TEMPLATE for that case: adjust the entity
# names/formats to your board (inspect with `media-ctl -p -d /dev/mediaN`).
#
# Usage:  MEDIA=/dev/media0 SENSOR=rs300 W=1280 H=720 ./mipi_setup.sh
set -euo pipefail

MEDIA="${MEDIA:-/dev/media0}"
W="${W:-1280}"
H="${H:-720}"
# Sensor/ISP entity names are board-specific — read them from `media-ctl -p`.
SENSOR_ENT="${SENSOR_ENT:-m00_b_rs300 4-003c}"   # <-- edit to your sensor entity
ISP_ENT="${ISP_ENT:-rkisp-isp-subdev}"
CSI_ENT="${CSI_ENT:-rockchip-csi2-dphy0}"

echo "== MIPI pipeline on ${MEDIA} (${W}x${H}) =="
command -v media-ctl >/dev/null || { echo "media-ctl not found (apt install v4l-utils)"; exit 1; }

# 1) Show the current topology (for debugging).
media-ctl -p -d "${MEDIA}" || true

# 2) Configure the sub-device formats along the sensor -> CSI -> ISP path.
#    UYVY8/NV12 etc. must match what the sensor emits (see the sensor driver).
media-ctl -d "${MEDIA}" --set-v4l2 "'${SENSOR_ENT}':0 [fmt:UYVY8_2X8/${W}x${H}]" || true
media-ctl -d "${MEDIA}" --set-v4l2 "'${CSI_ENT}':0 [fmt:UYVY8_2X8/${W}x${H}]"    || true
media-ctl -d "${MEDIA}" --set-v4l2 "'${ISP_ENT}':0 [fmt:UYVY8_2X8/${W}x${H}]"    || true
media-ctl -d "${MEDIA}" --set-v4l2 "'${ISP_ENT}':2 [fmt:YUYV8_2X8/${W}x${H}]"    || true

echo "== done. Verify the capture node yields frames:"
echo "   v4l2-ctl -d /dev/videoN --stream-mmap --stream-count=3"
