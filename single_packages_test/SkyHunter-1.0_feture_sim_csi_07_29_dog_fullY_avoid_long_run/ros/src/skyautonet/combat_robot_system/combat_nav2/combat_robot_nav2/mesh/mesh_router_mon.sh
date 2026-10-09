#!/bin/bash
# mesh_router_mon.sh — OpenWrt mesh 라우터들의 airtime% + tx failed 증가율 실시간 모니터.
# 호스트에서 실행. mesh 포화(airtime→100%, failed 급증) 진단용.
#   사용:  bash mesh_router_mon.sh            (기본 라우터 .2 .3 .254)
#          ROUTERS=".2 .5" bash mesh_router_mon.sh
# 필요: sshpass. 라우터 root 비번 = $RPW (기본 Skyautonet1!).
RPW="${RPW:-Skyautonet1!}"
ROUTERS="${ROUTERS:-192.168.1.2 192.168.1.3 192.168.1.254}"
SSH="sshpass -p $RPW ssh -o ConnectTimeout=6 -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ControlMaster=no"

# busy%(순간) = (busy2-busy1)/(active2-active1). failed 증가량도.
snap() { # $1=router → "busy active failed"
  $SSH root@"$1" '
    M=$(iw dev 2>/dev/null | awk "/Interface/{i=\$2} /type mesh/{print i}" | head -1)
    B=$(iw dev $M survey dump 2>/dev/null | awk "/in use/{u=1} u&&/busy time/{print \$4; exit}")
    A=$(iw dev $M survey dump 2>/dev/null | awk "/in use/{u=1} u&&/active time/{print \$4; exit}")
    F=$(iw dev $M station dump 2>/dev/null | awk "/tx failed/{s+=\$3} END{print s+0}")
    echo "$B $A $F"' 2>/dev/null
}

echo "[mesh_router_mon] routers: $ROUTERS  (busy%>60 혼잡, failed/s 급증=붕괴신호)  Ctrl-C 종료"
declare -A pb pa pf
for r in $ROUTERS; do read pb[$r] pa[$r] pf[$r] < <(snap "$r"); done
while true; do
  sleep 3
  line="$(date +%H:%M:%S)  "
  for r in $ROUTERS; do
    read b a f < <(snap "$r")
    da=$(( a - ${pa[$r]:-a} )); db=$(( b - ${pb[$r]:-b} )); df=$(( f - ${pf[$r]:-f} ))
    busy=0; [ "$da" -gt 0 ] && busy=$(( db * 100 / da ))
    line+="$(echo $r|grep -oE '[0-9]+$'):busy${busy}% fail+$((df/3))/s  "
    pb[$r]=$b; pa[$r]=$a; pf[$r]=$f
  done
  echo "$line"
done
