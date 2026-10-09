#!/usr/bin/env bash
# hostapd + dnsmasq 기반 자체 AP 진입 스크립트.
# start_ap_mode.sh 의 step 5 에서 호출되거나 단독 실행 가능합니다.
#
# 설정 override 우선순위:
#   1) 환경변수로 직접 전달 (AP_SSID=… ./scripts/start_ap.sh)
#   2) scripts/ap_config.env  (gitignored)
#   3) 아래 기본값
#
# 의존성: hostapd, dnsmasq, iw, iproute2
#   미설치 시: sudo apt install -y hostapd dnsmasq
#
# 동작:
#   - hostapd 설정과 dnsmasq 설정을 /run/combatrobot_ap/ 아래 임시 파일로
#     생성 (재부팅 시 자동 초기화)
#   - 기존 hostapd/dnsmasq 인스턴스 종료
#   - 인터페이스에 정적 IP 부여
#   - hostapd 와 dnsmasq 를 백그라운드 daemon 으로 기동

set -o pipefail

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

# 1) 설정 로딩 ---------------------------------------------------------
CONFIG_FILE="${SCRIPT_DIR}/ap_config.env"
if [ -f "${CONFIG_FILE}" ]; then
  # shellcheck disable=SC1090
  source "${CONFIG_FILE}"
  echo "[INFO] loaded config: ${CONFIG_FILE}"
fi

AP_SSID="${AP_SSID:-CombatRobot-1}"
AP_PASSPHRASE="${AP_PASSPHRASE:-change-me-12345678}"
AP_CHANNEL="${AP_CHANNEL:-6}"
AP_COUNTRY="${AP_COUNTRY:-KR}"
AP_IP="${AP_IP:-192.168.50.1}"
AP_DHCP_START="${AP_DHCP_START:-192.168.50.10}"
AP_DHCP_END="${AP_DHCP_END:-192.168.50.50}"
WLAN_IFACE="${WLAN_IFACE:-}"

# WPA2-PSK 패스프레이즈 길이 검증 (8-63 chars).
ap_pass_len=${#AP_PASSPHRASE}
if [ "${ap_pass_len}" -lt 8 ] || [ "${ap_pass_len}" -gt 63 ]; then
  echo "[FAIL] AP_PASSPHRASE 는 8-63 자여야 합니다 (현재 ${ap_pass_len}자)."
  exit 1
fi
if [ "${AP_PASSPHRASE}" = "change-me-12345678" ]; then
  echo "[WARN] AP_PASSPHRASE 가 기본값입니다 — 운영 전에 ap_config.env 로 변경하세요."
fi

# 2) WLAN 인터페이스 감지 -----------------------------------------------
if [ -z "${WLAN_IFACE}" ]; then
  WLAN_IFACE=$(iw dev 2>/dev/null | awk '$1=="Interface"{print $2; exit}')
fi
if [ -z "${WLAN_IFACE}" ]; then
  WLAN_IFACE=$(ip -o link show 2>/dev/null | awk -F': ' '/: wl/{print $2; exit}')
fi
if [ -z "${WLAN_IFACE}" ]; then
  echo "[FAIL] WLAN 인터페이스를 찾지 못했습니다. WLAN_IFACE=<name> 으로 지정해주세요."
  exit 1
fi
echo "[INFO] WLAN_IFACE=${WLAN_IFACE} SSID=${AP_SSID} CH=${AP_CHANNEL} IP=${AP_IP}"

# 3) 의존성 체크 --------------------------------------------------------
for bin in hostapd dnsmasq iw ip; do
  if ! command -v "${bin}" >/dev/null 2>&1; then
    echo "[FAIL] '${bin}' 가 설치되어 있지 않습니다. 다음을 실행하세요:"
    echo "       sudo apt install -y hostapd dnsmasq iw iproute2"
    exit 1
  fi
done

# 4) 임시 설정 디렉터리 ------------------------------------------------
RUN_DIR="/run/combatrobot_ap"
sudo mkdir -p "${RUN_DIR}"

HOSTAPD_CONF="${RUN_DIR}/hostapd.conf"
DNSMASQ_CONF="${RUN_DIR}/dnsmasq.conf"
HOSTAPD_PID="${RUN_DIR}/hostapd.pid"
DNSMASQ_PID="${RUN_DIR}/dnsmasq.pid"

sudo tee "${HOSTAPD_CONF}" >/dev/null <<EOF
interface=${WLAN_IFACE}
driver=nl80211
ssid=${AP_SSID}
country_code=${AP_COUNTRY}
hw_mode=g
channel=${AP_CHANNEL}
ieee80211n=1
wmm_enabled=1
auth_algs=1
ignore_broadcast_ssid=0
wpa=2
wpa_passphrase=${AP_PASSPHRASE}
wpa_key_mgmt=WPA-PSK
wpa_pairwise=CCMP
rsn_pairwise=CCMP
EOF

sudo tee "${DNSMASQ_CONF}" >/dev/null <<EOF
interface=${WLAN_IFACE}
bind-interfaces
no-resolv
domain-needed
bogus-priv
dhcp-range=${AP_DHCP_START},${AP_DHCP_END},255.255.255.0,12h
dhcp-option=3,${AP_IP}
dhcp-option=6,${AP_IP}
log-dhcp
pid-file=${DNSMASQ_PID}
EOF

# 5) 기존 인스턴스 정리 ------------------------------------------------
sudo pkill -F "${HOSTAPD_PID}" 2>/dev/null || true
sudo pkill -F "${DNSMASQ_PID}" 2>/dev/null || true
sudo pkill -x hostapd 2>/dev/null || true
sudo pkill -x dnsmasq 2>/dev/null || true
sleep 1

# 6) 정적 IP 부여 + 인터페이스 up --------------------------------------
sudo ip addr flush dev "${WLAN_IFACE}"
sudo ip link set "${WLAN_IFACE}" down
sudo ip link set "${WLAN_IFACE}" up
sudo ip addr add "${AP_IP}/24" dev "${WLAN_IFACE}"

# 7) hostapd 기동 (daemon) ---------------------------------------------
echo "[INFO] starting hostapd..."
if ! sudo hostapd -B -P "${HOSTAPD_PID}" "${HOSTAPD_CONF}"; then
  echo "[FAIL] hostapd 기동 실패. 'sudo hostapd ${HOSTAPD_CONF}' 로 foreground 실행해 원인을 확인하세요."
  exit 1
fi

# 8) dnsmasq 기동 (daemon) ---------------------------------------------
echo "[INFO] starting dnsmasq..."
if ! sudo dnsmasq --conf-file="${DNSMASQ_CONF}"; then
  echo "[FAIL] dnsmasq 기동 실패. 'sudo dnsmasq -d --conf-file=${DNSMASQ_CONF}' 로 디버그 실행하세요."
  sudo pkill -F "${HOSTAPD_PID}" 2>/dev/null || true
  exit 1
fi

echo "[SUCCESS] AP active — SSID '${AP_SSID}' on ${WLAN_IFACE} (${AP_IP}/24)"
echo "          hostapd: pid $(cat "${HOSTAPD_PID}" 2>/dev/null)"
echo "          dnsmasq: pid $(cat "${DNSMASQ_PID}" 2>/dev/null)"
