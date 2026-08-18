#!/usr/bin/env bash
# ============================
# Wi-Fi AP 모드 강제 전환 스크립트
# ============================
# WARNING: 이 스크립트는 wlan0 의 station 연결을 끊고 AP 모드로 전환합니다.
#          WiFi SSH 세션으로 실행하면 본인 세션이 끊깁니다.
#          반드시 HDMI / serial / Ethernet 로 접속한 상태에서 실행하세요.
#
# 설계 메모:
#   - 더 이상 `set -e` 를 쓰지 않습니다. 한 단계가 실패해도 다음 단계를
#     진단할 수 있도록 단계별로 결과를 출력합니다 (실패해도 계속 진행).
#   - 마지막에 단계별 OK / FAIL 요약을 찍어서, 어떤 게 막혔는지 한눈에
#     보이게 합니다.

WLAN_IFACE="${WLAN_IFACE:-}"
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
# AP_LAUNCH_SCRIPT 우선순위:
#   1) 환경변수로 직접 지정
#   2) Firefly stock BSP 가 깔린 경우 /userdata/config/start_ap.sh
#   3) 로컬 scripts/start_ap.sh (hostapd+dnsmasq 자체 구현)
if [ -z "${AP_LAUNCH_SCRIPT:-}" ]; then
  if [ -x "/userdata/config/start_ap.sh" ]; then
    AP_LAUNCH_SCRIPT="/userdata/config/start_ap.sh"
  else
    AP_LAUNCH_SCRIPT="${SCRIPT_DIR}/start_ap.sh"
  fi
fi

# 단계별 결과 누적
declare -A RESULT

# 0) WLAN 인터페이스 자동 감지 ------------------------------------------
if [ -z "${WLAN_IFACE}" ]; then
  WLAN_IFACE=$(iw dev 2>/dev/null | awk '$1=="Interface"{print $2; exit}')
fi
if [ -z "${WLAN_IFACE}" ]; then
  # 폴백: ip link 에서 wl 로 시작하는 인터페이스 찾기
  WLAN_IFACE=$(ip -o link show 2>/dev/null | awk -F': ' '/: wl/{print $2; exit}')
fi
if [ -z "${WLAN_IFACE}" ]; then
  echo "[FAIL] WLAN 인터페이스를 찾지 못했습니다."
  echo "       'iw dev' 또는 'ip link' 출력을 확인하고"
  echo "       WLAN_IFACE=<name> ./scripts/start_ap_mode.sh 로 명시해주세요."
  exit 1
fi
echo "[INFO] WLAN 인터페이스 = ${WLAN_IFACE}"

# 1) NetworkManager 중지 ------------------------------------------------
echo "[STEP 1/5] NetworkManager 중지..."
if systemctl list-unit-files 2>/dev/null | grep -qE '^NetworkManager\.service'; then
  if sudo systemctl stop NetworkManager 2>&1; then
    RESULT[NetworkManager]="OK"
  else
    RESULT[NetworkManager]="FAIL (systemctl stop 실패)"
  fi
else
  RESULT[NetworkManager]="SKIP (NetworkManager 미설치 — 다른 네트워크 매니저일 가능성)"
fi
echo "         ${RESULT[NetworkManager]}"

# 2) wpa_supplicant 종료 (계속 살아있으면 station 으로 재연결 시도) ----
echo "[STEP 2/5] wpa_supplicant 종료..."
if pgrep -af wpa_supplicant >/dev/null 2>&1; then
  sudo pkill -f wpa_supplicant
  sleep 1
  if pgrep -af wpa_supplicant >/dev/null 2>&1; then
    RESULT[wpa_supplicant]="FAIL (여전히 실행 중)"
  else
    RESULT[wpa_supplicant]="OK"
  fi
else
  RESULT[wpa_supplicant]="SKIP (실행 중이지 않음)"
fi
echo "         ${RESULT[wpa_supplicant]}"

# 3) WLAN IP 주소 초기화 ------------------------------------------------
echo "[STEP 3/5] ${WLAN_IFACE} IP 주소 초기화..."
if sudo ip addr flush dev "${WLAN_IFACE}" 2>&1; then
  RESULT[ip_flush]="OK"
else
  RESULT[ip_flush]="FAIL"
fi
echo "         ${RESULT[ip_flush]}"

# 4) WLAN 인터페이스 up -------------------------------------------------
echo "[STEP 4/5] ${WLAN_IFACE} 인터페이스 활성화..."
if sudo ip link set "${WLAN_IFACE}" up 2>&1; then
  RESULT[link_up]="OK"
else
  RESULT[link_up]="FAIL"
fi
echo "         ${RESULT[link_up]}"

# 5) Firefly BSP AP 진입 스크립트 호출 ----------------------------------
echo "[STEP 5/5] AP 모드 시작 (${AP_LAUNCH_SCRIPT})..."
if [ ! -x "${AP_LAUNCH_SCRIPT}" ]; then
  if [ -f "${AP_LAUNCH_SCRIPT}" ]; then
    RESULT[start_ap]="FAIL (${AP_LAUNCH_SCRIPT} 실행 권한 없음 — chmod +x 필요)"
  else
    RESULT[start_ap]="FAIL (${AP_LAUNCH_SCRIPT} 없음 — Firefly BSP 스크립트 경로 확인 필요)"
  fi
else
  if sudo "${AP_LAUNCH_SCRIPT}"; then
    RESULT[start_ap]="OK"
  else
    RESULT[start_ap]="FAIL (스크립트 종료 코드 != 0)"
  fi
fi
echo "         ${RESULT[start_ap]}"

# 요약 ------------------------------------------------------------------
echo ""
echo "================ 결과 요약 ================"
for key in NetworkManager wpa_supplicant ip_flush link_up start_ap; do
  printf "  %-16s %s\n" "${key}" "${RESULT[${key}]}"
done
echo "==========================================="

case "${RESULT[start_ap]}" in
  OK) echo "[SUCCESS] AP 모드로 전환 완료!" ;;
  *)  echo "[FAIL] AP 전환 실패. 위 요약의 FAIL/SKIP 항목을 확인하세요." ; exit 1 ;;
esac
