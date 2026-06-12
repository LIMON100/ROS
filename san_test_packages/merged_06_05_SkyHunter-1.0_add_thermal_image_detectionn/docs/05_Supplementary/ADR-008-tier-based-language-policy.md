# ADR-008 — Tier-based Language Policy for SkyHunter

> **Status**: Accepted
> **Date**: 2026-05-23
> **Decider**: 김태근 (PM, ㈜스카이오토넷)
> **Trigger**: Claude Code 의 PR #2 (DCN-2026-017 RTH) 분석 중 `san_mission` 의
> `ament_python` build_type (DCN-2026-002 D-007) 과 PR 사양의
> `san_mission/src/rth_action_node.cpp` (C++) 사이의 구조적 충돌 발견.
> **Related**: [[ADR-006]] (IPC unification — `rclcpp_action` 을 cross-language
> 표준 boundary 로 명시), [[ADR-007]] (RMW FastDDS 통합), DCN-2026-002 D-007
> (첫 rclpy 노드 `san_mission`), DCN-2026-017 (`san_rth` 신규 C++ 패키지).

---

## 1. Context

기존 PM 방침은 "C++ for all code" 단일언어 정책이었다. 그러나
DCN-2026-017 PR #2 의 구조 검토 과정에서 `san_mission` 패키지가 이미
다음과 같이 못박혀 있음을 재확인했다.

  - `san_mission/package.xml` `<build_type>` = `ament_python`
    (DCN-2026-002 D-007 — "first rclpy node" 의 reference template).
  - `mission_node.py` + `mission_bt.py` — 검증된 Python 구현, 회귀
    테스트 23 건 통과 중.
  - PR #2 사양이 `san_mission/src/rth_action_node.cpp` (C++) 을
    요구 → `ament_python` 과 양립 불가.

이 상황에서 3 가지 선택지를 비교했다.

| Option | 내용 | Trade-off |
|---|---|---|
| A | Hybrid — `san_mission` 을 `ament_cmake_python` 로 전환 | DCN-2026-002 D-007 의 ample_python 결정을 뒤집음, 회귀 테스트 재검증 필요. |
| **B** | **신규 패키지 `san_rth` (순수 C++ ament_cmake)** | **clean separation, `san_mission` 보존, ROS action 으로 cross-language 통신.** ★ |
| C | `san_mission` 전체를 C++ 로 재작성 | 2~3 주 sprint, Demo Day 일정 충돌, 회귀 위험. |

### 1.1 업계 표준 참조

Production ROS 2 시스템들이 C++ + Python hybrid 를 일상적으로 사용함을
확인했다:

  - **Boston Dynamics Spot SDK** — C++ controllers + Python mission scripts
  - **NVIDIA Isaac ROS** — C++ realtime + Python orchestration
  - **Open Robotics Nav2** — C++ + `BehaviorTree.CPP` XML
  - **Autoware** — C++ + Python (planning/perception tools)

세 가지 공통 패턴:
  1. C++ — realtime / safety-critical (lidar, EKF, controllers)
  2. Python — mission orchestration / behavior trees / scripting
  3. ROS interfaces (msg/srv/action) — 깔끔한 ABI boundary

### 1.2 SkyHunter 사업 컨텍스트

  - 신속시범사업 (rapid pilot demonstration), 양산이 아닌 데모.
  - Demo Day 9/4 + Phase-7 stabilization 9/18 + business closure 9/30.
  - Critical path: Leader Go2 integration W9-W14 — 일정 압박 높음.
  - `san_mission` 은 이미 검증되어 있고 23 건 회귀 테스트 통과 중.

---

## 2. Decision

단일언어 C++ mandate 대신 **Tier-based language policy** 를 채택한다.

### 2.1 Tier 1 — Realtime / Safety-critical: **C++ mandatory**

Latency, determinism, 또는 safety failure 의 영향이 critical 한 모듈:

  - `san_lidar`, `san_costmap` (lidar processing, costmap)
  - `san_localization` (EKF, robot_localization-based fusion)
  - `san_rtk_gnss` (NMEA parser, RTK heading)
  - `san_hub_slam` (Bayesian voting, swarm SLAM)
  - `san_role_management` (audit, role gating)
  - `san_lte_redundancy` (LTE failover)
  - `san_nav2` (MPPI controller, costmap)
  - `san_fire_authorization` (military-grade auth)
  - `human_detector` (YOLO, ByteTrack, gimbal PID)
  - `swarm_coordinator` (swarm pose aggregation)
  - `san_unitree_driver` (Leader Go2 SDK)
  - **`san_rth` (new — this DCN)**

### 2.2 Tier 2 — Mission / Orchestration: **C++ preferred for new, Python acceptable for existing**

Mission logic, behavior trees, demo orchestration:

  - **New code**: C++ default (예: DCN-2026-016 `gate_demo_orchestrator`)
  - **Existing verified Python**: defect 발견 전까지 유지
    - `san_mission/mission_node.py` (DCN-2026-002 D-007)
    - `san_mission/mission_bt.py`
  - Bridge between C++ and Python: ROS msg/srv/action (clean ABI)

### 2.3 Tier 3 — Test / Tools / Debug: **Either acceptable**

  - C++ gtest — performance-sensitive (load test, latency benchmark)
  - Python pytest — high-level integration test
  - Per-test 판단

### 2.4 Configuration

  - Launch: XML preferred (`.launch.xml`) for new
  - 기존 `.launch.py`: 수정 시 기회 봐서 점진 변환
  - Params: YAML
  - ADRs: Markdown

### 2.5 Migration policy

  - Production Python: defect 발견 전까지 유지
  - New code: Tier 가이드 준수
  - Refactor: explicit DCN 이 인가했을 때만 (예: Tier 2 Python → Tier 1 C++ promotion)

---

## 3. Compliance — "Is this code Tier 1?" checklist

새 모듈을 시작할 때 다음 체크리스트를 적용한다:

```
Q1. ROS 2 callback 또는 timer 가 < 50 ms deadline 내에서 동작하는가? → Tier 1
Q2. safety-critical 하드웨어 (motor, gimbal, gun) 를 다루는가?        → Tier 1
Q3. raw sensor 데이터를 > 10 Hz 로 처리하는가?                          → Tier 1
Q4. 이 코드의 실패가 사람/재산에 위해를 가하는가?                       → Tier 1
Q5. 상위 mission 을 orchestrate 하는가 (BT, state machines)?           → Tier 2
Q6. test, fixture, visualization 인가?                                  → Tier 3

Tier 1 질문 중 하나라도 YES → Tier 1 (C++ mandatory).
```

---

## 4. Consequences

### 4.1 Positive

  - 업계 표준 정렬 (Nav2, Autoware, Isaac 패턴).
  - 검증된 mission 코드 (`mission_node.py` + `mission_bt.py`) 보존.
  - Demo Day 일정에 2~3 주 sprint 충격 없음.
  - 향후 PR reviewer 에게 명확한 가이드.
  - ABI cleanliness: ROS interfaces 는 language-agnostic.
  - 신규 C++ 패키지 (`san_rth`) 가 일정 희생 없이 품질 demonstrate.

### 4.2 Negative

  - 패키지 수 증가 (`san_mission`, `san_rth`, …) — monolithic 보다 많음.
  - 두 테스트 프레임워크 (gtest + pytest) 를 CI 가 모두 실행.
  - 개발자가 자기 코드가 어느 Tier 인지 이해해야 함.

### 4.3 Neutral

  - Hybrid 는 ROS 2 community norm — 신규 위험 아님.
  - Performance: Python mission orchestration 은 realtime-critical 이 아니므로 영향 미미.

---

## 5. References

  - DCN-2026-002 D-007 — first rclpy node (`san_mission` ament_python).
  - DCN-2026-017 — this DCN (`san_rth` new C++ package).
  - [[ADR-006]] §4 — IPC unification (rclcpp_action layer 가 cross-language boundary).
  - [[ADR-007]] — RMW FastDDS 통합.
  - 업계 비교: Nav2 (C++ + BehaviorTree.CPP XML), Autoware (C++).

---

## 6. Rollback

Tier-based policy 가 문제가 될 경우:

1. 이 ADR 를 revert.
2. monoglot C++ policy 복원.
3. Python → C++ migration sprint (3~4 주) 별도 일정 편성.
4. Risk: Demo Day timeline 충격.

---

— 김태근 (PM, ㈜스카이오토넷)
*2026-05-23 Accepted*
