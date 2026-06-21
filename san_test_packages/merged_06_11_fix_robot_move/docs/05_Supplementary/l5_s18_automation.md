# L5 회귀 자동화 가이드 — S18 (SAN v1.5)

> **v1.5 갱신 (2026-05-12, DCN-2026-001 D-006)**: S18-1~6 시나리오 본문이 TST v1.5 §11에 정식 추가됨. 이전 v1.4에서는 개정 이력에만 약속되어 있었음. 본 가이드의 시나리오 매트릭스는 v1.4와 동일하나, 참조 문서는 v1.5로 갱신.

> Source: SAN-TST-INT-001 v1.5 §11 (S18-1~6 본문), SAN-SDD-SWARM-001 v1.5 §5.5–§5.7, SAN-IDS-CMD-001 v1.5 §5.15–§5.16

`san_l5_regression` 패키지는 PR #58 (PHASE 8) 의 Hub-Deputy / 4-tier
Leader / Limp Mode 정책을 **실로봇 + HIL 공통** 으로 자동 검증합니다.
시나리오 6 개 (S18-1~6) 를 차례로 실행하고 transition latency 를
JSON + Markdown 으로 기록합니다.

## 1. 동작 원리

회귀 러너 (`regression_main`) 는 다음을 한 프로세스에서 처리합니다.

1. **synthetic 신호 주입** — `FailureInjector` 가 `/swarm/robot_status`
   토픽에 SBC down / 배터리 변화 / Deputy 표식 등 합성 메시지를 발행.
   live 의 `san_role_management` 노드는 진짜 신호와 구별하지 않습니다.
2. **transition 감시** — `TopicWatcher<T>` 가 `/swarm/leader/role_announce`,
   `/swarm/hub/role_announce`, `/swarm/limp_mode/alert` 를 구독해 시나리오별
   matching predicate 가 충족될 때까지 deadline 만큼 block.
3. **타이밍 기록** — `ScenarioReport` 가 elapsed_ms / outcome /
   attributes (예: `promoted_robot_id=3`, `succession_priority=DEPUTY`) 를
   저장.
4. **리포트** — `ScenarioReportWriter::renderJson()` /
   `renderMarkdown()` 으로 `s18_report.json` + `s18_report.md` 산출.

shell 호출 0건, bash 스크립트 0건 정책을 유지합니다.

## 2. 시나리오 매트릭스

| ID | 시나리오 | 주입 | 매칭 토픽 / predicate | 기본 deadline |
|---|---|---|---|---|
| S18-1 | Leader → Deputy 승계 | S1 SBC down | `LeaderRoleAnnouncement.role==PROMOTED && robot_id==3 && succession_priority==DEPUTY` | 5 s |
| S18-2 | Leader + Deputy → Hub | S1, S3 SBC down | `... robot_id==2 && succession_priority==HUB` | 8 s |
| S18-3 | Hub → Deputy 인수 | S2 SBC down | `HubRoleAnnouncement.role==PROMOTED && robot_id==3 && lte_active && slam_aggregation_active && video_relay_active` | 7 s |
| S18-4 | 3 대 불능 → 배터리 최대 | S1, S2, S3 SBC down + S4-8 배터리 60/90/45/55/30 | `LeaderRoleAnnouncement.role==PROMOTED && succession_priority==BATTERY_MAX` | 10 s |
| S18-5 | Limp Mode 진입 | S2, S3 SBC down | `/swarm/limp_mode/alert` contains `LIMP_MODE_ACTIVE` | 8 s |
| S18-6 | Limp Mode 이탈 | S5 enter Limp → S3 복구 | alert contains `LIMP_MODE_EXITED` | 5 s |

deadline 은 `config/regression_scenarios.yaml` 에서 override 가능합니다
(HIL bench 가 느린 경우 등).

## 3. 실행

### live ROS2 도메인에서

```bash
# 1) san_role_management 가 이미 떠 있어야 함
ros2 launch san_role_management role_management.launch.xml &

# 2) 회귀 러너 실행 — exit code 0 = all PASS
ros2 launch san_l5_regression regression.launch.xml \
    report_dir:=/var/log/san/regression
```

산출물:
- `/var/log/san/regression/s18_report.json` (CI 파싱용)
- `/var/log/san/regression/s18_report.md`   (사람 검토용)
- stdout 에 markdown 표 그대로 출력

### docker compose 환경

Hub UGV SBC #2 의 기존 compose stack 에 회귀 러너 컨테이너를 임시로
부착하거나, 별도 `docker run` 으로 동일 `ROS_DOMAIN_ID=42` /
`CYCLONEDDS_URI` 환경 변수를 주입해 합류시킵니다. 시나리오는 실제
DDS 토픽 위에서 동작하므로 `infra/docker/sbc2/docker-compose.yml` 의
다른 서비스를 종료할 필요는 없습니다.

## 4. 리포트 예시 (Markdown 부분)

```
# SAN v1.4 L5 regression report

Summary: **6 PASS** / 0 FAIL / 0 TIMEOUT / total 6

| Scenario | Outcome | Elapsed | Deadline | Notes |
|---|---|---|---|---|
| **S18-1** — Leader → Deputy 승계 (≤ 5 s) | PASS | 1240 ms | 5000 ms | `promoted_robot_id=3` `succession_priority=DEPUTY` |
| **S18-2** — Leader + Deputy → Hub 승계 (≤ 8 s) | PASS | 3580 ms | 8000 ms | ... |
| **S18-3** — Hub → Deputy 인수 (LTE + SLAM + Video, ≤ 7 s) | PASS | 5210 ms | 7000 ms | `lte_active=true` `slam_aggregation_active=true` `video_relay_active=true` |
| **S18-4** — 3 대 불능 → 배터리 최대 follower 승계 (≤ 10 s) | PASS | 4860 ms | 10000 ms | `promoted_robot_id=5` |
| **S18-5** — Hub + Deputy 모두 불능 → Limp Mode 진입 (≤ 8 s) | PASS | 7110 ms | 8000 ms | ... |
| **S18-6** — Deputy 복구 → Limp Mode 이탈 (≤ 5 s) | PASS | 2420 ms | 5000 ms | ... |
```

## 5. 한계

- **물리적 KPP 측정 없음** — 본 자동화는 transition 타이밍 + 분기
  정확성만 검증합니다. FHD 1.5 Mbps E2E p99, 3 stream 동시 운용,
  Hailo8 추론 latency 같은 측정 KPP 는 별도 측정 도구 (Sprint 통합) 가
  담당합니다.
- **failure 주입은 신호 수준** — 실로봇 GbE 케이블 분리 / 컨테이너 kill /
  배터리 BMS 정전 같은 물리적 결함 주입은 본 도구 범위 외입니다. 그러나
  role manager 의 의사결정 신호 경로는 동일하므로 회귀 신뢰성에 영향이
  없습니다.
- **S15 / S16 미구현** — 본 PR 은 S18 만. S15 (영상 / LTE auto-rate) 와
  S16 (DEMO 6-phase) 은 후속 PR.

## 6. 테스트 (단위)

빌드 후:

```bash
colcon test --packages-select san_l5_regression
colcon test-result --verbose
```

세 가지 테스트 PASS 시 회귀 인프라가 정상입니다.

- `test_failure_injector` — RobotStatus / LteLinkQuality 발행 wire 검증
- `test_topic_watcher` — predicate satisfaction / timeout / reset
- `test_scenario_report` — JSON/Markdown 포맷 + allPassed 집계
