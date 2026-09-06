# RIPOSTE-SEEKER-SDD-001
## Riposte — 시커(L2 인지) 모듈 설계서

| 항목 | 내용 |
|---|---|
| 문서 ID | RIPOSTE-SEEKER-SDD-001 |
| 버전 | 3.9 (2026-08-18 코드리뷰 반영: **S-15 R-11 귀속 완결** — 잔존 2개 지점 수정(TRACK 정체성 인계가 슬롯 배정으로 귀속·재진입이 hit 없는 alive 큐로 증거 래치), 증거 래치는 전 지점 hit-게이트 / **S-13 페어링 계약 4항 신설** — T1 임베딩은 추론 프레임에서 crop(원좌표), 협각 협상 해상도=광각 필수(불일치 시 dual_eo 기동 거부/녹화 강등), 협각 dequeue 비블로킹(0 ms), 광각↔협각 프레임 시각차 상한(`NARROW_WIDE_SKEW_MAX_NS`) / **§5 S-3 pre-lock 히스테리시스**(`TRACKER_PRIMARY_SWITCH_MARGIN`) — 유사 크기 두 트랙의 확인 라이브락 차단 / **§8.2 dual 협각 입력을 named FIFO → 상속 파이프(fd 3, `pipe:3`)로 교체** — 세그먼트 회전 시 구 인코더 EOF 미도달·바이트 분할 결함 제거, 종료 시 dual 통계 보고 / §4.4 HEF 로드 후 target_class 재검증(fail-closed)·비-std 예외 격리 / 3.8: 2026-08-17: S-15 보강 — R-11 증거를 실제 추론 채널 기준으로 귀속(폴백 슬롯이 교차 확인을 유발하던 결함 수정) / 3.7: 2026-08-17: S-15 2채널 검출 정확도 — R-11 교차 확인(확인 가속·dual_confirmed 발행)·R-12 협각 표본 가중 / 3.6: 2026-08-17: S-14 실시간 데이터 경로(zero-copy/DMA) 신설 — 현재 상태와 목표 구분, 의도적 복사의 근거, 브링업 확인 항목 / 3.5: 2026-08-17: §4.2 `seeker.dual_eo` 기동 거부 명시 — 협각 추론 파이프라인 미배선 상태에서의 오해 방지, P4와 함께 규칙 삭제 / 3.4: 2026-08-16: §4.4.5 브링업 항목의 실행 절차를 RIPOSTE-BRINGUP-001 §3으로 위임) / 3.3: (2026-08-16: §2 S-12 + §4.5 **EmbedWorker** — T1 임베딩 deadline 격리로 TR-4 종결(인지 스레드는 동기 NPU 호출 금지가 불변식), 호스트 시험 7종 / 3.2: 2026-08-16 코드리뷰 P2-05/06: §4.1 VIDIOC_S_PARM/G_PARM 캡처레이트 설정·검증(불일치 경고)·§8 dual 녹화를 RecordWorker 스레드로 격리(depth-1 latest-wins, 협각 검출이 인지 루프 비차단) / 3.1: 2026-08-16: §8.2 2채널 구성 확정 — 두 번째 채널은 **EO 협각**(DUALEO/S-11 정합, `narrow_device`); IR(열영상)은 §10 DEFERRED로 이관(미래 검토, 파이프라인은 채널-불문) / 3.0: 2026-08-16: §8.1/§8.2 VideoRecorder **구현 완료**(P1-07 종결) — ffmpeg 파이프·30s 세그먼트·디스크 회수·dual hstack·drawtext 사이드카; 개정: 회전 시 비동기 좀비 회수(moov finalize 비차단), 60Hz 캡처→record_fps 입력 페이싱, open() 필터 capability 탐지(drawtext 부재 시 자막만 강등), fail-closed 기동 연계, `deploy/sysctl.d`(fs.pipe-max-size); 통합시험 5종(세그먼트 생성·회전·인코더 死 생존·dual 합성·기동 거부) / 2.9: 2026-08-16 심층 코드리뷰 P1-05/06 반영: §4.1 V4L2 DQBUF 경계 검증(index·data_offset·포맷별 최소 바이트·짝수 해상도, 순수 술어 호스트 시험)·§4.3 raw 디코드 finite/범위/후보 상한 / 2.8: 2026-08-16 심층 코드리뷰 P0-05 반영: §4.4.1 S-10 개정 — job별 입력/출력/bindings 소유, DeviceCtx runtime owner(완료 콜백 shared_ptr 캡처), 소멸자 bounded drain; **실 HW 브링업 검증 대기** / 2.7: 2026-08-15: T2 융합 파이프라인 배선(TRACKER-REQ TR-D-a) — main T1 임베더·T2 TrackFusion 연결·TrackState.visual_coast 발행·SIL 검증(SEARCH→TRACK·60fps), 단일 스레드(TR-4 분리는 브링업) / 2.6 (2026-08-15: T1 ReID 연관 배선(TRACKER-REQ TR-B) — Tracker embs 오버로드·갤러리 EMA·IEmbedder 경계(Synthetic/RKNN)·Preproc stretch resize, 레거시 경로 불변, 시험 seeker 145·preproc 153·assoc 42 / 2.5: 2026-08-15: S-11 배분 로직 구현 — SearchScheduler `channel()`/`dual_eo`·협각 자체 타일 스윕·광각 슬롯 기준 dwell, 단위시험 6건(search 209 checks), 단일 카메라 동작 불변 / 2.4: 2026-08-15: S-11 적응형 추론 레이트 배분(§4.2, R-10) — 캡처 60fps 고정 + 추론 스케줄 적응(탐색 광각30/협각30 교차 → TRACK 협각60+광각 재탐지), 비행 중 센서 모드 전환 금지 / 2.3: 2026-08-15: §4.4 구현 완료 — HailoDetector 장치 경로·HailoNmsParse·Preproc(CPU/RGA)·CMake 링크, 단위시험 hailoparse(34)·preproc(122) 통과·새니타이저 클린; 60Hz 카메라 보정 — SEEKER_FRAME_HZ 신설, 프레임 카운트 상수 시간 의미 보존(MAX_MISSES 16·CONFIRM 10중8·RECHECK 30·HAILO_FAULTS 30), SIL 카메라 페이싱 연동 / 2.2: §4.4 HailoDetector 장치 경로 설계 — NMS-on-device 주 경로·출력 포맷 자동 감지(S-7), 디바이스 저임계+호스트 동적 필터(S-8), RGA/CPU letterbox 전처리·무-OpenCV(S-9), 스테이징 버퍼 수명(S-10); `HailoNmsParse`·`Preproc` 신설, raw 헤드 디코드는 폴백으로 강등 / 2.1: 2026-08-13: §3·§4.3 ModelIo 신설 — 종횡비 보존 letterbox(S-6)·YOLO 디코드·NMS, HailoDetector는 장치 호출만 잔존 / 2.0: 2026-08-12 P6: §5 최대 크기 우선 primary + commit 후 sticky·`num_targets` 발행(패딩 배치·크기 고정) / 1.9: 2026-08-12 P2 §2·§3·§4.2 SearchScheduler 신설 — 광각/3x3 타일 탐색·5중4 확인 윈도우·주기 재탐지·ROI 크롭/좌표환산, 대상 클래스 config화, SIL 검출기 ROI 인지 / 1.8: 2026-07-04 심층 리뷰 반영: 협상 해상도 전파·드라이버 타임스탬프·stride 가드 §4.1, 확인 게이팅·등방 게이트·크기 단위 통일 §5, 핸드오프 속도 리셋 §6, 버스 개방 실패 치명화·seq 카운터 §7, 레코더 파이프 클램프·치수 가드 §8.1, 오버레이 원자 교체 §8.2 / 1.7: 다중대상 추적 §5·sticky primary·per-track α-β / 1.6: V4L2Camera MIPI-CSI/UVC 실구현 §4.1·mmap 스트리밍·NV12/YUYV→NV12 / 1.5: EO/IR 2채널 녹화+자막 §8.2·GpsBus / 1.4: 영상 녹화 VideoRecorder §8.1 / 1.3: Tracker/Estimator 단위시험 §9 / 1.2: SITL guidance 추종 / 1.1: 코딩표준 Rev C) |
| 대상 모듈 | `riposte-sw/seeker/` (프로세스 `riposte-seeker`) |
| 상위 문서 | RIPOSTE-SAD-001 (§3 Hailo 인지 재설계) |
| 계층 | L2 인지 (Seeker/Perception) |
| 주기 | 가변 ~60Hz (카메라 페이싱 SEEKER_FRAME_HZ, Hailo 비동기 오프로드) |
| 가속기 | Hailo-8 (M.2 PCIe, HailoRT 4.x) |

---

## 1. 목적 및 범위

`riposte-seeker`는 카메라 프레임에서 대상 드론을 탐지·추적하고, 대상의 **기체 상대 위치·속도(BODY FRD)** 를 추정해 TrackBus로 발행한다. **이 프로세스는 FC에 절대 접근하지 않는다** — Hailo/카메라 장애는 국소 장애로 처리되고, TrackBus 신선도 저하를 통해 OBC가 SM-7로 자동 disengage한다(가속기 장애의 비행안전 전파 차단).

---

## 2. 파이프라인 아키텍처

```
[Capture]  V4L2 DQBUF → 단조시각 스탬프 → latest-wins(큐깊이1, 구프레임 폐기)
   │
[Search]   SearchScheduler가 이 프레임의 탐색 영역(ROI) 결정 → NV12 크롭
   │
[Infer]    Hailo-8 비동기 추론(HailoRT) → Detection[] (ROI 정규화 bbox)
   │        → remap_to_frame(): 전체 프레임 정규화 좌표로 환산
   │
[Fusion]   Tracker(게이팅+α-β) → SearchScheduler 확인 판정(10중8) →
           TargetEstimator(픽셀→LOS→상대NED) → TrackBus 발행
```

인지 루프는 **단일 스레드**로 배선(`main.cpp`)한다: 카메라가 ~60Hz(SEEKER_FRAME_HZ) 페이싱 요소이고 Hailo 검출은 비동기 오프로드라 CPU 점유가 낮기 때문. 단, 인지 스레드에서 **동기 NPU 호출을 하지 않는다**는 것이 불변식이며, 이를 어기는 두 경로는 각각 전용 워커로 격리한다 — 협각 녹화 오버레이 검출은 `RecordWorker`(§8.2, P2-06), T1 외형 임베딩(RKNN)은 `EmbedWorker`(§4.5, TR-4).

**설계 결정 S-12 (EmbedWorker, TR-4)**: RKNN `rknn_run`은 드라이버 이상 시 **무기한 블록**될 수 있어, 인지 스레드에서 직접 호출하면 60 Hz 파이프라인 전체가 정지한다(K-2). 임베딩을 전용 스레드로 옮기되, 연관(association)은 *현재 프레임의* 검출에 정렬된 임베딩을 요구하므로 완전 비동기화는 의미가 없다. 따라서 **deadline worker** 형태를 취한다:

- 인지 스레드는 프레임+검출을 depth-1 latest-wins 슬롯에 게시하고, **`embed_deadline_ms` 동안만** 결과를 기다린다.
- 기한 내 도착 → 기존 동기 경로와 **바이트 단위로 동일한** 연관 입력.
- 기한 초과 → 그 프레임은 **운동 단독(T0, TR-3)** 으로 진행하고 늦게 도착한 결과는 세대 번호로 폐기한다. 인지 루프의 최악 지연은 `deadline`으로 상한된다.
- 워커가 아직 블록돼 있으면 새 프레임은 **게시하지 않는다**(제출 자체가 비차단). 큐가 자라지 않는다.
- 연속 `EMBED_DEADLINE_FAULTS`회 초과 시 임베더를 **강등**해 이후 프레임은 제출 없이 운동 단독으로 돌린다. 회복 판정은 하지 않는다 — 되살릴 근거가 없는데 60 Hz 루프를 다시 위험에 노출시킬 이유가 없다(치명 아님, TR-3).
- 종료 시 워커가 `rknn_run` 안에서 영구 블록돼 있으면 join이 걸리므로, 공유 상태를 `shared_ptr`로 워커와 공동 소유하고 **bounded drain 후 detach**한다(§4.4.1 S-10의 Hailo 소멸자와 같은 근거).

**설계 결정 S-1 (latest-wins)**: 추론이 밀리면 중간 프레임을 버리고 항상 최신 프레임만 처리(`FRAME_STALE_NS` 초과 폐기). 큐잉으로 인한 지연 누적을 방지 → 유도 루프 종단 지연 예산(<100ms) 보호.

---

## 3. 구성 요소

| 파일 | 클래스/인터페이스 | 역할 |
|---|---|---|
| `IDetector.h` | `IDetector`, `Detection`, `Frame` | 추론 경계 인터페이스 |
| `HailoDetector.*` | `HailoDetector` | HailoRT 어댑터 (`RIPOSTE_WITH_HAILO`) — 장치 경로 §4.4 구현 완료(브링업 대기) |
| `ModelIo.*` | `Letterbox`, `letterbox_fit/forward/undo`, `nms`, `decode_yolov8` | 모델 입출력 레이아웃 — 종횡비 보존 letterbox·YOLO 헤드 디코드·NMS(폴백 경로) |
| `HailoNmsParse.h` | `parse_nms_by_class` | NMS-on-device 출력 파서(헤더온리·HailoRT-free·경계검사) §4.4.2 |
| `Preproc.*` | `letterbox_nv12_rgb888` (CPU) / RGA 경로 | NV12→RGB888 letterbox 전처리, 무-OpenCV §4.4.3 (RGA는 `RIPOSTE_WITH_RGA`) |
| `SyntheticDetector.h` | `SyntheticDetector` | SIL 대체 탐지기 (표류·접근 대상 생성) |
| `CameraIngest.*` | `ICamera`, `V4L2Camera`, `SyntheticCamera` | 프레임 소스 추상화 (V4L2 mmap 실구현 §4.1) |
| `SearchScheduler.*` | `SearchScheduler`, `Roi`, `crop_nv12`, `remap_to_frame` | 탐색 영역 스케줄링(광각/타일/대상), 확인 윈도우, ROI 크롭·좌표 환산, S-11 채널 배분 `channel()` |
| `Tracker.*` | `Tracker` | 다중대상 게이팅+연관 + α-β 필터, sticky primary, T1 융합 연관(embs 오버로드) |
| `AssocCost.*` | `Embedding`, `assoc_cost`, `assoc_greedy_match` | T1 연관 비용 융합(운동 게이트+외관)·전역 그리디 — TRACKER-REQ TR-B |
| `IEmbedder.h` | `IEmbedder`, `SyntheticEmbedder` | T1 임베더 경계(실패=운동 단독 강등) |
| `RknnEmbedder.*` | `RknnEmbedder` | RK3588 NPU ReID (`RIPOSTE_WITH_RKNN`) ⚠️실 HW 브링업 대기 |
| `TargetEstimator.*` | `TargetEstimator` | 픽셀 → 상대 NED 기하 추정 |
| `main.cpp` | — | 파이프라인 배선 + TrackBus/HealthBus 발행 |

---

## 4. 인터페이스 경계 (HW 격리)

**설계 결정 S-2 (인터페이스 뒤 HW 격리)**: 실제 하드웨어(HailoRT·V4L2)는 인터페이스 뒤로 격리하고 CMake 옵션(`RIPOSTE_WITH_HAILO`/`_V4L2`)으로 제어한다. 옵션 OFF 시 `SyntheticDetector`/`SyntheticCamera`가 대체되어 **개발 PC에서 전체 파이프라인이 빌드·실행**된다.

```cpp
class IDetector {
    virtual bool init() = 0;
    virtual bool detect(const Frame& f, std::vector<Detection>& out) = 0;
    virtual bool healthy() const = 0;
};
class ICamera {
    virtual bool open() = 0;
    virtual bool grab(Frame& f, int timeout_ms) = 0;   // latest-wins
};
```

`Detection`은 **정규화[0,1] 좌표**로 표현 → 다운스트림 기하가 센서 해상도에 독립.

### 4.1 V4L2Camera — MIPI-CSI/UVC 캡처 (RIPOSTE_WITH_V4L2)

`V4L2Camera`는 표준 V4L2 **mmap 스트리밍**으로 카메라를 캡처한다(Airys Main2 `V4L2Capture` 참조 이식).

- **오픈 시퀀스**: `open(O_RDWR|O_NONBLOCK|O_CLOEXEC)` → `VIDIOC_QUERYCAP`(단일/멀티플레인 자동감지 — mplane 판정은 `capabilities`가 아닌 **`device_caps`** 기준: `capabilities`는 형제 노드의 mplane 비트를 실을 수 있음) → `VIDIOC_S_FMT` **포맷 후보 루프**(NV12→YUYV→UYVY, 드라이버가 실제로 설정한 read-back 포맷을 수락 — UVC가 NV12를 무시하고 YUYV를 돌려주는 경우 대응) → `VIDIOC_REQBUFS`(MMAP, 6개) → 버퍼별 `VIDIOC_QUERYBUF`+`mmap` → `VIDIOC_QBUF` 전량 → `VIDIOC_STREAMON`. 모든 ioctl은 `EINTR` 재시도 래퍼(`xioctl`) 경유. 드라이버가 stride를 보고하지 않으면(`bytesperline==0`) NV12는 `width`, 패킹 4:2:2(YUYV/UYVY)는 **`2×width`** 로 폴백(픽셀당 2바이트).
- **협상 해상도 전파**: 드라이버는 요청과 다른 모드로 대체할 수 있으므로, 협상(read-back)된 해상도를 `ICamera::width()/height()`로 노출하고 **버퍼 크기·기하를 유도하는 모든 소비자(레코더·Tracker aspect·Estimator)는 설정값이 아닌 협상값으로 구성**한다. 설정과 다르면 WRN 로그(`negotiated WxH (config WxH)`).
- **캡처(grab, latest-wins)**: 지난 grab이 넘긴 버퍼를 먼저 재큐 → `poll(POLLIN, timeout)` → `VIDIOC_DQBUF`로 큐를 **비우며 최신 프레임만 유지**(나머지 즉시 재큐). 에러 플래그·`bytesused==0`(단일평면 포함) 프레임은 재큐 후 스킵. `mono_ns`는 드라이버의 `buf.timestamp`가 **CLOCK_MONOTONIC**일 때 이를 우선 사용(진짜 캡처 시각으로 스테일 판정)하고, 아니면 `mono_now_ns()` 폴백 — 어느 쪽이든 파이프라인 단일 시간축(단조시각) 유지.
- **DQBUF 경계 검증 (2026-08-16, 심층 코드리뷰 P1-05)**: 드라이버가 돌려주는 버퍼 메타데이터는 **비신뢰 입력**으로 취급한다 — 종전에는 `bytesused==0`만 검사해, 잘못된 `index`가 `bufs.at()` 예외로 프로세스를 죽이거나 short buffer가 하류(전처리·NV12 변환)의 OOB read로 이어질 수 있었다. 검증 항목: ① `index < 버퍼 수`(위반 시 프레임 폐기, 예외 아님) ② mplane `data_offset`을 유효 payload 계산에 반영(`bytesused - data_offset`) ③ **포맷별 최소 바이트**: NV12 `stride×h×3/2`, 패킹 4:2:2 `stride×h×2` 미만이면 재큐 후 스킵 ④ 오픈 시 협상 해상도의 **짝수 폭·높이 강제**(NV12 4:2:0·YUYV 4:2:2의 서브샘플링 전제; 홀수면 open 실패 = fail-closed). 판정 술어(`v4l2_frame_bytes_required`/`v4l2_payload_ok`)는 순수 함수로 분리해 호스트 단위시험(malformed 매트릭스)으로 고정한다.
- **캡처 레이트 설정·검증 (2026-08-16, 코드리뷰 P2-05)**: `VIDIOC_S_PARM`으로 `seeker.fps`(=SEEKER_FRAME_HZ, 기본 60)를 요청하고 `VIDIOC_G_PARM`으로 부여된 레이트를 읽어 불일치 시 경고한다 — 종전에는 요청 fps를 저장만 하고 드라이버에 설정하지 않아, 실제 캡처 레이트가 다르면 프레임 카운트 기반 정책(Tracker miss budget·R-6 확인 윈도우·R-7 재탐지)의 시간 의미가 조용히 어긋났다. (프레임 카운트→경과시간 기반 전환은 브링업 항목.)
- **출력 포맷**: NV12(MIPI/ISP 네이티브)는 **제로카피**(mmap 포인터 그대로). UVC의 YUYV/UYVY는 소프트웨어로 **NV12 변환**(4:2:2→4:2:0, 짝수행에서 크로마 샘플). 디텍터는 항상 NV12를 받는다.
- **MIPI-CSI**: RK3588에서 D-PHY 레인·ISP 링크는 **커널 디바이스트리 + rkisp**가 구성하며, 유저스페이스는 ISP mainpath 캡처 노드(`/dev/videoN`)를 열어 NV12를 협상할 뿐이다(참조 프로젝트와 동일 — 센서는 I2C `v4l2_subdev`가 CSI2-DPHY로 보고, DT `of_graph`로 rkisp에 바인딩). 파이프라인 링크가 DT로 자동 연결되지 않는 보드는 `deploy/mipi_setup.sh`(media-ctl 템플릿)로 설정.
- **정리**: `VIDIOC_STREAMOFF` → 버퍼 `munmap` → `close(fd)` (소멸자·모든 open 실패 경로에서 호출). fd는 `O_CLOEXEC`라 녹화용 ffmpeg 서브프로세스가 상속하지 않음(코드리뷰 반영).
- **표준 준수**: V4L2 uAPI의 태그드 유니온 접근은 `cppcoreguidelines-pro-type-union-access` NOLINTBEGIN 영역으로 문서화된 deviation(SAD-001 §10.1). `RIPOSTE_WITH_V4L2=ON` 빌드에서 clang-tidy 0 findings·`-Werror` 경고 0 확인.

---

## 4.2 SearchScheduler — 탐색 영역 스케줄링과 확인 판정 (R-5/R-6/R-7)

요구 근거는 RIPOSTE-DUALEO-REQ-001 §4. 매 프레임 **어느 영역에 추론을 돌릴지**를 결정하고, 그 결과가 대상으로 확정되었는지 판정한다. 이미지 데이터를 소유하지 않는 순수 결정 객체라 카메라·NPU 없이 전 정책을 단위시험한다.

```
SEARCH_WIDE   전체 프레임 1회 추론. 가장 싸지만 원거리 대상은 검출 하한 아래로 축소됨
   │ wide_dwell(2) 프레임 무검출
SEARCH_TILE   3x3 타일 중 프레임당 1개씩 순차 검사(9프레임에 한 바퀴).
   │          타일은 네이티브 해상도로 모델에 들어가므로 원거리 대상이 비로소 검출 가능
   │ 검출              │ 한 바퀴 무검출 → SEARCH_WIDE 복귀
CONFIRM       검출 주변 영역 검사. 최근 CONFIRM_WINDOW(10)프레임 중 8회(80%) 연관 시 TRACK (~167ms @60Hz)
   │ 확정              │ 윈도우 내 달성 불가(2회 미스) → SEARCH_WIDE
TRACK         대상 주변 영역 검사 + recheck_period(30, ~0.5s @60Hz)마다 전체 프레임 재탐지(R-7)
                       │ Tracker가 트랙 폐기 → SEARCH_WIDE
```

- **좌표 계약**: 크롭 프레임에서 나온 검출은 `remap_to_frame()`으로 **전체 프레임 정규화 좌표**로 환산한 뒤 Tracker에 넣는다. Tracker·Estimator·오버레이는 종전대로 전체 프레임 좌표만 다룬다(기존 계약 불변).
- **NV12 크롭**: `crop_nv12()`는 크롭 사각형을 **짝수 픽셀 경계로 스냅**한다 — NV12 크로마는 2x2 서브샘플이라 홀수 오프셋은 U/V를 뒤바꾼다. 실제로 잘라낸 사각형을 반환하며, 환산은 요청값이 아닌 **이 값**으로 해야 검출이 어긋나지 않는다. 행 이동은 `width`가 아닌 `stride` 기준(정렬 패딩 대응). 크롭 버퍼는 재사용해 프레임당 할당이 없다.
- **확인 게이팅과 발행 (R-6)**: TRACK 이전에는 TrackBus에 `valid=0`으로 발행한다. 일관되게 관측되지 않은 대상에 OBC가 유도를 걸어서는 안 되기 때문이다. Tracker의 `MIN_TRACK_HITS`(글린트 필터)는 그대로 두고, **제어 세션 대상 확정은 이 10중8 윈도우가 담당**한다(두 단계 방어).
- **TRACK 이탈 조건**: 짧은 미검출은 Tracker가 이미 coast(`TRACKER_MAX_MISSES`)하므로 TRACK을 유지하고, **Tracker가 트랙을 폐기했을 때만** 탐색으로 돌아간다. 미검출 프레임마다 재확인을 돌리면 churn만 생긴다.
- **대상 ROI**: 중심은 대상, 한 변은 `max(track_roi_min, size × track_roi_scale)`이며 **픽셀 기준 정사각**(높이 비율 = 폭 비율 × aspect). 프레임 가장자리에서는 창을 줄이지 않고 안쪽으로 밀어 넣는다.
- **SIL 충실도**: `SyntheticDetector`는 대상을 **센서 좌표**에 두고 `Frame::src_roi`(그 프레임이 센서의 어느 영역인지)에 들어올 때만, 그 영역 좌표로 보고한다. 이것이 없으면 타일 패스마다 대상이 "발견"되어 탐색 정책을 SIL로 검증할 수 없다.

**설계 결정 S-11 (적응형 추론 레이트 배분 — R-10, 이중 EO 선행 설계)**: 이중 EO
도입 시(DUALEO-REQ §4.1) 캡처는 양 채널 60 fps 고정으로 두고, **어느 프레임에 어느
채널의 detect()를 돌릴지**를 SearchScheduler가 상태별로 배분한다 — 탐색/확인:
광각 30 Hz + 협각 30 Hz 교차(짝/홀 프레임), TRACK 확정 후: 협각 60 Hz 전 프레임 +
광각 주기 재탐지(~2–5 Hz, R-7 확장; 재탐지 프레임은 협각 1회 스킵으로 프레임당
추론 1회 유지). 근거: ① 종말 단계 정밀도 — 협각 60 Hz는 LOS 각속도 노이즈와 α-β
속도 추정·PN 지연을 절반으로 줄이고, 탐색은 커버리지 지배라 30 Hz로 충분. ② 센서
fps를 비행 중 바꾸는 대안은 V4L2 스트림 재협상으로 수백 ms 공백을 만들고, 그
시점이 추적 확정 직후라 coast 한도(~267 ms) 내 미복귀 시 트랙을 잃는다 — 추론
스케줄 방식은 공백 0. 전환 트리거는 R-6 윈도우(TRACK 진입/이탈) 그대로다. 구현 시
프레임 카운트 상수(MAX_MISSES·CONFIRM_WINDOW)는 캡처 프레임이 아니라 **해당
채널의 추론 횟수** 기준으로 세야 시간 의미가 유지된다(Tunables 규칙 참조).
저조도는 이륙 전 결정으로 30 fps 캡처 프로파일 선택(비행 중 전환 금지).
**구현(2026-08-15)**: 배분 로직은 `SearchScheduler::channel()`로 구현·시험 완료 —
`Params.dual_eo`(config `seeker.dual_eo`, 기본 off) 게이트, 협각은 자체 연속 타일
스윕(`tile_n_`, 탐색 재시작에만 리셋), 광각 상태기계의 dwell/타일은 광각 슬롯에서만
진행. off면 전 슬롯 WIDE로 기존 단일 카메라 동작과 완전 동일. 잔존은 P4 배선(두
번째 카메라·채널별 좌표 사상·REQ §3.6 보정값 적용·프레임 카운트 상수의 추론 횟수
기준 전환).

**설계 결정 S-15 (2채널 검출 정확도 — R-11/R-12, 2026-08-17)**: 두 카메라는 동축·
광축 평행이라 유효 기선이 0이고 **삼각측량 거리는 성립하지 않는다**. 정확도는 다음
두 기전으로만 올린다.

- **R-11 교차 확인 (확인 가속 + 신뢰 표시)**: 확인 윈도우(R-6) 안에서 같은 트랙이
  광각과 협각 **양쪽에서 각각 검출**되면, 같은 횟수의 단일 채널 히트보다 강한 증거다 —
  두 채널은 광학·분해능·잡음원이 독립이라, 한쪽의 오검출이 다른 쪽에서 같은 방위에
  다시 나타날 확률이 낮다. 따라서 필요한 히트 수를 `DUAL_CONFIRM_RELAX`만큼 낮춘다
  (하한 2). **올리는 방향으로는 절대 작동하지 않는다** — 협각은 좁은 창이라 "협각
  미검출"이 부재의 증거가 아니고, 오검출을 줄이자고 누락을 늘리면 목적을 배반한다.
  결과는 `TrackState.dual_confirmed`로 발행해 하류가 근거를 볼 수 있게 한다(패딩
  바이트 사용 — shm ABI 크기 불변).
  **증거의 채널은 배정이 아니라 실제 추론한 프레임 기준이다**(`TrackCue.hit_from_narrow`).
  협각 슬롯이라도 grab 실패나 FOV 밖이면 광각으로 폴백하는데, 그 검출을 협각 증거로
  기록하면 **단일 채널만으로 완화가 발동**한다 — 이 기능이 줄이려는 위험을 반대로
  키우는 셈이다(2026-08-17 병합 전 리뷰에서 발견·수정).
  **회귀 방어(2026-08-18)**: 이 규칙은 지점별 시험이 아니라 **참조 모델 속성 시험**으로
  고정한다(`test_search`의 `test_r11_matches_reference_model`). 요구에서 유도한 독립
  모델(증거는 실제 검출로만 생기고, 추론 채널에 귀속되며, 확인 중인 정체성에 속하고,
  탐색 재시작·정체성 변경 시 소거되고, TRACK 진입 시 동결)을 8000프레임 결정적 walk로
  구동해 `dual_confirmed()`와 매 프레임 대조한다. 큐 생성은 main 루프 의미론을 그대로
  모사한다(슬롯은 update 전 `channel()`, 협각 grab 실패·FOV 이탈 시 광각 폴백). 아래
  두 결함을 각각 재주입해 이 시험 하나만으로 검출됨을 확인했다. **주의**: 초기 버전은
  정체성이 매 프레임 바뀌어 TRACK에 도달하지 못한 채 통과했고(vacuous), 이를 막기 위해
  4개 모드 방문을 단언하는 커버리지 검사를 함께 둔다 — 소실/포착 위상을 지속시키지
  않으면 타일 스윕에도 도달하지 못한다.
  **완결(2026-08-18 코드리뷰)**: 같은 규칙이 빠져 있던 잔존 2개 지점을 수정했다 —
  ① TRACK 정체성 인계(새 primary 승격)가 `hit_from_narrow` 대신 **슬롯 배정**으로
  귀속하고 있었고(TRACK은 거의 전 슬롯이 협각 배정이라 광각 폴백 검출이 그대로 협각
  증거가 됨), ② 탐색 재진입(CONFIRM 포기 후 coast 중인 트랙의 재확인 진입)이
  `cue.hit` 없이 **alive만으로** 채널 비트를 래치했다(검출이 없던 프레임의
  `hit_from_narrow`는 슬롯 서술일 뿐 증거가 아니다). 증거 래치는 이제 **전 지점에서
  hit-게이트 + 추론 채널 귀속**이다. 회귀 시험: `test_search`의 TRACK 인계·재진입
  시나리오 2건.
- **R-12 채널별 정밀도 우선순위**: 협각 표본은 각분해능이 약 3.8배 높으므로 α-β
  평활에서 **측정을 더 신뢰**한다(α·β를 `NARROW_GAIN_SCALE`배, 각각 상한으로 클램프).
  광각 표본은 종전 게인 그대로다. 단일 카메라 구성에서는 모든 표본이 광각이므로
  동작이 **바이트 단위로 불변**이다.

**설계 결정 S-14 (실시간 데이터 경로 — zero-copy / DMA, 2026-08-17)**: 카메라 →
전처리 → AI 검출 → 후처리 경로에서 **불필요한 복사를 없애는 것이 실시간성의 핵심**이다.
프레임 하나가 1280×720 NV12 기준 약 1.3 MB이고 60 fps이면 복사 한 번마다 초당
약 80 MB의 메모리 대역이 소모된다 — RK3588에서 이 대역은 Hailo PCIe DMA·NPU·
2채널 캡처와 경합한다(K-5).

*현재 상태(정직한 구분)*:

| 구간 | 현재 | 목표 |
|---|---|---|
| 드라이버 → 앱 | **zero-copy**(V4L2 `MMAP`, 버퍼를 매핑해 포인터로 사용) | 유지, 단 `DMABUF` export로 전환해 하류와 fd 공유 |
| 전처리(letterbox) | RGA 경로 존재(`RIPOSTE_WITH_RGA`)하나 **가상주소 기반** — 커널이 내부적으로 매핑/복사 | RGA가 dma_buf fd를 직접 import/export |
| 검출 입력 | 호스트 버퍼를 HailoRT에 전달(런타임이 PCIe DMA 수행) | HailoRT dmabuf 입력으로 복사 제거 |
| ROI 크롭 | `crop_nv12`가 스크래치로 **복사** | 타일/ROI를 DMA 전송의 소스 사각형으로 지정해 복사 제거 |
| 스레드 경계 | `EmbedWorker`·`RecordWorker`로 프레임 **복사**(의도적) | 유지 — 아래 근거 |

*의도적으로 남기는 복사*: 워커로 넘기는 프레임 복사는 제거 대상이 아니다. `Frame.data`는
카메라 mmap 버퍼를 가리키고 그 버퍼는 **다음 grab에서 재사용**되므로, 복사하지 않으면
워커가 읽는 도중 내용이 바뀐다. 이 복사는 정확성의 대가이며, 없애려면 버퍼 풀 소유권
모델(참조 카운트 + 지연 재큐)이 선행되어야 한다.

*브링업 확인 항목*: DMABUF 전환은 드라이버·RGA·HailoRT 세 쪽의 실제 지원 여부에
달려 있으므로 BRINGUP-001 §3(B2)에서 실측 후 확정한다. **현 시점의 주장은 "드라이버
→ 앱 구간 zero-copy"까지이며, 종단 zero-copy는 미구현이다.**

**설계 결정 S-13 (P4 2채널 인지 배선, 2026-08-17)**: 협각을 인지 경로에 넣을 때의
계약이다.

- **공용 좌표계는 광각(WIDE) 정규화 좌표**다. Tracker·SearchScheduler cue·TrackFusion·
  TargetEstimator는 전부 지금 그대로 광각 좌표로 동작하며, 채널 개념을 모른다.
  협각 결과는 **경계에서 광각으로 되돌린다**(ChannelMap `narrow_to_wide`).
  중심뿐 아니라 **크기도 환산**해야 한다 — 정규화 크기는 HFOV에 반비례하므로
  `size_wide = size_narrow × (hfov_narrow / hfov_wide)`. 이를 빠뜨리면 단안 거리
  추정이 채널마다 달라져(협각에서 ~3.8배 크게 보임) 거리가 급변한다.
- **카메라 소유는 인지 루프**로 통일한다. `/dev/video1`은 동시에 두 번 열 수 없어
  녹화 워커와 인지가 각자 열 수 없기 때문이다. 두 카메라 모두 매 틱 grab 하고
  (캡처 60 fps 고정, S-11), **추론만 슬롯이 배정한 채널에 한다**.
- **ROI 좌표계 주의**: `tile_roi()`는 프레임 무관한 격자 분수라 어느 프레임에나 그대로
  적용된다. 반면 `target_roi()`는 cue(광각 좌표) 기반이므로 **협각 슬롯에서는
  `wide_to_narrow`로 옮겨** 잘라야 한다. 옮긴 결과가 협각 FOV 밖이면(`in_fov=false`)
  그 슬롯은 협각으로 볼 수 없으므로 **광각으로 폴백**한다 — 협각은 광각 중앙의 좁은
  창이라 대상이 조금만 벗어나도 보이지 않는다.
- **녹화 경로**: `RecordWorker`는 협각 카메라를 더 이상 소유하지 않고 인지 루프가
  두 프레임을 게시한다. dual_eo가 켜져 있으면 인지가 이미 낸 협각 검출을 오버레이
  힌트로 재사용하므로 **레코더 스레드의 두 번째 NPU 호출이 사라진다**. dual_eo가 꺼진
  녹화 전용 구성에서는 종전대로 레코더가 자체 검출기를 돌린다(프레임만 전달받음).
- **fail-closed**: `dual_eo=true`인데 협각 카메라가 열리지 않으면 기동 거부한다.
- **페어링 계약 (2026-08-18 코드리뷰 반영, 4항)**:
  ① **T1 임베딩은 추론한 프레임에서 crop한다** — 광각 환산 *이전의* 원좌표 박스로.
  협각 검출을 광각 환산 후 광각 프레임에서 자르면 박스가 ~(hfov_n/hfov_w)배로
  줄어 몇 픽셀짜리 블러 crop이 되고, 그 임베딩이 갤러리를 EMA 오염시켜 채널
  인계 순간 참 페어링을 하드 리젝트한다(TR-B). 환산은 임베딩 이후에 하되, FOV
  드롭 시 임베딩 배열을 **동일 인덱스로 압축**한다(`remap_detections_to_wide`
  정렬 유지 오버로드 — 어긋난 배열은 검출과 외관을 뒤바꿔 페어링한다).
  ② **협각 협상 해상도는 광각과 일치해야 한다** — ChannelMap의 aspect·레코더의
  프레임 페어링이 전부 광각 협상 해상도에서 유도되므로, 드라이버가 다른 모드로
  대체한 협각은 참여할 수 없다. dual_eo면 기동 거부(부재 카메라와 동일 규칙),
  녹화 전용이면 경고 후 단일 채널 강등.
  ③ **협각 dequeue는 비블로킹(0 ms)이다** — 협각 링은 프레임을 버퍼하므로 건강한
  채널은 대기 없이 최신 프레임을 내주고, 죽었거나 위상이 어긋난 채널은 기다릴
  것이 없다. 양(+)의 대기 예산은 협각이 무프레임인 **매 틱** 광각 채널(유도가
  의존하는 쪽)의 16.7 ms 예산에서 지불된다 — 10 ms 예산은 실기 추론 지연과
  겹치면 광각 루프를 ~40 Hz까지 끌어내렸다. SIL의 SyntheticCamera는 timeout 0에서
  **버퍼드 카메라 모델**(최신 프레임 재제공)로 동작해 실 V4L2 링과 동형이다.
  ④ **광각↔협각 프레임 시각차 상한**(`NARROW_WIDE_SKEW_MAX_NS`, 50 ms) — 협각
  측정은 광각 프레임의 타임스탬프로 발행되고 광각 루프 dt로 차분되므로, 두
  캡처는 (거의) 같은 순간이어야 한다. age-vs-now 가드는 광각 grab이 멈춘 사이
  링에서 늙은 협각 버퍼를 보지 못한다 — 복구 틱이 신선한 광각 프레임과 100 ms+
  묵은 협각 버퍼를 짝지으면 위치 점프가 **신선한 스탬프를 단 속도 과도**로
  발행되고 하류 신선도 검사는 이를 거를 수 없다. 상한 초과는 광각 폴백
  (`ChannelMap::frames_pairable`, 호스트 시험).

**`seeker.dual_eo=true`는 P4 배선 전까지 기동 거부였다(2026-08-17, fail-closed)**: 배분 정책은
구현·시험됐지만 `channel()`을 소비하는 코드가 파이프라인에 없고(프레임 취득은
`cam->grab()` 한 곳뿐), `ChannelMap`도 미배선이다. 켜면 NARROW 슬롯이 **광각
프레임을 협각 타일 ROI로 자를** 뿐이라, 좌표계가 자기일관적이어서 오류로 드러나지도
않는다 — 즉 운용자만 협각이 300 m를 획득한다고 오해한다. 강등(WIDE 고정)이 아니라
거부를 택한 이유는 SM-10 폴리곤과 같다(CR-04): 오퍼레이터가 명시한 구성을 임의로
축소하지 않는다. `SeekerConfig::validate()`의 해당 규칙은 **P4 배선과 함께 삭제**한다.

**단위시험 (`test/test_search.cpp`, ctest `search`)**: 137 checks — 광각→타일 폴백, 9타일 순차·중복 없음·합집합이 전체 프레임, 4중5 확정, 1미스 허용·2미스 포기, 트랙 폐기 시 리셋, 확정 전 `confirmed()=false`, 대상 ROI 중심/픽셀정사각/가장자리 클램프, coast 유지, R-7 주기 재탐지(주기 0=비활성), 좌표 환산(타일 중심·항등·모서리), NV12 크롭(치수·내용·짝수 스냅·경계 클램프·stride 패딩·비NV12/축퇴 거부).

---

## 4.3 ModelIo — 모델 입출력 레이아웃 (letterbox · 디코드 · NMS)

가속기를 **둘러싼** 수학을 HailoRT 어댑터 밖으로 분리한 계층. 테스트로 잡을 수 있는 종류의 오류는 전부 이쪽에 모아 호스트에서 검증하고, `HailoDetector.cpp`에는 장치 호출(vdevice·network group·vstream)만 남긴다.

**설계 결정 S-6 (종횡비 보존 letterbox)**: 프레임(또는 탐색 타일)을 정사각 모델 입력에 넣을 때 **늘리지 않고 패딩**한다. 1280×720의 3×3 타일은 **426×240**이라, 640×640으로 늘이면 폭과 높이의 배율이 달라진다. 그러면 모든 bbox의 폭/높이 비가 왜곡되고, Tracker의 폭-정규화 `size`가 Estimator의 단안 거리로 들어가므로 **왜곡이 곧 대상 거리 오차**가 된다 — 300 m 제어 세션 기하 전체가 이 한 숫자 위에 서 있다. 크래시도 경고도 없이 조용히 틀리는 종류의 오류라 단위시험으로 고정했다.

- **패딩 검출 기각**: 역변환 시 중심이 패딩 띠 안에 있는 검출은 버린다. 네트워크가 채움 영역에 반응한 것이지 대상이 아니며, 그대로 매핑하면 화면 가장자리에 유령 대상이 생겨 Tracker가 실대상과 구분할 수 없다.
- **NMS는 모델 공간에서**: 입력이 정사각이라 폭·높이가 같은 분모를 쓰므로 IoU가 의미를 갖는다. letterbox 역변환 뒤(비정사각 원본)에 돌리면 두 축의 정규화가 달라져 한쪽 축의 겹침을 계통적으로 잘못 잰다. 클래스가 다르면 서로 억제하지 않는다.
- **디코드**: `[4+nc, num_anchors]` row-major, 박스는 모델 픽셀, 클래스 점수는 [0,1] 가정(표준 YOLOv8 export). 앵커당 최고점수 클래스 1개만 방출. **버퍼 길이를 검사**해 `num_classes`/`num_anchors` 불일치 시 텐서 밖을 읽지 않고 **빈 결과로 실패**한다(그럴듯한 쓰레기를 내놓지 않음).
- **비신뢰 텐서 방어 (2026-08-16, 심층 코드리뷰 P1-06)**: raw 폴백의 텐서는 장치 출력 = 비신뢰 데이터다. ① 채택 앵커의 **모든 스칼라(cx/cy/w/h) finite 검사** — NaN 폭은 `w<=0` 검사를 통과해 Tracker/Estimator까지 전파됐다(NaN 비교는 모두 false) ② 좌표·크기 **범위 상한**(정규화 후 [−0.5, 1.5] 밖·크기 2 초과는 쓰레기로 폐기 — letterbox 역변환 전 1차 방어) ③ **후보 상한 `RAW_DECODE_MAX_CANDS`**(기본 512): 임계 통과 후보가 상한을 넘으면 점수순 top-K만 잔존시켜 O(N²) NMS의 최악 비용을 상수로 묶는다(8400² → 512²).
- **헤드 레이아웃 (2.2에서 확정)**: 제품 표준 HEF는 **NMS 내장**(§4.4 S-7)이므로 `decode_yolov8`+`nms`는 **폴백 경로**다 — NMS 없이 컴파일된 실험 모델을 물릴 때만 쓰인다. 폴백의 `[4+nc, anchors]` 단일 텐서 가정은 실 HEF에서 확인 필요(스케일별 분리 브랜치로 나올 수 있음). `init()`에서 HEF의 실제 입력 변·클래스 수·앵커 수를 읽어 config 값을 덮어쓴다 — config는 로드 전 기대치일 뿐이다.

**단위시험 (`test/test_modelio.cpp`, ctest `modelio`)**: 74 checks — letterbox(정사각 무패딩, 가로/세로 긴 원본의 패딩 축, 축퇴 입력 거부, 중심↔중심, **위치·크기 왕복**, **픽셀 정사각 보존**(늘림 회귀 방지), 패딩 검출 기각(상·하단 및 경계 직내부), 모서리 왕복), NMS(중첩 억제·분리 유지·클래스별·점수 정렬·임계 경계 IoU=1/3·빈/단일 입력·군집에서 최고만 잔존), 디코드(단일 앵커 값 정확도, 임계 적용, 앵커당 최고 클래스 1개, 버퍼 부족/널/클래스0 거부, 축퇴 박스 skip), 그리고 **디코드→NMS→역변환 전 파이프라인**.

---

## 4.4 HailoDetector — HailoRT 장치 경로 (Hailo-8 실구현 설계)

`HailoDetector.cpp`의 INTEGRATE 지점을 채우는 설계. 검증 계보는 SkyHunter-1.0
`human_detector`(Hailo-8 M.2 실기 검증) → 구 Riposte `perception/detector_hailo.cpp`
(강화판)이며, **HailoRT 호출 시퀀스와 방어 패턴만 가져오고 전처리(stretch resize·
OpenCV)는 가져오지 않는다** — 전처리는 S-6 letterbox와 S-9를 따른다.

**설계 결정 S-7 (NMS-on-device 주 경로 + 출력 포맷 자동 감지)**: 제품 표준 HEF는
Hailo Model Zoo `nms_postprocess`를 포함해 컴파일한다(NMS-by-class 출력). 근거:
① raw YOLOv8 헤드는 스케일별 분리 브랜치 + DFL(16-bin) 디코드 + 역양자화를 호스트가
져야 해 구현·검증 비용이 크고 조용히 틀리기 쉽다. ② NMS-by-class wire format은
모델 세대(v8/v10/v11)가 바뀌어도 불변이라 **모델 교체 시 호스트 코드 불변** — 최신화
계약으로 이쪽이 안정적이다. ③ 사내 실기 검증 계보가 전부 이 경로다.
`init()`은 출력 vstream 메타데이터의 format order로 분기한다:

| 출력 format order | 경로 | 후처리 |
|---|---|---|
| `HAILO_FORMAT_ORDER_HAILO_NMS*` | **주 경로** | `parse_nms_by_class` → 클래스/점수 필터 → `letterbox_undo` |
| float32 raw 텐서 | 폴백 | `decode_yolov8` → `nms` → `letterbox_undo` (§4.3) |

분기는 init()에서 한 번 결정되어 Impl에 고정된다. 어느 쪽이든 출력은 **모델 입력
정규화 좌표**이므로 §4.3의 `letterbox_undo`(패딩 검출 기각 포함)가 공통 종단이다.

**클래스 id 규약**: NMS-by-class 버퍼는 클래스 섹션 순서가 곧 id(0-based)다. 구
코드의 1-based(+1, 0=background)는 그쪽 라벨맵의 선택이었을 뿐이므로 **가져오지
않는다** — 파서는 0-based를 그대로 방출해 config `seeker.target_class=0` 관례와
`decode_yolov8` 폴백이 같은 좌표계를 쓴다.

**설계 결정 S-8 (디바이스 저임계 + 호스트 동적 필터)**: HEF 컴파일 시 score
threshold는 낮게(기본 0.15, `SCORE_THR_DEVICE`) 잡거나 configure 시
`set_nms_score_threshold`로 낮춰 두고, 운용 임계는 **호스트에서 프레임마다**
`seeker.score_thr`로 필터한다. 근거: 탐색(저임계·고recall)/추적(고임계) 모드별
임계 분기는 호스트 로직이고, 디바이스 임계를 운용 임계로 쓰면 recall을 재컴파일
없이 올릴 수 없다. NaN-safe 비교(`!(score >= thr)` 기각)를 사용한다.

### 4.4.1 AsyncInfer 래퍼 (HailoDetector.cpp 내부, HailoRT 4.x InferModel API)

```
init():   VDevice::create → create_infer_model(hef) → [set_nms_score_threshold]
          → configure → create_bindings
          → 입력 vstream: frame size·shape 읽어 params.model_size 덮어씀,
            포맷 검증(RGB888/NHWC/uint8 기대 — 불일치 시 init 실패, fail closed)
          → 출력 vstream: format order로 S-7 분기, NMS면 클래스 수·max_bboxes,
            raw면 num_anchors·num_classes를 HEF에서 읽어 config 덮어씀
          → target_class를 **로드된 클래스 수에 대해 재검증** — 기동 검증은
            config의 model_classes 기대값만 봤으므로, 단일 클래스 HEF에
            target_class=1을 배포하면 모든 레코드가 필터로 걸러져 detect()는
            true, healthy()도 true인 채 **영구 빈 결과**가 된다(2026-08-18
            코드리뷰: 불일치는 init 실패, fail closed)
detect(): Preproc(§4.4.3) → 입력 스테이징에 기록 → wait_for_async_ready(1s)
          → run_async + promise/future 브리지 → future wait(INFER_TIMEOUT_MS)
          → 타임아웃 시 false(검출 없음 아님 — 장애), 연속 N회 실패 시 healthy()=false
```

- **예외 격리는 std::exception + catch(...) 쌍**이다(2026-08-18 코드리뷰): vendor
  런타임은 std::exception 파생을 보장하지 않으며, 비-std throw가 init/detect
  경계를 벗어나면 핸들러 없는 인지 루프에서 std::terminate — 국소 장애(healthy()
  강등, SM-7)여야 할 것이 프로세스 사망이 된다. RknnEmbedder/EmbedWorker 경계와
  동일 패턴으로 통일.

- **설계 결정 S-10 (job별 자원 소유 — 2026-08-16 개정, 심층 코드리뷰 P0-05)**:
  **모든 job-가시 자원은 그 job이 소유한다.** 종전(2.2~2.7)의 "입력 스테이징 1회
  할당 재사용 + bindings 재사용"은 타임아웃 시 두 가지 결함을 남겼다: ① detached
  job이 아직 DMA-read 중인 입력 버퍼를 다음 detect()가 덮어써 혼합 프레임 추론이
  되고, ② 공유 bindings 객체를 in-flight job과 다음 호출이 동시에 만졌다(HailoRT는
  이를 보장하지 않는다). 개정 설계:
  - **입력 버퍼·출력 버퍼·bindings를 detect() 호출(=job)마다 생성**하고, 그
    `shared_ptr` 가드 전부를 완료 콜백 캡처에 실어 **detached job보다 오래
    살게** 한다. 60Hz에서 mmap+memset 비용은 브링업 실측 항목(§4.4.5); 버퍼 풀
    최적화는 DEFERRED(정확성 먼저).
  - **runtime owner**: VDevice·InferModel·ConfiguredInferModel을 `DeviceCtx`
    하나로 묶어 `shared_ptr`로 소유하고, **완료 콜백도 이 shared_ptr를 캡처**한다
    — 소멸자가 어떤 순서로 불려도 마지막 outstanding job의 콜백이 끝날 때까지
    런타임이 살아 있음이 타입 수준에서 보장된다(리뷰가 요구한 "수명 보장이 증명된
    runtime owner"). DeviceCtx 내부 선언 순서(vdevice→infer_model→configured)의
    역순 파괴가 HailoRT의 요구 순서와 일치한다.
  - **소멸자 bounded drain**: outstanding 카운터(콜백에서 감소)를 소멸자가
    2×INFER_TIMEOUT까지 대기하고, 그래도 남으면 로그 후 진행한다 — 콜백 캡처가
    자원을 쥐고 있으므로 진행해도 UAF가 아니며, 대기는 질서 있는 종료를 위한
    것일 뿐이다.
  - 타임아웃 후 장치 backpressure는 종전과 동일: 다음 detect()의
    `wait_for_async_ready`가 실패해 fault로 계수되고 healthy() 게이트로 전파.
- **동기 경계**: `IDetector::detect()`는 동기 계약이므로 promise/future로 브리지.
  타임아웃 시 파이프라인은 최대 `INFER_TIMEOUT_MS`(1000ms) 블록 — S-1 latest-wins가
  밀린 프레임을 폐기하고, 지속 장애는 healthy() → HealthBus → TrackBus 신선도
  저하 → SM-7 disengage로 전파된다(§1의 국소 장애 원칙 유지).
- **예외 경계**: HailoRT C++ API 예외는 detect()/init() 경계에서 전부 catch해
  false로 변환한다. 예외가 파이프라인 루프를 뚫으면 시커 프로세스가 죽고, 그건
  국소 장애가 아니라 supervisor 재시작 이벤트가 된다.

### 4.4.2 HailoNmsParse — NMS-by-class 파서 (호스트 시험 대상)

구 `perception/hailo_parse.hpp`(경계검사 강화판)를 이식한 **헤더온리·HailoRT-free**
파서. wire layout — 클래스 c ∈ [0, class_count) 순서로:

```
float32 det_count;
det_count × { float32 y_min, x_min, y_max, x_max, score }   // 20 bytes
```

버퍼는 NPU/드라이버가 쓴 **비신뢰 장치 데이터**로 취급한다: 모든 read는 버퍼 크기에
대해 경계검사, `det_count`는 정수 변환 **전에** 유한성·[0, max_dets] 검증(NaN/음수/
거대값의 float→int 변환은 UB), bbox 레코드는 유한성·[0,1]·비반전 검증 실패 시 개별
드롭, 프레이밍이 깨지면(잘린 레코드·불량 count) 그 지점에서 중단한다. 출력은
`Detection{cx,cy,w,h}`(모델 입력 정규화, 0-based cls)로 변환해 방출 — 이후는
`letterbox_undo` 공통 종단.

### 4.4.3 Preproc — NV12→RGB888 letterbox 전처리 (무-OpenCV)

**설계 결정 S-9 (RGA 주 경로 + CPU 참조 폴백, OpenCV 불사용)**: 모델 입력 변환
(NV12 크롭/프레임 → `model_size²` RGB888, S-6 letterbox 배치)은 두 구현을 둔다:

| 경로 | 조건 | 구현 |
|---|---|---|
| **RGA** (RK3588 im2d) | `RIPOSTE_WITH_RGA` | NV12 src → RGB888 dst rect(`pad_x,pad_y,content_w,content_h`) 스케일+색변환 1-pass, 패딩은 init 시 1회 채움 |
| **CPU 참조** | 항상 컴파일 | 자체 구현 bilinear NV12 스케일 + BT.601 YUV→RGB (~100줄, 외부 의존 0) |

근거: OpenCV는 이 한 변환 때문에 끌고 오기엔 과대 의존이고(§common의 최소 의존
원칙), libyuv조차 CPU 폴백에는 불필요 — 폴백은 SIL/브링업용이라 성능이 아닌
**정확성 기준(참조 구현)** 이 목적이다. 운용 성능은 RGA가 담당한다. CPU 경로는
호스트 단위시험 대상이며, 브링업에서 RGA 출력을 CPU 참조와 비교해(최대 픽셀 오차
한계) 검증한다. 패딩 값은 모델 학습 관례(114,114,114)를 `Tunables`에 상수화.

### 4.4.4 Config / Tunables / CMake 변경

- config: 기존 `seeker.hef`·`score_thr`·`nms_iou`·`target_class` 유지(의미 불변 —
  `score_thr`는 호스트 운용 임계). `nms_iou`는 폴백 경로에서만 쓰인다.
- `Tunables.h` 추가: `INFER_TIMEOUT_MS=1000`, `SCORE_THR_DEVICE=0.15f`,
  `HAILO_MAX_CONSEC_FAULTS`(healthy() 강하 임계), `LETTERBOX_PAD_VALUE=114`.
- CMake: `RIPOSTE_WITH_HAILO`에 `find_library(hailort)`+링크 연결(기존 INTEGRATE
  해소), `RIPOSTE_WITH_RGA` 옵션 신설(librga im2d, HAILO와 독립 토글).
  `Preproc.cpp`(CPU)·`HailoNmsParse.h`는 옵션과 무관하게 항상 컴파일(호스트 시험).

### 4.4.5 브링업 체크리스트 (실 HW에서 확인해야 설계가 닫힘)

> 실행 절차·판정 기준·기록표는 **RIPOSTE-BRINGUP-001 §3(B2)** 에 있다. 아래는
> 그 항목의 설계 근거다.

1. 입력 vstream 포맷이 RGB888/NHWC/uint8인지 (다르면 init fail-closed가 잡는다)
2. 출력 format order가 `HAILO_NMS_BY_CLASS`인지, 클래스 수·max_bboxes 값
3. 보유 HailoRT 버전에서 `set_nms_score_threshold`/`set_nms_iou_threshold` 지원 여부
   (미지원이면 HEF 컴파일 임계를 `SCORE_THR_DEVICE`로 낮춰 동일 효과)
4. RGA 출력 vs CPU 참조 픽셀 비교(허용 오차 내), RGA의 stride/정렬 제약
5. 추론 지연 실측(EMA)·발열, 타임아웃/연속 실패 → SM-7 전파 실동작

---

## 4.5 EmbedWorker — T1 임베딩 격리 (TR-4, S-12)

`IEmbedder`(RKNN)를 인지 스레드에서 떼어내는 deadline worker. §2 S-12가 정책,
여기가 계약이다.

```cpp
class EmbedWorker {
public:
    EmbedWorker(std::unique_ptr<IEmbedder> emb, uint64_t deadline_ns);
    ~EmbedWorker();                       // bounded drain 후 detach (S-10과 동일 근거)
    void start();
    // 인지 스레드: 제출 → deadline까지 대기 → 결과. 항상 비차단 제출.
    // true = out이 dets에 인덱스 정렬된 임베딩(동기 경로와 동일).
    // false = 이 프레임은 운동 단독(T0). out은 비워진다.
    bool embed_by_deadline(const Frame& f, const std::vector<Detection>& dets,
                           std::vector<Embedding>& out);
    bool degraded() const;                // 연속 기한 초과로 영구 강등됨
    uint64_t deadline_misses() const;      // HealthBus 계수
};
```

계약 세부:

| 항목 | 규정 |
|---|---|
| 프레임 소유권 | `Frame.data`는 카메라 mmap 포인터라 다음 grab까지만 유효 → 제출 시 **복사**(RecordWorker와 동일 근거) |
| 세대(generation) | 제출마다 증가. 워커 결과는 세대가 일치할 때만 수용 → 늦게 온 결과는 조용히 폐기 |
| 워커 점유 중 제출 | 게시하지 않고 즉시 false(운동 단독). 큐 없음 = 지연 누적 없음 |
| 강등 | 연속 `EMBED_DEADLINE_FAULTS`회 초과 → 이후 제출 없음. **회복 판정 없음**(TR-3, 치명 아님) |
| 소멸 | stop 신호 → `EMBED_DRAIN_NS`까지 대기 → 미응답이면 detach. 공유 상태는 `shared_ptr`로 워커와 공동 소유하므로 detach 후 접근도 안전 |
| SIL | `SyntheticEmbedder`는 마이크로초 단위라 항상 기한 내 — SIL 동작은 불변 |

**호스트 검증 가능**(브링업 불필요): 느린/영구 블록 임베더 테스트 더블로
기한 상한·운동 단독 강등·늦은 결과 폐기·소멸 무교착을 전부 시험한다. 실 HW에서
남는 것은 `embed_deadline_ms` 실측 튜닝뿐이다.

---

## 5. Tracker — 다중대상 추적

최대 `TRACKER_MAX_TRACKS`개 트랙을 동시에 유지한다.

- **연관(association)**: 우선순위 순(primary 먼저, 이후 품질 내림차순)으로 각 트랙이
  자기 게이트(`TRACKER_GATE_PX` 정규화 환산) 내 **최고점수 미할당 검출**을 선점. 탐욕적
  per-track 연관 — 단일대상 시 기존 동작과 동일.
- **신규 트랙**: **모든** 기존 트랙의 게이트 밖 검출만 신규 대상으로 spawn(한 대상의
  중복 검출은 게이트 안이라 유령 트랙을 만들지 않음). `TRACKER_MAX_TRACKS` 상한 초과 시 무시.
- **필터**: 트랙별 α-β(위치 `TRACKER_ALPHA`, 속도 `TRACKER_BETA`). 등속 예측 → 잔차 피드백.
- **소실 처리**: 연속 미검출 `TRACKER_MAX_MISSES` 초과 시 트랙 폐기, 그 전까지 품질 감쇠하며 coast.
- **확인 게이팅 (2026-07-04 심층 리뷰 반영)**: 연관 검출 `MIN_TRACK_HITS`(=2, Tunables 신규) 미만인 트랙은 valid가 아니며 primary 후보도 되지 못한다 — **1프레임 오검출(글린트)이 제어 세션 대상이 되는 것을 차단**.
- **등방 게이트 (2026-07-04 심층 리뷰 반영)**: 연관 거리는 폭-정규화 단위로 계산하되 dy를 `1/aspect`로 스케일해 **픽셀 기준 등방**(정규화 좌표 그대로 쓰면 세로 게이트가 aspect배 넓어짐). `TRACKER_GATE_PX`(120px)는 **기준 폭 1280px**에서 정의 — 다른 센서에서는 영상 폭의 고정 비율(≈수평 FOV의 고정 비율)로 동작.
- **크기 단위 통일 (2026-07-04 심층 리뷰 반영)**: 트랙 `size`는 `max(w, h/aspect)` **폭-정규화** — Detection의 h는 높이-정규화라 그대로 섞으면 Estimator의 폭 기반 초점거리와 단위가 어긋나 거리 추정이 왜곡된다. aspect는 협상 해상도 기반(§4.1).

**설계 결정 S-3 (최대 크기 우선 + commit 후 sticky, 2026-08-12 R-8 반영)**: 트랙 하나를
**primary(제어 세션 대상)** 로 지정하고, `current()`/TrackBus는 primary만 발행한다(단일대상 계약
유지 → OBC·하위 호환). 선정 기준과 고정 시점은 다음과 같다.

- **선정 = 겉보기 크기 최대**(동률 시 quality). 물리적 크기가 비슷한 대상들 사이에서 가장 큰
  bbox는 가장 가까운 대상이므로, 이것이 R-8의 "가장 큰 드론을 먼저 대응"이자 "가장 가까운
  위협부터"이다. 단일대상·동일크기 상황에서는 종전(품질 최고)과 동작이 동일하다.
- **고정(sticky)은 commit 이후에만**. `lock_primary(true)`가 걸리기 전까지는 매 프레임 재선정
  하고, 걸린 뒤에는 소멸 시까지 고정된다. 시커는 SearchScheduler가 대상을 확정(TRACK)한
  순간 lock을 건다. 무조건 sticky로 두면 **작은 대상이 먼저 확정되었다는 이유만으로 한두
  프레임 뒤 나타난 더 큰 대상을 영영 놓치게 되어** R-8과 충돌한다. 반대로 lock이 없으면
  세션 활성 중 이웃에게 대상을 빼앗긴다. 두 요구는 "확정 전에는 열려 있고 확정 후에는 닫힌다"로
  양립한다.
- **pre-lock 재선정에는 히스테리시스가 있다 (2026-08-18 코드리뷰 반영)**: lock 전이라도
  살아 있는 현직(primary)은 **도전자가 결정적으로 클 때만**(`TRACKER_PRIMARY_SWITCH_MARGIN`,
  1.2배) 자리를 내준다. 마진 없이 매 프레임 최대 크기를 뽑으면 크기가 비슷한 두 트랙이
  박스 노이즈로 순위를 반복해서 뒤집고, 뒤집힐 때마다 스케줄러의 확인 윈도우가 정체성
  결합(TR-7) 규칙대로 리셋되어 — 두 대상이 모두 살아 있는 동안 **확인이 영원히 완료되지
  못하고 아무것도 발행되지 않는** 라이브락이 된다. 진짜 더 가까운 대상은 크기가 1/거리로
  자라므로 20 % 마진을 금방 넘는다(R-8 유지). 현직 부재·소멸 시 재선정은 종전과 같다.
- primary 소멸 시에는 lock 여부와 무관하게 재선정된다(sticky ≠ stuck).
- 전체 트랙 집합은 `tracks()`, 확정 트랙 수는 `confirmed_count()`로 노출. 헝가리안 전역 최적
  연관은 여전히 DEFERRED(탐욕적 연관으로 충분).

**대상 개수 발행 (R-8)**: `TrackState.num_targets` = 확정 트랙 수(미확정 글린트 제외). 이 필드는
**기존 패딩 바이트에 배치**해 `sizeof(TrackState)`를 48바이트로 유지했다 — SeqSlot의 교차
프로세스 ABI 검사가 크기 기준이라, 크기가 바뀌면 시커와 OBC가 동시 재배포될 때까지 조용히
통신이 끊긴다. 크기는 `static_assert`로 고정했고, 필드 추가는 패딩 소진 범위 내에서만 허용한다.
구형 writer는 0을 보내므로 "미발행"으로 읽히지 틀린 개수로 읽히지 않는다.

**단위시험(`test_seeker`)**: 신규 4건 — 분리대상 2개 spawn/독립 갱신(연관 교차오염 없음),
sticky primary→소멸 후 핸드오프, `TRACKER_MAX_TRACKS` 용량 상한. 기존 단일대상 5건(획득·
클래스필터·α-β·게이팅·최고점수·coast/drop) 모두 유지. ASan/UBSan clean.

---

## 6. TargetEstimator — 단안 기하 추정

```
픽셀 정규화중심 → LOS각(az/el, HFOV 기반) → 거리(대상 크기 기반) → BODY FRD 위치
                                                                → 유한차분 → 상대 속도
```

- **거리 추정**: `range = (target_size_m × focal_norm) / bbox_size`. 대상 겉보기 크기 기반 단안 추정. `bbox_size`는 Tracker의 폭-정규화 `size`(§5)로 초점거리와 단위 일치.
- **출력 프레임**: BODY FRD(x전방·y우·z하). NED 변환은 OBC가 담당(자세 정보 소유).
- **핸드오프 속도 리셋 (2026-07-04 심층 리뷰 반영)**: 유한차분 속도는 **같은 트랙**에 대해서만 의미가 있다 — primary 핸드오프로 track id가 바뀐 프레임에 그대로 차분하면 서로 다른 대상 간 위치 차가 1틱짜리 속도 스파이크로 유도에 유입된다. id 변경 시 속도 0을 발행하고 차분을 재시드한다.

**설계 결정 S-4 (단안 거리추정 + 후속검증)**: 초기엔 대상 크기 기반 단안 거리로 PN을 시드. 정확도 검증은 DEFERRED이며, 부정확 시 레인지 센서 추가를 검토(SAD-001 §13).

---

## 7. 발행 (TrackBus / HealthBus)

- **TrackBus** (`SHM_TRACK`, ~60Hz): 매 프레임 `TrackState` 발행. 대상 없음도 `valid=0`으로 **항상 발행** → OBC가 "대상 없음"을 신선하게 인지. `TrackState.seq`는 **발행마다 증가하는 실제 카운터**(수신측 갱신 판별용, 2026-07-04 심층 리뷰 반영). `valid`는 SearchScheduler가 확정(TRACK)한 뒤에만 1이 된다(§4.2, R-6). `num_targets`는 제어 세션 여부와 무관하게 확정 대상 수를 싣는다(§5, R-8).
- **HealthBus** (`SHM_SEEKER_HEALTH`, ~2Hz): fps·추론지연·카메라/Hailo 상태.

**설계 결정 S-5 (무대상도 발행)**: 침묵이 아니라 `valid=0` 명시 발행. OBC의 SM-7은 "스테일"과 "명시적 무대상"을 구분해 각각 처리(전자는 coast 후 disengage, 후자는 즉시 유도불가).

**버스 개방 실패 = 치명 (2026-07-04 심층 리뷰 반영)**: TrackBus/HealthBus **WRITER `open()` 실패 시 ERR 로그 후 프로세스 종료**. 실패를 무시하면 아무것도 발행하지 못하는 채로 조용히 돌고, 하류는 이를 "대상 없음"으로 오독한다(무발행 침묵 실행 제거). systemd가 on-failure로 재시작한다.

---

## 8. 추적성

| 요구/결정 | 구현 |
|---|---|
| A-1 인지/제어 분리 | 독립 프로세스, FC 무접근 |
| 가속기 장애 격리 | `IDetector.healthy()` → TrackBus 신선도 → SM-7 |
| S-1 latest-wins | `FRAME_STALE_NS` 폐기, 큐깊이1 |
| S-2 HW 격리 | `IDetector`/`ICamera` + CMake 옵션 |
| C-1 프레임 규약 | BODY FRD 발행 |

---

## 8.1 영상 녹화 (VideoRecorder, 선택)

카메라 스트림을 **H.264/MP4 30초 세그먼트**로 기록한다(증거·디버깅용). 설계 원칙:

- **인지 루프 비차단**: 인코딩을 `ffmpeg` 서브프로세스에 오프로드(raw NV12를 파이프로 전달). **파이프에 프레임 하나가 통째로 들어갈 여유가 있을 때만** 기록하고(`can_write` — `POLLOUT`은 1바이트만 보장하므로 `F_GETPIPE_SZ`−`FIONREAD`로 여유공간을 프레임 단위로 확인; 단일 writer라 확인 후 여유는 늘기만 함 → 블로킹 없음), 없으면 프레임을 **통째 드롭**(latest-wins)한다. 인코더가 죽으면(`POLLERR`) 즉시 감지·회수(`waitpid`)하고 **다음 프레임에 새 세그먼트 재시작**(30초 회전을 기다리지 않음). 컴파일타임 코덱 의존성 없음 — 타깃은 config로 하드웨어 인코더(`h264_rkmpp`) 선택 가능. ffmpeg 부재 시 자동 비활성(인지 무영향).
- **파이프 용량 클램프 (2026-07-04 심층 리뷰 반영)**: `F_SETPIPE_SZ` 요청(프레임 4개분)을 시스템 한도 **`fs.pipe-max-size`로 클램프** — 한도 초과 요청은 부분 승인이 아니라 **통째 실패**해 64KiB 기본값이 남고, 이는 NV12 프레임 하나도 못 담는다. 클램프 후에도 부여 용량이 프레임 1개 미만이면 모든 write가 블록될 것이므로 **ERROR 1회 로그 후 녹화를 영구 비활성**(제어/인지 루프 보호가 녹화보다 우선). **배포 노트**: 720p NV12는 프레임당 1.38MB로 기본 `fs.pipe-max-size`(1MB)를 초과 — 720p 이상 녹화는 `fs.pipe-max-size` 상향 필요.
- **치수 가드 (2026-07-04 심층 리뷰 반영)**: 레코더는 협상 해상도(§4.1)로 구성되고, 구성과 **치수가 다른 프레임은 드롭**(rate-limit ERROR) — 그대로 쓰면 `width×height×3/2` 바이트를 읽어 프레임 버퍼 경계를 초과한다. NV12 `stride > width`인 프레임(정렬 패딩)은 행 단위 재패킹(`compact_nv12`) 후 기록 — 패딩 바이트가 화소로 해석되어 생기는 전단(shear) 왜곡 제거. `stride == width`면 제로카피.
- **파일명**: `riposte_YYYYMMDD_HHMMSS_NNN.mp4`(벽시계 + 인스턴스 세그먼트 시퀀스 — 2026-08-16 개정: 인코더 死 재시작이 같은 초 안에 새 세그먼트를 열면 시각만으로는 이름이 충돌해 `-y`가 앞 파일의 증거를 조용히 덮어쓴다). `-movflags +faststart`로 전원손실 시에도 재생 가능.
- **디스크 자동 회수 (S-5)**: 세그먼트 회전마다 `statvfs`로 사용률 확인. **80% 도달 시 오래된 영상부터 삭제해 용량의 10% 확보**. 회수 판정(`plan_eviction`)은 파일시스템 무관 **순수 함수로 분리해 단위시험**(G11.2). 회수는 전용 녹화 볼륨을 가정(공유 볼륨이 타 데이터로 가득 차면 녹화만 삭제되므로).

### 8.2 EO 광각/협각 2채널 녹화 + 자막 임베딩 (선택)

**(2026-08-16 개정)** 2채널 녹화의 두 번째 채널은 **EO 협각**이다 — 이 시스템의 이중 카메라 구성은 DUALEO-REQ의 **EO 광각/EO 협각**이며(S-11 배분과 동일 채널 개념), 종전 §8.2가 상정했던 IR(열영상) 채널은 **미래 검토 항목으로 이관**한다(§10 DEFERRED — 파이프라인 구조는 채널-불문이라 IR 도입 시 두 번째 입력만 바꾸면 된다).

EO 광각·협각 2대 카메라를 **한 파일에 side-by-side**(광각=Ch1 좌 / 협각=Ch2 우, `hstack`)로 합성해 기록한다. `ffmpeg`는 광각을 stdin, 협각을 **상속 파이프 fd 3**(`-i pipe:3`)으로 입력받는다 — 세그먼트마다 stdin과 같은 방식의 **새 익명 파이프**를 만들어 자식에 물려준다. 두 프레임은 **쌍으로 쓰거나 함께 드롭**해 합성이 어긋나지 않는다.

**협각 입력의 named FIFO 폐지 (2026-08-18 코드리뷰 반영)**: 종전 설계는 협각을 named FIFO 하나로 전달하고 세그먼트마다 같은 경로를 `O_RDWR`로 재개방했다. 그 재개방이 **직전 세그먼트의 인코더가 아직 드레인 중인 파이프 객체에 writer를 되살려** 인코더가 EOF를 영영 못 보고(미종료·미회수), 신구 인코더 둘이 같은 FIFO를 읽으며 **새 세그먼트의 NV12 스트림을 바이트 단위로 나눠 갖는** 결함이 있었다. 세그먼트별 FIFO로 바꾸면 이번엔 인코더 기동보다 빠른 회전에서 open() 랑데부 레이스가 생긴다(실측으로 확인 — 그 형태는 세그먼트가 아예 생성되지 않았다). 상속 파이프에는 두 실패 모드가 모두 없다 — 파일시스템 랑데부 자체가 없고, EOF는 우리 write end가 닫히는 순간 성립한다(단일 채널 경로와 동형). 자식의 fd 3 배치는 표준 디스크립터가 닫힌 채 기동된 경우까지 방어한다(`F_DUPFD`로 4 이상으로 옮긴 뒤 `dup2`). 레거시 FIFO 잔재는 open()에서 청소한다. 종료 시 dual 경로도 단일 채널처럼 **기록/드롭 통계를 보고**하며 치수 불일치 드롭을 따로 집계한다(무음 공백 녹화의 시그니처). 회귀 시험: `test_recorder` dual 회전(150 ms 세그먼트 다회전, 세그먼트별 독립 재생 가능 + FIFO 잔재 0 + **40회전에 걸친 fd 누수 없음** — 세그먼트마다 파이프를 2개 여는 구조라 닫기 하나만 빠져도 장시간 녹화에서만 드러난다). 원 결함을 재주입하면 이 시험이 "encoder stuck — SIGKILL"과 함께 실패해 진단이 실측으로 확인된다.

**자막 번인(drawtext)**: `시간 / GPS(위경도·고도) / 광각(WIDE) 탐지 픽셀좌표 / 협각(NARROW) 탐지 픽셀좌표`를 화면 상단에 상시 표시(증거영상 표준 — 어떤 플레이어에서도 보이고 편집 불가). ~10Hz로 사이드카 텍스트 파일을 갱신하고 drawtext `reload=1`로 재로딩. 사이드카 갱신은 **임시 파일 작성 후 `rename()` 원자 교체**(2026-07-04 심층 리뷰 반영) — `reload=1`은 매 프레임 파일을 재읽기하므로 제자리 쓰기는 절단된/반쯤 쓰인 줄이 번인될 수 있다. GPS는 OBC가 발행하는 **GpsBus**(shm `/riposte_gps`, 기록 전용·비행 무관)를 시커가 best-effort로 소비(fix 없으면 `NO-FIX`). 협각 탐지 좌표는 협각 프레임에 동일 EO 디텍터(같은 HEF)를 돌려 최고점수 대상 중심으로 산출한다.

설정 키(`[seeker]`): (§8.1) + `record_dual`, `record_overlay`, `narrow_device`. 협각 채널이 열리지 않으면 자동으로 단일채널 녹화로 강등. *(주의: 이 협각 채널은 현재 녹화 전용이다 — S-11 협각 추론 파이프라인 연결은 P4에서.)*

**구현 (2026-08-16, P1-07 종결)**: §8.1/§8.2 설계대로 구현 완료. 설계 대비 확정·개정 사항:
- **회전 시 비동기 회수**: `+faststart`의 moov 재작성은 100ms대까지 걸릴 수 있어, 세그먼트 회전의 `finish_segment()`는 파이프만 닫고(WNOHANG 1회) 미종료 인코더 pid를 보류 목록에 넣어 이후 프레임에서 회수한다 — 인지 루프가 finalize에 블록되지 않는다. `close()`(종료)만 인코더당 최대 5s 대기 후 SIGKILL.
- **입력 페이싱**: 캡처는 60Hz(SEEKER_FRAME_HZ), 인코더 입력은 `record_fps`(기본 30) — write가 이른 프레임을 조용히 스킵한다(정상 데시메이션, drop 카운터와 구분 — drop은 문제 신호).
- **capability 탐지**: open()에서 `ffmpeg -version`(실행 가능성 — 실패 시 open 실패 = P1-07 기동 거부)과 `-filters`(hstack: dual의 본질이라 부재 시 open 실패 / drawtext: static 빌드에 흔히 없음 — 부재·폰트 부재 시 **자막만 강등**, 녹화는 계속)를 1회 검사한다. 스폰 후 死 루프로 발견하는 대신 기동 시 판정.
- **배포**: 720p NV12(1.38MB/프레임)는 기본 `fs.pipe-max-size`(1MB) 초과 — `deploy/sysctl.d/99-riposte.conf`(16MB)를 타깃에 설치. 미설치 시 ERROR 1회 후 녹화 자체 비활성(§8.1 클램프 정책).
- **스폰 연속 실패 상한**(5회) — 인코더가 계속 못 뜨면 녹화를 접고 인지를 보호.
- **dual 녹화 스레드 격리 (2026-08-16, 코드리뷰 P2-06)**: dual 경로의 협각 오버레이 검출은 동기 NPU 호출이라 EO 캡처 케이던스를 막을 수 있었다(종전엔 인지 스레드에서 `record_step`이 실행). 이제 `RecordWorker`(`seeker/src/RecordWorker.{h,cpp}`)가 **레코더·협각 카메라·협각 검출기·GPS 리더를 소유하는 전용 스레드**로 실행되고, 인지 스레드는 **depth-1 latest-wins 슬롯**에 EO 프레임을 `post()`할 뿐이다 — 워커가 밀리면 대기 프레임을 덮어써 **녹화 프레임을 드롭할 뿐 인지 루프는 결코 정체되지 않는다**. post는 EO 바이트를 복사한다(카메라 mmap 버퍼가 다음 grab에 재사용되므로) — ~1.3MB memcpy는 대체된 detect()보다 훨씬 저렴하다. 단일채널 녹화는 워커 없이 인라인 유지(비차단 파이프 write 1회). SIL dual 실증: 협각 검출이 워커에서 돌아 광각\|협각 합성 MP4가 정상 생성, ctest 23/23·TSan clean.

**단위·통합시험 (`test/test_recorder.cpp`, ctest `recorder`)**: 순수 — `plan_eviction` 경계(80% 미만 유지·오래된 것부터 10% 확보·빈 목록·용량0 가드), `compact_nv12` 무손실 재패킹, `format_overlay_text` 자리표시자(NO-FIX·EO/IR --)·픽셀좌표 환산. 통합(ffmpeg 부재 시 자동 SKIP; CI는 ffmpeg 설치로 실행 보장) — MP4 세그먼트 생성(ftyp 매직·payload 크기), 세그먼트 회전(≥2 파일), **인코더 死 생존**(-version만 성공하는 가짜 ffmpeg — 감지·드롭·재스폰·무크래시), dual 광각|협각 합성(overlay 요청이 녹화를 깨지 못함), 인코더 부재 시 open 거부(P1-07). 로컬 검증: dual 산출물 ffprobe로 h264 128×48(=2×W) 확인. ENOSPC·h264_rkmpp·drawtext 실번인은 실 타깃 브링업 항목.

---

## 9. 시험

- **단위시험 (`test/test_seeker.cpp`, ctest `seeker`)**: Tracker·TargetEstimator 순수 로직을 카메라/NPU 없이 검증(G11.2/G17.13). Tracker — 획득·클래스 필터·α-β 평활·게이팅(밖 검출 기각)·최고점수 연관·coast/드롭(MAX_MISSES). Estimator — 무효/저품질 기각, 중심대상 순수 전방거리, 우/하 편심 부호(FRD +Y/+Z), 크기-거리 반비례, 접근속도(-X) 부호, track_id/quality 전파. 검출은 **목 `IDetector`** 로 주입하고, `detect()==false`(가속기 장애) 시 트랙 coast→드롭→`valid=0` 전파(SM-7 상류 조건)를 detect→track→estimate 체인으로 확인.
- **SIL**: `SyntheticCamera`+`SyntheticDetector`로 표류·접근 대상 생성 → Tracker/Estimator/TrackBus 수치 검증(실행 확인 완료).
- **PX4 SITL 통합 (2026-07-03)**: 시커 전체 파이프라인이 OBC GuidanceSource와 실연동되어 PX4(SIH) 기체를 PN으로 추종함을 확인 — 인지→유도 체인 end-to-end 실동작 입증. 절차는 OBC-SDD-002 §6 / `riposte-sw/test/sitl/run_guidance_tracking.sh`.
- **단위시험 (§4.4 구현 완료)**: `test_hailoparse.cpp`(ctest `hailoparse`, 34 checks) —
  NMS-by-class 파서: 정상 다클래스 파싱, 잘린 헤더/레코드 중단, NaN/음수/거대
  det_count 거부, 불량 bbox(NaN·범위 밖·반전) 개별 드롭, max_dets 상한, 0-count
  클래스 통과, 파스→`letterbox_undo` 왕복. `test_preproc.cpp`(ctest `preproc`) —
  CPU letterbox: 패딩 값·위치(pad_x/pad_y), content 배치 정확도, 단색/그라디언트
  변환 정확도(BT.601), 정사각 보존(늘림 회귀 방지), 축퇴 입력 거부.
- **벤치(실HW)**: Hailo 추론 지연/발열, V4L2 캡처 지터 실측, RGA vs CPU 참조 픽셀 비교(§4.4.5).

---

## 10. ASSUMPTION / DEFERRED

| 태그 | 항목 |
|---|---|
| ASSUMPTION | 탐지 모델 ONNX→HEF 변환본 제공(학습 별도 트랙) |
| ASSUMPTION | 카메라 V4L2 노출, 30fps 이상 |
| DEFERRED | HailoDetector **구현 완료(§4.4)** — 실 HW 브링업만 잔존(체크리스트 §4.4.5: 입출력 포맷·threshold API·RGA 픽셀 비교·지연 실측 + 60fps 센서 모드 확인) ⚠️ |
| ASSUMPTION | 제품 표준 HEF는 NMS 내장 컴파일(S-7) — 입력 RGB888/NHWC/uint8, 브링업 체크리스트 §4.4.5로 확인 |
| ASSUMPTION | (폴백 한정) raw 헤드 `[4+nc, anchors]` 단일 텐서 — 실 HEF에서 확인 필요(§4.3) |
| DEFERRED | **IR(열영상) 채널** — 미래 검토(2026-08-16 §8.2에서 이관). 녹화 파이프라인은 채널-불문이라 도입 시 두 번째 입력 교체 + IR 전용 검출 모델만 필요 |
| DEFERRED | 출력 버퍼 1회 할당 재사용 최적화(S-10은 job별 할당+가드로 시작) |
| DEFERRED | 이중 EO(광각/협각) 2채널 파이프라인 + S-11 적응형 추론 레이트 배분(R-10) — RIPOSTE-DUALEO-REQ-001 §4.1 |
| DEFERRED | 이동 대상 상태 추정(EKF/IMM·자기운동 보상·단안 거리 관측성) — RIPOSTE-ESTIMATION-REQ-001 |
| DEFERRED | 단안 거리추정 정확도 검증 — 레인지 센서 미채택, 크기+자기기동 시차로만(ESTIMATION-REQ §5.2) |
| DEFERRED | RK3588 NPU 추적 보조 AI(ReID 연관·템플릿 추적) — 전략은 RIPOSTE-TRACKER-REQ-001 |
