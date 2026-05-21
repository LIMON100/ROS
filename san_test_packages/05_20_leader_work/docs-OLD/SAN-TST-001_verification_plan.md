# SAN-TST-001 — Phase D Verification Plan

| 항목 | 값 |
|---|---|
| 문서 ID | SAN-TST-001 |
| 버전 | Rev.A |
| 작성 | 2026-Q4 (Phase D 직전) |
| 대상 | Phase D 통합 시험 검증 |
| 기준 | KPP §2.1.1 + UC-1 ~ UC-11 |

## 1. 목적

P1+P2 (33 stories, 175 SP) 완료 후 통합 시험을 통해 다음을 검증:

- KPP §2.1.1 5종 모두 통과
- 11개 user scenario 모두 시연 가능
- Phase E pre-MP 진입 자격 충족

## 2. 검증 환경

### 2.1 Sim (Phase D 1주차)

- Gazebo Classic 11 + ROS 2 Humble
- 4-robot (minimum, v1.5) + 8-robot (standard, v1.5) 두 시나리오
- CI에서 자동 실행 (kpp.yml workflow)

### 2.2 HW (Phase D 2주차 이후)

- 5x RK3588J + Unitree Go2 + 1x Hub UGV
- iptime AX2004M mesh router
- u-blox F9P + NTRIP
- Field site: 전용 시험장 (사전 협의)

## 3. 검증 항목

### 3.1 KPP 검증

| KPP | 측정 방법 | 통과 기준 | 검증 환경 |
|---|---|---|---|
| KPP-1 | 8-robot V-shape 60 s 추종 (v1.5) | 평균 오차 ≤ 2 m | Sim + HW |
| KPP-2 | Geofence intrusion 주입 | 정지 ≤ 300 ms | Sim + HW |
| KPP-3 | Leader→follower roundtrip 200 회 | p95 ≤ 150 ms | Sim + HW |
| KPP-4 | Leader 강제 종료 | 새 leader_pose ≤ 10 s | Sim + HW |
| KPP-5 | 20 회 집결 시도 | 성공률 ≥ 95 % | Sim |

### 3.2 User Scenario 검증

(참조: SAN-USC-SWARM-001 Rev.A §5)

| UC | 시나리오 | 측정 |
|---|---|---|
| UC-1  | Boot + pairing | < 60 s |
| UC-2  | Mission start (M1) | First move < 5 s |
| UC-3  | Anomaly detection | Push latency < 200 ms |
| UC-4  | WiFi6 → LTE failover | ≤ 10 s |
| UC-5  | RTK loss → T2 | < 2 s |
| UC-6  | Leader reconfiguration | KPP-4 |
| UC-7  | Test mode + PIN | < 3 s |
| UC-8  | RTH 100 m | < 90 s |
| UC-9  | Emergency stop | < 100 ms |
| UC-10 | Geofence | KPP-2 |
| UC-11 | Leader rollback | < 2 s trigger |

### 3.3 운용 모드 검증

| Mode | d (m) | θ (°) | 속도 (m/s) | 검증 |
|---|---|---|---|---|
| DEV_TEST | 3  | 60  | 1.0 | PIN auth + 속도 강제 |
| NARROW   | 3  | 40  | 1.3 | 4.1 m 회랑 통과 |
| RECON    | 5  | 90  | 1.3 | 360° 시야 |
| WIDE     | 7  | 120 | 1.3 | 분산 감시 |
| ASSAULT  | 15 | 60  | 1.3 | 122 mm 살상반경 회피 |

### 3.4 9 Formation 시각 검증

각 formation에 대해:

- 8-robot이 안정적으로 형성 (5 초 내, v1.5)
- 운영자 단말에 시각적 정확도 확인
- 전환 시 follower 충돌 없음 (Hungarian 검증)

### 3.5 안전 시스템 검증

| 항목 | 검증 |
|---|---|
| Geofence       | UC-10 + 100 회 random walk 0 violation |
| Battery RTH    | 20 %, 10 % 임계값 동작 + dev override 가능 |
| Health Monitor | 4 sensor 임의 disable → degraded 상태 + 회복 |
| Audit Log      | 1 시간 운용 후 chain 무결 100 % |
| AI Invariants  | 100 회 cmd_vel 시도 모두 차단 + audit 기록 |
| Time Sync      | 모든 robot offset < 1 ms (PTP) |

## 4. 검증 절차

### 4.1 Sim 검증 (Day 1-3)

```bash
docker compose -f sim/docker-compose.gazebo.yml up

python sim/scripts/measure_kpp_in_sim.py \
    --world seoul_urban_2km --robots 9 \
    --duration 600 --output results/sim_kpp_run1.json

for uc in 1 2 3 4 5 6 7 8 9 10 11; do
    python sim/scripts/run_uc_test.py --uc $uc \
        --output "results/sim_uc${uc}.json"
done
```

### 4.2 HW 검증 (Day 4-10)

`doc/phase_d_hw_bringup.md` 절차에 따라 10-item field trial checklist 모두
통과 후 진행.

## 5. 합격 / 불합격 기준

### 합격

- KPP 5종 모두 PASS (sim + HW)
- UC-1 ~ UC-11 모두 시연 성공
- 안전 시스템 6개 모두 OK
- 운용 모드 5개 모두 검증
- 9 formation 시각 검증 통과

### 불합격

- KPP 1개 이상 FAIL → 해당 항목 재설계 + 재측정
- UC 1개 이상 실패 → 코드 디버그 + 재시험
- 안전 시스템 위반 → 즉시 중단 + root cause 분석

## 6. 보고

### 6.1 자동 생성

- `.github/kpp_report.json` (P2-13 CI artifact)
- `/var/log/patrol/audit/` (모든 trial run)
- `results/sim_*.json`, `results/hw_*.json`

### 6.2 최종 보고서

`doc/SAN-TST-001_verification_report_<date>.md`:

- 모든 trial 결과 요약
- KPP measured vs threshold 표
- UC pass/fail matrix
- 발견된 이슈 + 조치 계획
- Phase E 진입 권고 / 재작업 필요 항목

## 7. 일정

| 주차 | 작업 |
|---|---|
| Week 1 | Sim 환경 build + KPP CI 첫 run |
| Week 2 | UC-1 ~ UC-7 sim 검증 |
| Week 3 | UC-8 ~ UC-11 sim 검증 + HW bring-up |
| Week 4 | HW field trial (KPP + UC 일부) |
| Week 5 | HW 잔여 UC + 안전 시스템 |
| Week 6 | 보고서 작성 + Phase E 결정 |

## 8. 위험

| Risk | 영향 | 대응 |
|---|---|---|
| Gazebo 시뮬 부정확 (실제 ms-level latency 다름) | KPP-3 sim PASS, HW FAIL 가능 | HW 측정 우선 |
| 시험장 RTK 음영 | KPP 측정 불가 | NTRIP 보강 + LTE 백홀 |
| WiFi6 mesh 불안정 | KPP-3, UC-4 변수 증가 | iptime 사전 검증 |
| Field 일정 지연 | 시험 압박 | Phase E 진입 1 주 buffer |
| 안전 사고 (사람 접근) | trial 중단 | Geofence + 안전 거리 30 m |
