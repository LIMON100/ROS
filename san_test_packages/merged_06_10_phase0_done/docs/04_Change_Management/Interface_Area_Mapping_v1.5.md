# 인터페이스 영역 매핑 — SkyHunter v1.5

> **작성**: 2026-05-12 / **버전**: v1.5 / **권원**: DCN-2026-001
> **목적**: 사용자가 문의한 5개 인터페이스 영역(API / Network / Sensor / URD / USC)이 현 v1.5 문서 체계에서 어디에 어떻게 cover되는지 명시. 별도 명명된 인터페이스 문서를 신규 작성하지 않고 기존 5개 문서로 운용함을 확정.

---

## 1. 영역별 cover 매핑

### 1.1 API (Application Programming Interface)

| 하위 영역 | 권원 문서 (v1.5) | 절 |
|---|---|---|
| Inter-robot ROS2 메시지 IDL (Protobuf 호환 .proto) | **SAN-IDS-CMD-001 v1.5** | §3 (Operator 메시지), §4 (Operator HW 인터페이스), §5 (Robot-internal/inter-robot, 16종), §6 (Infra/System), §7 (QoS 정책) |
| Android 운용병사 앱 ↔ Hub API | **SAN-IDS-CMD-001 v1.5** | §3.1 RobotCommand, §3.2 OperatorAck, §3.3 VideoStreamRequest, §3.4 SwarmStatusBundle |
| Hub UGV (S2) 듀얼 SBC 내부 IPC | **SAN-SDD-SWARM-001 v1.5** | §3.3 SBC1 ↔ SBC2 ZeroMQ 토픽 (`/internal/ipc/*`) |
| AI 추론 인터페이스 (RK3588 NPU / Hailo M.2) | **SAN-SDD-SWARM-001 v1.5** | §6 AI Inference Path (TensorRT INT8 / Hailo-8 .hef 변환) |
| ROS 2 토픽 / 서비스 / 액션 카탈로그 | **SAN-IDS-CMD-001 v1.5** | §8 카탈로그 (Operator 12 + Inter-Robot 16 + Infra 11 = **39종**) |
| Limp Mode 발사 명령 인증 (HMAC-SHA256) | **SAN-SDD-SWARM-001 v1.5** | §5.7.2.1 (v1.5 신규), **SAN-OPS-SOP-001 v1.5** §7.11 |

✅ **결론**: API 인터페이스 권원은 **SAN-IDS-CMD-001 v1.5**가 단일 권원. v1.5에서 §5.15/§5.16 신규로 4-tier Leader 승계 + Hub-Deputy 이중화 broadcast 메시지 정식 정의 완료.

### 1.2 Network (네트워크 토폴로지 + 통신 프로토콜)

| 하위 영역 | 권원 문서 (v1.5) | 절 |
|---|---|---|
| 하이브리드 통신망 (LTE + Wi-Fi 6 Mesh + LoRa) | **SAN-SDD-SWARM-001 v1.5** | §5 (Communication Architecture), §5.5 LTE 이중화 |
| LTE 모뎀 fail-over chain (`lte_backup_chain`) | **SAN-SDD-SWARM-001 v1.5** | §5.5 — v1.5: 모든 UGV (S2~S8) LTE 표준 탑재, chain = [3, 4, 5, 6, 7, 8] |
| Wi-Fi 6 Mesh routing (BATMAN-adv) | **SAN-SDD-SWARM-001 v1.5** | §5.3 |
| LoRa 비상 채널 (telemetry-only, 2 km) | **SAN-SDD-SWARM-001 v1.5** | §5.4 |
| CycloneDDS NIC binding (eth0 default, wlan0 priority 50) | **SAN-SDD-SWARM-001 v1.5** | §10.2 (D3 결정 권원) |
| GStreamer SRT 운용병사측 / UDP 로봇 간 | **SAN-SDD-SWARM-001 v1.5** | §10.1.1 (★v1.5 신규 — 표준 툴체인) |
| iptime 라우터 / 정적 라우팅 설정 | **deployment/iptime_setup.md** (보조) | — |
| LTE auto-rate 조정 정책 | **deployment/lte_auto_rate.md** (보조) | — |
| Hub 듀얼 SBC 토폴로지 | **deployment/hub_dual_sbc.md** (보조) | — |

✅ **결론**: Network 권원은 **SAN-SDD-SWARM-001 v1.5 §5 + §10.1.1**이 메인. 상세 설정은 deployment/*.md 보조 가이드로 분리.

### 1.3 Sensor (센서 인터페이스 + 사양)

| 하위 영역 | 권원 문서 (v1.5) | 절 |
|---|---|---|
| EO/IR 카메라 (광학 30배 줌 + 열화상 640×512) | **SAN-SDD-SUR-001 v1.5** | §2 카메라 사양, §2.3 Pan-tilt 안정화 |
| Robosense E1 솔리드스테이트 LiDAR | **SAN-SDD-SUR-001 v1.5** | §4 LiDAR 사양 (VFOV 90°, 탐지거리 75m, IP67), **전술로봇_HW구성도** |
| RF 스캐너 (광대역 SDR, 2 km 드론 탐지) | **SAN-SDD-SWARM-001 v1.5** | §6 (AI 객체 식별 + RF 신호 처리 통합) |
| 거리측정 레이저 (탄도 솔루션 입력) | **SAN-SDD-SWARM-001 v1.5** | §7 사격제어 인터페이스 |
| 360° Pan-tilt 마운트 (정확도 0.01°) | **SAN-SDD-SUR-001 v1.5** | §2.3 + **전술로봇_HW구성도** |
| Sector 분배 알고리즘 (감시 영역) | **SAN-SDD-SUR-001 v1.5** | §3.2 전방 180° 분배, §3.3 Hub 후방 180°, §3.2★v1.5 Deputy 그림자 담당 |
| SLAM 통합 (multirobot_map_merge + g2o) | **SAN-SDD-SUR-001 v1.5** | §5 SLAM, **SAN-SDD-SWARM-001 v1.5** §9 |
| 센서 캘리브레이션 절차 | **calibration_procedure.md** (보조) | — |

✅ **결론**: Sensor 권원은 **SAN-SDD-SUR-001 v1.5**(감시 측면) + **SAN-SDD-SWARM-001 v1.5 §4** UGV Platform Specs(섀시 측면)가 분담. v1.5에서 Deputy UGV 그림자 담당 추가.

### 1.4 URD (User Requirements Document)

| 하위 영역 | 권원 문서 | 절 |
|---|---|---|
| 사업수요신청서 (KPP, 운용군, 예산, 기간) | **262차_신속시범사업사업수요신청서_주스카이오토넷_260108.pdf** | 표지~§12 |
| KPP 정량 지표 (35개) | **사업수요신청서** | §4 주요성능(목표) |
| 작전 시나리오 (공격/방어 모드) | **사업수요신청서** | §3 운용 개념(안) |
| 군집 제어 협업 운용 | **사업수요신청서** | §3 라) 군집 제어 기반 협업 |
| 운용병사-로봇 통제 개념 | **SAN-OPS-SOP-001 v1.5** | §1~§3 (운용 핵심 규칙) |
| SRR 발표 자료 (KPP 매트릭스 + WBS) | **SAN-PRD-SRR-001 v1.1.pptx** | 전체 |
| ⚠️ v1.5 보완 권장 | 별도 신규 문서 (선택) | 아래 §3 참조 |

⚠️ **부분 cover**: 사업수요신청서 PDF가 사실상 URD 역할을 수행하나, 형식적인 "URD 문서"는 별도 작성되어 있지 않음. PDR 시 평가위원 요청에 따라 별도 작성 검토.

### 1.5 USC (Use Case / User Scenario)

| 하위 영역 | 권원 문서 | 절 |
|---|---|---|
| 작전 시나리오 (공격/방어 모드) | **사업수요신청서 PDF** | §3 운용 개념 (실전 시나리오 6단계: 초기화 → 자율 군집 기동 → 위협 탐지 → 돌격 → 정밀 타격 → 자율 복귀) |
| 운용병사 표준 운용 절차 | **SAN-OPS-SOP-001 v1.5** | §3 Day-1 ~ Day-N 운용 절차 |
| FAQ 기반 운용 시나리오 14종 | **SAN-OPS-SOP-001 v1.5** | §7 (v1.1: 4종, v1.3: 2종, v1.4: 2종, v1.5: §7.11 Limp Mode 신규 = 총 14종) |
| 단독/소대급/로봇집단/MUM-T 전술 운용 4종 | **사업수요신청서 PDF** | §3 다) 전술 운용 방식 |
| 시연 절차 (DEMO 6-Phase) | **SAN-OPS-SOP-001 v1.5** | §8 |
| 색상 가이드 (지도 마커) | **SAN-OPS-SOP-001 v1.5** | §9.1 부록 |

✅ **결론**: USC 권원은 **SAN-OPS-SOP-001 v1.5** + **사업수요신청서 PDF** 조합으로 충분.

---

## 2. 인터페이스 영역별 v1.5 변경 영향

| 영역 | v1.5 주요 변경 영향 |
|---|---|
| **API** | §5.15 LeaderRoleAnnouncement / §5.16 HubRoleAnnouncement 신규 (Total 35→**39 메시지**). API 클라이언트는 새 메시지 핸들러 추가 필요. |
| **Network** | `lte_backup_chain` [3,5] → **[3,4,5,6,7,8]** (모든 UGV LTE 표준). LTE SIM 6개로 증가. `cycldds_uri` NIC 우선순위 변경 없음. |
| **Sensor** | Deputy UGV (S3) Hub와 동일 HW 적용 (E1 LiDAR + Pan-tilt + EO/IR). 신규 그림자 담당 sweep 알고리즘 적용. |
| **URD** | 속도 KPP 표면적 7 km/h (사업수요신청서) ↔ 내부 설계 10 km/h (HW 스펙). PDR 시 KPP 정정 신청 또는 마진 확보 입장 선택 필요. |
| **USC** | §7.11 Limp Mode FAQ 신규 (Option A 발사 허용 정책 — v1.4 "발사 금지" 입장 폐기). 운용병사 교육자료 갱신 필요. |

---

## 3. 별도 신규 문서 신규 작성 필요성 검토

| 후보 신규 문서 | 작성 권장도 | 사유 |
|---|---|---|
| **SAN-API-001** (External API Specification) | ❌ 불필요 | IDS v1.5 §3, §8.1이 외부 API(Android 앱)를 충분히 정의. 중복 위험. |
| **SAN-NET-001** (Network Architecture) | △ 선택 | SDD v1.5 §5 + §10.1.1 + deployment/*.md 조합으로 cover됨. PDR 시 평가위원이 별도 통신망 문서 요청한다면 추출 작성. |
| **SAN-SENS-001** (Sensor Specifications) | △ 선택 | SDD-SUR + 전술로봇_HW구성도 PDF로 cover됨. 추가 작성이 더 명확성 제공할 수 있음. |
| **SAN-URD-001** (User Requirements) | ⚪ 권장 | 현재 사업수요신청서 PDF가 URD 역할을 하나 형식적 분리가 좋음. PDR 단계에서 작성. |
| **SAN-USC-001** (Use Case Document) | ⚪ 권장 | 현재 OPS + 사업수요신청서로 분산. 단일 USC 문서로 통합하면 검토 효율 증가. PDR 단계에서 작성. |

### 권장 사항

**현 PDR 준비 단계에서는 5개 핵심 문서(SDD-SWARM + IDS + TST + OPS + SDD-SUR) 체계로 충분**합니다. 별도 SAN-URD-001, SAN-USC-001 작성은 평가위원 사전 검토 요청이 있을 경우 후속 작업으로 진행 권장.

---

## 4. 본 매핑 문서의 위치

본 문서는 v1.5 산출물 패키지에 동봉되며, PDR 검토자료의 일부로 활용 가능:

- 평가위원이 "API 명세는 어디 있나요?"라고 물으면 → IDS v1.5 §3, §5, §8을 안내
- 평가위원이 "Network 토폴로지는?"이라고 물으면 → SDD-SWARM v1.5 §5 + §10.1.1을 안내
- 평가위원이 "Sensor 사양은?"이라고 물으면 → SDD-SUR v1.5 §2, §4 + 전술로봇_HW구성도 PDF를 안내

이렇게 응답할 수 있도록 본 매핑 표를 사전 인지하는 것이 PDR 시 효율적입니다.

---

## 5. 변경 이력

| 버전 | 일자 | 변경 내용 |
|---|---|---|
| v1.5 (초안) | 2026-05-12 | DCN-2026-001 반영. 5개 인터페이스 영역 매핑 정리. API/Network/Sensor 신규 문서 작성 보류 결정 (PDR 시 재검토). |
