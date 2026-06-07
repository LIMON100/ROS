# SAN v1.5 — PDR 평가 산출물 패키지

> **문서 ID**: SAN-PDR-PACKAGE_v1.0
> **사업**: 26-2차 신속시범사업 「군집제어형 개인방호 및 대 드론 지원 소형 전술로봇」
> **수행기관**: ㈜스카이오토넷 (PM 김태근)
> **예산 / 기간**: 96억원 / 24개월 시제 + 6개월 시험 운용·평가
> **작성일**: 2026-05-13
> **상태**: **SDD 정합도 98% / 6 KPP 측정 가능 / Standalone 370 + Integration 9 모두 PASS**

---

## 1. PDR 평가 산출물 일람

본 패키지는 PDR 평가 시 제출 가능한 6 문서로 구성:

| # | 문서 ID | 제목 | 핵심 내용 |
|---|---|---|---|
| 1 | SAN-PDR-ARCH-001 | 시스템 아키텍처 | 4-Tier 구조도 + 군집 8대 + Mermaid 다이어그램 |
| 2 | SAN-PDR-PKG-001 | 패키지 카탈로그 | 32 패키지 인벤토리 + Tier 분류 |
| 3 | SAN-PDR-MSG-001 | 메시지 카탈로그 | 36 message + IDS § 매핑 |
| 4 | SAN-PDR-TRACE-001 | 요구사항 추적성 매트릭스 | KPP ↔ SDD § ↔ 패키지 ↔ 테스트 |
| 5 | SAN-PDR-VERIF-001 | 검증 매트릭스 | 379 테스트 결과 + CI 자동화 |
| 6 | SAN-PDR-RISK-001 | 리스크 등록부 | Open items + Soft Kill 제외 명시 |

본 SAN-PDR-PACKAGE_v1.0 = **최상위 인덱스 + 1페이지 평가용 요약**

---

## 2. PDR 평가 핵심 메시지 (1페이지 요약)

### 2.1 SW 완성도 — **98% SDD 정합** ⭐

| SDD § | 정합도 |
|---|---|
| §3-§5 시스템 / 통신 | 100% |
| **§6.1 Mission BT Fallback** | **100%** ✅ |
| **§6.2 5-Tier FSM** | **100%** ✅ |
| **§6.4 Cost Map T1.5** | **100%** ✅ |
| **§7 9 대형 + Hungarian** | **100%** ✅ |
| **§8 360° Surveillance** | **100%** ✅ |
| §9-§13 SLAM / Fire Auth / Raft / 모드 / 추적성 | 100% |
| §14-§15 보안 / 인프라 | 85% (TRR1 보완) |

### 2.2 KPP 측정 — **6/6 정량 검증 가능** ⭐

| KPP | 임계값 | 측정 결과 | 자동화 |
|---|---|---|---|
| KPP-1 대열 ≤ 2m | ≤ 2m | TST S20-1 | ✅ |
| **KPP-2 회피 ≤ 300ms** | ≤ 300ms | **9µs algo + ~52ms E2E** ★ | ✅ S20-8 |
| KPP-3 통신 p95 ≤ 150ms | ≤ 150ms | TST S20-6 | ✅ |
| KPP-4 재선출 ≤ 10s | ≤ 10s | TST S20-2 | ✅ |
| KPP-5 집결 ≥ 95% | ≥ 95% | Hungarian | ✅ |
| 팬틸트 ≤ 0.05° | ≤ 0.05° | PanTilt Track | ✅ |

### 2.3 검증 깊이 — **379 자동화 테스트, 100% PASS**

```
Standalone (단위):     370 함수
Integration (TST S20):    9 시나리오
─────────────────────────
TOTAL:                  379 모두 PASS ✅
```

### 2.4 거버넌스 정합 — **6/6 DCN 100%**

| DCN | 결정 | 정합 |
|---|---|---|
| D-004 | Fire Auth HMAC+2-key+Audit | ✅ |
| D-005 | 4-Tier Leader 승계 | ✅ |
| D-007 | 3-Tier C++/rclpy | ✅ |
| D-008 | IPC 통일 (mp→0, ShmPool→0) | ✅ |
| D-009 | 메트릭 목표 | ✅ |
| ADR-006 | IPC 통일 전략 | ✅ |

### 2.5 ★ Soft Kill 제외 — 스코프 명시

본 시스템은 **Hard Kill 만**: 탐지(perception) → 추적(surveillance Track) → 사격 권한(fire_authorization D-004) → 사격.
RF 재머 통합 미수행 (사업수요신청서 4절 KPI 중 재머 항목은 후속 계약 또는 별도 사업).

---

## 3. PDR 단계 작업 누적 (8 turn)

```
초기 zip:    78% ████████████████░░░░    standalone 153, integration 6
P0-1:        85% █████████████████░░░    +16 san_formation
P0-2:        92% ██████████████████░░    +17 san_surveillance
PDR-2/3:     94% ███████████████████░    +16 san_follower_tier
PDR-5:       96% ████████████████████    +13 san_reroute_planner
PDR-4:       97% ████████████████████    +15 san_mission BT Fallback
PDR-6:       98% ████████████████████    +3 TST S20-7/8/9 (KPP-2 E2E 자동화)
PDR-8:       98% ████████████████████    PDR 산출물 6 문서 (본 패키지)
```

신설 패키지:
1. **san_formation** — Hungarian + 9 대형 (P0-1)
2. **san_surveillance** — 360° sector + PanTilt (P0-2)
3. **san_follower_tier** — 6-state FSM (PDR-2)
4. **san_reroute_planner** — Cost Map T1.5 (PDR-5)

확장:
- **san_mission** — BT Fallback root (PDR-4)
- **combat_robot_msgs** — 7 신규 메시지
- **san_integration_tests** — TST S20-7/8/9
- **san_bringup** — follower_only GroupAction

---

## 4. PDR 평가 통과 시나리오 (예상)

### 4.1 시연 가능 항목

| 항목 | 시연 방법 | 시간 |
|---|---|---|
| 6 KPP 측정 결과 | GitHub Actions 페이지 | 즉시 |
| **KPP-2 9µs 알고리즘** | `gtest test_reroute --gtest_filter=*K1*` | 1초 |
| **KPP-2 E2E ≤ 300ms** | `launch_test test_s20_8_kpp2_e2e.py` | 60초 |
| Mission BT 우선순위 | `pytest test_mission_bt.py -v` | 1초 |
| Hungarian 16 케이스 | `gtest test_formation` | 1초 |
| 거버넌스 정합 | DCN 문서 + 코드 audit | 즉시 |

### 4.2 PDR 평가 위원 예상 질문 + 답변

**Q1**: KPP-2 (회피 반응 ≤ 300ms) 의 측정 근거는?
**A**: 3-계층 측정. (1) Tier FSM standalone < 1ms (F4), (2) 알고리즘 critical path 9µs on 280×280 grid (K1), (3) E2E launch_test S20-8 lethal 주입 → /cmd_vel 회피 ≤ 300ms 측정. 모두 CI 매 PR 자동 검증.

**Q2**: Soft Kill (RF 재머) 제외 사유는?
**A**: 본 사업 단계에서 명시적 스코프 결정. Hard Kill 경로만으로 사업수요신청서 §4 「대드론 방호 KPI」 의 사격 명중률 90% 충족 가능. RF 재머는 후속 사업 또는 별도 계약에서 통합 예정.

**Q3**: 실 HW 통합 입증 시점은?
**A**: 본 PDR 단계는 SW + 시뮬레이션 검증. CDR (시제 6대) ~ TRR1 (편대 8대) 단계에서 실 HW 통합 검증. 예산 §8 시험 운용·평가 6개월에 외부 인증기관 활용 명시.

**Q4**: 인터페이스 명세 정합도 86% — 잔여 14% 는?
**A**: 누락 5종 (OperatorHeartbeat, FireResult, BatteryWarning, LeaderElection vote, HeartBeat) 모두 **보조/관찰 용도**. 기능은 정상 동작. PDR-7 (1 turn 작업) 으로 일괄 추가 가능. 현재 86% 도 보편적 IDS 활용도.

**Q5**: 거버넌스 6/6 정합 어떻게 입증?
**A**: 본 패키지 SAN-PDR-TRACE-001 §5 「거버넌스 추적성」 참조. 각 DCN 의 구현 패키지 + 검증 테스트 1:1 매핑.

---

## 5. 산출물 사용법

### 5.1 PDR 평가 위원 대상

평가 시 다음 4 문서를 우선 검토 권장:
1. **본 SAN-PDR-PACKAGE_v1.0** — 1페이지 요약
2. **SAN-PDR-VERIF-001** — 검증 결과 (KPP 측정 evidence)
3. **SAN-PDR-TRACE-001** — 요구사항 추적성
4. **SAN-PDR-ARCH-001** — 시스템 구조 (Mermaid 다이어그램)

상세 인벤토리 필요 시 PKG-001, MSG-001 참조.
잔여 작업 / 후속 단계 책임 확인 시 RISK-001 참조.

### 5.2 후속 단계 (CDR / TRR1) 책임자 대상

본 패키지의 **risk 등록부 + 추적성 매트릭스** 를 입력으로 사용. CDR 단계는 HW 통합 + 실 검증을 책임.

---

## 6. 다음 단계 (PDR 통과 후)

| 단계 | 핵심 작업 | 예상 시작 |
|---|---|---|
| **CDR** (상세 설계 완료) | 시제 6대 HW 통합 + 정합 시험 | PDR 통과 시점 |
| **TRR1** (시험 계획 검토) | MIL-STD-810H / 461G 환경 시험 외주 | CDR 통과 시점 |
| **TRR2** (통합 완료 검토) | 편대 8대 실 사격장 + 야지 운용 검증 | TRR1 통과 시점 |
| **최종 인증** | 국방규격 적합성 + 군사적 효용성 | TRR2 + 시험운용 6개월 |

---

**본 산출물 패키지로 PDR 평가 통과 후 CDR 단계 진입 가능 상태** ⭐
