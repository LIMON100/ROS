#!/usr/bin/env bash
# Sysfs PWM 준비 스크립트: export + 권한(일반 사용자 쓰기) + udev 규칙
# ===== 사용자 환경에 맞게 아래 변수만 조정 =====
set -euo pipefail

USER_NAME="firefly"          # 일반 사용자 계정
GROUP_NAME="gpio"            # 없으면 자동 생성
PWM_CHANNELS=("0:0")         # "chip:line" 목록 예) ("0:0" "0:1")
PWM_PERIOD_NS=20000000       # 50Hz = 20ms (서보/ESC 기본)
INITIAL_ENABLE=0             # 0=비활성(권장), 1=활성

log(){ echo -e "[trigger.sh] $*"; }
die(){ echo "[trigger.sh][ERROR] $*" >&2; exit 1; }

require_root(){
  if [[ $EUID -ne 0 ]]; then
    log "root 권한 필요 → sudo로 재실행합니다."
    exec sudo -E bash "$0" "$@"
  fi
}

ensure_group_and_membership(){
  getent group "${GROUP_NAME}" >/dev/null || (log "그룹 ${GROUP_NAME} 생성"; groupadd "${GROUP_NAME}")
  if id -nG "${USER_NAME}" | tr ' ' '\n' | grep -qx "${GROUP_NAME}"; then
    log "사용자 ${USER_NAME} 는 이미 ${GROUP_NAME} 그룹에 포함"
  else
    log "사용자 ${USER_NAME} 를 ${GROUP_NAME} 그룹에 추가"
    usermod -aG "${GROUP_NAME}" "${USER_NAME}" || die "usermod 실패"
    log "(참고) 그룹 적용은 다음 로그인부터 반영됩니다."
  fi
}

install_udev_rule(){
  local RULE=/etc/udev/rules.d/99-pwm-perms.rules
  cat >"$RULE" <<EOF
# PWM 채널(pwm*)이 생성될 때 그룹/권한을 자동 부여
SUBSYSTEM=="pwm", KERNEL=="pwm*", ACTION=="add", RUN+="/bin/chgrp -R ${GROUP_NAME} /sys/class/pwm/%k", RUN+="/bin/chmod -R g+w /sys/class/pwm/%k"
EOF
  log "udev 규칙 설치: ${RULE}"
  udevadm control --reload
  udevadm trigger --subsystem-match=pwm || true
}

export_and_perm_one(){
  local chip="$1" line="$2"
  local base="/sys/class/pwm/pwmchip${chip}"
  local pwm="${base}/pwm${line}"

  [[ -d "${base}" ]] || die "pwmchip${chip} 경로 없음: ${base}"

  # 이미 있으면 재생성(권한 재적용 목적)
  if [[ -d "${pwm}" ]]; then
    log "이미 존재: pwmchip${chip}/pwm${line} → 재생성"
    echo "${line}" > "${base}/unexport" || true
    sleep 0.05
  fi

  # export (root만 가능)
  echo "${line}" > "${base}/export"
  sleep 0.05

  # 기본 설정
  echo 0                  > "${pwm}/enable"      || true
  echo "${PWM_PERIOD_NS}" > "${pwm}/period"       || die "period 설정 실패"
  echo 0                  > "${pwm}/duty_cycle"   || die "duty_cycle 설정 실패"
  echo "${INITIAL_ENABLE}" > "${pwm}/enable"      || true

  # 그룹/권한 부여 (udev RUN+ 보강)
  chgrp -R "${GROUP_NAME}" "${pwm}" || true
  chmod -R g+w               "${pwm}" || true

  # 현재 사용자에게 즉시 RW 보장(새 로그인 없이) — setfacl 있으면 사용
  if command -v setfacl >/dev/null 2>&1; then
    setfacl -m u:${USER_NAME}:rwX -R "${pwm}" || true
  fi

  log "준비 완료: pwmchip${chip}/pwm${line} (period=${PWM_PERIOD_NS}ns, enable=${INITIAL_ENABLE})"
}

export_and_perms_all(){
  for pair in "${PWM_CHANNELS[@]}"; do
    IFS=: read -r chip line <<<"${pair}"
    export_and_perm_one "${chip}" "${line}"
  done
}

main(){
  require_root "$@"
  log "시작: USER=${USER_NAME}, GROUP=${GROUP_NAME}, 채널=${PWM_CHANNELS[*]}"
  ensure_group_and_membership
  install_udev_rule
  export_and_perms_all
  log "완료! 이제 ${USER_NAME} 계정으로 sudo 없이 /sys/class/pwm/pwmchipX/pwmY 하위 파일에 쓰기 가능합니다."
  log "(참고) 새 로그인 이후 그룹 권한이 완전히 반영됩니다. ACL 덕에 현재 세션에서도 바로 될 수 있습니다."
}

main "$@"

