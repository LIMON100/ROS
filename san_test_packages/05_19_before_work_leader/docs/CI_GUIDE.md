# SkyHunter v1.5 — CI/CD Guide

> **작업일**: 2026-05-12 (Phase 2-E 완료 후)
> **권원**: ADR-006, DCN-2026-002

---

## 개요

3 단계 워크플로우 + 로컬 hooks 로 코드 품질 자동 검증:

| 워크플로우 | 트리거 | 소요 시간 | 검증 범위 |
|---|---|---|---|
| **Standalone Tests** | push, PR | <5 분 | 100 gtest + 53 pytest (pure logic) |
| **ROS 2 CI** | push, PR | 15-30 분 | colcon build + test (전체 워크스페이스) |
| **Lint** | push, PR | <3 분 | clang-format, ruff, yamllint |
| **pre-commit** (로컬) | git commit | 초 단위 | 위 lint + 일반 hygiene |

---

## 1. Standalone Tests (`.github/workflows/standalone-tests.yml`)

ROS 2 없이 순수 로직 테스트를 빠르게 검증. PR 의 1차 게이트.

### 작동 방식

`.github/scripts/run_standalone_gtest.sh` 와 `run_standalone_pytest.sh` 가:

1. **모든 패키지의 test/ 디렉토리 스캔**
2. **자동 분류**:
   - `#include <rclcpp/rclcpp.hpp>` 가 있으면 → SKIP (ROS 2 CI 가 처리)
   - 없으면 → 컴파일 + 실행
3. **결과 집계**: PASS / FAIL / SKIP count

### Phase 2-E 누적 standalone 테스트

| 패키지 | gtest | pytest |
|---|---|---|
| san_lte_redundancy | 13 (AT) | - |
| san_rtk_gnss | 15 (NMEA) | - |
| san_ntrip_client | 8 (RTCM3) | - |
| san_imu_driver | 10 (Binary frame) | - |
| san_cameras | 11 (Frame meta) | - |
| san_lidar | 10 (LRF) | - |
| san_comm_link | 12 (LinkHealthMonitor) | - |
| san_hub_orchestrator | 21 (Swarm + Threat) | - |
| san_fire_authorization | 46 (HMAC + 2-key + Audit) | - |
| san_mission | - | 21 |
| san_perception | - | 19 |
| san_ble_control | - | 13 |
| san_slam_fusion | - | 11 |
| **합계** | **146+** | **64+** |

### 수동 실행

```bash
bash .github/scripts/run_standalone_gtest.sh
bash .github/scripts/run_standalone_pytest.sh
```

---

## 2. ROS 2 CI (`.github/workflows/ros2-ci.yml`)

`osrf/ros:humble-desktop` 컨테이너에서 전체 워크스페이스 빌드 + 테스트.

### 작동 방식

1. **`rosdep install`** — 패키지 의존성 자동 해결
2. **`colcon build --symlink-install`** — Release 모드 빌드
3. **`colcon test`** — 모든 ament_cmake_gtest + ament_python pytest 실행
4. **squadron-launch-smoke** — `squadron.launch.py` 5초 실행 후 SIGTERM, 노드 UP 라인 검증

### 트리거

- `main`, `develop`, `release/*` 브랜치 push
- 위 브랜치 대상 PR
- 수동 (`workflow_dispatch`)

---

## 3. Lint (`.github/workflows/lint.yml`)

| Job | 도구 | 대상 |
|---|---|---|
| cpp-format | clang-format-14 | `ros/.../san_*/{include,src,test}/*.{cpp,hpp,h}` |
| python-lint | ruff (E,F,W,I,N,UP) | `ros/.../san_*/san_*/`, `test/`, `launch/` |
| yaml-lint | yamllint | `config/`, `.github/workflows/` |

**현재 정책**: 경고만 (soft). 워크스페이스 안정화 후 `--exit-zero` 제거하여 strict 전환.

---

## 4. pre-commit (`.pre-commit-config.yaml`)

로컬 개발자 측에서 commit 전 자동 검증:

```bash
# 설치 (1회)
pip install pre-commit
pre-commit install

# 수동 실행 (전체 파일)
pre-commit run --all-files
```

**구성된 hooks**:
- trailing-whitespace, end-of-file-fixer
- check-yaml, check-merge-conflict
- check-added-large-files (>500 KB 차단)
- clang-format (C++)
- ruff + ruff-format (Python)
- shellcheck (.sh)

---

## 신규 패키지 추가 시 체크리스트

1. **테스트 파일 작성**
   - C++: `package/test/test_*.cpp` — 가능하면 pure-logic 만 include (no rclcpp.hpp)
   - Python: `package/test/test_*.py` — pure-logic 만 import (no rclpy)
2. **CMakeLists.txt 또는 setup.py 에 테스트 등록**
   - C++: `ament_add_gtest(test_xxx test/test_xxx.cpp src/xxx.cpp)`
   - Python: `tests_require` 또는 자동 discovery
3. **CI 변경 불필요** — 스크립트가 자동 스캔

신규 standalone-runnable 테스트는 다음 push 부터 자동 검증.

---

## 트러블슈팅

### Q. Standalone 워크플로우가 내 테스트를 SKIP 한다.

A. 테스트 파일에 `#include <rclcpp/rclcpp.hpp>` 가 있는지 확인. 있으면 ROS 2 CI 가 처리. 없는데도 SKIP 된다면 `#include "<pkg>/<module>.hpp"` 로 참조하는 모듈의 .cpp 파일이 rclcpp 에 의존하는지 확인.

### Q. ROS 2 CI 가 너무 느리다.

A. `concurrency.cancel-in-progress: true` 로 진행 중인 워크플로우를 새 푸시가 취소함. PR 에서 여러 번 푸시해도 마지막 push 만 완료까지 실행.

### Q. clang-format 차이가 있다고 한다.

A. 로컬에서 `clang-format -i <file>` 또는 `pre-commit run clang-format --all-files` 실행 후 커밋.

---

## 측정 — Phase 2-E 종료 시점

| 지표 | 값 |
|---|---|
| 워크플로우 수 | 3 (standalone, ros2, lint) |
| 자동화된 테스트 수 | 210+ (146 gtest + 64 pytest) |
| 평균 PR 검증 시간 | ~20 분 (3 워크플로우 병렬) |
| Lint 경고만 (soft 모드) | clang-format, ruff |
| pre-commit hooks | 7개 |
