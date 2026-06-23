# SkyAutoNet RK3588 보드 프로비저닝 가이드 (DCN-2026-011 D-035)

## 개요

각 RK3588 보드는 5개 역할 중 하나로 운영됩니다:

| 역할 | 설명 |
|---|---|
| `leader-go2`  | Leader (Unitree Go2 SBC 탑재) |
| `hub-sbc1`    | Hub UGV 의 1번 SBC (primary) |
| `hub-sbc2`    | Hub UGV 의 2번 SBC (secondary) |
| `deputy`      | Deputy UGV (Hub 백업) |
| `follower`    | Follower UGV (군집 4~8) |

설치 한 번으로:
- `/etc/skyautonet/sbc_id` 작성 (DCN-2026-011 D-032 resolver 가 읽음)
- 해당 역할의 `.service` 를 `/etc/systemd/system/` 에 복사
- `systemctl daemon-reload && enable`

## 1회 프로비저닝 절차

```bash
# 1. 저장소 동기화 + 빌드 (관리자)
cd /opt/skyautonet
git pull origin main
colcon build --symlink-install

# 2. 프로비저닝 (역할 선택 + systemd 등록)
cd infra/systemd
sudo ./install.sh
# → 1-5 메뉴 선택

# 3. 서비스 시작
sudo systemctl start skyautonet-<role>.service

# 4. 정상 동작 확인 (10초 후)
sudo systemctl status skyautonet-<role>.service
sudo journalctl -u skyautonet-<role>.service -f
```

## 역할 변경 (재프로비저닝)

같은 보드의 역할을 바꿔야 할 때 (예: Follower → Deputy 승격):

```bash
sudo systemctl stop skyautonet-follower.service
sudo systemctl disable skyautonet-follower.service
cd /opt/skyautonet/infra/systemd
sudo ./install.sh    # 새 역할 선택
sudo systemctl start skyautonet-deputy.service
```

`install.sh` 는 idempotent — 같은 역할로 다시 실행해도 안전합니다.

## Follower robot_id 오버라이드

Follower 기본 `ROBOT_ID=4`. 5/6/7/8 인 경우 drop-in 파일을 만드세요:

```bash
sudo mkdir -p /etc/systemd/system/skyautonet-follower.service.d
sudo tee /etc/systemd/system/skyautonet-follower.service.d/robot_id.conf <<'EOF'
[Service]
Environment="ROBOT_ID=5"
EOF
sudo systemctl daemon-reload
sudo systemctl restart skyautonet-follower.service
```

## Hub 이중화 확인

두 SBC 모두 정상 작동하면 `RobotStatus.sbc1_healthy` / `sbc2_healthy` 가 모두 `true` 가 됩니다:

```bash
# Hub SBC1 또는 SBC2 에서
ros2 topic echo /swarm/robot_2/status --once
# 기대: sbc1_healthy=true, sbc2_healthy=true
```

한 SBC 가 죽으면 다른 쪽 SBC 가 3초 안에 `peer=false` 로 보고합니다 (DCN-2026-011 D-033 heartbeat).

## 트러블슈팅

### 서비스가 즉시 다운됨

```bash
sudo journalctl -u skyautonet-<role>.service -n 100 --no-pager
```

가장 흔한 원인:
- `/opt/skyautonet/install/setup.bash` 가 존재하지 않음 → `colcon build` 가 안 됐음
- `skyautonet` 사용자 / 그룹이 없음 → `sudo useradd -r -s /usr/sbin/nologin skyautonet`
- `ROS_DOMAIN_ID` 충돌 (동일 LAN 에 다른 ROS 시스템) → service 파일의 `Environment="ROS_DOMAIN_ID=42"` 수정

### `sbc_id` 가 잘못 되어 있음

```bash
cat /etc/skyautonet/sbc_id   # 0/1/2 중 하나여야 함
sudo bash -c 'echo "1" > /etc/skyautonet/sbc_id'   # 수동 수정
sudo systemctl restart skyautonet-hub-sbc1.service
```

### 두 Hub SBC 가 서로 안 보임

```bash
# 같은 네트워크 / 같은 ROS_DOMAIN_ID 인지 확인:
ros2 topic list | grep heartbeat
# 기대: /hub_internal/sbc1/heartbeat + /hub_internal/sbc2/heartbeat
```

방화벽 / DDS multicast 가 막혀 있으면 두 토픽 중 하나만 보임.

## 참고

- 본 가이드는 DCN-2026-011 D-035 의 산출물입니다.
- `san_bringup/systemd/san-squadron.service` (v1.5.2) 는 1-board-fits-all 의 옛 패턴 — 새로 배치하는 보드는 본 가이드의 역할별 service 를 사용하세요.
