# DCN-2026-002 — Design Change Notice

> **권원**: SAN-CFG-001 (Configuration Management Process)
> **승인**: 김태근 대표이사 (㈜스카이오토넷)
> **발효일**: 2026-05-12
> **개정 대상**: DCN-2026-001 D-007 (표준 툴체인)
> **수정 종류**: Amendment (D-007 본문 교체 + 신규 D-008 추가)

---

## 1. 개요 (Summary)

DCN-2026-001 D-007 의 "Python 프로토타입은 코딩 표준에서 폐기" 항목을
**3-Tier 아키텍처 + IPC 통일 정책**으로 개정한다.

본 개정의 핵심은 **언어 선택 문제 (C++ vs Python) 가 아니라 IPC 통일
문제 (DDS 통일 vs Queue/SHM 혼용)** 임을 명시화하는 데 있다.

| 항목 | DCN-2026-001 D-007 (이전) | DCN-2026-002 D-007 (현재) |
|---|---|---|
| 표준 툴체인 | Ubuntu 22.04 + ROS 2 Humble + C++ only | Ubuntu 22.04 + ROS 2 Humble + 3-Tier 언어 정책 |
| 강제 사항 | Python 폐기 | **모든 IPC 는 ROS 2 (rclcpp/rclpy)** |
| 안전-critical | C++ | C++ (변경 없음) |
| Application / AI | C++ 강제 | **rclpy 허용** |
| Queue/SHM/manager.dict | 허용 | **금지** (`multiprocessing.*` 전면 폐기) |

---

## 2. 배경 (Background)

### 2.1 DCN-2026-001 D-007 의 문제점

DCN-2026-001 D-007 "Python 프로토타입 폐기" 문구는 다음 부작용을 야기:

1. **작업량 과대 추정**:
   - Phase 2-E 가 209개 Python 파일 전면 C++ 전환으로 해석됨
   - 실제 작업 추정 6~12개월 (사업 일정 24개월 중 25~50% 점유)
   - 시제개발 (18개월) + 운용시험 (6개월) 균형 붕괴

2. **AI/perception 개발 속도 저하**:
   - NPU 추론 (YOLOv8 등) C++ 직접 작성 시 Python 대비 디버깅 속도 50%↓
   - NPU 추론 자체는 C++ kernel 호출이라 Python wrapper overhead 무의미

3. **권원 ↔ 코드 정합 평가 시 역질문 위험**:
   - PDR 평가위원: "왜 application/test 까지 C++ 강제?"
   - 안전 분석: "safety-critical 경계가 불명확하면 trust boundary 정의 곤란"

### 2.2 실제 코드 분석 결과

`main.py` + `core/ipc.py` 정밀 분석에서 발견된 실제 문제:

| 항목 | 발견 |
|---|---|
| `rclpy` 사용 여부 | **0** (전체 codebase) |
| IPC 메커니즘 | **3종 혼용**: `mp.Queue` (~40 토픽) + `ShmPool` (camera/lidar) + `manager.dict()` (auth state) |
| Process 간 통신 | host-local only (mp 한계). Multi-host (LTE/Mesh) 불가 |
| QoS / 타입 안전 / 표준 도구 | 모두 부재 |

**핵심 통찰**: 진짜 문제는 언어 (Python) 가 아니라 **자체 제작 IPC 가
ROS 2 생태계와 단절**된 것이다. ROS 2 위에 올라가면 언어는 모듈별
적절히 선택해도 동작한다.

### 2.3 권원 정합 측면

| v1.5 권원 요구 | mp.Queue 기반 | ROS 2 기반 |
|---|---|---|
| SDD-SWARM §5.5 LTE 이중화 (multi-host) | ❌ 불가능 | ✅ DDS native |
| SDD-SWARM §5.6 Leader 승계 (mesh broadcast) | ❌ 불가능 | ✅ DDS broadcast |
| IDS v1.5 §5 (35+ 토픽 권원) | ❌ 토픽 정의 무의미 | ✅ 1:1 매핑 |
| IDS v1.5 §7 QoS 권원 | ❌ 표현 수단 없음 | ✅ rclcpp::QoS |

**즉 ROS 2 IPC 통일은 v1.5 권원 정합의 필수 전제 조건**이며, 언어
전환은 그 부수 효과일 뿐이다.

---

## 3. 결정 사항 (Decisions)

### D-007 (개정): 3-Tier 표준 툴체인

표준 툴체인을 다음 3 계층으로 정의한다:

#### Tier 1 — Hard Real-Time / Safety-Critical (C++ 의무)
대상 모듈:
- 발사 인가 (`san_fire_authorization`) — Phase 2-D 완료
- 역할 승계 (`san_role_management` — Leader/Hub/Deputy/Limp Mode) — Phase 1 완료
- LTE 이중화 (`san_lte_redundancy`) — Phase 1 완료
- SLAM / Hub SLAM (`san_slam`, `san_hub_slam`) — Phase 1 완료
- Cost map (`san_costmap`) — Phase 1 완료
- 회귀 시험 (`san_l5_regression`) — Phase 1 완료
- HW 제어 루프 (Unitree Go2 motor command, RCWS gimbal, LiDAR driver)

언어 / 빌드 / IPC:
- **언어**: C++17 의무
- **빌드**: ament_cmake + rclcpp
- **IPC**: ROS 2 topic/service/action 의무
- **테스트**: gtest 의무

근거:
- 결정론적 메모리 / 실시간 보장 (GC 없음)
- DO-178C / IEC 61508 / MIL-STD-882E high-integrity zone 정합
- 발사 / 살상 무력 결정에 GC pause 부적합

#### Tier 2 — Soft Real-Time / Coordination (C++ 권장, rclpy 허용)
대상 모듈:
- 임무 상태 기계 (`san_mission`)
- 군집 조정 (`swarm_coordinator`)
- 센서 융합 (`san_sensor_fusion`)
- 표적 추적 응용 (NPU 출력 후처리, 위 시 좌표 계산 등)

언어 / 빌드 / IPC:
- **언어**: C++17 권장, rclpy 허용
- **빌드**: ament_cmake (C++) 또는 ament_python (Python)
- **IPC**: **ROS 2 topic/service/action 의무** (언어 무관)
- **테스트**: gtest 또는 pytest

근거:
- 100 Hz 미만 제어 루프 (10 Hz 표적 추적, 1 Hz 조정) — Python rclpy 충분
- rclpy 와 rclcpp 는 동일 DDS backend → 언어 경계 invisible

#### Tier 3 — Application / AI / Tooling (rclpy 권장)
대상 모듈:
- Perception orchestration (`san_perception` — NPU 입력/출력 wrapper)
- 시뮬레이션 (`sim/`)
- 분석 / 시각화 도구 (`tools/`, `scripts/`)
- 운용자 UI bridge
- 시험 시나리오 작성 (`tests/`)

언어 / 빌드 / IPC:
- **언어**: rclpy 권장 (C++ 가능)
- **빌드**: ament_python
- **IPC**: **ROS 2 topic/service/action 의무** (언어 무관)
- **테스트**: pytest

근거:
- NPU 추론 자체는 C++ kernel → Python wrapper overhead 무의미 (< 1%)
- 개발 속도 / 가독성 / 시각화 라이브러리 (matplotlib, plotly) 우위
- 평가위원에게 "AI/test 는 Python, 안전-critical 은 C++"이 명료

### D-008 (신규): IPC 통일 의무

다음 IPC 메커니즘 사용을 **전면 금지**한다:

1. `multiprocessing.Process` (시스템 부트스트랩 포함)
2. `multiprocessing.Queue` (모든 토픽)
3. `multiprocessing.shared_memory` / `ShmPool` (camera/lidar 등)
4. `multiprocessing.Manager().dict()` 등 모든 Manager 객체

대신 다음을 의무화한다:
- **inter-process 통신**: ROS 2 topic / service / action / parameter
- **intra-process 고대역 (camera frame)**: ROS 2 intra-process zero-copy (IPC=true)
- **공유 상태**: ROS 2 parameter (read-only 분배) 또는 service (mutation)
- **systemd**: process 생명주기 관리 (`Restart=on-failure`)
- **ROS 2 launch**: 다중 노드 기동 (`squadron.launch.py`)

예외:
- 단일 노드 내 thread-local 자료구조 (`std::mutex`, `std::atomic`) 는 ROS 2 와 무관 → 허용
- 외부 의존 SDK (Unitree SDK, RKNN, HailoRT) 가 내부적으로 shared memory 사용하는 것은 SDK 내부 구현 → 무관

### D-009 (신규): 권원 ↔ 코드 정합 측정 기준

다음 측정 기준을 통해 v1.5 권원 정합을 정량 평가한다:

| 측정 항목 | 목표 (PDR 시점) | 측정 방법 |
|---|---|---|
| Active `multiprocessing.*` import | **0** | `grep -rn "^from multiprocessing\|^import multiprocessing" --include="*.py"` |
| Active `ShmPool` 사용 | **0** | `grep -rn "ShmPool\|shared_memory" --include="*.py"` |
| Tier 1 모듈의 비-C++ 코드 | **0** | 패키지별 `*.py` 카운트 (test/ 제외) |
| Tier 2+3 모듈의 비-ROS 2 IPC | **0** | 위 grep 적용 |
| IDS v1.5 권원 토픽 ↔ 코드 매핑 | 100% | 매뉴얼 inspection |

---

## 4. 영향 받는 권원 문서 (Affected Reference Docs)

| 문서 | 변경 |
|---|---|
| SDD-SWARM v1.5 §10.1.1 (표준 툴체인) | "C++ only" → "3-Tier (위 D-007 참조)" |
| SDD-SWARM v1.5 §10.1.2 (신규 추가) | IPC 통일 의무 (D-008) |
| IDS v1.5 §1 (Communication Overview) | "DDS 단일 백엔드" 명문화 |
| OPS-SOP v1.5 §3.1 (배치 모드) | systemd unit 표준 명시 |

본 DCN 발효 즉시 적용 — v1.5.1 docx 갱신은 PDR 자료 작성 시점에 통합 수행.

---

## 5. 영향 받는 코드 (Affected Code)

### 5.1 즉시 영향 없음 (이미 정합)

Phase 1 ~ 2-D 에서 완성된 Tier 1 C++ 코드는 모두 D-007 개정 본문에
이미 정합 — 변경 불요:

- `san_fire_authorization` (Phase 2-D) — ✅
- `san_role_management` (Phase 1) — ✅
- `san_lte_redundancy` (Phase 1) — ✅
- `san_slam` / `san_hub_slam` (Phase 1) — ✅
- `san_costmap` (Phase 1) — ✅
- `swarm_coordinator` (Phase 1) — ✅
- `san_l5_regression` (Phase 1) — ✅

### 5.2 Phase 2-E 작업 범위 재정의 (큰 변경)

**이전 (DCN-2026-001 D-007 해석)**:
> 209 Python 파일 전면 C++ 전환 — 6~12개월 작업

**현재 (DCN-2026-002 D-007 정합)**:
> 209 Python 파일 중:
> - Tier 1 후보 (HW 제어 / 안전): ~12 파일 → **C++ 의무 전환**
> - Tier 2 후보 (조정 / 융합): ~20 파일 → 점진 평가
> - Tier 3 (perception/test/sim): ~150 파일 → **rclpy 전환만**
> - 전환 불요 (개발 도구, infra): ~27 파일
>
> **모든 파일의 공통 작업**: mp.Queue/ShmPool → ROS 2 topic 으로 교체

추정 작업량: **3개월** (사업 일정 정합)

### 5.3 `main.py` 처리

기존 multiprocessing 기반 부트스트랩 → ROS 2 launch 로 대체:
- 신규: `ros/src/skyautonet/combat_robot_system/san_bringup/launch/squadron.launch.py`
- `main.py` 는 archive 로 이동 (`archive/v15_python_prototype/`)
- systemd unit 으로 squadron.launch.py 기동

---

## 6. 호환성 (Backwards Compatibility)

### 6.1 즉시 호환 불가 항목 (의도된 break)

- `mp.Queue` 기반 임의 코드는 본 DCN 발효 후 신규 작성 금지
- 신규 노드는 모두 rclcpp 또는 rclpy 의무

### 6.2 점진 deprecation

- 기존 Python adapter 들 (`UnitreeGo2Adapter`, `LteModemAdapter` 등) 은
  Phase 2-E 작업 기간 동안 동작 상태 유지
- 새 rclpy/rclcpp 노드로 1:1 대체 후 archive 디렉토리로 이동
- v1.5.1 (또는 v1.6) 시점에 archive 디렉토리 완전 삭제

### 6.3 Python prototype 의 권원적 위치

본 DCN 발효 후 Python prototype 의 위치:
- ❌ 권원에 등재된 안정 코드가 아님
- ❌ 안전 분석 / 검증 / 사업 평가 대상 아님
- ✅ Phase 2-E 진행 중 fallback 용도로만 보유
- ✅ archive 이동 후 historical reference 용도

---

## 7. 승인 (Approval)

| 역할 | 이름 | 일자 | 비고 |
|---|---|---|---|
| 사업 책임 | 김태근 대표이사 | 2026-05-12 | (서명) |
| 권원 책임 | (PM) | 2026-05-12 | |
| 안전 책임 | (Safety lead) | 2026-05-12 | D-008 IPC 통일 검토 |
| 시스템 책임 | (CTO / Architect) | 2026-05-12 | 3-Tier 아키텍처 검토 |

---

## 8. 부록: 권장 진행 순서

DCN-2026-002 발효 후 권장 작업 우선순위:

| 우선 | 작업 | 추정 turn | Tier |
|---|---|---|---|
| 1 | ADR-006 작성 (IPC Unification Strategy) — 본 DCN 의 기술 근거 | 1 | 거버넌스 |
| 2 | `san_bringup` 패키지 + `squadron.launch.py` 초안 (기존 ROS C++ 노드만 포함) | 2 | Tier 1 |
| 3 | HW 어댑터 C++ rclcpp 전환 (Unitree, LTE, RTK, NTRIP) | 6~8 | Tier 1 |
| 4 | 센서 어댑터 C++ rclcpp 전환 (IMX678, Thermal, LiDAR, IMU) | 6~8 | Tier 1 |
| 5 | Perception / Mission rclpy 노드 전환 (Python 유지) | 4~6 | Tier 2-3 |
| 6 | 시뮬레이션 / 시험 / 분석 rclpy 전환 (Python 유지) | 2~3 | Tier 3 |
| 7 | `main.py` archive 이동 + 최종 회귀 시험 | 1 | 정리 |

총 추정: **24~30 turn** (= 3 개월 작업)

---

## 9. 참조

- DCN-2026-001 (원안)
- ADR-006 (예정) IPC Unification Strategy
- SDD-SWARM v1.5 §10.1.1 표준 툴체인
- IDS v1.5 §1 Communication Overview
- 사업수요신청서 §9 SE 기반 기술 검토 단계
