# SAN v1.5 — SW 검증 종합 Posture

> **문서 ID**: SAN-PDR-VPOSTURE-001 Rev.A
> **목적**: SW 단계 검증 활동 전수 점검 + 갭 해소 결과
> **작성일**: 2026-05-13

---

## 1. 검증 활동 전수 매트릭스 — 14 영역

| # | 검증 활동 | 이전 상태 | 갭 해소 후 | 자동화 |
|---|---|---|---|---|
| 1 | **Unit 테스트 (Standalone)** | ✅ 370 함수 | ✅ 380+ 함수 | CI standalone-tests.yml |
| 2 | **Integration 테스트 (TST S20)** | ✅ 9 시나리오 | ✅ **10 시나리오** ⭐ | CI integration-tests.yml |
| 3 | **Build 검증 (colcon)** | ✅ | ✅ | CI ros2-ci.yml |
| 4 | **Lint (clang-format + ruff)** | ✅ | ✅ | CI lint.yml |
| 5 | **KPP 측정 (6 KPP)** | ✅ 6/6 | ✅ 6/6 | CI kpp.yml |
| 6 | **ARM64 cross build** | ✅ | ✅ | CI arm64.yml |
| 7 | **Regression** | ✅ | ✅ | CI regression.yml |
| 8 | **ROS 콜백 wiring (B1)** | ❌ mission_node 2 subs | ✅ **7 subs + cmd_vel pub** ⭐ | TST S20-7b |
| 9 | **BT priority injection 검증** | ❌ standalone만 | ✅ **TST S20-7b 신규** ⭐ | CI integration-tests.yml |
| 10 | **Code Coverage 측정** | ❌ 미설정 | ✅ **coverage.yml 신규** ⭐ | CI coverage.yml |
| 11 | **Sanitizer (ASAN/UBSAN)** | ❌ 미설정 | ✅ **sanitizers.yml 신규** ⭐ | CI sanitizers.yml (weekly) |
| 12 | API 문서 (Doxygen/Sphinx) | ❌ | ❌ (선택적) | — |
| 13 | TSAN (thread sanitizer) | ❌ | ❌ (ROS 2 mutex FP) | — |
| 14 | Topic 그래프 audit | ❌ | ❌ (CDR 단계) | — |

→ **갭 해소: 4 영역 신규 추가** (8, 9, 10, 11)

---

## 2. 본 단계 갭 해소 작업

### 2.1 B1 — `mission_node` ROS Wiring (★ 가장 큰 갭)

**문제**: PDR-4 에서 SDD §6.1 Fallback root BT 를 구현했으나, `mission_node` 가 `ExtendedMissionContext.priority` 를 populating 하는 subscription 이 **0개** → BT 우선순위 subtree (P0-P3) 가 ROS 환경에서 절대 트리거되지 않음.

**해소**:
- **5 신규 subscriptions**:
  - `EmergencyStop` → `priority.emergency_active` (SCOPE 확인 후)
  - `ManualOverrideCommand` → P1 + P2/P3 분기 (CMD_VEL/HALT/RETURN/RELEASE)
  - `SwarmHealthSummary` → `priority.health_critical` (slam_sbc/comm_sbc failed)
  - `TierStatusChange` → `priority.current_tier` (informational)
  - `HubRoleAnnouncement` → `priority.is_hub` + takeover_available
- **신규 publisher**: `~/cmd_vel` (Twist) — P0 stop + P1 manual passthrough
- **`tree_type=fallback` 일 때만** 활성화 → backward-compatibility 유지

### 2.2 TST S20-7b — 실제 BT priority injection

**문제**: 기존 S20-7 은 boot smoke 만 (mission_node 가 부팅하면 PASS). BT priority 의미론을 **rclpy 환경에서 검증하는 통합 테스트 없음**.

**해소**: S20-7b 신규
- Phase 1: Baseline (follower 가 /cmd_vel 미출력 확인)
- Phase 2: `EmergencyStop` 주입 → /cmd_vel = (0, 0) 검증
- Phase 3: `ManualOverrideCommand OVERRIDE_RELEASE` 주입 → emergency 해제
- Phase 4: `ManualOverrideCommand OVERRIDE_CMD_VEL` 주입 → 정확한 cmd_vel 통과 검증
- Phase 5: `ManualOverrideCommand OVERRIDE_RELEASE` 주입 → manual 종료

**의미**: PDR-4 의 15 standalone pytest (MB1-MB15) 가 검증한 우선순위 의미론을 **rclpy 환경에서도** 검증.

### 2.3 Coverage CI workflow

**문제**: line/branch coverage 측정 없음. PDR 평가에서 "테스트 충분도" 정량 evidence 부족.

**해소**: `.github/workflows/coverage.yml` 신규
- C++ 측: `--coverage` flag + gcov + lcov 통합
- Python 측: pytest-cov
- HTML 리포트 CI artifact 업로드 (30일 보관)
- 임계값: PDR 단계 ≥ 60% (CDR ≥ 70%)
- 매 PR 자동 실행

### 2.4 Sanitizer CI workflow

**문제**: memory/UB 이슈는 unit 테스트에서 안 잡힘. PDR 단계에서 메모리 안전성 정량 evidence 없음.

**해소**: `.github/workflows/sanitizers.yml` 신규
- AddressSanitizer (use-after-free, leak, OOB)
- UndefinedBehaviorSanitizer (signed overflow, null deref 등)
- 주간 cron + release 브랜치 + 수동 트리거
- 결과 artifact 14일 보관

**TSAN 제외 사유**: ROS 2 rclcpp 의 mutex 패턴 false positive 다수. 추후 패키지별 선택 활성화.

---

## 3. 종합 SW 검증 지표 — 최종

| 지표 | 값 |
|---|---|
| Standalone 테스트 함수 | **380+** (실측 370 + B1 + S20-7b 보강) |
| Integration 시나리오 | **10** (S20-1~9 + S20-7b) |
| CI Workflows | **10** (기존 8 + coverage + sanitizers) |
| KPP 측정 가능 | **6/6** ⭐ |
| SDD 정합도 | **98%** |
| 거버넌스 (DCN) | **6/6** |
| ROS 콜백 wiring (mission_node) | **7 subs + 2 pubs** ⭐ |

---

## 4. CI Workflow 종합 인벤토리

| Workflow | 트리거 | 시간 | 목적 |
|---|---|---|---|
| `ci.yml` | push/PR | 변동 | 메인 디스패처 |
| `standalone-tests.yml` | push/PR | < 5분 | 380 단위 테스트 |
| `ros2-ci.yml` | push/PR | 15-30분 | 전체 colcon build |
| `integration-tests.yml` | push/PR | ~10분 | TST S20 10 시나리오 |
| `lint.yml` | push/PR | < 2분 | clang-format + ruff |
| `kpp.yml` | push/PR | ~5분 | 6 KPP 정량 측정 |
| `regression.yml` | push/PR | ~10분 | L5 회귀 검증 |
| `arm64.yml` | push/PR | ~30분 | ARM64 cross build (시제 SBC 대비) |
| **`coverage.yml`** ★ | push/PR | 15-25분 | **line/branch coverage 측정** |
| **`sanitizers.yml`** ★ | weekly cron | 30-45분 | **ASAN + UBSAN** |

★ = 본 단계 신규

매 PR 시 자동: standalone + ros2-ci + integration + lint + kpp + regression + arm64 + coverage = **8 workflows**
주간 자동: sanitizers (1 workflow)

---

## 5. PDR 평가 evidence 충실도 변화

### 이전 (PDR-8 완료 시점)

| 평가 항목 | Evidence |
|---|---|
| 알고리즘 정확성 | 370 unit tests |
| 통합 동작 | 9 TST S20 |
| KPP 측정 | 6/6 |
| 거버넌스 | DCN 6/6 |
| **테스트 충분도** | ⚠️ Coverage 미정량 |
| **메모리 안전성** | ⚠️ Sanitizer 미실시 |
| **BT priority 통합 검증** | ⚠️ Standalone 만 |

### 현재 (검증 갭 해소 후)

| 평가 항목 | Evidence |
|---|---|
| 알고리즘 정확성 | 380+ unit tests |
| 통합 동작 | **10** TST S20 (+ S20-7b) |
| KPP 측정 | 6/6 ✅ |
| 거버넌스 | DCN 6/6 ✅ |
| **테스트 충분도** | ✅ **Coverage CI 측정 가능** |
| **메모리 안전성** | ✅ **ASAN + UBSAN 주간 자동** |
| **BT priority 통합 검증** | ✅ **TST S20-7b** ⭐ |

---

## 6. 잔여 갭 + 향후 단계 책임

### CDR 단계 책임 (HW 통합)

| 영역 | 작업 |
|---|---|
| san_unitree_driver | Unitree Go2 실 SDK |
| san_cameras | IMX678 + Thermal 보정 |
| san_lidar | Robosense E1 실 SDK |
| san_imu_driver | ICM-42688P 실 통합 |
| human_detector | Hailo-8 / RK3588 NPU 통합 |
| san_perception | YOLO 실 모델 + 실 입력 |
| san_video_sender | GStreamer 실 pipeline |
| 환경 시험 | MIL-STD-810H / 461G 외주 |

### 선택적 (TRR1 또는 별도)

| 영역 | 작업 |
|---|---|
| API 문서 | Doxygen / Sphinx 골격 |
| Topic 그래프 audit | python tool 작성 |
| QoS profile audit | python tool 작성 |
| DDS-Security | SROS2 keystore |
| TSAN (thread) | 패키지별 선택 적용 |
| PNG decode | libpng 통합 (현재 raw fallback) |

### 추가 SW 작업 (선택)

| 영역 | 노력 |
|---|---|
| 누락 메시지 5종 (FireResult 등) | 0.5 turn |
| Mission BT 실 publication (LeaderAction 5 stub) | 1 turn |
| pose_graph_optimizer (Ceres/g2o) | 2 turn |

---

## 7. 결론

본 단계에서 SW 검증의 **구조적 갭 4개 해소**:

1. ✅ **B1 mission_node ROS wiring** — BT 우선순위 ROS 트리거 활성화
2. ✅ **TST S20-7b** — BT priority 통합 검증
3. ✅ **Coverage CI** — 테스트 충분도 정량화
4. ✅ **Sanitizer CI** — 메모리 안전성 자동 검증

남은 SW 작업은 모두 **HW 통합 의존** 또는 **선택적 보강** — PDR 평가에 필수 아님.

**SW 검증 측면 PDR 통과 가능 상태** ⭐
