#!/bin/bash
# netprobe.sh <라벨> — mesh 부하 측정: 보드/라우터 RTT·손실 + 보드 CPU load(1분)
LABEL="${1:-probe}"
echo "──── [netprobe:$LABEL] $(date +%H:%M:%S) ────"
for ip in 12 13 14 254; do
  R=$(ping -c8 -W1 -i 0.15 192.168.1.$ip 2>/dev/null | tail -2)
  LOSS=$(echo "$R" | grep -oE "[0-9]+% packet loss" | head -1)
  RTT=$(echo "$R" | grep -oE "= [0-9.]+/[0-9.]+/[0-9.]+" | cut -d/ -f2)
  printf "  .%-3s loss=%-4s rtt_avg=%sms\n" "$ip" "${LOSS:-?}" "${RTT:-DOWN}"
done
for s in 2 3 4; do
  L=$(timeout 8 ssh -o ControlPath=none -o ConnectTimeout=5 mesh_s$s "uptime | grep -oE 'average: [0-9.]+' | grep -oE '[0-9.]+'" 2>/dev/null)
  echo "  s$s load1m=${L:-N/A}"
done
