#!/bin/bash
# ============================
# NetworkManager 서비스 시작 스크립트
# ============================

set -e  # 에러 발생 시 즉시 종료

echo "[INFO] NetworkManager 서비스 시작..."
sudo systemctl start NetworkManager

echo "[SUCCESS] NetworkManager 실행 완료!"
