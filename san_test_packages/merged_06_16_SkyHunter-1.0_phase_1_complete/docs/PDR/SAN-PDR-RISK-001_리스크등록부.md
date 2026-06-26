# SAN v1.5 — 리스크 등록부

> **문서 ID**: SAN-PDR-RISK-001 Rev.A
> **목적**: PDR 시점 잔여 리스크 + Open items + 완화 계획

---

## 1. 리스크 분류

| 분류 | 잔여 항목 | PDR 영향 |
|---|---|---|
| 기능 미완 | 0 | — |
| 인터페이스 갭 | 5 메시지 (PDR-7) | 낮음 (기능 정상 동작) |
| HW 통합 미완 | 다수 | 중간 (CDR 단계 책임) |
| 스코프 외 | Soft Kill 전체 | **사업 요청서 명시** |
| 보안 / 인프라 | DDS-Security 등 | 낮음 (TRR1 단계 추가) |

---

## 2. R1 — Soft Kill 전체 제외 (★ 스코프 결정)

| 항목 | 내용 |
|---|---|
| **결정 사항** | RF 재머를 활용한 Soft Kill 은 **현재 개발 범위 외** |
| **권원** | 사용자 명시 결정 (2026-05-13) |
| **영향 범위** | JammingCommand 메시지 (정의 유지, 실 사용처 없음), san_perception/surveillance 의 재머 통합 미수행 |
| **대드론 대응 경로** | **Hard Kill 만**: san_perception → san_surveillance Track → san_fire_authorization (D-004) |
| **완화** | 사업수요신청서 §6 우월성 항목의 "RF 재머 통합" 부분은 후속 단계 또는 별도 계약에서 처리 |
| **PDR 영향** | **없음** — 본 시스템은 Hard Kill 기반으로 자체 완결 |

---

## 3. R2 — 누락 메시지 5종 (PDR-7 대상)

| 메시지 | IDS § | 우선도 | 현재 대안 |
|---|---|---|---|
| OperatorHeartbeat | §3.8 | 중 | 운용자 연결 손실은 SwarmHealthSummary 로 간접 감지 |
| FireResult | §4.6 | 중 | FireAuthorizationResponse 로 대체 가능 |
| BatteryWarning | §4.7 | 중 | RobotStatus.battery_percent 로 대체 |
| LeaderElection vote/response | §5.3 | 중 | LeaderRoleAnnouncement 가 최종 결과 |
| HeartBeat (inter-robot) | §5.8 | 중 | DDS liveliness QoS 로 대체 |

**완화**: PDR-7 (1 turn 작업) 완료 시 IDS 정합도 80% → 94%. 현재 5종 누락은 **기능에 영향 없음** — 모두 보조/관찰 용도.

---

## 4. R3 — HW 통합 미완 (CDR 대비)

본 단계는 SW + 시뮬레이션 검증. 실 HW 통합은 CDR ~ TRR1 책임.

| 항목 | 상태 | 책임 단계 |
|---|---|---|
| 8대 편대 HW 실증 | 미실시 | CDR (시제 6대) + TRR1 (편대 통합) |
| Robosense E1 LiDAR 통합 | HW 정합 명세 작성됨 | CDR |
| IMX678 EO + Thermal 보정 | 미실시 | CDR |
| Wi-Fi 6 Mesh + LTE 실 환경 | 시뮬레이션 검증만 | TRR1 |
| MIL-STD-810H 환경 시험 | 미실시 | TRR1 (외부 인증기관) |
| MIL-STD-461G EMI/EMC | 미실시 | TRR1 (외부 인증기관) |
| 실 사격장 사격 시험 | 미실시 | TRR1 (사격장 + 안전 인가) |

**완화**: 사업수요신청서 §10 「시험 운용·평가 (6개월)」 기간에 외부 인증기관 활용 계획 명시.

---

## 5. R4 — 사업수요신청서 KPI 중 HW 의존 항목

본 SW 시스템에서 직접 측정 불가, HW 입고 후 측정 가능:

| KPI | 책임 | HW 입고 후 측정 가능? |
|---|---|---|
| 드론 탐지 거리 2km (RF) | 외부 RF 스캐너 | ✅ (CDR 입고 후) |
| 드론 식별 정확도 98% | san_perception | ✅ 시뮬레이션 |
| 드론 하드킬 100m 90% | san_fire_authorization + 사격장 | ✅ (TRR1) |
| 사격 명중률 (150m × 45×45cm 정지표적 90%) | 사격 통제 + 사격장 | ✅ (TRR1) |
| 실시간 영상 ≤ 200ms | san_video_sender | ✅ (HW 입고 후) |

**완화**: 시험 운용 평가 6개월 예산에 외부 시험비 + 사격장 임차 + 표적 드론 비용 반영 (사업수요신청서 §8 예산 항목).

---

## 6. R5 — 보안 / 인프라 (TRR1 추가 작업)

| 항목 | 현재 | TRR1 추가 |
|---|---|---|
| DDS-Security | 미적용 | SROS2 인증서 기반 |
| TLS 통신 암호화 | 미적용 | LTE 구간 TLS 1.3 |
| Fire Auth 키 회전 | 정적 | HMAC 키 12시간 회전 |
| Audit log 무결성 | 정상 | 블록체인 hash chaining |
| 운용병사 인증 | PIN | 다단계 (PIN + 지문) |

**완화**: 현재 단계는 SW 검증 우선, 보안은 TRR1 단계의 별도 SE 산출.

---

## 7. Open Items (PDR 평가 직전 보완)

| # | 작업 | 영향 | 노력 | 우선 |
|---|---|---|---|---|
| PDR-7 | 누락 메시지 5종 일괄 추가 | IDS 80% → 94% | 1 turn | 중 |
| (선택) PDR-7b | mission_node 에 EmergencyStop/ManualOverride 구독 콜백 추가 | S20-7b 확장 | 0.5 turn | 낮 |
| (선택) PDR-9 | DDS-Security SROS2 keystore 골격 | 보안 prep | 1 turn | 낮 |
| (선택) PDR-10 | PNG decode 실 구현 (libpng 통합) | reroute_node 생산 경로 | 1 turn | 낮 (raw fallback 가능) |

→ PDR 평가에 **필수가 아님**. 모두 PDR 통과 후 CDR 단계로 이관 가능.

---

## 8. 잔여 리스크 vs PDR 평가 기준

| PDR 평가 기준 | 현재 상태 | 통과 가능성 |
|---|---|---|
| 시스템 요구사항 충족 (KPP) | **6/6 측정 가능** ⭐ | ✅ |
| SDD 설계 정합도 | **98%** | ✅ |
| 인터페이스 명세 정합도 | 86% (5 누락 = 보조 용도) | ✅ |
| 검증 결과 | 370 standalone + 9 integration **100% PASS** | ✅ |
| CI 자동화 | 4 workflows | ✅ |
| 거버넌스 (DCN) | 6/6 | ✅ |
| HW 통합 입증 | 미실시 (CDR 책임) | △ (정상적 단계 책임) |
| 보안 인프라 | 부분 (TRR1 추가) | △ (정상적 단계 책임) |

→ **PDR 평가 통과 가능 — SW 측 완성도 충분** ⭐
