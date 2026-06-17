# SAN v1.5 — 요구사항 추적성 매트릭스

> **문서 ID**: SAN-PDR-TRACE-001 Rev.A
> **목적**: SDD-SWARM v1.5 / IDS-CMD v1.5 요구사항 ↔ 구현 패키지 ↔ 검증 테스트 1:1 매핑
> **범위 제한**: Soft Kill 제외

---

## 1. KPP 추적성 (★ PDR 평가 핵심)

| KPP | 임계값 | SDD § | 구현 패키지 | Standalone 테스트 | Integration TST | 측정 가능 |
|---|---|---|---|---|---|---|
| **KPP-1 대열 ≤ 2m** | 평균 ≤ 2m | §7 | san_formation | test_formation (16) | TST S20-1 | ✅ |
| **KPP-2 회피 ≤ 300ms** | 최악 ≤ 300ms | §6.2 (T1.5), §6.4 | san_follower_tier + san_reroute_planner | test_tier_fsm (16, F4=KPP-2 timing) + test_reroute (13, K1=9µs) | **TST S20-8** ★ | ✅ |
| **KPP-3 통신 p95 ≤ 150ms** | p95 ≤ 150ms | §5 | san_comm_link + san_lte_redundancy | (시뮬레이션) | TST S20-6 | ✅ |
| **KPP-4 재선출 ≤ 10s** | dt ≤ 10s | §11 | san_role_management (D-005) | (role_manager 단위) | TST S20-2 | ✅ |
| **KPP-5 집결 ≥ 95%** | 95% 이상 | §7 (Hungarian) | san_formation | test_formation (Hungarian) | (단계 추적) | ✅ |
| **팬틸트 ≤ 0.05°** | Track 모드 | §8.3 | san_surveillance | test_surveillance (17, PanTilt Track) | (PanTilt standalone) | ✅ |

→ **6/6 KPP 모두 정량 측정 가능** ⭐

---

## 2. SDD §6 Mission BT 추적성

| SDD 항 | 요구사항 | 구현 | 테스트 |
|---|---|---|---|
| §6.1 | Fallback root + 5 priority | san_mission/mission_bt.py | test_mission_bt MB1-15 |
| §6.1 P0 | EmergencyHandler | mission_bt build_emergency_handler | MB2, MB3, MB15 |
| §6.1 P1 | ManualOverride | build_manual_override | MB4 |
| §6.1 P2 | DegradedHealth | build_degraded_health | MB5 |
| §6.1 P3 | BatteryCritical | build_battery_critical | MB6, MB7 |
| §6.1 default | NormalMissionFlow | build_normal_mission_flow | MB1, MB9-14 |
| §6.2 T0 | PREDICTIVE_TRACK | san_follower_tier::TierFsm | test_tier_fsm F1, F2 |
| §6.2 T1 | NORMAL | TierFsm | F3 |
| §6.2 T1.5 | AUTO_REROUTE (★ KPP-2) | TierFsm + san_reroute_planner | F4 (FSM <1ms), K1 (9µs), TST S20-8 |
| §6.2 T2 | CATCH_UP | TierFsm | F6 |
| §6.2 T3 | HARD_CATCH_UP | TierFsm | F7 |
| §6.2 T4 | BREADCRUMB_RECOVERY | TierFsm | F8, F9, F10, TST S20-9 |
| §6.3 | 1초 예측 broadcast 10Hz | san_formation::formation_node | test_formation (Leader velocity 추정) |
| §6.4 | Cost Map ±2m 우회 | san_reroute_planner | test_reroute C1-C5, E1-E5, K1 |

→ **§6 정합도 100%** ✅

---

## 3. SDD §7 대형 (9 Formations + Hungarian) 추적성

| SDD 항 | 구현 | 테스트 |
|---|---|---|
| §7.1 9 대형 (Column/Line/V/Diamond/Echelon×2/Box/VeeInverted/FreeSpread) | san_formation::formations.cpp | test_formation Formations.* |
| §7.2 4 preset (협로/정찰/광역/돌격) | san_formation::formations.cpp | test_formation Presets.* |
| §7.3 Hungarian O(n³) | san_formation::hungarian.cpp | test_formation Hungarian.* |
| §7.4 자동 재할당 (alignment > 2m) | formation_node::onAssignTick | test_formation (re-plan trigger) |

→ **§7 정합도 100%** ✅

---

## 4. SDD §8 감시 (360° Sector + Pan-Tilt) 추적성

| SDD 항 | 구현 | 테스트 |
|---|---|---|
| §8.1 8대 360° sector 분배 | san_surveillance::sector_allocator | test_surveillance Sector.* |
| §8.2 Recon/Defence/Assault 모드 | sector_allocator | test_surveillance Modes.* |
| §8.3 Pan-Tilt Track ≤ 0.05° | pan_tilt_controller | test_surveillance PanTilt.Track |
| §8.4 Sweep dps 공식 | pan_tilt_controller | test_surveillance PanTilt.Sweep |
| §8.5 Gap-fill on loss | sector_allocator | test_surveillance Sector.GapFill |
| §8.6 Threat focus | sector_allocator | test_surveillance Sector.ThreatFocus |

→ **§8 정합도 100%** ✅. **Soft Kill 제외 처리**: Engage 모드 = Hard Kill 사격 표적 추적 전용.

---

## 5. 거버넌스 추적성

| DCN | 항목 | 구현 | 검증 |
|---|---|---|---|
| **DCN-2026-001 D-004** | Fire Auth HMAC+2-key+Audit | san_fire_authorization | TST S20-4 |
| **DCN-2026-001 D-005** | 4-Tier Leader 승계 | san_role_management | TST S20-2 |
| **DCN-2026-002 D-007** | 3-Tier C++/rclpy | 28 C++ + 4 rclpy 분리 | colcon build |
| **DCN-2026-002 D-008** | IPC 통일 (mp→0, ShmPool→0) | Phase 2-E 전면 포팅 | grep audit |
| **DCN-2026-002 D-009** | 메트릭 목표 | Phase 2-E 결과 100% | metric report |
| **ADR-006** | IPC 통일 전략 | DCN-2026-002 정합 | — |

→ **거버넌스 6/6 정합 100%** ✅

---

## 6. 사업수요신청서 KPI 추적성 (참조)

본 시스템과 사업수요신청서 4절 「주요성능(목표)」 의 매핑:

### 대드론 방호 KPI

| 사업수요신청서 KPI | 현재 측정 가능? | 책임 패키지 |
|---|---|---|
| 드론 탐지 거리 2km (RF) | ❌ HW 의존 | (외부 RF 스캐너 — TRR1 단계) |
| 드론 식별 정확도 98% | ✅ 시뮬레이션 | san_perception (YOLO) |
| **재머 유효반경 1km** | ❌ **Soft Kill 제외** | — |
| **드론 소프트킬 ≤ 3sec** | ❌ **Soft Kill 제외** | — |
| 드론 하드킬 100m 90% | ❌ HW 의존 | san_fire_authorization + 사격장 |

### 군집 주행 KPI (★ KPP 와 직접 매핑)

| 사업수요신청서 KPI | 본 시스템 KPP | TST 시나리오 |
|---|---|---|
| 대열 유지 평균오차 ≤ 2m | **KPP-1** | TST S20-1 ✅ |
| 근접 위험 회피 반응 ≤ 300ms | **KPP-2** | **TST S20-8** ★ ✅ |
| 군집 제어 통신 지연 ≤ 150ms | **KPP-3** | TST S20-6 ✅ |
| 리더 이탈/고장 시 재구성 ≤ 10s | **KPP-4** | TST S20-2 ✅ |
| 집결 성공률 ≥ 95% | **KPP-5** | Hungarian standalone ✅ |
| 팬-틸트 추적 오차 ≤ 0.05° | **팬틸트 KPP** | PanTilt standalone ✅ |

→ **사업수요신청서 군집 주행 KPI 6/6** 모두 측정 가능 ⭐

---

## 7. PDR 평가 시 제시 가능한 정량 evidence

| 항목 | 수치 |
|---|---|
| 총 패키지 | **32** (D-007 3-Tier 정합) |
| 총 메시지 | **36** (IDS 정합 86%) |
| Standalone 테스트 케이스 | **370 함수** (실측) |
| Integration 시나리오 (TST S20) | **9** (6 기존 + 3 신규 PDR-6) |
| KPP 측정 가능 | **6/6** ⭐ |
| SDD §6/§7/§8 정합도 | **100%** ✅ |
| 종합 SDD 정합도 | **98%** |
| 거버넌스 (DCN) 정합 | **6/6** ✅ |
| CI 자동화 (GitHub Actions) | **3 workflows** (standalone + ros2-ci + integration) |
