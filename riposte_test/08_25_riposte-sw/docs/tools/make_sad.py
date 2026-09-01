#!/usr/bin/env python3
"""RIPOSTE-SAD-001 소프트웨어 아키텍처 설계서 — Markdown + Word 생성기.

사용법:
    python3 docs/tools/make_sad.py     # docs/ 에 .md 와 .docx 를 만든다

내용은 여기에만 있고 출력이 둘이다(docgen). .md 는 형상관리 대상이고 .docx 는
산출물이라 커밋하지 않는다(.gitignore) — Word 파일은 zip 바이너리라 diff 도 리뷰도
되지 않기 때문이다. 문서를 고칠 때는 이 파일을 고친 뒤 재생성한다.

2026-08-17 에 구 RIPOSTE-SW-ARCH-001 을 흡수해 아키텍처 문서를 하나로 합쳤다. 출처는
리포의 승인 문서다 — OBC-SDD(상태기계·SM-1~10), SEEKER-SDD(인지 파이프라인·S-x), COMMON-SDD(IPC·C-5),
SUPERVISOR-SDD, AGENTS(I1~I6), BRINGUP-001(실기 검증). 여기서 새 설계를 만들지
않는다: 원 문서가 바뀌면 이 요약도 따라 고쳐야 하고, 충돌 시 원 문서가 이긴다.
기술 상태는 2026-08-17 구현 기준.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docgen  # noqa: E402

BLOCKS = [
    ("h", 0, "RIPOSTE 무인기 미션 컴퓨터 소프트웨어 아키텍처 설계서"),
    ("p", "RIPOSTE-SAD-001 · 버전 0.4 · 2026-08-17"),
    ("table", ["항목", "내용"], [
        ["문서 ID", "RIPOSTE-SAD-001"],
        ["버전 / 작성일", "0.4 (Draft) / 2026-08-17"],
        ["개정", "0.4: A-8 을 구현 기준으로 갱신(R-11 교차 확인·R-12 협각 가중, 실측 항목 분리) / 0.3: RIPOSTE-SW-ARCH-001 흡수 — 아키텍처 문서를 하나로 통합(용어 규약 §0 포함) / 0.2: 실시간 데이터 경로(A-7)·2채널 검출 정확도(A-8) / 0.1: 최초 작성"],
        ["대상", "RK3588 기반 무인기 미션 컴퓨터 소프트웨어 스택"],
        ["하드웨어", "RK3588(시커+제어) · Hailo-8(검출 NPU) · RK NPU(보조 추적) · Pixhawk 6X/PX4 · SiK 433 MHz"],
        ["상위 문서", "RIPOSTE-SRS-001(요구사항)"],
        ["하위 문서", "RIPOSTE-OBC-SDD-001 · modules/RIPOSTE-{SEEKER,COMMON,SUPERVISOR}-SDD-001"],
        ["기술 기준", "2026-08-17 구현 상태. 충돌 시 원 설계문서가 우선"],
        ["생성", "docs/tools/make_sad.py (내용은 이 스크립트에만 존재)"],
    ], [3.5, 12.5]),

    ("h", 1, "0. 용어 규약"),
    ("p", "본 문서군과 코드는 소프트웨어 엔지니어링 중립 용어로만 기술한다. 정규 대응은 "
           "다음과 같으며, 서술뿐 아니라 식별자·파일명·시험 토큰·설정 키에도 동일하게 적용된다"
           "(전체 규약과 근거는 리포 루트 CLAUDE.md \"서술 규약\")."),
    ("table", ["개념", "정규 용어", "코드 상 이름"], [
        ["제어 세션", "control session", "engage/DISENGAGE 상태 전이, engage_timebox_s"],
        ["만남점 기하 해", "rendezvous", "obc/src/Rendezvous.{h,cpp}, solve_rendezvous()"],
        ["접근·도달 판정", "closure", "run_gazebo_closure.sh, GAZEBO_CLOSURE_PASS"],
        ["추종 제어", "tracking", "AttitudeTrackingSource, obc.att_track_pitch_deg"],
        ["추적 대상 / 자기 기체", "target / ownship", "gz 모델 target, ownship"],
    ], [4.0, 4.0, 8.0]),
    ("p", "engage(동사)만 예외로 유지한다 — PX4 비행 스택의 표준 어휘(\"offboard engaged\")이자 "
           "운용자 CLI 동사·설정 키·명령 opcode로서 외부 계약이기 때문이다. 명사 engagement 은 "
           "쓰지 않으며 그 개념은 \"제어 세션\"이다."),

    ("h", 1, "1. 목적 및 설계 원칙"),
    ("p", "본 문서는 Riposte 미션 컴퓨터 소프트웨어의 아키텍처 — 계층 구조, 프로세스 경계, "
           "데이터 흐름, 핵심 설계 결정 — 를 정의한다. 모듈 내부 상세는 각 SDD에 위임한다."),
    ("p_bold", "설계를 지배하는 원칙 (충돌 시 이들이 이긴다)"),
    ("table", ["원칙", "내용", "아키텍처적 귀결"], [
        ["I1 오퍼레이터 인증", "탐지·AI 판단만으로 상위 임무 명령이 활성화되지 않는다",
         "인증 게이트를 제어 프로세스 안에 두고, 명령 경로를 데이터 경로와 분리"],
        ["I2 인지–제어 격리", "인지 프로세스는 FC에 절대 쓰지 않는다",
         "인지/제어를 별도 프로세스로 분리(A-1). 인지 장애는 버스 신선도 저하로만 전파"],
        ["I3 감시 계층 불변", "안전 감시는 우회·비활성화되지 않는다",
         "감시가 성립할 수 없으면 기동·진입 거부(fail-closed)"],
        ["I4 비신뢰 장치 데이터", "장치·프로세스 경계를 넘은 데이터는 검증 후 사용",
         "수신 측 default-deny 검증(A-6)"],
        ["I5 조용한 기하 오류 방지", "정규화 크기는 거리로 직결된다",
         "채널 변환 시 중심과 크기를 함께 환산(A-5)"],
        ["I6 AI 무결 강등", "보조 AI가 전부 죽어도 기본 추적으로 동작",
         "보조 계층은 기한·워커로 격리(A-4)"],
    ], [3.0, 6.0, 7.0]),

    ("h", 1, "2. 계층 아키텍처"),
    ("code",
     "┌──────────────────────────────────────────────────────────┐\n"
     "│ L5 감독/안전 (cross-cutting)                             │\n"
     "│    OperatorAuthorization · HealthMonitor · Blackbox      │\n"
     "├──────────────────────────────────────────────────────────┤\n"
     "│ L4 유도 Guidance   (OBC 내부 GuidanceSource로 삽입)      │\n"
     "│    TrackBus 소비 · IMM 추정 · 유도 법칙 · Setpoint 생성  │\n"
     "├──────────────────────────────────────────────────────────┤\n"
     "│ L3 제어 Offboard Control                                 │\n"
     "│    OffboardController(FSM) · SetpointStreamer            │\n"
     "│    · SafetyMonitor(SM-1~10) · FcuLink(MAVSDK)            │\n"
     "├──────────────────────────────────────────────────────────┤\n"
     "│ L2 인지 Seeker/Perception                                │\n"
     "│    CameraIngest(2ch) · Detector(Hailo-8) · Scheduler     │\n"
     "│    · ChannelMap · Tracker/Fusion · TargetEstimator       │\n"
     "├──────────────────────────────────────────────────────────┤\n"
     "│ L1 통신 Comms                                            │\n"
     "│    mavlink-router · SiK(GCS) · shm 버스 · UDS 명령채널   │\n"
     "├──────────────────────────────────────────────────────────┤\n"
     "│ L0 플랫폼 Platform                                       │\n"
     "│    Linux · systemd · 단조시각 · Config · Log             │\n"
     "└──────────────────────────────────────────────────────────┘"),

    ("h", 1, "3. 프로세스 및 스레드 구조"),
    ("table", ["프로세스", "계층", "주기 특성", "설계 근거"], [
        ["riposte-seeker", "L2", "캡처 60 fps 고정, 추론은 슬롯 배분", "캡처가 페이싱 요소이고 추론 부하는 스케줄로 흡수"],
        ["riposte-obc", "L3+L4", "결정론적 고정 20 Hz", "제어·유도·안전은 지터 격리가 필수"],
        ["riposte-supervisor", "L5", "저빈도", "버스 신선도 집계·블랙박스 기록"],
        ["mavlink-router", "L1", "systemd 서비스", "최우선 기동, 트래픽 단일 라우팅"],
    ], [3.5, 1.5, 4.0, 7.0]),
    ("p_bold", "핵심 결정 A-1 (지터 격리)"),
    ("p", "인지와 제어를 별도 프로세스로 분리한다. 인지는 추론 부하로 주기가 흔들리지만 제어는 "
           "고정 20 Hz를 지켜야 한다. 한 프로세스에 묶으면 비전 스톨이 제어 지터로 전파되어 "
           "SM-5 위반을 유발한다."),
    ("p_bold", "시커 내부 스레드 구성 (핵심 결정 A-4)"),
    ("table", ["스레드", "책임", "격리 이유"], [
        ["인지 루프", "캡처 → 검출 → 추적 → 융합 → 추정 → 발행", "동기 NPU 호출을 하지 않는 것이 불변식"],
        ["EmbedWorker", "보조 추적용 임베딩(RK NPU)",
         "드라이버가 무기한 블록될 수 있어, 기한 내 미응답 시 그 프레임은 기본 추적으로 강등"],
        ["RecordWorker", "영상 합성·오버레이·기록",
         "인코딩과 파일 I/O가 인지 주기를 잡지 않도록 depth-1 latest-wins로 분리"],
    ], [3.2, 6.3, 6.5]),

    ("h", 1, "4. 데이터 흐름"),
    ("h", 2, "4.1 핫패스 (유도 루프)"),
    ("code",
     "Camera(2ch, 60fps) → Detector(Hailo-8) → Tracker/Fusion\n"
     "   → TargetEstimator(상대 NED) → [TrackBus: shm SeqSlot]\n"
     "   → GuidanceSource @20Hz (검증 → 신선도 → IMM → 유도 법칙)\n"
     "   → Setpoint → SafetyMonitor.clamp → MAVSDK → PX4"),
    ("p_bold", "핵심 결정 A-7 (실시간 데이터 경로 — zero-copy / DMA)"),
    ("p", "1280×720 NV12 프레임은 약 1.3 MB이고 60 fps이므로 복사 한 번이 초당 약 80 MB의 "
           "메모리 대역을 소모한다. 이 대역은 검출 가속기의 PCIe DMA·보조 추적 NPU·2채널 캡처와 "
           "직접 경합하므로(K-5), 복사 제거는 최적화가 아니라 실시간성의 전제다."),
    ("table", ["구간", "현재", "목표"], [
        ["드라이버 → 앱", "zero-copy (V4L2 MMAP, 버퍼를 매핑해 포인터로 사용)", "DMABUF export 로 전환해 하류와 fd 공유"],
        ["전처리(letterbox)", "RGA 2D 가속 경로 존재, 단 가상주소 기반", "RGA 가 dma_buf fd 를 직접 import/export"],
        ["검출 입력", "호스트 버퍼 전달(런타임이 PCIe DMA 수행)", "검출 런타임 dmabuf 입력으로 복사 제거"],
        ["ROI 크롭", "스크래치 버퍼로 복사", "DMA 전송의 소스 사각형 지정으로 복사 제거"],
        ["스레드 경계", "워커로 프레임 복사 (의도적)", "유지 — 아래 근거"],
    ], [3.2, 6.4, 6.4]),
    ("p_note", "워커로 넘기는 복사는 제거 대상이 아니다. 프레임 포인터는 카메라 mmap 버퍼를 "
               "가리키고 그 버퍼는 다음 캡처에서 재사용되므로, 복사하지 않으면 워커가 읽는 도중 "
               "내용이 바뀐다. 이 복사는 정확성의 대가이며, 없애려면 참조 카운트 기반 버퍼 풀 "
               "소유권 모델이 선행되어야 한다."),
    ("p_note", "현 시점에서 보장하는 범위는 드라이버→앱 구간까지다. 종단 zero-copy 는 드라이버· "
               "2D 가속기·검출 런타임 세 계층의 실제 지원 여부에 달려 있어 브링업(B2)에서 확정한다."),

    ("h", 2, "4.2 명령패스 (세션 활성화 승인)"),
    ("code",
     "GCS(오퍼레이터) → SiK → mavlink-router → OBC\n"
     "   → OperatorAuthorization(토큰 검증) → 명령 게이트 통과"),
    ("p", "핫패스는 결정론적 20 Hz, 명령패스는 이벤트 기반 저빈도이며 두 경로는 서로 다른 IPC "
           "채널을 쓴다. 명령이 데이터 경로를 막거나, 데이터가 명령을 대신 활성화하는 일이 "
           "구조적으로 불가능하다(I1)."),
    ("h", 2, "4.3 IPC 설계"),
    ("table", ["채널", "방향", "용도"], [
        ["/riposte_track", "시커 → OBC", "추적 상태(핫패스)"],
        ["/riposte_seeker_health", "시커 → 감독", "인지 건전성"],
        ["/riposte_obc_status", "OBC → 감독", "상태기계·안전 판정"],
        ["/riposte_gps", "OBC → 시커", "기록 오버레이용 좌표"],
        ["/run/riposte/obc.sock", "GCS/도구 → OBC", "이벤트성 명령(UDS)"],
    ], [5.0, 4.0, 7.0]),
    ("p_bold", "핵심 결정 A-2 (버스 = 공유메모리 SeqSlot)"),
    ("ul", [
        "고빈도·최신값 우선·단일 생산자/소비자 조건에 적합하다. 큐 방식은 스테일 누적과 지연 변동을 유발하므로 배제했다.",
        "페이로드는 POD이며 shm ABI 크기를 static_assert로 고정한다. 부분 기록(torn read)은 seq 규약으로 배제한다.",
        "설계 결정 C-5: 페이로드를 원자 워드로 접근한다. 그 결과 TSan suppression을 두지 않으며, TSan 보고는 전부 실제 결함으로 취급한다.",
    ]),
    ("p_bold", "핵심 결정 A-6 (경계 입력 default-deny)"),
    ("p", "버스를 넘어온 표본은 유한값·범위·열거 바이트·타임스탬프를 검증한 뒤에만 사용한다. "
           "불량 표본은 '새 표본 없음'으로 처리해 캐시와 추정기를 오염시키지 않는다. 검증이 없으면 "
           "비정상 값이 추정기를 통과하면서도 세션은 정상으로 보이는 상태가 만들어진다(I4)."),

    ("h", 1, "5. 계층별 상세"),
    ("h", 2, "5.1 L2 인지"),
    ("table", ["구성요소", "책임"], [
        ["CameraIngest", "V4L2 mmap 캡처(광각·협각), 프레임별 단조시각 스탬프, 경계 검증"],
        ["Detector (Hailo-8)", "대상 검출. 비동기 오프로드이며 장애는 건전성 저하로 전파"],
        ["SearchScheduler", "탐색/확인/추적 모드 전이, 슬롯별 채널·ROI 배분, 확인 윈도우 판정"],
        ["ChannelMap", "광각↔협각 좌표·크기 환산(설계 결정 S-13)"],
        ["Tracker / TrackFusion", "다중대상 연관·ID 유지. 검출이 항상 권위이고 템플릿은 보조"],
        ["TargetEstimator", "화면 좌표 → 상대 3D 상태. 품질·신선도 태그와 함께 발행"],
        ["EmbedWorker / RecordWorker", "동기 NPU 호출과 기록 I/O를 인지 스레드에서 격리"],
    ], [4.5, 11.5]),
    ("p_bold", "핵심 결정 A-5 (공용 좌표계는 광각)"),
    ("p", "추적기·추정기는 채널 개념을 모른 채 광각 정규화 좌표에서만 동작하고, 협각 결과는 경계에서 "
           "광각으로 환산된다. 이때 중심뿐 아니라 크기도 함께 환산해야 한다 — 정규화 크기는 시야각에 "
           "반비례하므로 같은 대상이 협각에서 약 3.8배 크게 보이고, 크기를 두면 단안 거리 추정이 "
           "채널 전환마다 붕괴한다. 물리적 원인이 없는 그럴듯한 값이라 증상으로 드러나지 않는다(I5)."),
    ("p_bold", "핵심 결정 A-8 (2채널 검출 정확도 — 시차가 아니라 분해능과 교차 확인)"),
    ("p", "두 카메라는 네스티드 동축·광축 평행 배치라 유효 기선이 사실상 0이고, 따라서 "
           "삼각측량 기반 거리는 성립하지 않는다. 정확도 향상의 기전은 두 가지뿐이며 "
           "둘 다 구현돼 있다(2026-08-17, R-11/R-12)."),
    ("table", ["기전", "구현", "제약"], [
        ["교차 확인 (R-11)", "확인 윈도우 안에서 두 채널이 각각 검출하면 필요한 히트 수를 낮춘다(하한 2). 결과는 TrackState.dual_confirmed 로 발행 — 기존 패딩 바이트를 써서 shm ABI 크기 불변",
         "올리는 방향으로는 작동하지 않는다. 협각은 좁은 창이라 '협각 미검출'이 부재의 증거가 아니고, 오검출을 줄이자고 누락을 늘리면 목적을 배반한다"],
        ["정밀도 우선순위 (R-12)", "협각 표본은 같은 잔차에 대해 추정을 더 많이 움직인다(α-β 게인 가중)",
         "가중은 [1, 상한]으로 클램프 — 설정 실수가 필터를 재튜닝하거나 튜닝값보다 덜 신뢰하게 만들 수 없다. 단일 카메라 구성은 구조적으로 불변"],
    ], [3.4, 6.3, 6.3]),
    ("p_note", "완화폭과 가중값 자체는 실측 항목이다(BRINGUP-001 B2-7/B3-6): 실모델 오검출률과 "
               "채널별 잔차 통계가 있어야 정할 수 있다. 현재 값은 설계 의도를 담은 초기값이다."),
    ("h", 2, "5.2 L3 제어 · L4 유도"),
    ("ul", [
        "OffboardController: 연결→준비→선스트리밍→활성→이탈/착륙의 상태기계. 모든 전이와 지령 송신은 제어 틱 안에서 일어난다.",
        "SafetyMonitor: SM-1~SM-10 평가와 최종 클램프. 클램프는 출처와 무관하게 지령의 마지막 단계다.",
        "GuidanceSource: 트랙 검증 → 신선도 판정 → IMM 추정 → 유도 법칙. 유도를 OBC 안에 둔 것이 결정 A-3이다.",
    ]),
    ("p_bold", "핵심 결정 A-3 (유도를 제어 프로세스 내부에 배치)"),
    ("p", "유도는 제어 결정에 직결되므로 트랙 신선도와 같은 자리에서 판단해야 한다. 그래야 대상 소실을 "
           "제어가 즉시 인지하고 이탈로 수렴한다. 별도 프로세스에 두면 이 결합이 IPC 경계를 넘어 "
           "지연과 불일치를 만든다."),
    ("h", 2, "5.3 L5 감독 · L1 통신 · L0 플랫폼"),
    ("ul", [
        "OperatorAuthorization: 인증 토큰 없이는 상위 임무 명령이 성립하지 않는다(I1). 진입 전 승인 / 진행 중 감시의 이중 구조.",
        "Supervisor: 세 버스의 신선도를 집계하고 이탈 사유를 블랙박스에 기록(회전·보존 포함).",
        "mavlink-router: FC·GCS·OBC 트래픽 단일 라우팅. SiK 저대역이므로 스트림 레이트 관리가 필요하다.",
        "L0: systemd 유닛으로 기동 순서를 고정하고, 신선도 판정 기준은 단조시각으로 통일한다.",
    ]),

    ("h", 1, "6. 안전 아키텍처"),
    ("p", "안전은 특정 모듈이 아니라 관통 요소다. 진입 전에는 인증과 사전조건 게이트가, 진행 중에는 "
           "SM-1~SM-10이, 마지막에는 클램프가 작동하는 3중 구조다."),
    ("table", ["단계", "기전", "실패 시 동작"], [
        ["진입 전", "오퍼레이터 인증, 텔레메트리·배터리·경계 사전조건", "진입 거부(READY 유지)"],
        ["진행 중", "SM-1~SM-10 주기 평가", "즉시 이탈 절차로 전이, 사유를 기록"],
        ["지령 출력", "속도·자세 클램프(최종 방어선)", "한계 내로 강제"],
        ["구성 오류", "설정 범위·능력 검증", "기동 거부 — 축소 동작을 택하지 않는다"],
    ], [3.0, 6.5, 6.5]),
    ("p_note", "fail-closed를 축소 동작보다 우선하는 이유: 오퍼레이터가 명시적으로 구성한 보호 수단을 "
               "소프트웨어가 임의로 제거하면, 시스템은 정상으로 보이면서 실제 보호는 사라진 상태가 된다. "
               "이 상태는 증상이 없어 비행 중 발견되지 않는다."),

    ("h", 1, "7. 핵심 설계 결정 요약"),
    ("table", ["ID", "결정", "근거"], [
        ["A-1", "인지/제어 프로세스 분리", "비전 스톨의 제어 지터 전파 차단"],
        ["A-2", "버스 = 공유메모리 SeqSlot", "고빈도·최신값·단일 생산자/소비자에 최적"],
        ["A-3", "유도 법칙을 제어 프로세스 내부에", "신선도–판단 결합, 대상 소실 즉시 인지"],
        ["A-4", "인지 스레드에서 동기 NPU 호출 금지", "드라이버 블록이 60 Hz 루프 전체를 정지시킴"],
        ["A-5", "공용 좌표계는 광각, 협각은 경계 환산", "추적기·추정기를 채널 불문 유지, 거리 불연속 방지"],
        ["A-6", "경계 입력 default-deny 검증", "비신뢰 데이터 오염이 정상으로 보이는 경로 차단"],
        ["A-7", "실시간 데이터 경로의 복사 최소화", "프레임 복사가 메모리 대역을 가속기와 경합시킴"],
        ["A-8", "2채널 교차 확인·협각 가중으로 검출 정확도 증대(R-11/R-12)", "동축 배치라 시차가 없으므로 각분해능과 교차 확인이 유일한 기전"],
        ["C-5", "버스 페이로드 원자 접근", "TSan suppression 없는 상태 유지"],
        ["D-2", "자동 세션 활성화 경로 배제", "오퍼레이터 인증 게이트"],
    ], [1.3, 6.2, 8.5]),

    ("h", 1, "8. 배포 및 검증 아키텍처"),
    ("table", ["구분", "내용"], [
        ["배포", "systemd 유닛 4종(라우터·시커·OBC·감독), sysusers/sysctl/udev 규칙 동반. 비특권 계정으로 기동"],
        ["빌드 격리", "모든 벤더 의존은 컴파일 옵션 뒤 단일 TU에 격리. 옵션 OFF에서 호스트 빌드·시험이 항상 성립"],
        ["호스트 검증", "단위시험 24종 + 새니타이저(ASan/UBSan/TSan). 안전 판정은 순수 함수로 분리해 고정"],
        ["벤더 경로", "stub 헤더로 문법 검사(동작은 실기 몫)"],
        ["시뮬레이션", "SIL(합성 파이프라인), PX4 SITL(상태기계·결함 주입·유도), Gazebo(물리 이동 대상)"],
        ["실기", "RIPOSTE-BRINGUP-001 체크리스트 B0~B8"],
    ], [3.2, 12.8]),

    ("h", 1, "9. 미결 및 가정"),
    ("table", ["구분", "내용"], [
        ["가정", "제품 표준 검출 모델은 NMS 내장 컴파일, 입력 RGB888/NHWC/uint8 — 브링업 B2로 확인"],
        ["가정", "보조 추적 모델 지연 ≤5 ms/frame — 브링업 B3로 확인"],
        ["해소됨", "공유메모리 IPC 구현 가능성 — 구현·원자화 완료(C-5)"],
        ["해소됨", "상태 추정 상세 — 다중 모델(IMM) 기반으로 구현"],
        ["미결", "유도 계수·종말 전략의 비행 튜닝"],
        ["미결", "광각↔협각 정렬 오프셋 실측 교정"],
        ["미결", "종단 zero-copy(DMA fd 공유) 지원 여부 — 드라이버·RGA·검출 런타임 3계층 실측"],
        ["미결", "R-11 완화폭·R-12 가중값 실측 튜닝 — 실모델 오검출률과 채널별 잔차 통계 확보 후(B2-7/B3-6)"],
        ["보류", "서브보드 안전 인터록 재도입 — POC 이후"],
        ["보류", "영상 스트림의 GCS 전송 경로 — 저대역 한계, 보조 링크 검토"],
    ], [2.5, 13.5]),

    ("p_note", "본 문서는 아키텍처의 권위 문서다(구 RIPOSTE-SW-ARCH-001 을 2026-08-17 에 흡수). "
               "모듈 내부 상세는 각 SDD 가 권위이며, 그 범위에서는 SDD 가 우선한다."),
]

if __name__ == "__main__":
    d = docgen.out_dir()
    docgen.emit(BLOCKS,
                os.path.join(d, "RIPOSTE-SAD-001_SW아키텍처설계서.md"),
                os.path.join(d, "RIPOSTE-SAD-001_SW아키텍처설계서.docx"))

# --------------------------------------------------------------------------
# English edition. docgen.check_parallel below fails generation if the two
# languages stop describing the same document.
BLOCKS_EN = [
    ("h", 0, "RIPOSTE UAV Mission Computer — Software Architecture Document"),
    ("p", "RIPOSTE-SAD-001 · Revision 0.4 · 2026-08-17"),
    ("table", ["Item", "Content"], [
        ["Document ID", "RIPOSTE-SAD-001"],
        ["Revision / date", "0.4 (Draft) / 2026-08-17"],
        ["History", "0.4: A-8 restated against the implementation (R-11/R-12) / 0.3: absorbed RIPOSTE-SW-ARCH-001 into a single architecture document / 0.2: real-time data path (A-7), dual-channel accuracy (A-8) / 0.1: initial issue"],
        ["Target", "Software stack on the RK3588 UAV mission computer"],
        ["Hardware", "RK3588 (perception + control) · Hailo-8 (detection NPU) · RK NPU (assisted tracking) · Pixhawk 6X/PX4 · SiK 433 MHz"],
        ["Parent document", "RIPOSTE-SRS-001 (requirements)"],
        ["Child documents", "RIPOSTE-OBC-SDD-001 · modules/RIPOSTE-{SEEKER,COMMON,SUPERVISOR}-SDD-001"],
        ["Basis", "Implementation as of 2026-08-17. Where this disagrees with a module SDD, the SDD wins in its own scope"],
        ["Generated by", "docs/tools/make_sad.py (the content lives only in that script)"],
    ], [3.5, 12.5]),

    ("h", 1, "0. Terminology convention"),
    ("p", "This document set and the code use neutral software-engineering vocabulary only. The canonical "
           "mapping is below, and it applies to identifiers, file names, test tokens and configuration keys "
           "as well as prose (the full convention and its rationale are in CLAUDE.md)."),
    ("table", ["Concept", "Canonical term", "Name in code"], [
        ["Control session", "control session", "engage/DISENGAGE transitions, engage_timebox_s"],
        ["Meeting-point geometry", "rendezvous", "obc/src/Rendezvous.{h,cpp}, solve_rendezvous()"],
        ["Approach / arrival judgement", "closure", "run_gazebo_closure.sh, GAZEBO_CLOSURE_PASS"],
        ["Following control", "tracking", "AttitudeTrackingSource, obc.att_track_pitch_deg"],
        ["Tracked object / own vehicle", "target / ownship", "gz models target, ownship"],
    ], [4.0, 4.0, 8.0]),
    ("p", "The verb engage is the single exception: it is standard flight-stack vocabulary (\"offboard "
           "engaged\") and an external contract — the operator CLI verb, configuration keys and a command "
           "opcode. The noun engagement is not used; that concept is a control session."),

    ("h", 1, "1. Purpose and design principles"),
    ("p", "This document defines the architecture — layers, process boundaries, data flow and the key "
           "design decisions. Module internals are delegated to the individual SDDs."),
    ("p_bold", "Principles that govern the design (they win where anything conflicts)"),
    ("table", ["Principle", "Content", "Architectural consequence"], [
        ["I1 operator authorization", "Detection or AI judgement alone never activates a mission command",
         "The authorization gate lives inside the control process, and the command path is separate from the data path"],
        ["I2 perception/control isolation", "The perception process never writes to the FC",
         "Separate processes (A-1); a perception fault propagates only as bus staleness"],
        ["I3 inviolable monitor layer", "Safety monitoring is never bypassed or disabled",
         "If monitoring cannot be established, startup or entry is refused (fail-closed)"],
        ["I4 untrusted device data", "Data crossing a device or process boundary is validated before use",
         "Default-deny validation at the reader (A-6)"],
        ["I5 no silent geometry errors", "Normalized size feeds range directly",
         "Channel conversion carries centre and size together (A-5)"],
        ["I6 graceful AI degradation", "The baseline tracker survives total assisted-AI failure",
         "Assisted layers are isolated behind workers and deadlines (A-4)"],
    ], [3.0, 6.0, 7.0]),

    ("h", 1, "2. Layered architecture"),
    ("code",
     "+----------------------------------------------------------+\n"
     "| L5 supervision / safety (cross-cutting)                  |\n"
     "|    OperatorAuthorization · HealthMonitor · Blackbox      |\n"
     "+----------------------------------------------------------+\n"
     "| L4 guidance   (inserted into the OBC as GuidanceSource)  |\n"
     "|    TrackBus consumer · IMM estimation · guidance law     |\n"
     "+----------------------------------------------------------+\n"
     "| L3 offboard control                                      |\n"
     "|    OffboardController (FSM) · SetpointStreamer           |\n"
     "|    · SafetyMonitor (SM-1..10) · FcuLink (MAVSDK)         |\n"
     "+----------------------------------------------------------+\n"
     "| L2 perception (seeker)                                   |\n"
     "|    CameraIngest (2ch) · Detector (Hailo-8) · Scheduler   |\n"
     "|    · ChannelMap · Tracker/Fusion · TargetEstimator       |\n"
     "+----------------------------------------------------------+\n"
     "| L1 comms                                                 |\n"
     "|    mavlink-router · SiK (GCS) · shm buses · UDS commands |\n"
     "+----------------------------------------------------------+\n"
     "| L0 platform                                              |\n"
     "|    Linux · systemd · monotonic clock · config · logging  |\n"
     "+----------------------------------------------------------+"),

    ("h", 1, "3. Processes and threads"),
    ("table", ["Process", "Layer", "Timing", "Rationale"], [
        ["riposte-seeker", "L2", "Fixed 60 fps capture, inference allocated per slot", "Capture is the pacing element; inference load is absorbed by the schedule"],
        ["riposte-obc", "L3+L4", "Deterministic fixed 20 Hz", "Control, guidance and safety require jitter isolation"],
        ["riposte-supervisor", "L5", "Low rate", "Bus freshness aggregation and blackbox recording"],
        ["mavlink-router", "L1", "systemd service", "Starts first; single routing point for all traffic"],
    ], [3.5, 1.5, 4.0, 7.0]),
    ("p_bold", "Key decision A-1 (jitter isolation)"),
    ("p", "Perception and control are separate processes. Perception absorbs inference load through its "
           "schedule while control must hold a fixed 20 Hz; in one process a vision stall would propagate "
           "into control jitter and trip SM-5."),
    ("p_bold", "Seeker thread structure (key decision A-4)"),
    ("table", ["Thread", "Responsibility", "Why it is isolated"], [
        ["Perception loop", "capture -> detect -> track -> fuse -> estimate -> publish", "Making no synchronous NPU call is the invariant"],
        ["EmbedWorker", "Assisted-tracking embeddings (RK NPU)",
         "The driver can block indefinitely; past the deadline the frame degrades to motion-only tracking"],
        ["RecordWorker", "Video composition, overlay and recording",
         "Encoding and file I/O must not hold the perception period (depth-1 latest-wins)"],
    ], [3.2, 6.3, 6.5]),

    ("h", 1, "4. Data flow"),
    ("h", 2, "4.1 Hot path (guidance loop)"),
    ("code",
     "Camera (2ch, 60fps) -> Detector (Hailo-8) -> Tracker/Fusion\n"
     "   -> TargetEstimator (relative NED) -> [TrackBus: shm SeqSlot]\n"
     "   -> GuidanceSource @20Hz (validate -> freshness -> IMM -> guidance law)\n"
     "   -> setpoint -> SafetyMonitor.clamp -> MAVSDK -> PX4"),
    ("p_bold", "Key decision A-7 (real-time data path — zero copy / DMA)"),
    ("p", "A 1280x720 NV12 frame is about 1.3 MB, so at 60 fps one copy costs roughly 80 MB/s of memory "
           "bandwidth — contended directly by the detection accelerator's PCIe DMA, the assisted-tracking NPU "
           "and two capture streams (K-5). Copy removal is therefore a precondition for real-time behaviour, "
           "not an optimization."),
    ("table", ["Segment", "Today", "Target"], [
        ["Driver -> application", "zero copy (V4L2 MMAP maps the driver's buffers)", "Switch to DMABUF export and share the fd downstream"],
        ["Preprocessing (letterbox)", "RGA 2D path exists but is addressed virtually", "RGA imports/exports the dma_buf fd directly"],
        ["Detection input", "Host buffer handed over (the runtime performs PCIe DMA)", "dmabuf input to the detection runtime"],
        ["ROI crop", "Copied into a scratch buffer", "Expressed as the source rectangle of a DMA transfer"],
        ["Thread boundary", "Frame copied to the worker (deliberate)", "Kept — see below"],
    ], [3.2, 6.4, 6.4]),
    ("p_note", "The copy handed to the workers is not a defect to remove. The frame pointer refers to the "
               "camera mmap buffer, which is recycled on the next capture, so without the copy the worker would "
               "read data that changes underneath it. It is the price of correctness; removing it requires a "
               "refcounted buffer-pool ownership model first."),
    ("p_note", "What is guaranteed today is the driver-to-application segment. End-to-end zero copy depends on "
               "the driver, the 2D accelerator and the detection runtime all supporting it, and is settled at "
               "bring-up (B2)."),

    ("h", 2, "4.2 Command path (session authorization)"),
    ("code",
     "GCS (operator) -> SiK -> mavlink-router -> OBC\n"
     "   -> OperatorAuthorization (token check) -> command gate passed"),
    ("p", "The hot path is deterministic at 20 Hz; the command path is low-rate and event driven, and the two "
           "use different IPC channels. Neither can a command block the data path, nor can data activate a "
           "command on its own (I1)."),
    ("h", 2, "4.3 IPC design"),
    ("table", ["Channel", "Direction", "Use"], [
        ["/riposte_track", "seeker -> OBC", "Track state (hot path)"],
        ["/riposte_seeker_health", "seeker -> supervisor", "Perception health"],
        ["/riposte_obc_status", "OBC -> supervisor", "State machine and safety verdicts"],
        ["/riposte_gps", "OBC -> seeker", "Coordinates for the recording overlay"],
        ["/run/riposte/obc.sock", "GCS/tools -> OBC", "Event commands (UDS)"],
    ], [5.0, 4.0, 7.0]),
    ("p_bold", "Key decision A-2 (buses are shared-memory SeqSlots)"),
    ("ul", [
        "The traffic is high rate, latest-value, single producer and single consumer, which is what a seqlock suits. A queue would accumulate staleness and add latency variance.",
        "Payloads are POD and the shm ABI size is fixed by static_assert. Torn reads are excluded by the sequence protocol.",
        "Decision C-5: payload words are accessed atomically. As a result there are no TSan suppressions, and every TSan report is treated as a real defect.",
    ]),
    ("p_bold", "Key decision A-6 (default-deny on boundary input)"),
    ("p", "A sample arriving over a bus is used only after its finiteness, ranges, enumerated bytes and "
           "timestamp are validated. A rejected sample is treated as 'no new sample', leaving the cache and "
           "estimator untouched. Without that, an abnormal value passes into the estimator while the session "
           "still reports healthy (I4)."),

    ("h", 1, "5. Layer detail"),
    ("h", 2, "5.1 L2 perception"),
    ("table", ["Component", "Responsibility"], [
        ["CameraIngest", "V4L2 mmap capture (wide and narrow), per-frame monotonic stamp, boundary validation"],
        ["Detector (Hailo-8)", "Target detection, asynchronously offloaded; failure surfaces as degraded health"],
        ["SearchScheduler", "Search/confirm/track transitions, per-slot channel and ROI allocation, confirmation window"],
        ["ChannelMap", "Wide/narrow coordinate and size conversion (decision S-13)"],
        ["Tracker / TrackFusion", "Multi-target association and identity, detection-authoritative template fusion"],
        ["TargetEstimator", "Image coordinates to relative 3D state, published with quality and freshness tags"],
        ["EmbedWorker / RecordWorker", "Isolate synchronous NPU calls and recording I/O from the perception thread"],
    ], [4.5, 11.5]),
    ("p_bold", "Key decision A-5 (the shared frame is the wide channel)"),
    ("p", "The tracker and estimator work only in wide-channel normalized coordinates and know nothing about "
           "channels; narrow results are converted at the boundary. The conversion must carry size as well as "
           "centre: normalized size scales inversely with field of view, so the same target measures about "
           "3.8x larger in the narrow frame, and leaving the size alone would collapse monocular range the "
           "moment a slot changed channel — a plausible number with no physical cause (I5)."),
    ("p_bold", "Key decision A-8 (dual-channel accuracy — resolution and agreement, not parallax)"),
    ("p", "The cameras are nested-coaxial with parallel axes, so the effective baseline is essentially zero "
           "and triangulated range does not exist. There are exactly two mechanisms, and both are implemented "
           "(2026-08-17, R-11/R-12)."),
    ("table", ["Mechanism", "Implementation", "Constraint"], [
        ["Cross-channel confirmation (R-11)", "When both channels detect the target inside the confirmation window, the required hit count drops (floor of 2). The verdict is published as TrackState.dual_confirmed, using an existing pad byte so the shm ABI size is unchanged",
         "It may only lower the bar. The narrow FOV is a small window, so a target missing from it is not evidence of absence, and trading misses for false positives would defeat the purpose"],
        ["Precision priority (R-12)", "A narrow sample moves the estimate further for the same residual (weighted alpha-beta gain)",
         "The weight is clamped to [1, ceiling], so a configuration mistake can neither retune the filter nor make it trust measurements less than tuned. Single-camera behaviour is unchanged by construction"],
    ], [3.4, 6.3, 6.3]),
    ("p_note", "The relaxation width and the weight itself are measurements, not settings (BRINGUP-001 "
               "B2-7/B3-6): they need real-model false-positive rates and per-channel residual statistics. "
               "The current values express design intent only."),
    ("h", 2, "5.2 L3 control and L4 guidance"),
    ("ul", [
        "OffboardController: the connect -> ready -> prestream -> active -> exit/land state machine. Every transition and every setpoint send happens inside the control tick.",
        "SafetyMonitor: evaluates SM-1..SM-10 and applies the final clamp, which is the last step regardless of the setpoint's source.",
        "GuidanceSource: validate the track, judge freshness, run the IMM estimate, apply the guidance law. Placing guidance inside the OBC is decision A-3.",
    ]),
    ("p_bold", "Key decision A-3 (guidance lives inside the control process)"),
    ("p", "Guidance feeds control directly, so it must be judged in the same place as track freshness. That is "
           "how the loss of a target is recognized by control immediately and converges to an exit. Across an "
           "IPC boundary that coupling would acquire latency and disagreement."),
    ("h", 2, "5.3 L5 supervision, L1 comms, L0 platform"),
    ("ul", [
        "OperatorAuthorization: without the token a mission command does not exist (I1). Approval before entry, monitoring during — two independent layers.",
        "Supervisor: aggregates the freshness of three buses and records exit causes into the blackbox with rotation and retention.",
        "mavlink-router: single routing point for FC, GCS and OBC traffic. The SiK link is narrowband, so stream rates need management.",
        "L0: systemd units fix the startup order, and all freshness judgements use the monotonic clock.",
    ]),

    ("h", 1, "6. Safety architecture"),
    ("p", "Safety is cross-cutting rather than a module. Authorization and preconditions act before entry, "
           "SM-1..SM-10 act during the session, and the clamp acts last — three layers."),
    ("table", ["Stage", "Mechanism", "On failure"], [
        ["Before entry", "Operator authorization; telemetry, battery and boundary preconditions", "Entry refused (stay in READY)"],
        ["During the session", "SM-1..SM-10 evaluated每 tick", "Immediate transition to the exit sequence, with the cause recorded"],
        ["Command output", "Velocity and attitude clamp (final defensive line)", "Forced within limits"],
        ["Configuration error", "Range and capability validation", "Startup refused — reduced operation is not chosen"],
    ], [3.0, 6.5, 6.5]),
    ("p_note", "Why fail-closed beats reduced operation: if software silently removes a protection the operator "
               "configured explicitly, the system looks healthy while the protection is gone. That state has no "
               "symptom, so it is not discovered in flight."),

    ("h", 1, "7. Key design decisions"),
    ("table", ["ID", "Decision", "Rationale"], [
        ["A-1", "Separate perception and control processes", "Stops a vision stall from propagating into control jitter"],
        ["A-2", "Buses are shared-memory SeqSlots", "Suits high-rate, latest-value, single producer/consumer traffic"],
        ["A-3", "Guidance law inside the control process", "Couples freshness to the decision; target loss recognized immediately"],
        ["A-4", "No synchronous NPU call on the perception thread", "A blocking driver would stop the entire 60 Hz loop"],
        ["A-5", "Shared frame is wide; narrow converted at the boundary", "Keeps tracker and estimator channel-agnostic and range continuous"],
        ["A-6", "Default-deny validation of boundary input", "Blocks the path where corrupt data still looks healthy"],
        ["A-7", "Minimize copies on the real-time data path", "Frame copies contend for bandwidth with the accelerators"],
        ["A-8", "Cross-channel confirmation and narrow weighting (R-11/R-12)", "With coaxial optics there is no parallax, so resolution and agreement are the only mechanisms"],
        ["C-5", "Atomic access to bus payloads", "Keeps the tree free of TSan suppressions"],
        ["D-2", "No automatic session activation path", "Operator authorization gate"],
    ], [1.3, 6.2, 8.5]),

    ("h", 1, "8. Deployment and verification architecture"),
    ("table", ["Category", "Content"], [
        ["Deployment", "Four systemd units (router, seeker, OBC, supervisor) with sysusers, sysctl and udev rules. Runs unprivileged"],
        ["Build isolation", "Every vendor dependency sits behind a compile option in a single TU; with options off the host build and tests always succeed"],
        ["Host verification", "24 unit-test suites plus sanitizers. Safety decisions are isolated as pure functions"],
        ["Vendor paths", "Syntax-checked against stub headers (behaviour belongs to hardware)"],
        ["Simulation", "SIL (synthetic pipeline), PX4 SITL (state machine, fault injection, guidance), Gazebo (physically simulated moving target)"],
        ["Hardware", "RIPOSTE-BRINGUP-001 checklist B0..B8"],
    ], [3.2, 12.8]),

    ("h", 1, "9. Open items and assumptions"),
    ("table", ["Type", "Content"], [
        ["Assumption", "The production detection model is compiled with NMS on device and takes RGB888/NHWC/uint8 input — confirmed by bring-up B2"],
        ["Assumption", "The assisted-tracking model costs no more than 5 ms per frame — confirmed by bring-up B3"],
        ["Resolved", "Feasibility of the shared-memory IPC — implemented, with atomic payloads (C-5)"],
        ["Resolved", "State-estimation detail — implemented on a multiple-model (IMM) basis"],
        ["Open", "Flight tuning of the guidance gains and terminal strategy"],
        ["Open", "Wide/narrow alignment offset calibration"],
        ["Open", "Whether the driver, 2D accelerator and detection runtime support end-to-end zero copy"],
        ["Open", "R-11 relaxation width and R-12 weighting, to be measured (B2-7/B3-6)"],
        ["Deferred", "Reintroducing the sub-board safety interlock — after the POC"],
        ["Deferred", "Video streaming to the GCS — narrowband limit, secondary link under review"],
    ], [2.5, 13.5]),

    ("p_note", "This is the authoritative architecture document (it absorbed RIPOSTE-SW-ARCH-001 on "
               "2026-08-17). Module internals are owned by the individual SDDs, which take precedence in "
               "their own scope."),
]

if __name__ == "__main__":
    docgen.check_parallel(BLOCKS, BLOCKS_EN)
    _d = docgen.out_dir()
    docgen.emit(BLOCKS_EN,
                os.path.join(_d, "RIPOSTE-SAD-001_Software_Architecture_Document.md"),
                os.path.join(_d, "RIPOSTE-SAD-001_Software_Architecture_Document.docx"))
