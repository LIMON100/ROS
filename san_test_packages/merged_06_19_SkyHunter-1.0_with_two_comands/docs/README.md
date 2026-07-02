# SkyHunter v1.5 산출물 패키지 — 26-2차 신속시범사업

> **사업명**: 군집제어형 개인방호 및 대 드론 지원 소형 전술로봇
> **사업번호**: 26-2차 신속시범사업
> **수행기관**: ㈜스카이오토넷
> **문서 버전**: v1.5 (DCN-2026-001 적용)
> **발행일**: 2026-05-12
> **PM**: 김태근 대표이사 (taegeun.kim@skyautonet.com)

---

## 1. 패키지 구성

본 ZIP은 SkyHunter 사업의 v1.5 개발 산출물 전체를 6개 폴더로 정리한 것입니다.

```
SkyHunter_v1.5_Package/
├── 01_EN_DOCX/          # 영어 DOCX 6개 (편집 가능 마스터)
├── 02_KO_DOCX/          # 한국어 DOCX 6개 (편집 가능 마스터)
├── 03_EN_PDF/           # 영어 PDF 6개 (변경 불가, 배포용)
├── 04_Change_Management/ # 변경 명세서 + 인터페이스 매핑
├── 05_Supplementary/    # 보조 문서 9개 (BOM, ADR, 가이드)
└── 06_HW_Reference/     # HW 참조 (사업수요신청서, HW 구성도)
```

---

## 2. 파일별 안내

### 01_EN_DOCX/ — 영어 DOCX 6개

5개 핵심 v1.5 문서 + 1개 ARCHIVED. Word "Compare" 기능으로 v1.4 ↔ v1.5 변경 추적 가능.

| 파일명 | 내용 |
|---|---|
| `SAN-SDD-SWARM-001_v1.5_EN.docx` | 통합 시스템 설계서 (foundation document) |
| `SAN-IDS-CMD-001_v1.5_EN.docx` | 인터페이스 설계 명세 (39종 메시지) |
| `SAN-TST-INT-001_v1.5_EN.docx` | 통합 시험 명세 (66종 시나리오) |
| `SAN-OPS-SOP-001_v1.5_EN.docx` | 운용 표준 절차서 (14 FAQ) |
| `SAN-SDD-SUR-001_v1.5_EN.docx` | 감시 설계 보충 (sector 분배, SLAM) |
| `SAN_SwarmOperation_Integrated_v1.0_ARCHIVED.docx` | 폐기 통합본 (이력 보존용) |

### 02_KO_DOCX/ — 한국어 DOCX 6개

EN과 동일 권원의 한국어 마스터. 운용병사 / 평가위원 / 한국어 검토자용. v1.0 ARCHIVED는 양국 안내가 이미 표지에 포함되어 EN과 동일 파일.

### 03_EN_PDF/ — 영어 PDF 6개

EN DOCX를 PDF로 변환. 외부 배포 / PM 회람 / 평가위원 사전 검토 / 입찰서 첨부용. 변경 불가 형태로 권원 보존.

### 04_Change_Management/ — 변경관리 (2개)

| 파일명 | 내용 |
|---|---|
| `SkyHunter_v1.5_DCN_2026-05-12.md` | **DCN-2026-001** — v1.4 → v1.5의 7개 결정사항 (D-001 ~ D-007) 매트릭스, KPP 도출값 표, 문서별 변경 요약, 검토 체크리스트 |
| `Interface_Area_Mapping_v1.5.md` | API / Network / Sensor / URD / USC 5개 인터페이스 영역이 5개 v1.5 문서의 어디에 cover되는지 매핑. PDR 시 평가위원 응답용 가이드. |

### 05_Supplementary/ — 보조 문서 9개

핵심 5개 문서를 보완하는 코딩 표준, 자동화 가이드, 부품 명세, 아키텍처 의사결정 기록(ADR).

| 파일명 | 내용 |
|---|---|
| `cost_map_4layer.md` | Cost Map 4-layer 코딩 표준 (★ v1.5: max speed 12→10 km/h 정합) |
| `l5_s18_automation.md` | L5 회귀 자동화 가이드 — S18 (★ v1.5: 권원 참조 갱신) |
| `BOM_v1.1.md` | Bill of Materials v1.1 (변경 없음, LTE/배터리는 그대로) |
| `deployment_modes.md` | deployment_mode 5종 표준 (이미 v1.5 정합) |
| `ADR-001-hub-dual-sbc.md` | Hub UGV 듀얼 SBC 의사결정 |
| `ADR-002-slam-aggregation-period.md` | SLAM 통합 주기 30s → 5s 결정 |
| `ADR-003-gstreamer-srt-vs-webrtc.md` | GStreamer SRT vs WebRTC 선택 |
| `ADR-004-openwrt-vs-stock-firmware.md` | OpenWrt vs 펌웨어 선택 |
| `ADR-005-robosense-e1-vs-mid360.md` | Robosense E1 vs MID360 LiDAR 선택 |

ADR(Architecture Decision Record)는 의사결정 시점의 맥락을 보존하는 immutable 문서로, v1.5 정합 갱신 대상이 아닙니다.

### 06_HW_Reference/ — HW 참조 (2개)

| 파일명 | 내용 |
|---|---|
| `262차_신속시범사업사업수요신청서_주스카이오토넷_260108.pdf` | 사업수요신청서 (2026-01-08 제출, KPP 권원) |
| `전술로봇_HW구성도.pdf` | 전술로봇 HW 구성도 (AVTBOT TinS-13 + Pan/Tilt + Battery + Robosense E1) |

---

## 3. 인터페이스 영역별 권원 (요약)

사용자가 자주 묻는 인터페이스 영역이 어디에 정의되어 있는지 빠른 안내:

| 인터페이스 영역 | 주 권원 | 보조 권원 |
|---|---|---|
| **API** (ROS 2 메시지, Android 앱 API) | SAN-IDS-CMD-001 v1.5 §3, §5, §8 | — |
| **Network** (LTE/Wi-Fi 6/LoRa, CycloneDDS) | SAN-SDD-SWARM-001 v1.5 §5, §10.1.1 | deployment/*.md (보조) |
| **Sensor** (E1 LiDAR, EO/IR, Pan-tilt) | SAN-SDD-SUR-001 v1.5 + SAN-SDD-SWARM-001 v1.5 §4 | 전술로봇_HW구성도.pdf |
| **URD** (User Requirements) | 사업수요신청서 PDF + SAN-PRD-SRR-001 v1.1.pptx | — |
| **USC** (Use Case / Scenario) | SAN-OPS-SOP-001 v1.5 + 사업수요신청서 §3 | — |

자세한 매핑은 `04_Change_Management/Interface_Area_Mapping_v1.5.md` 참조.

---

## 4. v1.5 주요 변경 (7개 결정사항)

| # | 결정 | 영향 |
|---|---|---|
| D-001 | 8대 편대 (Deputy UGV 별도 정의) + 모든 UGV LTE | 편대 표 / lte_backup_chain 갱신 |
| D-002 | UGV 최대 속도 10 km/h (AVTBOT TinS-13 HW 정합) | SDD §4.5 + cost_map_4layer.md |
| D-003 | v1.0 통합본 ARCHIVED (빨간색 박스) | SAN_SwarmOperation_Integrated 폐기 |
| D-004 | Limp Mode 발사 권한 유지 (Option A) | SDD §5.7.2.1 + OPS §7.11 신규 |
| D-005 | IDS §5.15/§5.16 본문 정식 추가 (총 39 메시지) | IDS 4-tier 승계 권원화 |
| D-006 | TST S18-1~6 본문 추가 (총 66 시나리오) | san_l5_regression 자동화 |
| D-007 | 표준 툴체인 명시 (Ubuntu 22.04 + ROS 2 Humble + GStreamer 1.20+) — **DCN-2026-027 로 Ubuntu 24.04 + ROS 2 Jazzy + GStreamer 1.24 로 갱신** | SDD §10.1.1 신규 |

---

## 5. 검증 결과

| 검증 항목 | 결과 |
|---|---|
| XML schema validity (16개 docx) | All validations PASSED ✅ |
| Revision History v1.5 행 (10개 docx) | 모두 존재 ✅ |
| 표지/분류표/Related Docs 버전 핀 | v1.5로 일관 ✅ |
| 보조 문서 v1.5 정합 (cost_map, l5_s18) | 갱신 완료 ✅ |
| 인터페이스 영역 매핑 분석 | 별도 신규 문서 작성 보류 결정 ✅ |
| EN PDF 변환 (6개) | 모두 정상 (총 ~3.2 MB) ✅ |

---

## 6. 사용 권장 시나리오

| 시나리오 | 권장 파일 |
|---|---|
| **PDR 발표 자료** | 03_EN_PDF/*.pdf 전체 + 04/Interface_Area_Mapping_v1.5.md |
| **편집 및 v1.6 준비** | 01_EN_DOCX 또는 02_KO_DOCX 마스터 |
| **운용병사 교육** | 02_KO_DOCX/SAN-OPS-SOP-001_v1.5_KO.docx |
| **평가위원 사전 검토** | 03_EN_PDF/*.pdf 전체 + 04/SkyHunter_v1.5_DCN_2026-05-12.md |
| **외부 발주처 / 협력기관 공유** | 03_EN_PDF/*.pdf (변경 불가) |
| **변경 영향 추적 (v1.4 → v1.5)** | Word "Compare" 기능 + 04/SkyHunter_v1.5_DCN_2026-05-12.md |
| **개발 코드 정합 작업** | 04/SkyHunter_v1.5_DCN_2026-05-12.md §5 체크리스트 |

---

## 7. 다음 단계 (개발 측면)

v1.5 문서 권원 확정 후 코드 정합 우선순위:

1. **`combat_robot_msgs`** — `LeaderRoleAnnouncement.msg`, `HubRoleAnnouncement.msg` 정의 (IDS §5.15/§5.16 권원)
2. **`san_role_management`** — 4-tier Leader 승계 + Hub-Deputy 인수 로직 (SDD §5.6/§5.7 권원)
3. **`san_lte_redundancy`** — `lte_backup_chain` 확장 (SDD §5.5 권원)
4. **`san_slam`** — 토픽 네이밍 정합 (PATCH_A-ME-1)
5. **`swarm_coordinator`** — MAX_ROBOTS=8 / MIN_ROBOTS=4 정합
6. **`san_l5_regression`** — S18-1~6 시나리오 등록 (TST §11 권원)

---

## 8. 연락처

**개발사 ㈜스카이오토넷**
- 대표이사 김태근
- 010-3116-1310
- taegeun.kim@skyautonet.com

**문서 / 변경 명세 문의**
- DCN-2026-001 권원: 본 패키지 04_Change_Management/SkyHunter_v1.5_DCN_2026-05-12.md
- 인터페이스 영역 안내: 본 패키지 04_Change_Management/Interface_Area_Mapping_v1.5.md

---

## 2026-05-12 Governance Updates

### DCN-2026-002 — D-007 Amendment (3-Tier Toolchain + IPC Unification)

`04_Change_Management/SkyHunter_v1.5_DCN-2026-002_2026-05-12.md`

DCN-2026-001 D-007 ("Python prototype 폐기") 를 3-Tier 언어 정책으로 개정.
핵심: 언어 (C++/Python) 선택보다 IPC 통일 (ROS 2 DDS) 이 우선.

- **Tier 1** (HW 제어 / 안전-critical): C++ 의무
- **Tier 2** (조정 / 융합): C++ 권장, rclpy 허용
- **Tier 3** (Application / AI / Tooling): rclpy 권장
- **공통**: 모든 IPC 는 ROS 2 (`multiprocessing.*` 전면 금지)

### ADR-006 — IPC Unification Strategy

`05_Supplementary/ADR-006-ipc-unification-strategy.md`

DCN-2026-002 의 기술적 근거 문서. 5 가지 대안 (전면 C++ / Python only /
ZeroMQ / Hybrid+mp.Queue / 본 결정) 비교 분석 + 구현 가이드.

상세 내용: ROS 2 (DDS) 단일 IPC 통일 + 3-Tier 언어 정책. 작업량
6~12개월 → 3개월 단축, 권원 정합 즉시 달성.
