# DCN-2026-027 — 개발 플랫폼 이관 (Ubuntu 24.04 / ROS 2 Jazzy / Boardcon EM3588)

> **Status**: **APPROVED (ratified)** — PM 승인 2026-06-15 (김태근). 구현 PR #266 머지(`c48bba2`, 4/4 CI green, Jazzy 빌드 실증). 후속: 열 derating → [[ADR-009]], RMW drift → DCN-2026-028.
> **Origin**: 메인 개발보드 교체 공지 (공기종, 2026-06-15) — Custom RK3588 → Boardcon EM3588 채택에 따른 공식 개발 플랫폼(OS/ROS) 일괄 갱신
> **Supersedes**: docs/README D-007 (§10.1.1 표준 툴체인 baseline: Ubuntu 22.04 + ROS 2 Humble + GStreamer 1.20+)
> **Related**: ADR-001 (Hub dual-SBC), ADR-006 (IPC Unification), ADR-007 (RMW FastDDS), DCN-2026-002 (3-Tier IPC), BOM v1.1
> **Document Owner**: 김태근 (PM, ㈜스카이오토넷)
> **Created**: 2026-06-15
> **Implementation**: 단일 PR (브랜치 `docs/DCN-2026-027`) — 버전 문자열 기계적 치환 중심, 소스 무변경

---

## 1. 배경

메인 개발보드가 **Custom RK3588** 보드에서 상용 **Boardcon EM3588**(Rockchip
RK3588 기반)로 교체됨에 따라, 공식 개발·시험 환경의 OS 및 ROS 버전이
함께 상향된다. SkyHunter 1.0 의 권원 문서(D-007)는 표준 툴체인을
"Ubuntu 22.04 + ROS 2 Humble" 로 고정하고 있으므로, 본 DCN 으로 이를
갱신하고 리포지토리의 버전 문자열을 일괄 정합화한다.

## 2. 변경 내역

| 항목 | Legacy | Official (본 DCN) | 비고 |
|---|---|---|---|
| 개발 보드 | Custom RK3588 | **Boardcon EM3588** | Rockchip RK3588 기반 — SoC 동일 |
| OS | Ubuntu 22.04 (Jammy) | **Ubuntu 24.04 (Noble)** | Python 3.10 → 3.12 |
| ROS | ROS 2 Humble | **ROS 2 Jazzy Jalisco** | 대부분 소스 호환 |
| 시뮬레이터 | Gazebo Harmonic | **Gazebo Harmonic (변경 없음)** | Jazzy 의 native 페어링 |
| GStreamer | 1.20+ | 1.24 (24.04 기본) | 1.20+ 제약 계속 충족 |

## 3. 영향 분석

### 3.1 변경 필요 (실행 경로 — 반드시 수정)

| 분류 | 파일 | 변경 |
|---|---|---|
| CI 워크플로 | `.github/workflows/coverage.yml` | `ubuntu-22.04`→`24.04`, `osrf/ros:humble-desktop`→`jazzy-desktop`, `/opt/ros/humble`→`jazzy` |
| CI 워크플로 | `.github/workflows/gate1-regression.yml` | `ubuntu-22.04`→`24.04`, `ros:humble-ros-base`→`jazzy-ros-base`, source 경로 |
| CI 워크플로 | `.github/workflows/sanitizers.yml` | 동일 |
| CI 템플릿 | `docs/ci_templates/{coverage,gate1-regression}.yml.template` | 라이브 미러 동일 |
| Docker | `infra/docker/sbc1/Dockerfile`, `sbc2/Dockerfile` | `ARG ROS_DISTRO=humble`→`jazzy` + **하드코딩 CMD `source /opt/ros/humble`→`jazzy`** (ARG 우회분 별도) |
| systemd | `san_bringup/systemd/san-squadron.service` | `Description (ROS 2 Humble)`→`Jazzy`, `ExecStart` source 경로 |

### 3.2 변경 필요 (문서 — 정합화)

`CLAUDE.md`, `README.md`, `docs/README.md`(D-007 supersede 주석), `docs/CI_GUIDE.md`,
`docs/SOP-CI-001.md`, `infra/docker/SECURITY.md`(SBOM base image), `config/README-lidar.md`
(`ros-jazzy-rslidar-sdk`), `san_external_mocks/README.md`, `SAN-TST-S20_test_plan.md`
(실행 환경표), `san_hub_slam/CMakeLists.txt`(`ros-jazzy-libg2o` 주석), `requirements-arm64.txt`(prereq 주석).

### 3.3 변경 불필요 (영향 없음 — 중요)

- **소스 코드 / `package.xml` / `find_package()`**: distro 핀 없음. ~43개 패키지는 distro-agnostic. CMakeLists/requirements 의 "humble/22.04" 잔여는 전부 주석.
- **NPU / RKNN / 영상 디코더**: EM3588 도 **RK3588 SoC 동일** → `human_detector/src/rk3588_npu_backend.cpp`, `rknn-toolkit-lite2`, `san_video_decoder` 전부 무영향. 보드 *모델명*만 문서 변경.
- **Gazebo 시뮬**: Harmonic 유지 → `san_sim_gazebo` 무변경. (Harmonic 은 Jazzy 의 native 페어링이라 마찰 오히려 감소.)
- **RMW**: ADR-007 로 FastDDS 확정 — 두 distro 공통 기본값이라 무영향. (Docker/systemd 의 `rmw_cyclonedds_cpp` 잔여 설정은 본 DCN 범위 밖, 별도 추적.)

## 4. 검증 요건 (머지 전 필수)

본 변경은 버전 문자열 치환이 대부분이나, **실제 호환성은 Jazzy 빌드로만 입증**된다:

1. **Jazzy `colcon build` + `colcon test`** — CI(`ubuntu-24.04` + `jazzy` 컨테이너) 그린 확인. Windows 호스트에서는 불가.
2. **Nav2 API churn** — `san_nav2` / BT navigator 의 Humble→Jazzy API 차이 점검(유일한 실질 포팅 리스크).
3. **Python 3.10 → 3.12** — `distutils` 제거 영향. (`requirements.txt` 상한 핀 없음 확인 — 저위험.) → **실측(PR #266 CI)**: `coverage.yml` 의 `pip3 install pytest-cov` 가 PEP 668 `externally-managed-environment` 로 실패 → apt `python3-pytest-cov`(noble universe) 로 교체, 표준 러너 best-effort pip 은 `--break-system-packages` 추가. distutils 영향 없음.
4. **standalone pure-logic 러너는 검증 근거 아님** — ROS 미링크라 distro 무관 통과.

## 5. Open Items (PM 결정 필요)

| # | 항목 | 내용 |
|---|---|---|
| O-1 | **RK3588J → RK3588 확정 (RESOLVED)** | PM 결정(2026-06-15): EM3588 탑재 실리콘은 **일반 RK3588(비-J)**. BOM v1.1 상단 주석으로 SBC `RK3588J`→`RK3588` 적용 명시 + 라이브 문서(CLAUDE/README/docker-compose) 갱신. ⚠ RK3588 은 일반등급(약 0~80℃)이라 RK3588J(−40~85℃) 대비 **옥외 운용 온도 derating** — enclosure 방열/히터 또는 J-grade SKU 등 온도 사양 재검토 필요. BOM 표 v1.0 열·소스 perf-provenance 주석은 측정/변경 이력이라 유지. |
| O-2 | `ros-jazzy-rslidar-sdk` apt 패키지 | Jazzy 용 apt 바이너리 부재 시 from-source 경로 사용 (README-lidar 에 병기됨). |
| O-3 | Docker/systemd RMW 잔여 | `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` 가 ADR-007(FastDDS)과 불일치 — 선재 drift, 별도 DCN 으로 정리 권장. |

## 6. 롤백

전 변경이 버전 문자열 치환 + 문서이므로 `git revert` 단일 커밋으로 완전 원복.
보드/OS/ROS 미수령 환경은 Docker `ARG ROS_DISTRO=humble` 오버라이드로 임시 구 환경 빌드 가능.
