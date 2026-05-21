#!/usr/bin/env bash
# SAN v1.5.2 — DCN-2026-011 D-035
# Hardware provisioning for skyautonet systemd services on RK3588.
#
# Run ONCE per board with sudo. Idempotent — re-run to switch roles.

set -euo pipefail

if [[ "${EUID}" -ne 0 ]]; then
    echo "Error: must run with sudo / as root" >&2
    exit 1
fi

ROLES=("leader-go2" "hub-sbc1" "hub-sbc2" "deputy" "follower")
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "=== SkyAutoNet RK3588 보드 프로비저닝 (DCN-2026-011 D-035) ==="
echo
echo "이 보드의 역할을 선택하세요:"
for i in "${!ROLES[@]}"; do
    printf "  %d) %s\n" "$((i+1))" "${ROLES[i]}"
done
echo
read -rp "선택 (1-5): " choice

if ! [[ "${choice}" =~ ^[1-5]$ ]]; then
    echo "Error: 잘못된 선택 '${choice}'" >&2
    exit 1
fi
ROLE="${ROLES[$((choice-1))]}"
SERVICE="skyautonet-${ROLE}.service"
echo "선택된 역할: ${ROLE}"
echo

# 1. sbc_id 프로비저닝 — DCN-2026-011 D-032 의 resolver 가 읽음.
mkdir -p /etc/skyautonet
case "${ROLE}" in
    hub-sbc1) echo "1" > /etc/skyautonet/sbc_id ;;
    hub-sbc2) echo "2" > /etc/skyautonet/sbc_id ;;
    *)        echo "0" > /etc/skyautonet/sbc_id ;;
esac
chmod 644 /etc/skyautonet/sbc_id
echo "  sbc_id = $(cat /etc/skyautonet/sbc_id)  (/etc/skyautonet/sbc_id)"

# 2. systemd unit 설치.
SERVICE_PATH="${SCRIPT_DIR}/${SERVICE}"
if [[ ! -f "${SERVICE_PATH}" ]]; then
    echo "Error: ${SERVICE_PATH} 파일을 찾을 수 없습니다" >&2
    exit 1
fi
cp "${SERVICE_PATH}" /etc/systemd/system/
chmod 644 "/etc/systemd/system/${SERVICE}"
echo "  installed /etc/systemd/system/${SERVICE}"

# 3. Reload + enable.
systemctl daemon-reload
systemctl enable "${SERVICE}" >/dev/null
echo "  systemctl enable ${SERVICE}  done"

echo
echo "===  프로비저닝 완료  ==="
echo
echo "서비스 시작:"
echo "  sudo systemctl start ${SERVICE}"
echo
echo "상태 확인:"
echo "  sudo systemctl status ${SERVICE}"
echo "  sudo journalctl -u ${SERVICE} -f"
