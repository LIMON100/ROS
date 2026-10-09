#!/bin/bash
# mesh_netmon.sh — mesh 네트워크 부하/건강 실시간 모니터 (보드에서 실행).
# eth1(mesh) rx/tx 대역폭 + 멀티캐스트 패킷율(DDS discovery vs 기타) 표시.
# full swarm 시 mesh 포화 진단용. sudo 필요(tcpdump).
#   사용:  bash mesh_netmon.sh [iface]   (기본 eth1)   Ctrl-C 종료
IF="${1:-eth1}"
echo "[mesh_netmon] iface=$IF  (DDS discovery=239.255.0.1:7400/7401 이 많으면 멀티캐스트 flooding)"
printf "%-8s %10s %10s %8s %8s %8s\n" "time" "rx_KB/s" "tx_KB/s" "mc/s" "dds_mc/s" "other_mc/s"
prev_rx=0; prev_tx=0
while true; do
  read rx tx < <(awk -v i="$IF:" '$1==i{print $2, $10}' /proc/net/dev)
  # 1초 멀티캐스트 캡처(백그라운드), 병렬로 대역폭 계산
  mc_all=$(sudo timeout 1 tcpdump -i "$IF" -n "multicast" 2>/dev/null)
  mc=$(echo "$mc_all" | grep -c .)
  dds=$(echo "$mc_all" | grep -c "239.255.0.1")
  other=$((mc - dds))
  sleep 1
  read rx2 tx2 < <(awk -v i="$IF:" '$1==i{print $2, $10}' /proc/net/dev)
  rxk=$(( (rx2 - rx) / 2 / 1024 )); txk=$(( (tx2 - tx) / 2 / 1024 ))
  printf "%-8s %10d %10d %8d %8d %8d\n" "$(date +%H:%M:%S)" "$rxk" "$txk" "$mc" "$dds" "$other"
done
