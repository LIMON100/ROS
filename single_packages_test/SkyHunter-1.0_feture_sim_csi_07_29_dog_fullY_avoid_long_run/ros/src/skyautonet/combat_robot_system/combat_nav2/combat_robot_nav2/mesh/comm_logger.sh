#!/bin/bash
# comm_logger.sh — 보드 로컬에서 통신량/상태를 파일로 기록(통신 끊겨도 보존해서 나중에 확인).
#   기록: eth1(mesh) rx/tx 대역폭 + peer 보드 ping(mesh 경로 지연/손실) [+ 멀티캐스트 pps(sudo시)].
#   로그 파일: ~/comm_log_<host>.csv  (홈=재부팅 후에도 보존; /tmp 아님)
#   사용(보드에서, detached):
#     nohup setsid bash <mesh>/comm_logger.sh <peer_ip> [interval_s] </dev/null >/dev/null 2>&1 &
#     예) s2 에서:  comm_logger.sh 192.168.1.14      (peer=s4)
#         s4 에서:  comm_logger.sh 192.168.1.12      (peer=s2)
#   확인:  tail -f ~/comm_log_<host>.csv   (통신 복구 후 언제든 열람)
#   종료:  pkill -f comm_logger.sh
PEER="${1:?peer IP 필요 (상대 보드)}"
INT="${2:-3}"
IF="${IF:-eth1}"
LOG="$HOME/comm_log_$(hostname).csv"
echo "# started $(date '+%F %T'), host=$(hostname), peer=$PEER, iface=$IF, interval=${INT}s" >> "$LOG"
echo "time,rx_KBps,tx_KBps,ping_avg_ms,ping_max_ms,loss_pct" >> "$LOG"
while true; do
  read r1 t1 < <(awk -v i="$IF:" '$1==i{print $2,$10}' /proc/net/dev)
  # peer ping (5회 빠르게) → mesh 경로 상태
  po=$(ping -c5 -W1 -i0.2 "$PEER" 2>/dev/null)
  loss=$(printf '%s' "$po" | grep -oE '[0-9]+% packet loss' | grep -oE '^[0-9]+')
  avg=$(printf '%s' "$po" | sed -n 's#.*= [0-9.]*/\([0-9.]*\)/\([0-9.]*\).*#\1#p')
  mx=$(printf '%s' "$po" | sed -n 's#.*= [0-9.]*/[0-9.]*/\([0-9.]*\).*#\1#p')
  sleep "$INT"
  read r2 t2 < <(awk -v i="$IF:" '$1==i{print $2,$10}' /proc/net/dev)
  rx=$(( (r2 - r1) / INT / 1024 )); tx=$(( (t2 - t1) / INT / 1024 ))
  echo "$(date '+%T'),$rx,$tx,${avg:-NA},${mx:-NA},${loss:-100}" >> "$LOG"
done
