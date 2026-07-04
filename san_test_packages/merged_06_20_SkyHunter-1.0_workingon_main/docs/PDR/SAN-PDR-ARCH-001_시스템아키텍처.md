# SAN v1.5 — 시스템 아키텍처 명세서

> **문서 ID**: SAN-PDR-ARCH-001 Rev.A
> **목적**: PDR 평가 시 제시할 시스템 구조도 + 패키지 의존성 + 메시지 흐름
> **권원**: SDD-SWARM v1.5, IDS-CMD v1.5, DCN-2026-001/-002
> **범위 제한**: **Soft Kill (RF 재머) 제외** — JammingCommand 메시지 정의는 존재하나 실 사용처 없음

---

## 1. 4-Tier 시스템 아키텍처

```mermaid
flowchart TB
    subgraph T4["Tier 4 — Test & Integration"]
        T4_INT["san_integration_tests<br/>(9 TST S20 시나리오)"]
        T4_REG["san_l5_regression<br/>(L5 회귀)"]
    end

    subgraph T3["Tier 3 — Application (Mission)"]
        T3_MIS["san_mission<br/>(BT Fallback root - SDD §6.1)"]
        T3_OPS["san_operation_control<br/>(D-004 Fire Auth orchestrator)"]
        T3_FA["san_fire_authorization<br/>(HMAC + 2-key + Audit)"]
        T3_RM["san_role_management<br/>(D-005 Modified Raft + Hub 승계)"]
    end

    subgraph T2["Tier 2 — Coordination (Swarm)"]
        T2_FORM["san_formation<br/>(Hungarian + 9 대형 - SDD §7)"]
        T2_SURV["san_surveillance<br/>(360° sector + 4 mode - SDD §8)"]
        T2_TIER["san_follower_tier<br/>(6-state FSM - SDD §6.2)"]
        T2_REROUTE["san_reroute_planner<br/>(T1.5 cost map - SDD §6.4)"]
        T2_HUB["san_hub_orchestrator<br/>(D-007 3-Tier)"]
        T2_LTE["san_lte_redundancy<br/>(WiFi6 + LTE + LoRa 삼중화)"]
    end

    subgraph T1["Tier 1 — Drivers / IO"]
        T1_UN["san_unitree_driver<br/>(Go2 SDK)"]
        T1_RTK["san_rtk_gnss<br/>(F9P)"]
        T1_NT["san_ntrip_client<br/>(RTCM3)"]
        T1_IMU["san_imu_driver"]
        T1_CAM["san_cameras<br/>(IMX678 + Thermal)"]
        T1_LIDAR["san_lidar<br/>(Robosense E1)"]
        T1_VID["san_video_sender<br/>(GStreamer)"]
        T1_SLAM["san_slam + san_hub_slam<br/>(Local + Aggregated)"]
        T1_COMM["san_comm_link + san_comm + san_hub_comm<br/>(저수준 통신)"]
        T1_CM["san_costmap<br/>(4-layer Local Cost Map)"]
        T1_PERC["san_perception<br/>(YOLO + DeepSORT)"]
        T1_HD["human_detector<br/>(Detection)"]
    end

    subgraph T0["Tier 0 — Infrastructure"]
        T0_MSG["combat_robot_msgs<br/>(36 message types)"]
        T0_BR["san_bringup<br/>(squadron.launch.py)"]
        T0_COS["combat_robot_operation_system"]
        T0_SC["swarm_coordinator<br/>(top-level)"]
    end

    T4_INT --> T3
    T4_INT --> T2
    T3 --> T2
    T2 --> T1
    T1 --> T0_MSG
    T2 --> T0_MSG
    T3 --> T0_MSG
    T0_BR --> T1
    T0_BR --> T2
    T0_BR --> T3

    classDef pdrAdd fill:#fff4e6,stroke:#d97706,stroke-width:2px,color:#000
    class T3_MIS,T2_FORM,T2_SURV,T2_TIER,T2_REROUTE pdrAdd
```

**주황색 = PDR 준비 단계에서 신설 / 확장**

---

## 2. 군집 8대 역할 아키텍처

```mermaid
graph LR
    OP["운용병사<br/>Galaxy Tab S9<br/>(WiFi)"]

    subgraph SWARM["8대 군집 (S1 + Hub + 6 Followers)"]
        S1["S1 Leader<br/>(Unitree Go2)"]
        HUB["Hub UGV<br/>(D-005 승계 대상)"]
        F1["S3 Follower"]
        F2["S4 Follower"]
        F3["S5 Follower"]
        F4["S6 Follower"]
        F5["S7 Follower"]
        F6["S8 Follower"]
    end

    OP <-->|"WiFi 6 + LTE"| S1
    OP <-->|"WiFi 6 + LTE"| HUB
    S1 -.->|"FollowerTarget 10Hz<br/>+ Breadcrumb"| HUB
    HUB -.->|"Mesh broadcast"| F1
    HUB -.->|"Mesh broadcast"| F2
    HUB -.->|"Mesh broadcast"| F3
    HUB -.->|"Mesh broadcast"| F4
    HUB -.->|"Mesh broadcast"| F5
    HUB -.->|"Mesh broadcast"| F6
    F1 <-->|"Mesh"| F2
    F2 <-->|"Mesh"| F3

    classDef leader fill:#dbeafe,stroke:#2563eb,color:#000
    classDef hub fill:#fef3c7,stroke:#d97706,color:#000
    classDef follower fill:#dcfce7,stroke:#16a34a,color:#000
    class S1 leader
    class HUB hub
    class F1,F2,F3,F4,F5,F6 follower
```

| 역할 | 대수 | 핵심 책임 |
|---|---|---|
| **Leader (S1)** | 1 | Unitree Go2, FollowerTarget broadcast, 대형 결정 |
| **Hub UGV** | 1 | D-005 Leader 승계 대상, SLAM 통합, 비디오 중계 |
| **Followers** | 6 | 자율 추종 + 360° sector 감시 |

D-005 Modified Raft: Leader 사망 시 Hub 가 **4-tier 우선순위** 로 승계 (1순위: 직전 LeaderID, 2: 최저 ID, 3: 최고 SLAM 건강도, 4: 최고 battery%).

---

## 3. SDD §6 BT + 5-Tier 자율 회피 흐름

```mermaid
flowchart TB
    START(("BT tick 10Hz"))
    START --> ROOT["Selector (Fallback root)"]
    
    ROOT --> P0{"P0<br/>EmergencyStop?"}
    P0 -->|Yes| P0_ACT["Stand + WaitForRelease"]
    P0 -->|No| P1{"P1<br/>ManualMode?"}
    
    P1 -->|Yes| P1_ACT["Forward cmd_vel"]
    P1 -->|No| P2{"P2<br/>HealthCritical?"}
    
    P2 -->|Yes| P2_ACT["RTH or Stand"]
    P2 -->|No| P3{"P3<br/>Battery ≤30%?"}
    
    P3 -->|Yes| P3_ACT["Stand (Follower)<br/>or Hub Takeover"]
    P3 -->|No| NORM["NormalMissionFlow"]
    
    NORM --> PRECHK{"PreCheck<br/>PTP+RTK+SLAM+Sensors"}
    PRECHK -->|Pass| ROLE["ResolveRole<br/>(Leader / Hub / Follower)"]
    PRECHK -->|Fail| FAIL["FAILURE"]
    
    ROLE --> TIER_FSM["Tier FSM 평가<br/>T0/T1/T1.5/T2/T3/T4"]
    
    TIER_FSM --> T15{"obstacle?"}
    T15 -->|Yes| REROUTE["san_reroute_planner<br/>±2m lateral evasion<br/>★ KPP-2 ≤ 300ms"]
    T15 -->|No| TRACK["Body-frame PID<br/>또는 catchup 가속"]
    
    classDef priority fill:#fee2e2,stroke:#dc2626,color:#000
    classDef normal fill:#dcfce7,stroke:#16a34a,color:#000
    classDef kpp fill:#fef3c7,stroke:#d97706,stroke-width:3px,color:#000
    class P0,P1,P2,P3,PRECHK priority
    class ROLE,TIER_FSM normal
    class REROUTE kpp
```

---

## 4. KPP 측정 인프라 — 6/6 측정 가능 ✅

```mermaid
graph LR
    subgraph KPP["6 KPP 정량 측정 경로"]
        K1["KPP-1<br/>대열 ≤ 2m"]
        K2["KPP-2<br/>회피 ≤ 300ms ★"]
        K3["KPP-3<br/>통신 p95 ≤ 150ms"]
        K4["KPP-4<br/>재선출 ≤ 10s"]
        K5["KPP-5<br/>집결 ≥ 95%"]
        K6["팬틸트 ≤ 0.05°"]
    end

    K1 --> M1["FormationStatus<br/>.avg_alignment_error_m"]
    K2 --> M2["RerouteNode<br/>KPP-2 timing log<br/>+ TST S20-8"]
    K3 --> M3["TST S20-6<br/>E2E latency"]
    K4 --> M4["TST S20-2<br/>Leader failover"]
    K5 --> M5["Hungarian assignment<br/>+ FormationStatus"]
    K6 --> M6["PanTilt Track 모드<br/>last_track_error_deg"]

    classDef kpp fill:#fff4e6,stroke:#d97706,color:#000
    classDef measure fill:#dbeafe,stroke:#2563eb,color:#000
    class K1,K2,K3,K4,K5,K6 kpp
    class M1,M2,M3,M4,M5,M6 measure
```

---

## 5. Soft Kill 제외 — 명시적 처리

본 시스템에서 **RF 재머를 활용한 Soft Kill 은 현재 개발 범위 외**. 영향:

| 항목 | 처리 |
|---|---|
| `JammingCommand.msg` (IDS §3.4) | 메시지 정의는 유지 (legacy compatibility) |
| RF 재머 제어 노드 | **신규 패키지 생성 없음** |
| Soft Kill BT 액션 | **NormalMissionFlow 에 분기 없음** |
| Surveillance Engage 모드 | **Hard Kill 사격 표적 추적 전용** |
| 드론 무력화 경로 | **Hard Kill 만**: san_perception → san_surveillance Track → san_fire_authorization (D-004) |

---

## 6. 거버넌스 정합

| DCN | 영향 | 정합도 |
|---|---|---|
| DCN-2026-001 D-004 | Fire Auth HMAC + 2-key + Audit | **100%** ✅ |
| DCN-2026-001 D-005 | 4-Tier Leader 승계 | **100%** ✅ |
| DCN-2026-002 D-007 | 3-Tier C++/rclpy 분리 | **100%** ✅ |
| DCN-2026-002 D-008 | IPC 통일 (multiprocessing→0, ShmPool→0) | **100%** ✅ |
| DCN-2026-002 D-009 | 메트릭 목표 달성 | **100%** ✅ |
| ADR-006 | IPC 통일 전략 | **100%** ✅ |
