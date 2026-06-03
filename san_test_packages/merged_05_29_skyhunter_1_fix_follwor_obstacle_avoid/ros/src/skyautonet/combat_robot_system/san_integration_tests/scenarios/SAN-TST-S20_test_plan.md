# SAN-TST-S20 — System Integration Test Plan (v1.5)

> **문서 ID**: SAN-TST-S20
> **버전**: Rev.A
> **작성일**: 2026-05-12
> **권원**: SAN-TST-001 (Phase D Verification Plan) 의 system-level 확장
> **대상**: ROS 2 squadron 통합 동작 검증 (Phase 2-E 사후)

---

## 1. 목적

Phase 2-E 에서 완료된 28 ROS 2 패키지의 **통합 동작 검증**. 단위 테스트
(153 standalone) 가 보장하지 못하는 시스템-레벨 행위를 자동화 시나리오로
재현하여, PDR/TRR1 산출물의 시험 절차서로 직접 활용.

KPP §2.1.1 (5 종) 매핑:

| KPP | 매핑 시나리오 | 측정 방법 |
|---|---|---|
| KPP-1: V-shape 60s 추종 ≤ 2 m | TST S20-1 (squadron stability) | 60s 가동 후 노드 health |
| KPP-2: Geofence 정지 ≤ 300ms | (HW 시험으로 이관) | — |
| KPP-3: Roundtrip p95 ≤ 150ms | TST S20-6 (latency) | `ros2 topic delay` |
| KPP-4: Leader 재선출 ≤ 10s | TST S20-2 (leader failover) | leader_changed 토픽 시점차 |
| KPP-5: 집결 성공률 ≥ 95% | (시뮬레이션으로 이관) | Gazebo |

---

## 2. 시나리오 카탈로그

### TST S20-1 — Squadron 부팅 + 5분 안정 가동
- **범위**: `squadron.launch.py` (Hub role) 5 분 가동
- **기대**: 모든 always_on + hub_only 노드 UP 라인 출력, 5분 동안 die 없음
- **자동화**: `test_s20_1_squadron_stability.py` (launch_testing)
- **PASS 기준**: 7 핵심 노드 UP 라인 모두 grep 매치 + 종료 시 ProcessExitedEvent 없음

### TST S20-2 — Leader 실패 → Re-election ≤ 10s
- **범위**: `san_role_management::role_manager_node` 가 leader 강제 종료 후 follower 들이 새 leader 합의
- **기대**: 새 leader_id 가 `/swarm/leader_id` 에 publish 되는 시점 ≤ 10s
- **자동화**: `test_s20_2_leader_failover.py`
- **PASS 기준**: SIGKILL(leader) → `/swarm/leader_id` 변경 ≤ 10000 ms

### TST S20-3 — RTCM 주입 → RTK_FIX 달성
- **범위**: `san_ntrip_client → /rtcm_corrections → san_rtk_gnss` 양방향
- **기대**: NTRIP stub 활성 시 `~/rtk_status.fix_type` 가 FIX_RTK_FIX(4) 또는 FIX_RTK_FLOAT(5) 도달
- **자동화**: `test_s20_3_rtcm_chain.py`
- **PASS 기준**: 30s 내 fix_type ∈ {4, 5}

### TST S20-4 — Fire Authorization End-to-End
- **범위**: `san_fire_authorization` HMAC + Two-Key + Audit log
- **기대**: 유효 HMAC + 2개 key holder approval → `fire_authorized` 발행, audit log 생성
- **자동화**: `test_s20_4_fire_auth.py`
- **PASS 기준**: 정상 시나리오 → fire_authorized=true 5s 내, 잘못된 HMAC → fire_denied
- **참고**: 기존 `test_integration_scenarios.cpp` (TST S18 시리즈) 의 system-level 확장

### TST S20-5 — Comm Link 페일오버 (WiFi6 → LTE)
- **범위**: `san_comm_link` 히스테리시스 상태 머신
- **기대**: WiFi6 health degrades (N_FAIL=3 연속) → active_link 가 LTE 로 전환
- **자동화**: `test_s20_5_comm_link_failover.py`
- **PASS 기준**: 시뮬레이션된 link degrade → active_link 전환 1s 내, switch_count 증가

### TST S20-6 — End-to-End Latency (Roundtrip p95 ≤ 150ms)
- **범위**: leader → follower 토픽 왕복 지연 측정
- **기대**: 200회 측정 p95 ≤ 150ms (KPP-3)
- **자동화**: `test_s20_6_e2e_latency.py`
- **PASS 기준**: p95 ≤ 150ms (DDS intra-process zero-copy 기준)

---

## 3. 실행 환경

| 구분 | 값 |
|---|---|
| OS | Ubuntu 22.04 |
| ROS 2 | Humble |
| RMW | Cyclone DDS (기본) |
| 모드 | **Stub** (HW 부재 시 자동) |
| 실행 도구 | `ros2 launch san_integration_tests <scenario>.launch.py` 또는 `launch_test` |

각 시나리오는 **HW 없이** 통과하도록 설계 (stub mode 활용). HW 통합 시
동일 스크립트 재실행하면 실 HW 로 동작.

---

## 4. CI 통합

`.github/workflows/integration-tests.yml` (신규):
- 트리거: PR + push to main/develop
- 컨테이너: osrf/ros:humble-desktop
- 소요 시간: ~10 분 (6 시나리오 × 평균 100s + 빌드)
- 산출: JUnit XML + colcon test-result

---

## 5. 산출물

각 시나리오별 생성:
- `scenarios/test_s20_<n>_<name>.py` — launch_testing 자동화
- 본 문서의 PASS 기준 사용
- CI JUnit 출력 → PDR 검증 매트릭스 자동 채움

---

## 6. KPI 측정 자동화

| KPI | 측정 토픽/방법 | TST 시나리오 |
|---|---|---|
| Squadron 부팅 시간 | 최후 UP 라인 출력 timestamp | S20-1 |
| Leader 재선출 시간 | leader_id 변경 시점 차 | S20-2 |
| RTK 획득 시간 | rtk_status.fix_type 도달 시점 | S20-3 |
| Fire auth roundtrip | command → authorized 시점 차 | S20-4 |
| Comm link 전환 시간 | active_link 변경 시점 차 | S20-5 |
| End-to-end latency | topic_tools/measure | S20-6 |

---

## 7. PDR/TRR1 매핑

| SE 산출물 | TST S20 시리즈 활용 |
|---|---|
| TRR1 시험 계획서 | 본 문서 직접 활용 |
| TRR1 시험 환경 구성 | 환경 절 (§3) 활용 |
| TRR2 통합 완료 보고서 | CI JUnit 결과 통합 |
| TRR2 시험 절차서 | 각 .py 시나리오 코드 자체가 절차 |

본 시나리오들이 **검증된 기술 성숙도 TRL 6-7** 의 정량 증빙.

---

## 8. PDR-6 추가 시나리오 (2026-05-13) — S20-7/8/9

### S20-7 — Mission BT Fallback Root Smoke

**검증**: SDD §6.1 Fallback root + 5 priority subtree.
**자동화**: `test_s20_7_mission_bt_boot.py`
**절차**: mission_node 가 `tree_type=fallback` 으로 4초 내 부팅 후 5Hz 틱 안정 동작.
**PASS 기준**: 프로세스 alive + crash 없음. 우선순위 의미론 검증은 san_mission/test_mission_bt.py (15 standalone) 와 보완.

### S20-8 — KPP-2 E2E Timing (★ 핵심)

**검증**: KPP-2 회피 반응 ≤ 300 ms (cost_map 갱신 → /cmd_vel 회피 명령).
**자동화**: `test_s20_8_kpp2_e2e.py`
**절차**:
  1. tier_node + reroute_node (robot_id=3) 부팅
  2. 1초간 warm-up (status + free cost map)
  3. lethal cell 주입 → 시점 t_inject 기록
  4. /cmd_vel 회피 명령 (angular ≠ 0) 수신 → 시점 t_cmd 기록
  5. `(t_cmd - t_inject) ≤ 300 ms` 검증

**PASS 기준**: 측정값 ≤ 300 ms. PDR 평가 evidence.

### S20-9 — T4 BREADCRUMB_RECOVERY

**검증**: SDD §6.2 T4 진입 (60s comm timeout) + T4→T0 복구.
**자동화**: `test_s20_9_t4_recovery.py`
**절차** (테스트 시간 단축을 위해 `comm_timeout_ms=2000` 으로 가속):
  1. tier_node 부팅, target+status pump → T0 진입 확인
  2. target 게시 중단, 2.5s 대기 → T4 진입 확인 (reason="comm_timeout_60s")
  3. target 재개 → T0 복구 확인 (reason="comm_restored")

**PASS 기준**: T4 진입 확인 + 복구 시 reason 정확.
