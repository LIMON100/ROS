# DCN-2026-025 — 시뮬레이션 ONNX 디텍터 백엔드 (+ Hailo-8 async InferModel + ByteTrack 추적)

> **Status**: **APPROVED (ratified)** — PM 승인 2026-06-05 (구현: PR #257, merged 2026-06-04)
> **Origin**: `limon/features_gazebo_sim` 브랜치 리뷰 → PR #257 review follow-up
> **Extends**: DCN-2026-004 D-011 (InferenceBackend factory) · DCN-2026-003 D-003 (detector pipeline)
> **Document Owner**: 김태근 (PM, ㈜스카이오토넷)
> **Created**: 2026-06-05
> **Implementation**: PR #257 (squash `9395b53`, merged 2026-06-04) — CI all-green

---

## 1. 배경

`human_detector` 의 AI 가속기 추상화(`InferenceBackend`, DCN-2026-004 D-011)는
지금까지 세 가지 백엔드만 제공했다:

| 백엔드 | 가드 | 용도 |
|---|---|---|
| `rk3588` | `HAVE_RKNN` | Phase 1 온보드 NPU (RK3588, 6 TOPS) |
| `hailo8` | `HAVE_HAILORT` | PoC Hailo-8 M.2 (26 TOPS) |
| `stub` | 항상 | SDK 부재 host/CI — **무탐지** |

문제: **시뮬레이션(Gazebo)에는 RK3588·Hailo HW 가 없으므로 `stub` 만 선택
가능했고, stub 은 빈 탐지를 반환**한다. 따라서 워크스테이션 sim 에서 인지
파이프라인(탐지→추적→사격권한 등)을 end-to-end 로 검증할 수단이 없었다.

본 DCN 은 sim/데스크톱에서 실제 추론이 가능한 **ONNX Runtime 기반 디텍터
백엔드**를 추가하고, 동반 변경으로 **Hailo-8 경로를 신형 async InferModel
API 로 재작성**하며 **ByteTrack 다중객체 추적**을 통합한 PR #257 을
형상관리상 비준한다.

## 2. 변경 내역

### 2.1 ONNX 백엔드 (주 변경)

- `OnnxBackend` (`onnx_backend.{hpp,cpp}`) + `YoloEngine`
  (`yolo_engine.{hpp,cpp}`) — ONNX Runtime C++ API. CUDA EP 우선, 실패 시
  CPU provider 로 폴백. YOLO 출력 NMS 후 `Detection` 으로 변환.
- 팩토리(`inference_backend.cpp`)에 `"onnx" | "sim"` 분기 추가.
- **게이트-스텁 준수(§4)**: ONNX Runtime 미설치 host/CI 에서는 `HAVE_ONNX`
  미정의 → `OnnxBackend::initialize()` 가 `false` 반환 → 팩토리가
  `rk3588 → stub` 으로 폴백. 빌드·링크 무손상.

### 2.2 Hailo-8 async InferModel 재작성

- `Hailo8Backend::initialize()` 가 저수준 `Hef::create` +
  `vdevice_->configure(ConfiguredNetworkGroup)` 경로 대신 신형 고수준
  **InferModel** API(`AsyncModelInfer`, `hailo_async_inference.{hpp,cpp}`)를
  사용: `create_infer_model(hef_path)` → `infer_model->configure()` →
  `run_async()`. `HAVE_HAILORT` 일 때만 컴파일.

### 2.3 ByteTrack 다중객체 추적

- `tracking_lib/` (BYTETracker/STrack/KalmanFilter/Rect/Object + lapjv) —
  탐지 박스에 트랙 ID 부여. MIT 라이선스 외부 포팅(§6 라이선스).

### 2.4 인터페이스 · 자산

- `combat_robot_msgs/msg/Detection.msg` 에 `uint32 track_id` 추가(§3).
- `san_bringup/squadron.launch.xml` — sim↔실HW 백엔드/모델 선택 로직
  (`hw_backend`, `detector_model_path`, `hailo_model_path`,
  `rk3588_model_path`).
- `models/y5s_person_drone.hef` (Hailo 가중치) 추가, `models/README.md`
  (프로비저닝 관례) 신규.

## 3. IDS / 인터페이스 영향

`Detection.msg` (권원: SDD-SWARM v1.5 §4.2, IDS v1.5 §5.21)에
`uint8 class_id` 다음 위치로 `uint32 track_id` 필드가 **삽입**되었다.

- **영향**: 메시지 타입 해시 변경 → 모든 publisher/subscriber **재빌드 필요**
  (소스 호환은 유지, 바이너리·직렬화 비호환).
- **소비자**: 현재 `track_id` 를 읽는 다운스트림은 없음(추적 ID 는 발행만).
  향후 surveillance/fire-authorization 이 트랙 연속성에 활용 가능(별도 작업).
- **IDS 문서 갱신**: IDS §5.21 Detection 필드표에 `track_id` 반영 필요
  (본 DCN 승인 후 문서 DCN 으로 처리).

## 4. 아키텍처 규칙 준수

| 규칙 | 준수 여부 |
|---|---|
| **ADR-006 / DCN-2026-002** 3-layer compile/link/runtime 게이트-스텁 | ✅ `HAVE_ONNX`/`HAVE_HAILORT` 가드 — vendor SDK 없는 host/CI 빌드·링크 통과(CI full colcon build green) |
| **DCN-2026-002 / ADR-006** no shell-out (system/popen/subprocess/gst-launch 금지) | ✅ 신규 코드에 셸-아웃 없음 |
| **ADR-008** Tier 언어 정책 | ✅ Tier 1/2 C++ (`rclcpp`), Python 신규 없음 |
| 모델 프로비저닝 | ✅ `/opt/san/models/` 관례 일원화(`models/README.md`) |

## 5. 영향 모듈

| 모듈 | 변경 |
|---|---|
| `human_detector` | ONNX 백엔드 + YoloEngine, Hailo async, ByteTrack tracking_lib, 팩토리·CMake·package.xml |
| `combat_robot_msgs` | `Detection.msg` +`track_id` |
| `san_bringup` | `squadron.launch.xml` 백엔드/모델 선택 |
| `models/` | `y5s_person_drone.hef`, `README.md` |
| `.pre-commit-config.yaml` | 모델 가중치(`models/*.{rknn,hef,onnx,…}`) large-file 훅 제외 |

## 6. 리스크 / 완화

- **ONNX Runtime 의존성**: 선택적(probe). 미설치 시 stub 폴백 → 차단 아님.
  배포 시 sim 호스트에 ONNX Runtime 패키징 필요(후속).
- **모델 미적재**: sim `.onnx` 는 미커밋 — 부재 시 `onnx → rk3588 → stub`
  graceful degrade(부팅 무영향, 탐지만 비활성).
- **라이선스(컴플라이언스)**: ByteTrack(C++ 포팅) · lapjv(Tomas Kazmar) 는
  MIT — `tracking_lib/THIRD_PARTY_NOTICES.md` 에 출처·MIT 전문 명시.
  SAN proprietary 헤더 비적용.
- **메시지 비호환(§3)**: 동일 릴리즈 내 전 노드 재빌드로 해소.
- **모델 git 용량**: `.hef` 7 MB 는 기존 `.rknn` 과 동일 관례로 raw 추적.
  LFS 전환은 별도 검토.

## 7. 검증

PR #257 CI **전부 green** (2026-06-04):

| 체크 | 결과 |
|---|---|
| C++ Coverage (gcov+lcov) — **full colcon build** | ✅ pass (7m) — ONNX 게이트-스텁 end-to-end 빌드 확인 |
| L5_26~L5_33 acceptance suite | ✅ pass (8m) |
| Gate-1 Regression (L5_26~33) | ✅ pass |
| Python Coverage (pytest-cov) | ✅ pass |

추가: ONNX stub-mode(`HAVE_ONNX` 미정의) `onnx_backend.cpp` +
`inference_backend.cpp` 가 `-Wall -Wextra` 로 컴파일·링크되고
`onnxruntime` 심볼을 끌어오지 않음을 별도 확인.

## 8. 결정 일정 / 승인

- **구현 + 머지**: 2026-W23 — PR #257 (CI green, squash merge) **완료**
- **PM 검토 / 비준**: 2026-W23 — ✅ **승인** (김태근 PM, 2026-06-05)
- **DCN ID 할당**: DCN-2026-025

> **비준 완료**: 본 DCN 은 PM 승인으로 ratified 되었으며, 후속(§9) 은 별도
> 작업/DCN 으로 등록한다.

## 9. 후속 (별도 작업 / DCN)

- IDS §5.21 Detection 필드표에 `track_id` 반영(문서 DCN).
- `human_detector.yaml` 의 RKNN 모델명 불일치 정합 — config 는
  `yolov8n_640.rknn`, 적재 파일은 `yolov5s-640-640_rk3588_251205_640.rknn`
  (`models/README.md` 참조).
- sim 호스트용 ONNX Runtime 배포 패키징 + sim `.onnx` 가중치 프로비저닝.
- `track_id` 다운스트림 소비(surveillance/fire-authorization 트랙 연속성).
- 모델 가중치 Git LFS 전환 검토.

## 10. Cross-refs

- DCN-2026-004 D-011 — `InferenceBackend` 팩토리(본 백엔드의 확장 지점)
- DCN-2026-003 D-003 — detector 파이프라인 gtest
- DCN-2026-002 / ADR-006 — IPC 통합 / 게이트-스텁 규칙(no shell-out)
- ADR-008 — Tier 기반 언어 정책
- SDD-SWARM v1.5 §4 (AI 가속기) · §4.2 / IDS v1.5 §5.21 (Detection)
- PR #257 (`9395b53`) · `tracking_lib/THIRD_PARTY_NOTICES.md` · `models/README.md`
