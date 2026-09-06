# RIPOSTE-ESTIMATION-REQ-001
## Riposte — 이동 대상 상태 추정 요구·연구 분석서

| 항목 | 내용 |
|---|---|
| 문서 ID | RIPOSTE-ESTIMATION-REQ-001 |
| 버전 | 1.0 (2026-08-16 코드리뷰 P2-07 / 운용개념 ④: GuidanceSource를 TargetImm으로 연결(기동대상 추종)·position_sigma 혼합 공분산 산포항 수정 §5.1 / 0.9: Draft, 2026-08-15 EST-P3 IMM 구현 — TargetImm(저Q/고Q 2모델 CA, likelihood 믹싱), 직진=저Q 우세·기동=고Q 전환 입증 / 0.8: EST-P7a 구현 — SizeRangeEkf(10상태, 수치 자코비안), 관측성 입증(정지=모호성 유지, 위빙=분리 수렴) / 0.7: §5.5 augmented-state EKF(크기-거리 공동추정) 설계·EST-P7 단계 신설 / 0.6: EST-6 부분 — EKF 공분산→발행 quality 강등(획득 불확실·발산), systematic 원거리 저신뢰는 augmented-state 후속 명시 / 0.5: EST-P3 일부 — TargetEkf CV→CA(등가속 9상태) 승격, 기동대상 추종(가속·선회) 시험 / 0.4: EST-B(실행위치=OBC, I2)·EST-P4 배선 — TargetEkf를 obc로 이동·GuidanceSource 통합(상대속도 정제, PN 불변) / 0.3: EST-P1 구현 완료 — TargetEkf(CV 절대NED·이방성 거리공분산·자기운동 분리), 시험 ekf(29) / 0.2: §5.3 LRF 검토 결과·미채택 확정(물리+경제성, 원가 200만원·≤200m) / 0.1: 연구 착수) |
| 대상 | 이동 대상의 거리·속도·경로 추정 (자기 기체 운동 보상 포함) |
| 상위 문서 | RIPOSTE-SAD-001 (아키텍처 설계서) |
| 관련 문서 | RIPOSTE-SEEKER-SDD-001 (§5 Tracker/TargetEstimator), RIPOSTE-OBC-SDD-001 (GuidanceSource), RIPOSTE-TRACKER-REQ-001 (T0/T1/T2) |
| 성격 | 요구 분석 + 갭 분석 + 알고리즘 연구 (구현 전 단계) |

---

## 1. 문제 정의

대상은 **이동체(moving target)**이며 회피 기동을 할 수 있다. 유도가 성립하려면
대상의 **상대 거리·속도·이동 경로**를 신뢰성 있게 추정해야 한다. 현행 스택은
이동 대상, 특히 기동 대상에 대해 추정이 약하다(§2). 본 문서는 갭을 분석하고,
EKF 기반 상태 추정 아키텍처와 자기 기체 운동(IMU 융합 결과)·유도(PID/MPC)
계층 관계를 연구로 정리한다.

**요구 요약 (EST-x)**:

| ID | 요구 |
|---|---|
| EST-1 | 이동 대상의 상대 위치·속도를 잡음에 강인하게 추정(1-틱 차분 대체) |
| EST-2 | 거리 추정의 불확실성을 정량화 — 단안 크기 기반의 한계를 숨기지 않음 |
| EST-3 | 자기 기체 운동(속도·자세, PX4 EKF2 출력)을 추정에 반영해 대상 고유 운동을 분리 |
| EST-4 | 기동 대상(선회·가속·회피)에 대한 모델 — 등속 단일 모델의 지연 극복 |
| EST-5 | 대상 예측 경로 산출(유도 리드/접근점의 입력) |
| EST-6 | 추정 실패·저품질을 안전 계층에 전파(관측성 상실 = 저신뢰, SM-7 연동) |

---

## 2. 현행 스택 갭 분석 (코드 근거)

| 계층 | 현재 구현 | 이동 대상에서의 한계 |
|---|---|---|
| 픽셀 추적 | `Tracker` α-β (등속 가정), cx/cy/size 평활 | 2차(가속도) 항 없음 → 기동 시 위상 지연. size 잡음이 거리로 직결 |
| 거리 | `TargetEstimator.cpp:31` `range = size_m·focal/size` | **대상 실크기 가정에 거리가 선형 비례**. 크기 미지·자세 변화 시 큰 오차. 불확실성 미정량 |
| 속도 | `TargetEstimator.cpp:45` 상대위치 **1-틱 유한차분** | 미분 잡음 증폭, 필터 없음. 자기 운동과 대상 운동이 상대 FRD에 혼재 |
| 유도 | `GuidanceSource` 이산 LOS-delta lead(PN 유사) | 대상 예측 경로 모델 없음, 등속 암묵 가정 |

**한 문장 진단**: 거리는 크기 가정에 의존하고, 속도는 필터 없는 1-틱 차분이며,
둘 다 등속 모델에 얹혀 있어 — 기동 이동 대상의 3D 상태를 신뢰성 있게 추정하지
못한다.

---

## 3. EKF / PID / MPC 계층 정리 (경쟁 아닌 스택)

세 기법은 서로 다른 계층이며 택1이 아니다. **연구 우선순위는 추정(EKF)** 이다.

| 계층 | 기법 | 역할 | Riposte 위치 |
|---|---|---|---|
| **상태 추정** | **EKF / IMM** | 비선형 측정(방위·고도각·크기)에서 대상 3D 상태 추정 | **본 문서 핵심** — α-β·1-틱 차분 대체 |
| 유도 | PN(현행) / **MPC**(선택) | 대상 예측 경로 + 기체 제약에서 명령/궤적 산출 | GuidanceSource 고도화. **EKF가 선행** |
| 저수준 제어 | **PID** | 명령 → 액추에이터 | PX4가 담당. 우리 쪽은 R-9 화면 중심 유지(픽셀오차→yaw/pitch)에 국한 |

**순서**: 좋은 추정(EKF)이 없으면 MPC도 소용없다. PID는 R-9에 국한된 별개
계층이다. 따라서 본 연구는 EKF/IMM에 집중하고, MPC·PID는 계층 관계만 명시한다.

---

## 4. 자기 기체 운동의 활용 (EST-3) — 이중 EKF 구조

**핵심 사실**: IMU raw를 직접 융합할 필요가 없다. **PX4의 EKF2가 이미 IMU·GPS·
자력계를 융합**해 자기 기체 상태를 제공하며, 이것이 `TelemetrySnapshot`으로 온다:

- `vel_ned_mps[3]` — 자기 기체 속도 (NED)
- `roll/pitch/yaw_rad` — 자기 기체 자세 (FRD↔NED DCM)
- `pos_ned_m[3]` — 자기 기체 위치 (EKF 원점 기준)

따라서 구조는 **이중 EKF**다:

```
[PX4 EKF2]  IMU + GPS + Mag  ─융합─▶  자기 기체 상태(pos/vel/att)  ─텔레메트리─┐
                                                                              ▼
[Target EKF]  카메라 방위/고도각 + 겉보기 크기  +  자기 기체 상태  ─▶  대상 상태
              (우리가 개발할 계층)
```

- **각속도(gyro)가 추가로 필요하면** MAVSDK `angular_velocity_body` 구독을
  `FcuLink`에 추가한다(현재는 attitude euler만 구독). 다만 대상 EKF의 상태
  전파에는 통상 자기 vel/att로 충분하다.
- **좌표계 결정 (설계 결정 EST-A)**: 대상 상태를 **절대 NED로 추정**하는 것을
  권한다. 상대 FRD로 추정하면 자기 기체가 기동할 때마다 상대 상태가 급변해
  필터가 자기 운동을 대상 운동으로 오인한다. 절대 NED 상태 = (자기 위치) +
  (거리 × 카메라 LOS를 자세 DCM으로 회전). 자기 운동은 관측 모델에 자연히
  들어가고, 대상의 **고유** 속도·경로가 상태로 분리된다.
- **실행 위치 결정 (설계 결정 EST-B)**: Target EKF는 **OBC에서 실행**한다
  (`obc/src/TargetEkf`). 자기 상태(pos/vel/att)가 필수 입력인데 그것은
  텔레메트리로 OBC만 가지며, **seeker는 FC에 접근하지 않는다(안전 불변식 I2)**.
  seeker는 단안 상대 FRD만 발행하고, OBC의 GuidanceSource가 자기 상태와
  융합해 EKF를 돌린다. (EST-P1에서 파일을 seeker/src에 둔 것은 이 관점에서의
  오배치였고 EST-P4에서 obc/src로 정정했다.)

---

## 5. 상태 추정 아키텍처 연구

### 5.1 상태 벡터와 모델 (EST-1/EST-4)

절대 NED 대상 상태(EST-A) 기준 후보:

- **CV (등속)**: `[p_n, p_e, p_d, v_n, v_e, v_d]` — 최소. 직진 대상에 충분.
- **CA (등가속)**: CV + `[a_n, a_e, a_d]` — 가속/감속 대응.
- **IMM (Interacting Multiple Model)**: CV + CA + CT(선회, coordinated turn)를
  확률 혼합. **기동 대상(EST-4)의 표준 해법**. 각 모델을 병렬로 돌리고 모델
  확률로 가중 — 직진 중엔 CV가, 선회 중엔 CT가 지배해 지연 없이 추종.

권고: **CV/CA 단일 EKF로 시작 → 기동 시나리오 실측 후 IMM 승격**(단계적, §7).

**구현(2026-08-15)**: TargetEkf를 CV(6상태)에서 **CA(등가속, 9상태 `[p,v,a]`)**
로 승격했다. 예측은 CA 전파(p+=v·dt+½a·dt², v+=a·dt) + 백색 저크(jerk) 프로세스
노이즈, 갱신은 위치 관측(H=[I 0 0])에 §5.2 이방성 R. 가속 상태를 실으므로 기동
대상(선회·가속)에서 CV 가정의 지연이 사라진다(EST-4). `relative_state` 인터페이스
불변이라 GuidanceSource 통합(EST-P4)은 그대로 — 기존 통합 시험 전부 통과. 시험
`test_ekf` +2(가속 대상: 가속도 복원+위치 추종, 선회 대상: 회전 중 위치 오차
유계). ctest 19/19 · 새니타이저 clean. IMM(다중 모델)도 구현했다(아래).

**IMM 구현(2026-08-15)**: `obc/src/TargetImm.{h,cpp}` — CA 커널 2모델(저 jerk=부드러움,
고 jerk=기민)을 표준 IMM 4단계(interaction/mixing → 모델별 필터 → likelihood로
모델확률 갱신 → 결합)로 혼합한다. TargetEkf에 IMM 인프라 추가(`get/set_state`,
관측 gaussian `last_likelihood`). 단일 고정 Q의 절충(작으면 기동 지연, 크면
직진에서 지터)을 피해 **직진 구간은 저Q, 기동 구간은 고Q가 지배**하도록 확률이
이동한다. 시험 `test/test_imm.cpp`(109 checks): 직진 시 저Q 우세, 강한 S턴 시
고Q로 확률 전환, 확률 정규화, reset. `relative_state` 인터페이스가 TargetEkf와
동일해 GuidanceSource 통합(EST-P4)을 후속에서 그대로 교체 가능. 모델 jerk·전이
확률의 최종값은 P2 Gazebo 실측 튜닝 항목. ctest 22/22 · 새니타이저 clean.

**IMM production 연결 + 혼합 공분산 수정(2026-08-16, 코드리뷰 P2-07 / 운용개념 항목 ④)**: `GuidanceSource`가 단일 `TargetEkf` 대신 `TargetImm`을 사용하도록 교체했다 — 인터페이스(predict/update/relative_state/position_sigma/initialized/reset)가 동일해 유도 seam은 불변이고, 기동대상(가속·선회)에서 저Q/고Q 확률 이동으로 추종 지연이 줄어든다(운용개념 ④: 속도·방향 기반 이동 예측). **동시에 `position_sigma()`의 결함을 수정**: 종전에는 각 모델 sigma를 확률가중 **선형 평균**(σ = Σμ_jσ_j)했는데, 이는 ① 분산이 아닌 sigma를 평균해 틀렸고 ② **IMM 결합 공분산의 모델 평균 산포항을 누락**했다 — 두 모델이 위치를 달리 주장하는 **기동 전환 구간**에서 결합 불확실성이 각 모델보다 커지는데 이를 과소평가했다(EST-6 quality 강등이 가장 필요한 순간). 올바른 혼합 1-sigma는
```
σ² = Σ_j μ_j ( σ_j² + ‖p_j − p_c‖² / 3 )      (p_c = 결합 위치, p_j = 모델 j 위치)
```
로, 각 모델 분산의 확률가중 평균에 모델 평균과 결합 평균의 산포(3축 평균)를 더한다. 시험 `test/test_imm.cpp`: 두 모델이 합의할 때 σ는 개별 모델 근처, S턴 전환으로 두 모델이 벌어지면 σ가 개별 최대보다 커짐(산포항 발현)을 확인.

### 5.2 관측 모델과 관측성 (EST-2) — 가장 어려운 부분

카메라 관측 = 방위각(az) + 고도각(el) + 겉보기 크기(size). 이를 상태로
사상하는 관측 함수 h(x)는 비선형(그래서 EKF/UKF).

- **bearing-only 관측성 문제**: 방위/고도각만으로는 **거리가 관측 불가**하다
  (한 점에서의 각도는 거리 정보를 주지 않음). 별도 거리 센서 없이 단안으로
  거리를 얻는 경로 2가지:
  1. **겉보기 크기 관측** — `range ∝ size_m/size`. 단 `size_m`(대상 실크기)이
     불확실하면 거리 불확실도 그만큼. **크기를 상태에 넣거나 사전분포로
     불확실성을 전파**해야 하며, EKF 공분산이 이 불확실성을 정직하게 실어야
     한다(EST-2). 이것이 필터 종류보다 성능을 지배한다.
  2. **자기 기동 유발 시차(parallax)** — 자기 기체가 횡방향으로 움직이면 각도
     변화율에서 거리가 관측 가능해진다. 자기 vel(EST-3)이 여기서 결정적.
     탐색/접근 중 의도적 위빙이 관측성을 높인다. **별도 센서 없이 관측성을
     끌어올리는 주 수단**이므로, EST-P2에서 자기 위빙 패턴과 추정 정확도의
     관계를 정량화한다.
- **정직한 한계(R7.6)**: 단안+크기만으로는 거리 정확도에 물리적 상한이 있다.
  EKF는 잡음을 줄이고 불확실성을 정량화하며 자기 기동 시차로 관측성을 보강할
  수 있으나, 크기 가정 오차는 EKF로도 못 없앤다 — 문서·발행 quality가 이를
  반영해야 한다.
- **[구현 한계, 2026-08-15] 크기 오차는 systematic**: 현 EKF는 관측을 절대 위치
  3D(크기 기반 range 포함)로 받고 이방성 R을 random noise로 모델한다. 그러나
  크기 가정(target_size_m) 오차는 프레임마다 같은 방향의 **systematic bias**라
  random R로 두면 반복 관측이 이를 낙관적으로 평균해 없앤다(실제로는 안 없어짐).
  원거리 저신뢰를 정확히 내려면 **크기(또는 log-range) 상태를 augment**해 bias를
  관측 불가로 두고 parallax로만 좁혀야 한다(EST 후속). 따라서 EST-6는 획득 직후
  불확실성·발산 감지까지만 커버하고, systematic 원거리 저신뢰는 미구현.

### 5.3 레이저 거리계(LRF) 검토 결과 — 미채택 확정 (2026-08-15)

거리 관측성(§5.2)을 LRF로 직접 해결할 수 있는지 시장 조사와 물리·경제성
분석으로 검토했다. **결론: 미채택.** 근거는 세 축이다.

- **물리 (빔 발산 대 소형 대상)**: 저가 LRF는 빔이 넓다. 검토한 PTGC-12X
  (Chengdu JRT, 실 데이터시트)는 발산 ≤10 mrad → 500m 스팟 5m·200m 스팟 2m로,
  0.35m 소형 대상은 스팟 면적의 0.5~3%뿐. 반사 대부분이 배경에서 오고(하늘
  배경이면 무반사, 지상이면 배경 거리 반환) 소형 공중 대상에 **거리 모호**.
  스펙 자체가 "밝은 색 벽 등 협조적 대형 대상·수직면·직사광 차폐" 전제라
  용도가 다르다. 측정 주기도 1~3 Hz로 느리다.
- **물리 (좁은 빔의 조준 부담)**: 소형 대상에 유효한 것은 좁은 빔(≤0.7 mrad)
  펄스형 1535nm 모듈(예: Lumispot 드론탐지 시리즈 0.6 mrad·33g, Jenoptik
  DLEM 20 <30g·25Hz, Newcon NX-5 0.5 mrad). 이들은 소형 드론 km급 측거가
  가능해 사거리·반사는 문제가 아니나, 빔이 좁아(200m 스팟 0.12m) **정밀
  지향이 필요 → 짐벌이 사실상 전제**다. 짐벌은 별도 원가·무게를 더한다.
- **경제성 (원가 200만원 목표)**: 드론 제작 원가 ≤200만원(≈$1,450)에서 LRF
  배정 가능액은 소액(~20~40만원)이다. 예산 내 저가 제품(SF30/C ~$300,
  ERDI/905nm류)은 빔 과대·명목사거리가 협조대상 기준이라 **소형 드론에
  부적합**하고, 소형 드론에 유효한 좁은 빔 1535nm 펄스형은 **LRF 단품이 드론
  원가에 육박·초과**한다(+짐벌 원가). 즉 이 예산에서 유효 제품이 없다.

**결정적 관찰**: LRF의 값어치는 단안 오차가 큰 **원거리**에 있는데, 본 시스템의
탐지 대역(≤200m 요구 시)은 카메라가 이미 강한 구간이다 — 협각 타일에서 200m
소형 대상이 ~12 px(DUALEO-REQ §3.2, 300m 8.3 px에서 환산)로 여유 있게 탐지되고,
단안 크기 오차의 절대값도 원거리보다 작다. LRF가 보탤 이득이 가장 작은 구간에서
가장 큰 비용·복잡도(짐벌·조달·수출통제)를 요구하므로 채택하지 않는다.

**상위 모델 옵션으로만 보존**: 원가 상한이 크게 오르고 짐벌이 이미 있는 상위
파생형에서는 좁은 빔 1535nm(Lumispot 등)이 원거리 거리 확정에 유효할 수 있다.
그때 재검토하되, 기본형(≤200만원)은 **카메라 단안 + 자기 기동 시차로 확정**한다.

### 5.4 실시간·구현

- RK3588 CPU에서 6~9상태 EKF, IMM(3모델)도 20 Hz는 경량이다. 병목은 연산이
  아니라 정확도·관측성이다.
- **순수 로직 분리**: 필터 수학(예측/갱신/자코비안)은 vendor·shm 무관한
  `TargetEkf` 클래스로 두어 호스트 단위시험한다(현행 ModelIo/AssocCost/
  TrackFusion과 같은 방식). 자기 상태·카메라 관측은 값으로 주입.
- 기존 `TargetEstimator`(단안 기하)는 **관측 함수 h(x)의 역(초기화·측정 변환)**
  으로 재사용하고, α-β는 EKF로 대체하되 인터페이스(TrackState 발행)는 유지.

### 5.5 Augmented-state EKF 설계 (크기-거리 공동 추정) — EST-P7 설계

**동기**: 현행 EKF는 관측을 절대 위치 3D(크기 기반 range 포함)로 받고 크기
불확실성을 random R로 둔다. 그러나 크기 가정 오차는 프레임마다 같은 방향의
**systematic bias**라 반복 관측이 낙관적으로 평균해 없앤다(§5.2 한계, EST-6이
그래서 부분적). 이를 근본 해결하려면 **대상 크기(또는 그 로그)를 상태에
augment**해 bias를 관측 불가로 두고, 자기 기동 시차(parallax)로만 크기와 거리를
분리한다.

**상태 벡터 (10차원)**:
```
x = [ p_ned(3), v_ned(3), a_ned(3), s ]     s = log(target_size_m)
```
운동학은 현행 CA(9)를 그대로 두고 크기 로그 1개를 추가한다. `s`의 프로세스
노이즈는 매우 작다(대상 실크기는 시간 불변) — 오직 관측으로만 갱신된다.

**관측 모델 (raw 단안, 위치 3D 아님)**:
```
z = [ u_frd(2 자유도, 단위 LOS 방향), log(a_px) ]     a_px = 겉보기 각크기
h(x):  rel_ned = p_ned − p_own ; rel_frd = DCMᵀ(att) · rel_ned
       u = rel_frd / |rel_frd|              (bearing, R 작음 — 방위 정밀)
       range = |rel_frd|
       log(a_px) = s − log(range) + const   (크기 관측: s와 range를 함께 봄)
```
- bearing 항은 R이 작다(정규화 bbox 중심 정밀). 크기 항의 R은 bbox 크기 측정
  잡음(random)이며, **크기 가정 오차는 이제 상태 `s`로 흡수**되어 systematic
  bias가 필터를 낙관하게 만들지 않는다.
- 자코비안 H(관측 × 10)는 비선형(atan2·정규화·로그) — EKF 표준. 크기 항의
  ∂/∂s = 1, ∂/∂range = −1/range가 관측성의 핵심 결합이다.

**관측성 (EST-2 정량화의 귀결)**:
- **자기 정지 + 대상 LOS 고정**: range 변화가 없어 `log(a_px)=s−log(range)`가
  s와 range를 분리 못 함 → 크기-거리 모호성 유지 → 공분산에 그대로 남아 **저신뢰**
  발행(EST-6이 자동으로 올바르게 동작). 낙관적 수렴이 사라진다.
- **자기 횡기동(위빙)**: bearing rate + range 변화가 s와 range를 분리 → 크기까지
  self-calibrating 추정 → 거리 신뢰 상승. 유도 궤적이 추정을 돕는 active
  sensing(§6 dual control)이 여기서 정량적 근거를 얻는다.

**파이프라인 영향 (배선 단계 EST-P7b)**:
- 현행 `TrackState.rel_pos_frd`는 이미 크기 가정을 적용한 값이라 raw 관측이
  아니다. Augmented EKF는 **raw bearing + 겉보기 크기**가 필요하므로 TrackState에
  이를 실어야 한다(예: `los_unit_frd[3]` + `ang_size_rad`, 기존 pad/확장 —
  shm ABI 조정, R5.2 append 규칙). seeker는 이미 이 값을 가진다(Tracker의
  cx/cy/size) — 발행만 추가.
- OBC의 EKF `update()`가 위치 3D 대신 bearing+size를 받도록 재작성. `relative_state`
  발행 인터페이스와 GuidanceSource 통합(EST-P4)·quality 강등(EST-6)은 유지.

**단계**:
- **EST-P7a (순수 로직, HW 불필요)**: 10상태 augmented EKF를 별도 필터로 구현,
  합성 궤적 호스트 시험 — 특히 (a) 자기 정지 시 크기-거리 모호성이 공분산에
  남고(저신뢰), (b) 자기 위빙 시 크기·거리가 분리 수렴함을 입증.
- **EST-P7b (파이프라인 배선)**: TrackState raw 관측 확장 + seeker 발행 +
  OBC EKF 교체. SITL 검증.

**리스크**: 비선형 관측 초기화·수렴(초기 크기 사전분포 폭이 지배), bearing
자코비안의 특이점(range→0), shm ABI 변경의 리더/라이터 동시 배포(R5.2).

**EST-P7a 구현(2026-08-15)**: `obc/src/SizeRangeEkf.{h,cpp}` — 10상태
`[p,v,a,log_size]`. 관측 h(x)=[az, el, log(apparent)]는 정확히 구현하고 자코비안은
**수치 미분**(finite difference)으로 두어 손유도 오류를 배제했다. 예측은 CA(9) +
log_size(거의 불변, 미소 프로세스 노이즈). 초기화는 크기 사전분포로 range를
seed. 시험 `test/test_sizeekf.cpp`(11 checks)이 §5.5의 관측성 주장을 직접
입증한다: **자기 정지 + 잘못된 크기 사전분포**면 range 공분산이 붕괴하지 않고
모호성이 남고(정직한 저신뢰 — §5.2 systematic 한계 해소), **자기 위빙**이면
parallax로 크기·거리가 분리 수렴해 잘못된 seed에서 truth 쪽으로 이동한다. ctest
21/21 · 새니타이저 clean. 잔존 EST-P7b: TrackState raw 관측 확장·seeker 발행·OBC
EKF 교체(파이프라인 배선, SITL).

---

## 6. 유도 계층과의 연결 (EST-5)

- EKF가 대상 절대 상태(위치·속도[·가속도])를 주면, 예측 경로
  `p_tgt(t+τ) = p + vτ + ½aτ²`를 산출한다 — 이것이 리드/접근점의 입력이다.
  기존 `obc/src/Rendezvous.{h,cpp}`(닫힌 형태 접근점, P5a)가 이 예측을 소비한다.
- 현행 GuidanceSource의 이산 LOS-delta lead는 EKF 예측으로 **대체 또는 보강**
  가능하나, 비행 검증된 PN 게인(flight-tuned)을 함부로 바꾸지 않는다 — EKF
  예측을 리드 입력으로 먹이되 PN 구조는 유지하는 점진 경로를 권한다.
- **MPC**는 그다음 단계 옵션: 기체 동역학·속도 제약(SM-4) 하에서 예측 경로를
  추종하는 최적 궤적. EKF 예측 품질이 확보된 뒤 평가한다.

---

## 7. 단계 로드맵

| 단계 | 내용 | 선행 | HW |
|---|---|---|---|
| EST-P1 | **완료(2026-08-15)** — `TargetEkf`(CV 절대NED, 이방성 거리공분산, 자기운동 분리, 공분산 quality), 호스트 시험 `test_ekf`(29 checks). CA/IMM 승격은 EST-P3 | — | 불필요 |
| EST-P2 | Gazebo 기동 대상 시나리오(등속·선회·회피·자기 위빙) + 추정 오차 실측, 관측성 정량화 | EST-P1, gz | 불필요(SITL) |
| EST-P3 | **CA 승격 + IMM 완료(2026-08-15)** — TargetEkf CA(9상태) + `TargetImm`(저Q/고Q 2모델 likelihood 믹싱). 게인·전이확률 최종 튜닝은 P2 실측 | EST-P2 | 불필요 |
| EST-P4 | **IMM 연결 완료(2026-08-16, P2-07)** — GuidanceSource가 TargetImm 사용(기동대상 추종), 혼합 공분산 산포항 포함 position_sigma 수정. 이전 배선(2026-08-15): TargetEkf 통합·상대속도 정제. 잔존: 리드/Closure 연동·SITL 추적 검증 | EST-P2 | 불필요(SITL) |
| EST-P7a | **완료(2026-08-15)** — `obc/src/SizeRangeEkf`(10상태, bearing+크기 관측, 수치 자코비안). 시험 `test_sizeekf`(11): 정지 시 크기-거리 모호성 유지(저신뢰), 위빙 시 분리 수렴 | — | 불필요 |
| EST-P7b | TrackState raw 관측 확장 + seeker 발행 + OBC EKF 교체 · SITL 검증 | EST-P7a | 불필요(SITL) |
| EST-P5 | (선택) MPC 유도 평가 | EST-P4 | — |

**EST-P1~P4는 HW 없이 순수 로직 + SITL로 진행 가능** — 현행 개발 방식과 일치.
거리는 별도 센서 없이 단안 크기 + 자기 기동 시차로만 추정한다(§5.2).

**EST-P1 진행 결과(2026-08-15)**: 신규 `seeker/src/TargetEkf.{h,cpp}` — 상태
`[p_ned(3), v_ned(3)]` 절대 NED CV EKF. 관측은 단안 상대 FRD 위치(현행
TargetEstimator 기하) + 자기 pos/att로 **절대 NED 위치 관측**으로 변환(자기 운동은
관측 모델에, EST-3). **이방성 측정 공분산 R**(§5.2 EST-2): LOS 횡방향은
`sigma_bearing_rel·range`, 방사(거리) 방향은 `sigma_range_rel·range`로 크게 —
`R = σ_perp²I + (σ_range²−σ_perp²)uuᵀ`. 예측은 블록형 CV 전파 + 가속도 백색잡음 Q.
6×3 칼만 이득은 3×3 innovation 역행렬만으로 계산(외부 선형대수 의존 0).
`relative_state()`가 발행용 상대 FRD로 되돌릴 때 **자기 vel을 빼서**(rel_vel =
target_vel − own_vel) 접근율을 낸다. `position_sigma()`(공분산 trace)가 발행
quality로 연결될 예정(EST-P4). 호스트 시험 `test/test_ekf.cpp`(29 checks):
초기화, 정지대상 수렴·공분산 감소, 등속대상 속도 수렴, **자기 기동 시 대상 절대
위치 불변**(자기운동 분리), 이방성(횡 tight·방사 wide), **자기 위빙 시차가 방사
불확실성을 넓히지 않음**, 자세 왕복, reset. ctest 18/18 · 새니타이저 clean.
잔존: CA/IMM(EST-P3), Gazebo 기동 시나리오 실측(EST-P2), 리드/Closure 연동(EST-P4 후속).

**EST-B + EST-P4 배선(2026-08-15)**: TargetEkf를 `seeker/src`→`obc/src`로 이동
(EST-B: 자기 상태 접근 때문에 OBC 실행, I2 유지). GuidanceSource에 EKF 멤버를
통합 — valid 트랙마다 텔레메트리의 자기 pos/vel/att로 `predict`+`update`하고,
캐시 트랙의 **상대 속도(`rel_vel_frd`)를 EKF 추정으로 덮어쓴다**(자기 운동 제거).
**위치·PN 기하는 seeker 발행값 그대로** — flight-tuned PN 거동 불변(기존
test_guidance PN 시험 전부 통과 유지). 개선된 상대 속도는 리드/접근점 소비자가
받는다. on_engage에서 EKF reset. 시험 `test_guidance` +1(접근 대상의 상대 전방
속도가 실제 접근율로 수렴). ctest 18/18 · 새니타이저 clean. 잔존: 리드 항에
EKF 속도 실제 연동(Closure/MissionSource) + SITL 추적 검증.

**EST-6 부분(2026-08-15)**: EKF 위치 공분산(`position_sigma`)이 크면 발행 quality를
게이트(`MIN_TRACK_QUALITY`)로 **강등**한다 — 게이트 아래로는 안 내려 유도는
지속하되 저신뢰가 OBC/블랙박스에 전파(GuidanceSource, 캐시 quality만; 게이트
자체는 seeker 발행값을 봐 disengage 유발 안 함). Tunables
`EKF_SIGMA_QUALITY_FULL_M`(20)·`_ZERO_M`(120). 시험 `test_guidance` +1(획득 직후
강등→수렴 회복, 게이트 하회 없음). **범위**: 획득 불확실성·발산 감지. systematic
원거리 저신뢰는 augmented state 후속(§5.2). ctest 19/19 · 새니타이저 clean.

---

## 8. ASSUMPTION / DEFERRED

| 태그 | 항목 |
|---|---|
| ASSUMPTION | 자기 기체 상태는 PX4 EKF2 융합 출력(vel_ned/attitude)이 텔레메트리로 충분히 신선하게 온다(TELEM 40 Hz, SM-1) |
| ASSUMPTION | 대상 실크기 사전분포를 세울 수 있다(드론/풍선 클래스별). 정확도는 이 분포 폭이 지배 |
| 결정 | **레인지 센서(LRF) 미채택 확정 (§5.3)** — 저가 넓은빔 LRF는 소형 대상 부적합, 유효한 좁은빔 1535nm+짐벌은 원가 200만원 초과. ≤200m 대역은 카메라가 이미 강해 이득 최소. 단안 크기+자기기동 시차로 확정. 상위 모델(짐벌 보유) 옵션으로만 보존 |
| DEFERRED | 각속도(gyro) 구독 추가 여부 — 대상 EKF에 필요성 확인되면 FcuLink에 `angular_velocity_body` |
| DEFERRED | IMM 채택 — EST-P2 기동 시나리오 실측 후 |
| DEFERRED | MPC 유도 — EST-P4 이후 |
| DEFERRED | 다중 대상 각각의 EKF(현안은 primary 단일) |
