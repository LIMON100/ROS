# DCN-2026-001 — SkyHunter v1.4 → v1.5 변경 명세

> **Document Change Notice 번호**: DCN-2026-001
> **발행일**: 2026-05-12
> **승인자**: PM (㈜스카이오토넷)
> **적용 범위**: SAN-SDD-SWARM-001, SAN-IDS-CMD-001, SAN-TST-INT-001, SAN-OPS-SOP-001, SAN-SDD-SUR-001, SAN_SwarmOperation_Integrated v1.0 (ARCHIVED)
> **작업 시작 ↔ 완료**: 2026-05-12

---

## 1. 사유

PDR 준비 단계에서 v1.4 문서 일관성 점검 중 다음 항목 정리 필요:
1. 9대 편대(v1.0) ↔ 8대 편대(v1.4) 잔존 참조 일괄 정리
2. SAN_SwarmOperation_Integrated v1.0의 archive 처리
3. Deputy UGV (S3) 사전 지정 (v1.4)에 따른 4-tier Leader 승계 메시지 본문 정식 추가
4. Limp Mode 발사 권한 정책 모호성 해소
5. 표준 OS/미들웨어/영상 툴체인 공식 명시
6. UGV 최대 속도 — 사업수요신청서 7 km/h ↔ HW 스펙 10 km/h 정합 결정

---

## 2. 7개 결정사항 매트릭스

| 결정 # | 영역 | 결정 | 권원 |
|---|---|---|---|
| **D-001** | 편대 구성 | 8대 최대 / 4대 최소(시험). S1 Leader(Go2) + S2 Hub UGV + **S3 Deputy UGV** + S4~S8 Follower UGV(1~5대 가변). **모든 UGV LTE 모뎀 표준 탑재** | SDD §1.2.1, §4.5 |
| **D-002** | UGV 최대 속도 | **10 km/h** (2.78 m/s). 사업수요신청서 7 km/h는 보수적 수치였으며 AVTBOT TinS-13 섀시 실제 HW 스펙(10 km/h)에 정합. Go2(편대 운용 시 1.3 m/s 제한)는 별도 | SDD §4.5 |
| **D-003** | v1.0 통합본 처리 | SAN_SwarmOperation_Integrated v1.0 **ARCHIVED**. 표지에 빨간색 ARCHIVED 박스 + 양국 안내. 후속 모든 검토/인용/구현은 SDD v1.5 사용 | (별도 archive) |
| **D-004** | Limp Mode 발사 | **Option A 확정 — 발사 권한 유지**. HMAC-SHA256 (mesh shared secret, LTE 우회) + Two-key arming + 적색 배너 "LIMP MODE — 화력 허용, 상황인식 저하" + 모든 발사에 limp_mode_fire=true audit log. v1.4 일부 초안의 "발사 금지/RTB 권장" 보수적 해석 폐기 | SDD §5.7.2.1, OPS §7.11 |
| **D-005** | §5.15/§5.16 본문 | **IDS §5.15 LeaderRoleAnnouncement / §5.16 HubRoleAnnouncement 본문 정식 추가**. v1.4까지 개정 이력에만 약속됐던 4-tier 승계 메시지의 IDL + QoS + Topic + Tested-in 정식 권원화. Total 메시지 35 → **39** | IDS §5.15, §5.16, §8.3 |
| **D-006** | S18-1~6 시험 | **TST §11 Deputy + Limp Mode 시나리오 본문 6개 추가** (S18-1~6, 각 ≤ 5~10 s deadline 강제). Scenario Index 60 → **66종**. san_l5_regression 자동화 등록 | TST §11, §12 |
| **D-007** | 표준 툴체인 | **Ubuntu 22.04 LTS + ROS 2 Humble (rmw_cyclonedds_cpp, C++ only) + GStreamer 1.20+ (C API only) + H.265 기본 + SRT/UDP**. Python 프로토타입은 코딩 표준에서 폐기 | SDD §10.1.1 |

---

## 3. KPP / 도출값 표

| 지표 | 변경 전 (v1.4) | 변경 후 (v1.5) | 근거 |
|---|---|---|---|
| **UGV max speed** | **10 km/h (2.78 m/s)** | AVTBOT TinS-13 섀시 HW 스펙 정합 + Cost Map look-ahead 2.5 s 도출 |
| **lte_backup_chain** | [3, 5] (Hub + S3만) | **[3, 4, 5, 6, 7, 8]** (모든 UGV) | D-001 |
| **편대 규모** | 8대 고정 (재정의 후) | **8대 최대 / 4대 최소(시험)** | D-001 |
| **IDS 메시지 수** | 35 | **39** (+ §5.15/§5.16) | D-005 |
| **TST 시나리오 수** | 60 (Sim 54 + Real 6) | **66 (Sim 59 + Real 7)** | D-006 |
| **deployment_mode** | 3종 | **5종** (+ lab_test, development) | D-007 |

### §4.5.1 Cost Map Derived Values (v1.5 재산정)

| 도출값 | 계산식 | v1.5 결과 | v1.4 (12 km/h 가정) |
|---|---|---|---|
| 7 m reach time | 7 m / 2.78 m/s | **2.52 sec** (predictive window) | 2.1 sec (12 km/h 가정) |
| Cost Map coverage | max(7 m, v × 2.5 sec margin) | 7 m baseline | — |
| 5 s 지연 KPP 마진 | 5.0 - 2.5 | **2.5 s** ✅ | 2.9 s |

> **재산정 근거**: 속도 KPP를 10 km/h로 정정하면서 (AVTBOT TinS-13 섀시 HW 스펙 정합 — 사업수요신청서의 보수적 7 km/h 대신 실 HW 성능 채택) 7 m forward coverage는 그대로 유지. 결과적으로 look-ahead 시간이 2.1 s → **2.5 s**로 0.4 s 증가 (안전 마진 약간 개선). Cost Map latency 5 s KPP에 대한 마진은 5.0 - 2.5 = 2.5 s (충분).

---

## 4. 문서별 변경 요약

| 문서 | v1.4 → v1.5 변경 |
|---|---|
| **SAN-SDD-SWARM-001** | §1.2.1 8대 표지 부제 정정 + Deputy UGV 별도 명시, §4.5 속도 10 km/h, §5.5 lte_backup_chain 확장, §5.7.2.1 신규 Limp Mode 발사 정책, §10.1.1 신규 표준 툴체인, 본문 9대 잔존 일괄 정정 |
| **SAN-IDS-CMD-001** | §5.15 LeaderRoleAnnouncement / §5.16 HubRoleAnnouncement 본문 신규 (IDL + QoS + Topic), §8.3 Inter-Robot (12) → (16), Total 35 → 39 |
| **SAN-TST-INT-001** | §11 Deputy + Limp Mode 시나리오 (S18-1~6 본문) 신규 + 요약 매트릭스, §12 (기존 §11) "66종"으로 재번호 |
| **SAN-OPS-SOP-001** | §7.11 신규 Limp Mode FAQ (Option A 확정), Rule #4 3종 → 5종 modes |
| **SAN-SDD-SUR-001** | §3.2 Deputy UGV (S3) 그림자 담당 안내 신규 (Hub와 phase-offset sweep) |
| **v1.0 통합본** | **ARCHIVED 처리** — 표지 상단 빨간색 박스 + 양국 안내. 신규 검토/인용/구현 금지 명시 |

---

## 5. 검토 체크리스트

### A.1 D-001 (편대 구성)

- [ ] SDD-SWARM v1.5 §1.2.1 표에서 Deputy UGV (S3) 행 확인
- [ ] SDD-SWARM v1.5 §5.5 lte_backup_chain [3, 4, 5, 6, 7, 8] 확인
- [ ] 모든 5개 v1.5 문서의 본문에서 9대 잔존 참조 없음 확인

### A.2 D-002 (10 km/h)

- [ ] SDD-SWARM v1.5 §4.5 표에서 10 km/h 확인
- [ ] SDD-SWARM v1.5 §4.5.1 Cost Map look-ahead 재산정 (2.5 s) 확인
- [ ] 사업관리: 사업수요신청서 KPP 7 km/h 정합 정책 결정 (옵션 a: 마진 확보 / 옵션 b: KPP 정정 신청)

### A.3 D-003 (v1.0 archive)

- [ ] v1.0 통합본 표지에 빨간색 ARCHIVED 박스 확인
- [ ] 파일명 `_ARCHIVED` 접미사 확인
- [ ] 형상관리: v1.0 archived → `docs/archive/` 이동 또는 SVN/Git label 처리

### A.4 D-004 (Limp Mode 발사)

- [ ] SDD-SWARM v1.5 §5.7.2.1 박스 확인 — Option A 명시
- [ ] OPS-SOP v1.5 §7.11 FAQ 확인 — "발사 가능" 명시
- [ ] 운용병사 교육자료 갱신 필요 (별도 작업)

### A.5 D-005 (§5.15/§5.16)

- [ ] IDS v1.5 §5.15 LeaderRoleAnnouncement 본문 (IDL 포함) 확인
- [ ] IDS v1.5 §5.16 HubRoleAnnouncement 본문 (IDL 포함) 확인
- [ ] IDS v1.5 §8.3 (16) + Total 39 확인
- [ ] 코드: `combat_robot_msgs` 패키지에 .msg 파일 신규 정의 필요

### A.6 D-006 (S18-1~6)

- [ ] TST v1.5 §11 6개 시나리오 본문 확인 (각 Setup / Pass Criteria / deadline)
- [ ] TST v1.5 §11.7 요약 매트릭스 확인
- [ ] TST v1.5 §12 "Scenario Index (66 total)" 확인
- [ ] 코드: `san_l5_regression/scenario_runner.cpp` 자동 등록 검증

### A.7 D-007 (표준 툴체인)

- [ ] SDD-SWARM v1.5 §10.1.1 신규 절 확인 — OS / 미들웨어 / 영상 모두 명시
- [ ] CI: Ubuntu 22.04 LTS GitHub Actions 환경 fixed
- [ ] 코딩 표준: `docs/coding_standards/` Python 프로토타입 폐기 안내 (별도 작업)

---

## 6. 변경 권한 / 형상관리

- 본 DCN 적용 후 SAN-SDD-SWARM-001, SAN-IDS-CMD-001, SAN-TST-INT-001, SAN-OPS-SOP-001, SAN-SDD-SUR-001 5개 문서는 모두 v1.5로 통일
- v1.6 변경은 별도 DCN 발행 후 진행
- Word "Compare" 기능으로 v1.4 ↔ v1.5 변경 추적 가능
- SVN/Git 커밋 메시지에 "DCN-2026-001 적용" 명시 권장
