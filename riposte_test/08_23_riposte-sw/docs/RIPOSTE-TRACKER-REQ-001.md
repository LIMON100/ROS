# RIPOSTE-TRACKER-REQ-001
## Riposte — RK3588+Hailo-8 이기종 AI 추대상 기체 요구 분석서

| 항목 | 내용 |
|---|---|
| 문서 ID | RIPOSTE-TRACKER-REQ-001 |
| 버전 | 0.4 (2026-08-16: **TR-4 완료** — EmbedWorker deadline 격리 구현·호스트 시험 7종, 잔존은 실측 튜닝 / 0.3: 2026-08-16 심층 코드리뷰 P1-10 부분 반영: RknnEmbedder 프레임당 임베딩 후보 상한(REID_EMBED_MAX_PER_FRAME, 점수순 top-K) — worker/deadline 격리는 TR-4와 동일 작업으로 브링업에 통합 / 0.2: 2026-08-16 심층 코드리뷰 P0-06 반영: TR-7 정체성 결합 신설 — 확인 윈도우·템플릿 앵커의 track_id 바인딩, TrackFusion 앵커 박스 종횡비 수정 / 0.1: Draft, 2026-08-15 전략 수립) |
| 대상 | Hailo-8 탐지 + RK3588 NPU 추적 보조 AI의 역할 분담 및 3계층 추대상 기체 |
| 상위 문서 | RIPOSTE-SAD-001 |
| 관련 문서 | RIPOSTE-SEEKER-SDD-001 (현행 Tracker §5, S-11), RIPOSTE-DUALEO-REQ-001 (R-8/R-10) |
| 성격 | 요구 분석 + 갭 분석 + 설계 전략 (구현 전 단계) |

---

## 1. 전략 요약

**역할 분담 원칙: Hailo-8은 탐지 전담(S-7/S-11 계약 불변), RK3588 NPU는 추적
보조 AI 전담.** 추가 AI 용량은 Hailo가 아니라 RK NPU에 있다 — Hailo에 모델을
더 얹으면 탐지 슬롯 예산(프레임당 12/16.7 ms)을 잠식하고 컨텍스트 스위칭 비용이
붙지만, RK3588 NPU(6 TOPS)는 현재 완전 유휴다.

현행 추대상 기체(tracking-by-detection: α-β + 탐욕 IoU 연관) 위에 AI 계층 2개를
얹는 **3계층 구조**로 하되, **T0 단독으로 현행과 동일하게 동작하는 것을 설계
불변식**으로 한다 — AI 계층 전체가 죽어도 비행 안전 경로는 그대로다.

---

## 2. 요구사항 정리

| ID | 요구 | 비고 |
|---|---|---|
| TR-1 | 유사 대상 교차 시 트랙 ID 유지(ID 스위치 방지) | R-8 primary lock의 실제 위협. 외관 재식별(T1) |
| TR-2 | 탐지 플리커/부재 프레임에서 LOS 연속 유지("시각 coast") | 원거리 소형 대상, S-11 탐색 슬롯·recheck 공백. 템플릿 추적(T2) |
| TR-3 | AI 추적 계층 장애 시 T0(현행)로 무결 강등 | 국소 장애 원칙(SM-7) 유지, quality 강등 발행 |
| TR-4 | RK NPU 작업이 60 Hz 파이프라인을 블록하지 않을 것 | **완료(2026-08-16)** — `EmbedWorker` 신설(SEEKER-SDD S-12/§4.5): T1 임베딩을 전용 스레드로 격리하고 인지 스레드는 `embed_deadline_ms`(기본 6 ms)까지만 대기, 초과 시 그 프레임은 운동 단독(T0, TR-3)으로 진행하고 늦은 결과는 세대 번호로 폐기. 워커 점유 중에는 제출 자체를 생략해 큐가 자라지 않는다(depth-1 latest-wins). 연속 `EMBED_DEADLINE_FAULTS`(30)회 무결과 시 영구 강등(회복 판정 없음). 소멸은 bounded drain 후 detach — `rknn_run` 영구 블록 시 join이 종료를 막지 않도록 공유 상태를 워커와 `shared_ptr` 공동 소유. 병행 유지: `REID_EMBED_MAX_PER_FRAME`(P1-10) 프레임당 후보 상한. 호스트 시험 `test_embedworker` 7종(기한 상한·늦은 결과 폐기·점유 중 무제출·강등·영구성·소멸 무교착·장치 실패 구분, TSan 클린). **잔존: 실 HW에서 `embed_deadline_ms` 실측 튜닝뿐** |
| TR-5 | 템플릿 단독 출력은 quality 강등으로 구분 발행 | OBC가 탐지 앵커/시각 coast를 정책적으로 구분 |
| TR-6 | 탐지(Hailo)가 항상 권위 — 템플릿은 대체·교차검증에만 사용 | 드리프트를 보정값·트랙에 흡수하는 실패 모드 차단 |
| TR-7 | 확인 윈도우(R-6)와 템플릿 앵커(T2)는 **트랙 정체성(track_id)에 결합** — primary 교체 시 확인은 새 대상의 윈도우로 재시작하고 템플릿 앵커는 즉시 폐기 | R-6은 "**동일 드론**" 조건이다(2026-08-16 심층 코드리뷰 P0-06). 미결합 시: A가 쌓은 확인 hit를 B가 승계해 자체 확인 없이 valid 발행되고, A에 앵커된 템플릿이 B의 miss 프레임에 A의 옛 위치를 B의 시각 coast로 발행한다. 파이프라인 전 경로(Tracker→TrackCue→SearchScheduler / TrackFusion 앵커)가 id를 나른다 |

---

## 3. 자원 인벤토리와 사용률 (전략 근거)

| 자원 | 용량 | 현재 사용 | 비고 |
|---|---|---|---|
| Hailo-8 | 26 TOPS | YOLOv8 탐지 60 Hz | **동기 detect() 슬롯이 병목** — 처리량 여유는 있으나 슬롯 예산 없음 |
| RK3588 NPU | **6 TOPS (2 TOPS×3코어, INT8, RKNN)** | **0 % 유휴** | 본 문서의 대상 |
| CPU 4×A76+4×A55 | — | 파이프라인·필터·OBC (경량) | A76 대부분 유휴 |
| RGA | 2D 엔진 | letterbox ~1.5 ms/frame | ReID crop 추가 사용자 — 경합 벤치 항목 |
| Mali-G610 | OpenCL | 0 % | NN 툴체인 미성숙 — 후순위 보류 |

---

## 4. 3계층 추대상 기체 아키텍처

```
[Hailo-8]  YOLOv8 탐지 (S-7/S-11 그대로) ─── Detection[] ──┐
                                                            ▼
[RK NPU①] T1 ReID 임베딩 — 검출 crop → 128-d 외관 벡터  [Fusion/CPU]
           (RGA crop, 검출당 ~0.5 ms, 프레임당 ≤8개)     연관 비용 = 운동 게이트
                                                         + 외관 거리 → T0 필터
[RK NPU②] T2 템플릿 추대상 기체(NanoTrack급 Siamese) — primary
           대상 매 프레임 60 Hz, 확정 탐지로 재앵커
```

- **T0 (기존, 불변)**: Hailo 탐지 + 운동 필터(α-β, 필요 시 칼만 승격) + 게이팅.
  비행 안전 앵커.
- **T1 (ReID 연관)**: 검출 박스 crop → 경량 임베딩 넷(OSNet-x0.25급, INT8
  RKNN) → 연관 비용을 "위치 게이트 + 외관 거리"로 확장(DeepSORT 계열 검증
  구조). TR-1 해결. 풍선 다수(T-3)·군집 시나리오의 R-8 primary 보호.
- **T2 (템플릿 추적)**: primary 대상 한정 Siamese 템플릿 추적을 매 프레임 실행.
  확정 탐지가 있는 프레임엔 템플릿 재앵커(드리프트 제거), 탐지가 빈 프레임엔
  템플릿 출력으로 LOS 유지 — coast가 탄도 예측에서 **시각 coast**로 격상(TR-2).
  종말 단계 LOS 연속성이 곧 PN 정밀도다.

**융합 원칙 (TR-6)**: 탐지가 항상 권위. 템플릿 출력은 ① 탐지 부재 프레임의
대체, ② 탐지 존재 프레임의 교차 검증(불일치 누적 시 템플릿 폐기·재초기화)에만
쓴다. 템플릿 단독 구간은 quality 강등 발행(TR-5).

---

## 5. 예산 검증

- **RK NPU 부하**: ReID(≤8 crop × ~0.1 GFLOPs) + NanoTrack급(~0.5 GFLOPs
  @60 Hz) ≈ **합계 1 TOPS 미만** — 코어별 모델 고정 배치(ReID→코어0,
  템플릿→코어1)로 1~2코어면 충분.
- **스레딩 (TR-4, 핵심 제약)**: 동기 detect()가 12 ms를 먹는 프레임에 NPU
  작업을 직렬로 붙이면 60 Hz가 깨진다. RK NPU 작업은 **별도 스레드 +
  latest-wins**로 돌리고 Fusion에서 병합 — SDD §2에 열어둔 InferThread 분리의
  첫 실사용. NPU 스톨은 HailoDetector와 동일한 격리 원칙(healthy() → quality
  강등, SM-7 경로 불변)으로 파이프라인에서 차단.
- **RGA 경합**: 사용자 2곳(Hailo 전처리 + ReID crop), 프레임당 ~3 ms 수준 —
  여유 예상, 벤치 항목.
- **열/전력**: RK NPU 가동 추가 ~2–3 W — 기체 전력·방열 예산 확인 항목.

---

## 6. 대안 기각 근거

| 대안 | 기각 사유 |
|---|---|
| Hailo 단독 멀티 모델 | 탐지 슬롯 예산 잠식 + 모델 스위칭 비용, RK NPU 유휴 방치 |
| Mali GPU (OpenCL NN) | 툴체인 미성숙, 유지보수 리스크 — 후순위 보류 |
| 트랜스포머급 E2E MOT | 6 TOPS/INT8 예산·실시간 제약에 부적합 |

---

## 7. 리스크

| ID | 리스크 | 완화 |
|---|---|---|
| K-1 | INT8 양자화 후 ReID recall 저하(임베딩은 양자화 민감) | 수락 기준을 "교차 시나리오 ID 유지율" 벤치로 고정, 필요 시 FP16 코어 검토 |
| K-2 | RKNN 런타임 지연 지터·드라이버 안정성 | 별도 스레드 격리(TR-4) + 실측, 연속 장애 시 T0 강등(TR-3) |
| K-3 | 템플릿 드리프트(배경 고착) | 확정 탐지 재앵커 주기 + 교차 검증 불일치 임계 + 드리프트 벤치 |
| K-4 | ReID 학습 데이터 부재 | 탐지 데이터셋에서 crop 파생, 별도 트랙 |
| K-5 | 메모리 대역 경합(Hailo PCIe DMA + NPU + 카메라) | 실 HW 동시 부하 벤치 |

---

## 8. 개발 단계 제안

| 단계 | 내용 | 선행 조건 |
|---|---|---|
| TR-A | RKNN 툴체인 브링업 — **호스트 절반 완료**(`RIPOSTE_WITH_RKNN`·`IEmbedder`·`RknnEmbedder`); 잔존: 실 HW 브링업, `ITemplateTracker`(TR-C에서) | RK3588 HW |
| TR-B | T1 ReID 연관: 연관 비용 행렬 로직(순수 수학, 호스트 시험) → SIL 합성 임베딩 검증 → RKNN 실모델 | 로직은 **선행 착수 가능**, 실모델은 TR-A |
| TR-C | T2 템플릿 추적 — **호스트 절반 완료**(ITemplateTracker·TrackFusion 융합 정책); 잔존: NanoTrack급 RKNN 포팅 + 드리프트/재앵커 벤치 | TR-A |
| TR-D | Fusion 파이프라인 배선 — **완료**(TR-D-a/b: main T1/T2 배선·TrackState.visual_coast·OBC 시각 coast 타임박스, TR-4 EmbedWorker 격리); 잔존: 비행 시험 | TR-B/C |

TR-B의 연관 비용 로직은 P2 방식(순수 로직 선행)으로 HW 없이 지금 착수할 수 있다.

**TR-B 선행 진행 결과(2026-08-15)**: 연관 비용 융합의 호스트 절반을 완료했다. 신규 `seeker/src/AssocCost.{h,cpp}` — `l2_normalize`(NaN/영노름 거부 — 장치 출력 비신뢰), `cosine_dist`(비교 불가 시 false → 운동 단독 폴백), `update_embedding`(트랙 갤러리 EMA + 재정규화, 무효 관측은 이력 보존), `assoc_cost`(**게이트 권위 원칙** — 외관은 게이트 밖 페어링을 구제하지 못함(TR-6), 게이트 안 재순위 + 임포스터 하드 리젝트(TR-1), 임베딩 부재/쓰레기 시 운동 단독 = T0 동일(TR-3)), `assoc_greedy_match`(**전역 최소 비용 그리디** — 고정 트랙 순서 선점이 이웃 검출을 빼앗는 문제 회피; Hungarian은 벤치에서 그리디 손실 확인 시 DEFERRED 해제). 운동 메트릭은 Tracker::dist2와 동일한 폭-정규화 등방 단위 — T0/T1이 동일하게 게이팅. Tunables: `ASSOC_APPEARANCE_WEIGHT`(0.5)·`ASSOC_APPEARANCE_REJECT`(0.7)·`ASSOC_EMBED_EMA_ALPHA`(0.2) — 값은 K-1 벤치 전 플레이스홀더. 단위시험 `test/test_assoc.cpp`(42 checks, ctest `assoc`) — 핵심은 **교차 시나리오 종단 검증**: 운동 단독이면 실제로 ID가 스왑되는 기하를 만들고, 외관 도입 시 하드 리젝트가 스왑을 차단해 ID가 유지됨을 양쪽 다 확인. ctest 16/16 · 새니타이저 clean. 잔존이던 Tracker 배선은 아래 2차 진행으로 완료.

**TR-A 호스트 절반 + TR-B 배선 완료(2026-08-15, 2차)**:
- **TR-A(호스트 절반)**: `IEmbedder` 경계 신설(`seeker/src/IEmbedder.h`) — 임베더는 연관을 "개선만" 할 수 있고 어떤 실패도 무효 임베딩→운동 단독 강등으로 흡수(TR-3). `SyntheticEmbedder`(SIL 플럼빙용 결정적 유닛 벡터 — 위치 기반이라 **ID 유지율 벤치에는 부적합**함을 명시), `RknnEmbedder`(`RIPOSTE_WITH_RKNN`, rknn_api 호출을 한 TU에 격리 — HailoDetector 패턴: init에서 모델 I/O 형상 질의·NHWC RGB888 검증 fail-closed·코어 핀, det별 crop_nv12→stretch resize→NPU, 개별 crop 실패는 그 임베딩만 무효, 디바이스 장애만 프레임 실패+연속 장애 healthy() 강하). `Preproc`에 `resize_nv12_rgb888` 추가(ReID crop은 의도적으로 **stretch** — S-6 letterbox 논거는 기하 측정에만 적용되고 임베딩은 "같은 대상이 같은 방식으로 왜곡"만 필요, ReID 학습 관례와 일치).
- **TR-B(배선)**: `Tracker::update(dets, embs, dt)` 오버로드 — embs가 dets와 정렬되고 후보 중 유효 임베딩이 1개 이상일 때만 T1 경로(융합 비용 + 전역 그리디, 갤러리 EMA 갱신, 스폰 시 갤러리 시드), 그 외는 레거시 경로 **불변**. 게이트는 레거시와 동일한 gate2/dist2.
- 단위시험: `test_seeker` +4건(교차 시 ID 유지 — 운동상 스왑이 더 싼 기하에서 외관이 식별 유지, 예측 위치 위 임포스터 기각→coast, 미정렬/전무효 임베딩 레거시 폴백, SyntheticEmbedder 결정성), `test_preproc` +3건(resize 색·단조성·계약). ctest 16/16(seeker 145·preproc 153) · 새니타이저 clean.
- **잔존: RKNN 실모델 브링업(TR-A 디바이스 절반, RK3588 HW)**. 파이프라인 통합은 완료 — main은 `EmbedWorker`를 통해서만 임베더를 호출한다(TR-4).

**TR-C 호스트 절반 완료(2026-08-15, 3차)**: T2 융합 정책의 순수 로직을 완료했다.
- `seeker/src/ITemplateTracker.h`: T2 경계 + `SyntheticTemplateTracker`(SIL —
  고정 드리프트로 정책을 시험하되 픽셀을 안 봄, ID/추적 품질 벤치엔 부적합).
- `seeker/src/TrackFusion.{h,cpp}`: 융합 정책. **탐지 권위 원칙(TR-6)** — 탐지
  프레임의 출력은 항상 탐지 박스이고 템플릿은 (a) 주기 재앵커, (b) 드리프트
  교차검증에만 쓴다. 탐지 부재 프레임은 템플릿 응답으로 **시각 coast(TR-2)**,
  quality를 강등하고 `visual_coast` 플래그를 세운다(TR-5, OBC가 구분). 템플릿이
  없거나(미앵커) 장치 실패면 **운동 coast로 폴백 = 기존 T0 동작 불변(TR-3)**.
  교차검증: 템플릿 track 위치가 탐지와 gate2 초과로 어긋나면 mismatch 누적,
  `FUSION_MISMATCH_MAX` 도달 시 강제 재앵커. 게이트는 Tracker와 동일 스케일.
- 장치(RK NPU) 호출은 인터페이스 뒤로 격리 — 정책은 Synthetic 주입으로 호스트
  시험. Tunables: `FUSION_REANCHOR_PERIOD`(15)·`FUSION_MISMATCH_MAX`(5)·
  `FUSION_COAST_QUALITY_SCALE`(0.6).
- 단위시험 `test/test_fusion.cpp`(42 checks, ctest `fusion`): 트랙 소멸 시
  템플릿 리셋, 탐지 권위(드리프트해도 탐지 박스 출력), 시각 coast 강등·추종,
  운동 coast 폴백(미앵커/장치 실패), 드리프트 누적 강제 재앵커, 일치 시 무재앵커.
  ctest 17/17 · 새니타이저 clean.
- **잔존: 파이프라인 통합(main 루프 배선 — TR-D), NanoTrack급 RKNN 실모델
  브링업(TR-C 디바이스 절반, RK3588 HW)**.

**TR-D-a 파이프라인 배선 완료(2026-08-15, 4차)**: 시커 main에 T1/T2를 배선했다.
- `TrackState.visual_coast`(1바이트, 기존 pad 소비 — sizeof 48 불변, static_assert
  유지): 이 샘플의 위치가 탐지 앵커가 아니라 T2 템플릿 coast에서 왔음을 표시(TR-5).
  발행은 신선(SM-7의 mono_ns는 최신)하되 탐지 기반이 아니므로 OBC가 더
  보수적으로 다룰 근거다. 라이터가 안 쓰면 0=탐지앵커(안전측) — num_targets 선례.
- main 배선: `make_embedder`(RKNN/Synthetic)·`make_template_tracker`(현재
  Synthetic만; NanoTrack RKNN은 브링업) 팩토리 신설. 프레임 루프: 검출→임베딩→
  `tracker.update(dets, embs, dt)`(T1)→`fusion.fuse()`(T2)→시각 coast면 트랙 위치·
  quality를 융합 결과로 대체→`estimator.estimate`→`ts.visual_coast` 발행. 임베더·
  템플릿 모두 **fail-soft**(null·장치 실패 시 운동 단독/운동 coast로 강등, TR-3).
- 단일 스레드 배선이다(현행 시커 관례). 60 fps 예산상 실 HW에서 T1/T2를 프레임
  경로에 직렬로 두면 초과하므로, EmbedWorker로 분리했다(TR-4 완료) — SIL에서는
  Synthetic 비용이 0이라 정합·발행을 먼저 검증했고, 격리 자체는 느린/블록되는
  임베더 더블을 쓰는 `test_embedworker`가 호스트에서 검증한다.
- 검증: 빌드 -Werror 0, ctest 17/17, 새니타이저 clean, **SIL 실행에서
  SEARCH_WIDE→TRACK 전환·60 fps 유지·track 발행 확인**.
- **TR-D-b 완료(2026-08-15, 5차)**: 시각 coast의 안전 계약을 닫았다. **위험**:
  시각 coast 발행은 신선(mono_ns 최신)하므로 SM-7의 신선도 기반 coast 타임아웃을
  우회한다 — 탐지가 끊겨도 T2 템플릿이 계속 신선 발행을 만들면 disengage가 안
  걸린다. **해결**: GuidanceSource의 coast 예산 신선도 시계를 발행 시각이 아니라
  **마지막 탐지-앵커 샘플(visual_coast==0)** 기준으로 잰다(`last_detection_ns_`).
  시각 coast는 종말 LOS를 정밀화(TR-2)하되 탐지 시계를 갱신하지 못하므로, 시각
  coast만으로는 기존 coast window(TRACK_STALE+TRACK_COAST)를 연장할 수 없고 SM-7이
  정상 발화한다. 새 상수 없이 기존 window 재사용, 더 보수적. 단위시험
  `test_guidance` +3(window 내 시각 coast 정상 유도, 시각 coast 스트림이 window
  연장 불가→disengage, 탐지 재획득 시 시계 리셋). ctest 17/17 · 새니타이저 clean.
- **잔존: RKNN ReID·NanoTrack 실모델 브링업(RK3588 HW)과 `embed_deadline_ms`
  실측 튜닝**. InferThread 분리(TR-4)는 EmbedWorker로 완료됐다.

---

## 9. ASSUMPTION / DEFERRED

| 태그 | 항목 |
|---|---|
| ASSUMPTION | RK NPU에서 NanoTrack급 60 Hz 실측 지연 ≤5 ms/frame (브링업에서 검증) |
| ASSUMPTION | ReID 임베딩 INT8 정확도가 교차 시나리오 ID 유지율 기준 통과 (K-1) |
| ASSUMPTION | ReID/템플릿 모델은 기존 탐지 데이터셋 파생으로 학습 가능 (K-4) |
| DEFERRED | 운동 필터 α-β → 칼만/IMM 승격 여부 — T2 도입 후 잔여 오차 실측으로 판단 |
| DEFERRED | TrackState quality 강등 인코딩(탐지 앵커/시각 coast 구분) 필드 설계 — TR-D |
| DEFERRED | 다중 대상 전부에 T2 확장(현안은 primary 한정) — NPU 예산 실측 후 |
