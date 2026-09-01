#!/usr/bin/env python3
"""RIPOSTE-SDD-001 소프트웨어 상세설계서(모듈·기능) — Markdown + Word 생성기.

사용법:
    python3 docs/tools/make_sdd.py     # docs/ 에 .md 와 .docx 를 만든다

아키텍처 설계서(SAD-001)가 계층·프로세스·설계 결정을 다룬다면, 본 문서는 그 아래 —
어떤 파일이 무엇을 하고, 어떤 계약을 지키며, 실패하면 어떻게 행동하는지 — 를 모듈
단위로 정리한다. 알고리즘의 유도와 대안 검토는 각 모듈 SDD 원본에 있고 여기서는
반복하지 않는다(충돌 시 원본 우선).

모듈 목록과 역할은 코드의 헤더 주석에서 확인해 옮긴 것이며, 여기서 새 설계를 만들지
않는다. 내용은 이 스크립트에만 있고 출력이 둘이다(docgen). .md 가 형상관리 대상이고
.docx 는 산출물이다. 기술 상태는 2026-08-17 구현 기준.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docgen  # noqa: E402

W3 = [3.6, 6.2, 6.2]

BLOCKS = [
    ("h", 0, "RIPOSTE 무인기 미션 컴퓨터 소프트웨어 상세설계서 (모듈·기능)"),
    ("p", "RIPOSTE-SDD-001 · 버전 0.2 · 2026-08-17"),
    ("table", ["항목", "내용"], [
        ["문서 ID", "RIPOSTE-SDD-001"],
        ["버전 / 작성일", "0.2 (Draft) / 2026-08-17"],
        ["개정", "0.2: 모듈·기능 상세로 확장(파일 단위 책임·계약·실패 동작) / 0.1: 요약본"],
        ["대상", "riposte-seeker · riposte-obc · riposte-supervisor · 공통 기반 · 시험 도구"],
        ["상위 문서", "RIPOSTE-SAD-001(아키텍처), RIPOSTE-SRS-001(요구사항)"],
        ["원본 설계문서", "RIPOSTE-OBC-SDD-001 · modules/RIPOSTE-{SEEKER,COMMON,COMMS,SUPERVISOR}-SDD-001"],
        ["기술 기준", "2026-08-17 구현 상태. 상세·근거·개정 이력은 원본 SDD, 충돌 시 원본 우선"],
        ["생성", "docs/tools/make_sdd.py (내용은 이 스크립트에만 존재)"],
    ], [3.5, 12.5]),

    ("h", 1, "1. 읽는 방법"),
    ("p", "각 모듈은 '책임 / 주요 계약 / 실패 시 동작' 세 가지로 기술한다. 실패 동작을 함께 "
           "적는 이유는, 이 시스템에서 정상 경로보다 실패 경로가 더 자주 문제를 일으키기 "
           "때문이다 — 특히 증상 없이 지나가는 실패가 그렇다."),
    ("p_bold", "설계 전반을 관통하는 규칙"),
    ("ul", [
        "실패는 예외가 아니라 강등으로 표현한다. 보조 계층이 죽어도 기본 경로는 계속 동작한다(I6).",
        "장치·프로세스 경계를 넘은 값은 검증 전에 쓰지 않는다(I4).",
        "안전 판정은 순수 함수로 분리해 호스트 시험으로 고정한다 — 컨트롤러가 장치를 직접 소유해 통째로는 시험할 수 없기 때문이다.",
        "구성이 요구를 만족시킬 수 없으면 축소 동작이 아니라 기동 거부를 택한다(fail-closed).",
        "매직넘버는 Tunables.h 한 곳에 모으고, 런타임 재정의가 필요한 값만 [CFG]로 표시한다.",
    ]),

    # ---------------------------------------------------------------- seeker
    ("h", 1, "2. riposte-seeker (L2 인지)"),
    ("h", 2, "2.1 처리 순서"),
    ("code",
     "[캡처] CameraIngest        광각·협각 2채널, 60 fps, 프레임별 단조시각 스탬프\n"
     "   │                       V4L2 MMAP — 드라이버 버퍼 매핑(복사 없음)\n"
     "   ▼\n"
     "[슬롯 배분] SearchScheduler 모드(탐색/확인/추적)·채널(광각/협각)·ROI 결정\n"
     "   ▼\n"
     "[전처리] Preproc / PreprocRga   ROI 크롭 → 종횡비 보존 letterbox → RGB888\n"
     "   ▼\n"
     "[검출] HailoDetector       Hailo-8 비동기 추론, NMS 내장 경로 우선\n"
     "   ▼\n"
     "[후처리] ModelIo / ChannelMap   좌표 환산(타일→프레임, 협각→광각) · 클래스 필터\n"
     "   ▼\n"
     "[연관] Tracker + AssocCost  운동 게이트 + (선택) 외형 임베딩 비용\n"
     "   ▼\n"
     "[융합] TrackFusion         검출 권위 · 템플릿 보조 · 시각 coast\n"
     "   ▼\n"
     "[추정] TargetEstimator     단안 기하 → 상대 위치·속도 → TrackBus 발행"),

    ("h", 2, "2.2 캡처·전처리"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["CameraIngest", "V4L2 mmap 스트리밍, 프레임 스탬프, 캡처 레이트 설정·확인. 드라이버가 돌려준 버퍼 메타(index·bytesused·data_offset)를 비신뢰로 보고 검증", "규격 밖 버퍼는 폐기하고 다음 프레임 — 루프는 정지하지 않음"],
        ["Preproc", "NV12 → letterbox RGB888(종횡비 보존). OpenCV 비의존", "순수 함수 — 축퇴 입력은 거부값 반환"],
        ["PreprocRga", "동일 변환의 RGA 2D 가속 경로(RIPOSTE_WITH_RGA)", "실패 시 CPU 폴백. 정확성 기준은 CPU 결과"],
        ["ModelIo", "letterbox 좌표 역환산, raw 출력 텐서 디코드, NMS", "후보 수 상한으로 폭주 차단"],
    ], W3),

    ("h", 2, "2.3 검출"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["IDetector", "검출기 경계 인터페이스. 정규화 좌표 계약(해상도 무관)", "—"],
        ["HailoDetector", "HailoRT 어댑터. .hef 로드, 비동기 추론, 출력 포맷 자동 감지", "연속 실패 시 healthy() 하강 → TrackBus 발행 중단 → OBC가 SM-7로 판단"],
        ["HailoNmsParse", "NMS-by-class 출력 버퍼 파서. HailoRT 비의존이라 호스트 시험 가능", "경계 밖 레코드는 폐기"],
        ["SyntheticDetector", "SIL 대체 검출기 — 프레임을 가로지르며 서서히 커지는 대상", "프로덕션 프로필에서는 기동 거부(I6)"],
    ], W3),

    ("h", 2, "2.4 탐색 스케줄·채널"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["SearchScheduler", "모드 전이(광각 탐색 → 타일 스윕 → 확인 → 추적), 슬롯별 채널 배분(S-11), ROI 산출, 확인 윈도우 판정(R-6), 주기 재탐지(R-7)", "순수 로직 — 전 구간 호스트 시험. 실패 경로 없음"],
        ["ChannelMap", "광각↔협각 좌표·크기 환산, cue ROI 환산, 검출 remap(S-13)", "협각 FOV 밖이면 in_fov=false 로 광각 폴백을 지시"],
    ], W3),
    ("p_note", "ROI 좌표계 주의: 타일 ROI는 프레임 무관한 격자 분수라 어느 프레임에나 적용되지만, "
               "cue 기반 ROI는 광각 좌표이므로 협각 슬롯에서는 반드시 환산해야 한다. 이 구분을 "
               "roi_is_cue_window() 가 알려준다."),

    ("h", 2, "2.5 추적·융합·추정"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["Tracker", "다중대상 연관(게이트 내 최고점수 탐욕 매칭) + α-β 평활, primary 선택(R-8)·sticky 유지", "검출 부재 프레임은 coast, 임계 초과 시 트랙 폐기"],
        ["AssocCost", "운동 게이트 + 외형 거리의 비용 결합(TR-B)", "임베딩 부재·무효 시 운동 단독으로 강등(TR-3)"],
        ["TrackFusion", "검출 권위 원칙 아래 템플릿 추적 결과 융합, 시각 coast 판정(TR-C/TR-5/TR-6)", "템플릿 부재 시 운동 coast"],
        ["TargetEstimator", "정규화 중심·크기 → 시선각·거리 → 상대 위치·속도(BODY FRD)", "품질 강등으로 관측성 상실을 전파(EST-6)"],
    ], W3),

    ("h", 2, "2.6 보조 AI 격리 워커"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["IEmbedder / RknnEmbedder", "검출 크롭당 L2 정규화 외형 벡터(RK NPU)", "장치 실패·예외 → 무효 임베딩 → 운동 단독"],
        ["EmbedWorker", "임베딩을 전용 스레드에서 수행하고 인지 스레드는 기한까지만 대기(TR-4/S-12)", "기한 초과·워커 점유·예외 모두 그 프레임은 운동 단독. 연속 무결과가 임계를 넘으면 영구 강등"],
        ["ITemplateTracker", "T2 템플릿 추적기 경계(NanoTrack급)", "부재 시 융합은 검출·운동만으로 동작"],
    ], W3),
    ("p_note", "인지 스레드는 동기 NPU 호출을 하지 않는다 — 이것이 이 프로세스의 불변식이다. "
               "드라이버가 무기한 블록되면 60 Hz 루프 전체가 정지하고, 그 결과는 제어 계층에서 "
               "트랙 신선도 상실로 나타난다."),

    ("h", 2, "2.7 기록"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["VideoRecorder", "ffmpeg 파이프 인코딩, 세그먼트 회전, 디스크 회수, dual 합성, 자막 번인", "인코더·필터 부재는 open 단계에서 판정. 운영자가 녹화를 요구했는데 열 수 없으면 기동 거부"],
        ["RecordWorker", "합성·오버레이를 전용 스레드에서 수행(depth-1 latest-wins). 프레임은 인지 루프가 게시", "밀리면 대기 프레임을 덮어쓴다(기록 프레임 유실 ≪ 인지 루프 정지)"],
    ], W3),

    ("h", 2, "2.8 설정"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["SeekerConfig", "런타임에서 쓰는 모든 키를 한 번만 파싱해 검증(범위·상호관계·능력)", "위반 시 사유를 명시하고 기동 거부 — 값 보정은 하지 않는다"],
    ], W3),

    # ---------------------------------------------------------------- obc
    ("h", 1, "3. riposte-obc (L3 제어 · L4 유도 · 인증)"),
    ("h", 2, "3.1 상태기계와 구동"),
    ("code",
     "IDLE → CONNECTING → READY ──(인증된 활성화 + 사전조건)──▶ PRESTREAM\n"
     "                      ▲                                      │(모드 진입 확인)\n"
     "                      │                                      ▼\n"
     "                 DISENGAGING ◀──(안전 위반 / 오퍼레이터)── OFFBOARD_ACTIVE\n"
     "                      │                                      │\n"
     "                      ▼                                      ▼\n"
     "                    READY                             AUTO_LANDING"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["OffboardController", "상태기계 전체. 모든 전이와 지령 송신은 tick() 안에서만 일어나고, 외부 명령은 요청 플래그만 세운다", "확인되지 않은 이탈은 완료로 보고하지 않고 재시도, 한계 초과 시 FAULT"],
        ["SetpointStreamer", "절대 시각 기준 고정 주기 구동(오차 누적 방지)", "지연은 다음 틱에 흡수, 지터는 SM-5가 판정"],
        ["FcuLink", "MAVSDK 경계. 텔레메트리 구독·Offboard 송신·arm/disarm", "구독 실패·미발견은 연결 타임아웃으로 수렴. 콜백 예외는 흡수"],
    ], W3),
    ("p_note", "PRESTREAM 은 지령 스트림을 먼저 채운 뒤 모드 진입을 확인한다. 확인 실패는 "
               "타임아웃 시점에만 판정하며 그 전의 지연은 정상으로 본다 — 비행제어장치의 저주기 "
               "하트비트가 진입 직후 이전 모드를 실어 보내는 것이 정상이기 때문이다."),

    ("h", 2, "3.2 안전"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["SafetyMonitor", "SM-1~SM-10 평가, 속도·자세 클램프(최종 방어선)", "위반 시 즉시 이탈 전이 + 사유 비트 기록"],
        ["Geofence", "볼록·오목 폴리곤 경계(로컬 NED), 내측 방향 산출", "폴리곤 미설정 시 검사하지 않음. 설정됐는데 투영 불가면 진입 거부"],
        ["OperatorAuthorization", "인증 토큰 검증 — 자동 활성화 경로를 원천 배제(I1)", "토큰 불일치·부재는 명령 거부"],
        ["TargetGate", "인증된 목표 명령의 페이로드 검증(유한·범위·시퀀스 단조)", "위반 시 큐를 래치하지 않고 사유 기록"],
        ["TrackValidate", "TrackBus 표본의 경계 검증(유한·범위·열거·타임스탬프)", "불량 표본은 '새 표본 없음'으로 처리 — 캐시·추정기 미오염"],
    ], W3),
    ("p_bold", "순수 판정 술어 (호스트 시험 대상)"),
    ("ul", [
        "disarm 인가 — armed·landed·상대고도를 각각 자기 스트림 스탬프로 검증한 뒤에만 인가.",
        "착륙 완료 — 관측된 disarm 에만 근거. 낡은 플래그로 감시를 끝내지 않는다.",
        "모드 오버라이드 — 확인 타임아웃 시점에만 판정.",
        "경계 투영 가능 여부 — 폴리곤이 설정된 경우 신선한 위치 정보를 요구.",
    ]),

    ("h", 2, "3.3 유도와 추정"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["GuidanceSource", "트랙 검증 → 신선도 판정 → IMM 추정 → 유도 법칙 → 속도 지령(A-3)", "신선도 상실 시 false 반환 → 컨트롤러가 이탈"],
        ["AttitudeTrackingSource", "자세 모드 종말 조종 — yaw 지향 + nose-down pitch", "트랙 부재·노후 시 안전 자세 유지 후 이탈"],
        ["TargetImm", "상호작용 다중 모델 추정(등속·기동)", "발산 시 품질 강등으로 전파"],
        ["TargetEkf / SizeRangeEkf", "단일 모델 추정, 크기·거리 결합 추정(EST-P7a)", "관측성 상실은 공분산 증가로 표현"],
        ["Rendezvous", "큐 기반 만남점 기하 해(R-2)", "해가 없으면 현재 위치 외삽으로 폴백하고 not-ok 로 보고"],
    ], W3),
    ("p_note", "만남점 해석 주의: 대상이 자기 기체보다 빨라도 횡단 기하에서는 해가 존재한다. "
               "접근속도 비교(closing = s − |V|)로 구현하면 틀린다."),

    ("h", 2, "3.4 지령 소스(전략)"),
    ("table", ["모듈", "용도"], [
        ["ISetpointSource / IAttitudeSource", "지령 생성 전략 경계. false 반환은 '유도 불가'를 뜻하고 컨트롤러가 이탈로 해석"],
        ["TestPatternSource", "브링업용 호버·정속·원형 패턴(시커 비의존)"],
        ["TestAttitudeSource", "브링업용 고정 자세 지령 — 대상 없이 자세 경로 점검"],
        ["MissionSource", "외부 큐 시나리오: 상승 → 안정화 → 큐 지점 비행 → 탐색 → 착륙"],
        ["BalloonPatrolSource", "풍선 시험 행동(T-1~T-5): 경계 내 탐색·접근·선회"],
        ["GuidanceSource", "시커 트랙 기반 추종 — 운용 경로"],
    ], [5.0, 11.0]),

    # ---------------------------------------------------------------- supervisor
    ("h", 1, "4. riposte-supervisor (L5 감독)"),
    ("table", ["모듈", "책임", "실패 시 동작"], [
        ["main", "세 버스(추적·인지 건전성·제어 상태) 신선도 집계 및 요약 기록", "버스 부재는 stale 로 기록하고 계속 동작"],
        ["Blackbox", "크기 상한 JSONL 기록 + 번호 보존 회전", "짧은 쓰기·플러시 실패 시 스트림을 닫아 조용한 유실을 막는다"],
    ], W3),
    ("p_note", "감독은 관찰자이지 제어 경로가 아니다. 기록 실패가 비행 기능에 영향을 주어서는 안 된다."),

    # ---------------------------------------------------------------- common
    ("h", 1, "5. 공통 기반 (common)"),
    ("table", ["모듈", "책임", "핵심 계약"], [
        ["SeqSlot", "단일 writer / 다중 reader 최신값 슬롯(seqlock). 프로세스 간(shm)·프로세스 내 두 형태", "페이로드는 원자 워드 접근(C-5). 홀수 seq 는 수리하지 않고 표본을 보류 — 찢어진 값을 넘기지 않는다"],
        ["Types", "버스 페이로드 정의(TrackState·SeekerHealth·TelemetrySnapshot 등)", "POD, shm ABI 크기 static_assert. 스트림별 도착 스탬프 포함(SM-1 필드별 신선도)"],
        ["Config", "INI 로더", "값 전체 소비 요구, 비유한값 거부, 시간값은 형변환 이전에 부호·범위 검증"],
        ["Tunables", "매직넘버 집결지", "[CFG] 표시만 런타임 재정의 가능"],
        ["CommandBus", "저빈도 명령 채널(UDS 데이터그램)", "핫패스와 물리적으로 분리 — 명령이 데이터 경로를 막지 못한다"],
        ["Clock", "제어 경로 시각은 CLOCK_MONOTONIC", "벽시계는 로그 파일명·기록 스탬프에만 사용"],
        ["Log", "구조적 stderr 로거", "journald 가 프로세스 정체성·벽시계를 부여"],
    ], [3.2, 5.4, 7.4]),

    # ---------------------------------------------------------------- tools
    ("h", 1, "6. 시험·운용 도구"),
    ("table", ["도구", "용도"], [
        ["engage_cli", "운용자 명령 전송(인증 토큰 동반). 세션 활성화·해제·목표 큐"],
        ["gz_track_bridge", "Gazebo 진리값 → TrackBus. 인지 스택 없이 유도·안전 경로를 시험(RIPOSTE_WITH_GZ)"],
        ["test/vendor_stubs + ci/vendor_syntax.sh", "벤더 SDK 없이 Hailo·RKNN TU 문법 검사 — 동작 보증은 아님"],
        ["test/sitl, test/gazebo", "PX4 SITL·Gazebo 시나리오 스크립트"],
    ], [5.5, 10.5]),

    # ---------------------------------------------------------------- verification
    ("h", 1, "7. 검증 대응표"),
    ("table", ["대상", "시험", "층위"], [
        ["안전 판정 술어·설정 검증", "test_safety_fsm · test_seekerconfig · test_config", "호스트"],
        ["좌표·크기 환산", "test_channelmap", "호스트"],
        ["탐색 스케줄·확인 윈도우", "test_search", "호스트"],
        ["연관·융합·추적", "test_assoc · test_fusion · test_seeker", "호스트"],
        ["추정(EKF/IMM/크기-거리)", "test_ekf · test_imm · test_sizeekf", "호스트"],
        ["만남점 기하·유도", "test_rendezvous · test_guidance · test_attitude", "호스트"],
        ["워커 격리·기한·강등", "test_embedworker", "호스트(스레드 구동)"],
        ["기록 정책", "test_recorder · test_blackbox", "호스트"],
        ["버스 동시성", "test_seqslot + TSan", "호스트"],
        ["상태기계·안전 감시 실동작", "SITL 시나리오 3종", "PX4 SITL"],
        ["이동 대상 추종·순찰", "Gazebo 시나리오 3종", "Gazebo"],
        ["드라이버·포맷·지연·열", "BRINGUP-001 B0~B8", "실기"],
    ], [5.0, 6.5, 4.5]),

    ("h", 1, "8. 미결"),
    ("table", ["구분", "내용"], [
        ["미결", "종단 zero-copy(DMA fd 공유) — 드라이버·2D 가속기·검출 런타임 3계층 지원 여부 실측(S-14)"],
        ["미결", "R-11 완화폭(DUAL_CONFIRM_RELAX)·R-12 가중(NARROW_GAIN_SCALE) 실측 튜닝 — 실모델 오검출률과 잔차 통계 확보 후"],
        
        ["미결", "광각↔협각 정렬 오프셋 교정값"],
        ["미결", "AI 실모델 브링업(검출 포맷·보조 추적 지연)"],
        ["미결", "유도 계수·종말 전략의 비행 튜닝"],
    ], [2.5, 13.5]),

    ("p_note", "본 문서는 각 모듈 SDD 원본에서 도출된 정리다. 알고리즘 유도·대안 검토·개정 "
               "이력은 원본을 따르며, 상충 시 원본이 우선한다."),
]

if __name__ == "__main__":
    d = docgen.out_dir()
    docgen.emit(BLOCKS,
                os.path.join(d, "RIPOSTE-SDD-001_SW상세설계서.md"),
                os.path.join(d, "RIPOSTE-SDD-001_SW상세설계서.docx"))

# --------------------------------------------------------------------------
# English edition. docgen.check_parallel below fails generation if a section
# exists in one language only.
BLOCKS_EN = [
    ("h", 0, "RIPOSTE UAV Mission Computer — Software Detailed Design (Modules and Functions)"),
    ("p", "RIPOSTE-SDD-001 · Revision 0.2 · 2026-08-17"),
    ("table", ["Item", "Content"], [
        ["Document ID", "RIPOSTE-SDD-001"],
        ["Revision / date", "0.2 (Draft) / 2026-08-17"],
        ["History", "0.2: expanded to module and function detail (per-file responsibility, contract, failure behaviour) / 0.1: summary"],
        ["Scope", "riposte-seeker · riposte-obc · riposte-supervisor · common foundation · test tooling"],
        ["Parent documents", "RIPOSTE-SAD-001 (architecture), RIPOSTE-SRS-001 (requirements)"],
        ["Source design documents", "RIPOSTE-OBC-SDD-001 · modules/RIPOSTE-{SEEKER,COMMON,COMMS,SUPERVISOR}-SDD-001"],
        ["Basis", "Implementation as of 2026-08-17. Detail, rationale and revision history live in the source SDDs, which take precedence"],
        ["Generated by", "docs/tools/make_sdd.py (the content lives only in that script)"],
    ], [3.5, 12.5]),

    ("h", 1, "1. How to read this"),
    ("p", "Each module is described as responsibility / contract / failure behaviour. Failure behaviour is "
           "given equal weight deliberately: in this system the failure paths cause more trouble than the "
           "normal ones — especially the failures that pass without a symptom."),
    ("p_bold", "Rules that run through the whole design"),
    ("ul", [
        "Failure is expressed as degradation, not exceptions. If an assisted layer dies the baseline path keeps running (I6).",
        "A value that crossed a device or process boundary is not used before validation (I4).",
        "Safety decisions are isolated as pure functions and pinned by host tests — the controller owns its devices directly and cannot be tested whole.",
        "Where a configuration cannot deliver what it asks for, startup is refused rather than reduced (fail-closed).",
        "Magic numbers live in Tunables.h, and only values that need runtime override are marked [CFG].",
    ]),

    ("h", 1, "2. riposte-seeker (L2 perception)"),
    ("h", 2, "2.1 Processing order"),
    ("code",
     "[capture]   CameraIngest       wide + narrow, 60 fps, per-frame monotonic stamp\n"
     "   |                           V4L2 MMAP — driver buffers mapped, no copy\n"
     "   v\n"
     "[allocate]  SearchScheduler    mode (search/confirm/track), channel, ROI\n"
     "   v\n"
     "[preprocess] Preproc/PreprocRga  ROI crop -> aspect-preserving letterbox -> RGB888\n"
     "   v\n"
     "[detect]    HailoDetector      Hailo-8 async inference, NMS-on-device preferred\n"
     "   v\n"
     "[postprocess] ModelIo/ChannelMap  coordinate mapping (tile->frame, narrow->wide)\n"
     "   v\n"
     "[associate] Tracker + AssocCost  motion gate + optional appearance cost\n"
     "   v\n"
     "[fuse]      TrackFusion        detection authoritative, template assists, visual coast\n"
     "   v\n"
     "[estimate]  TargetEstimator    monocular geometry -> relative state -> TrackBus"),

    ("h", 2, "2.2 Capture and preprocessing"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["CameraIngest", "V4L2 mmap streaming, frame stamping, capture-rate request and verification. Buffer metadata returned by the driver (index, bytesused, data_offset) is treated as untrusted and validated", "Out-of-spec buffers are dropped and the next frame is taken — the loop does not stall"],
        ["Preproc", "NV12 to letterboxed RGB888, aspect preserved. No OpenCV", "Pure function — degenerate input returns a rejection"],
        ["PreprocRga", "The same conversion on the RGA 2D accelerator (RIPOSTE_WITH_RGA)", "Falls back to CPU; the CPU result is the correctness reference"],
        ["ModelIo", "Letterbox coordinate inversion, raw output tensor decode, NMS", "Candidate caps stop a runaway tensor"],
    ], W3),

    ("h", 2, "2.3 Detection"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["IDetector", "Detector boundary interface, normalized-coordinate contract (resolution independent)", "—"],
        ["HailoDetector", "HailoRT adapter: load the .hef, run async inference, detect the output format", "Consecutive failures drop healthy(), publication stalls, and the OBC judges it through SM-7"],
        ["HailoNmsParse", "Parser for the NMS-by-class output buffer; HailoRT-free so it is host testable", "Records outside the bounds are discarded"],
        ["SyntheticDetector", "SIL stand-in — a target that drifts across the frame and slowly grows", "A production profile refuses to start on it (I6)"],
    ], W3),

    ("h", 2, "2.4 Search scheduling and channels"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["SearchScheduler", "Mode transitions (wide search -> tile sweep -> confirm -> track), per-slot channel allocation (S-11), ROI selection, confirmation window (R-6), periodic re-detection (R-7)", "Pure logic, fully host tested — no failure path"],
        ["ChannelMap", "Wide/narrow coordinate and size conversion, cue-ROI conversion, detection remap (S-13)", "Outside the narrow FOV it reports in_fov=false, which directs the caller to fall back to wide"],
    ], W3),
    ("p_note", "A note on ROI frames: a tile ROI is a frame-independent grid fraction and applies to whichever "
               "image it is handed, but a cue-based ROI is in wide coordinates and must be converted before "
               "cropping the narrow image. roi_is_cue_window() distinguishes the two."),

    ("h", 2, "2.5 Tracking, fusion and estimation"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["Tracker", "Multi-target association (greedy highest-score in-gate match) with alpha-beta smoothing, primary selection (R-8) and sticky retention", "Frames without a detection coast; the track is dropped past the miss limit"],
        ["AssocCost", "Combines the motion gate with appearance distance into one cost (TR-B)", "Missing or invalid embeddings degrade to motion-only (TR-3)"],
        ["TrackFusion", "Fuses template tracking under the detection-authoritative rule, decides visual coast (TR-C/TR-5/TR-6)", "Without a template it coasts on motion"],
        ["TargetEstimator", "Normalized centre and size to bearing and range, then relative position and velocity (body FRD)", "Loss of observability propagates as degraded quality (EST-6)"],
    ], W3),

    ("h", 2, "2.6 Assisted-AI isolation workers"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["IEmbedder / RknnEmbedder", "One L2-normalized appearance vector per detection crop (RK NPU)", "Device failure or an exception yields invalid embeddings, so association is motion-only"],
        ["EmbedWorker", "Runs embedding on its own thread while the perception thread waits only up to a deadline (TR-4/S-12)", "Deadline miss, busy worker or exception all leave that frame motion-only; sustained absence degrades permanently"],
        ["ITemplateTracker", "T2 template-tracker boundary (NanoTrack class)", "Absent, fusion runs on detection and motion alone"],
    ], W3),
    ("p_note", "The perception thread makes no synchronous NPU call — that is this process's invariant. A "
               "driver that blocks indefinitely would stop the entire 60 Hz loop, and the control layer would "
               "see it as loss of track freshness."),

    ("h", 2, "2.7 Recording"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["VideoRecorder", "ffmpeg pipe encoding, segment rotation, disk reclamation, dual composition, subtitle burn-in", "Missing encoder or filter is decided at open. If the operator asked for recording and it cannot open, startup is refused"],
        ["RecordWorker", "Composition and overlay on its own thread (depth-1 latest-wins); frames are posted by the perception loop", "If it falls behind the pending frame is overwritten (a dropped recording frame is far cheaper than a stalled perception loop)"],
    ], W3),

    ("h", 2, "2.8 Configuration"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["SeekerConfig", "Parses every runtime key once and validates it (range, relationships, capability)", "A violation names the reason and refuses startup — values are never corrected"],
    ], W3),

    ("h", 1, "3. riposte-obc (L3 control, L4 guidance, authorization)"),
    ("h", 2, "3.1 State machine and driving"),
    ("code",
     "IDLE -> CONNECTING -> READY --(authorized activation + preconditions)--> PRESTREAM\n"
     "                       ^                                                  |(mode entry confirmed)\n"
     "                       |                                                  v\n"
     "                  DISENGAGING <--(safety violation / operator)-- OFFBOARD_ACTIVE\n"
     "                       |                                                  |\n"
     "                       v                                                  v\n"
     "                     READY                                          AUTO_LANDING"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["OffboardController", "The whole state machine. Every transition and setpoint send happens inside tick(); external commands only set request flags", "An unconfirmed exit is never reported complete — it is retried, and past the bound becomes FAULT"],
        ["SetpointStreamer", "Fixed-rate driving against an absolute deadline (no error accumulation)", "Lateness is absorbed on the next tick; jitter is judged by SM-5"],
        ["FcuLink", "The MAVSDK boundary: telemetry subscription, Offboard sending, arm/disarm", "Subscription failure or non-discovery converges on the connect timeout; callback exceptions are absorbed"],
    ], W3),
    ("p_note", "PRESTREAM fills the setpoint stream first and only then confirms mode entry. Failure is judged "
               "at the timeout alone, and lateness before it is treated as normal — the flight controller's "
               "low-rate heartbeat legitimately reports the pre-entry mode just after the acknowledgement."),

    ("h", 2, "3.2 Safety"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["SafetyMonitor", "Evaluates SM-1..SM-10 and applies the velocity/attitude clamp (final defensive line)", "A violation transitions immediately to exit with the cause recorded as a bit"],
        ["Geofence", "Convex or concave polygon boundary in local NED, with inward direction", "An unconfigured polygon is not checked; a configured one that cannot be projected refuses entry"],
        ["OperatorAuthorization", "Token verification — there is no autonomous activation path (I1)", "A missing or wrong token rejects the command"],
        ["TargetGate", "Payload validation of an authenticated target command (finite, in range, monotonic sequence)", "A violation is not latched, and the reason is recorded"],
        ["TrackValidate", "Boundary validation of TrackBus samples (finite, range, enumerations, timestamp)", "A bad sample counts as 'no new sample' — cache and estimator stay uncontaminated"],
    ], W3),
    ("p_bold", "Pure decision predicates (host tested)"),
    ("ul", [
        "Disarm authorization — armed, landed and relative altitude each proved fresh by their own stream stamp.",
        "Landing completion — rests on an OBSERVED disarm; a stale flag never ends the landing watch.",
        "Mode override — judged only at the confirmation timeout.",
        "Fence projectability — a configured polygon demands a fresh position fix.",
    ]),

    ("h", 2, "3.3 Guidance and estimation"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["GuidanceSource", "Validate the track, judge freshness, run the IMM estimate, apply the guidance law, emit a velocity setpoint (A-3)", "Loss of freshness returns false and the controller exits"],
        ["AttitudeTrackingSource", "Attitude-mode terminal steering — yaw to face the target, nose-down pitch", "Without a track, or with a stale one, it holds a safe attitude and then exits"],
        ["TargetImm", "Interacting multiple-model estimation (constant velocity and manoeuvre)", "Divergence propagates as degraded quality"],
        ["TargetEkf / SizeRangeEkf", "Single-model estimation and joint size/range estimation (EST-P7a)", "Loss of observability is expressed as growing covariance"],
        ["Rendezvous", "Closed-form meeting-point geometry for the cue (R-2)", "With no solution it falls back to extrapolating the current position and reports not-ok"],
    ], W3),
    ("p_note", "A caution on the rendezvous solution: a target faster than the own vehicle can still be met on "
               "a crossing geometry. Implementing it as a closing-speed comparison is simply wrong."),

    ("h", 2, "3.4 Setpoint sources (strategies)"),
    ("table", ["Module", "Use"], [
        ["ISetpointSource / IAttitudeSource", "The setpoint strategy boundary. Returning false means 'cannot guide', which the controller reads as an exit"],
        ["TestPatternSource", "Bring-up hover, constant-velocity and circular patterns (no seeker dependency)"],
        ["TestAttitudeSource", "Bring-up fixed attitude command — exercises the attitude path with no target"],
        ["MissionSource", "External-cue scenario: climb, settle, fly to the cue point, search, land"],
        ["BalloonPatrolSource", "Balloon-trial behaviour (T-1..T-5): search, approach and turn inside a surveyed boundary"],
        ["GuidanceSource", "Seeker-driven tracking — the operational path"],
    ], [5.0, 11.0]),

    ("h", 1, "4. riposte-supervisor (L5 supervision)"),
    ("table", ["Module", "Responsibility", "On failure"], [
        ["main", "Aggregates the freshness of three buses (track, perception health, control status) and records a summary", "A missing bus is recorded as stale and operation continues"],
        ["Blackbox", "Size-bounded JSONL recording with numbered retention", "A short write or a failed flush closes the stream, so nothing is lost silently"],
    ], W3),
    ("p_note", "The supervisor is an observer, not a control path. A recording failure must never affect flight."),

    ("h", 1, "5. Common foundation"),
    ("table", ["Module", "Responsibility", "Key contract"], [
        ["SeqSlot", "Single-writer / multi-reader latest-value slot (seqlock), in shared-memory and in-process forms", "Payload words are accessed atomically (C-5). An odd sequence is not repaired — the sample is withheld rather than handing over a torn value"],
        ["Types", "Bus payload definitions (TrackState, SeekerHealth, TelemetrySnapshot and so on)", "POD, with the shm ABI size fixed by static_assert. Carries per-stream arrival stamps (field-level SM-1)"],
        ["Config", "INI loader", "Requires the whole value consumed, rejects non-finite numbers, and validates durations for sign and range before conversion"],
        ["Tunables", "The single home for magic numbers", "Only values marked [CFG] may be overridden at runtime"],
        ["CommandBus", "Low-rate command channel (UDS datagram)", "Physically separate from the hot path, so a command cannot block the data path"],
        ["Clock", "Control-path time is CLOCK_MONOTONIC", "Wall-clock is used only for log file names and record stamps"],
        ["Log", "Structured stderr logger", "journald supplies process identity and wall-clock"],
    ], [3.2, 5.4, 7.4]),

    ("h", 1, "6. Test and operational tooling"),
    ("table", ["Tool", "Use"], [
        ["engage_cli", "Operator command transmission with the authorization token: activate, release, target cue"],
        ["gz_track_bridge", "Gazebo ground truth into the TrackBus — exercises guidance and safety without the perception stack (RIPOSTE_WITH_GZ)"],
        ["test/vendor_stubs + ci/vendor_syntax.sh", "Syntax-checks the Hailo and RKNN TUs without a vendor SDK — not a behavioural guarantee"],
        ["test/sitl, test/gazebo", "PX4 SITL and Gazebo scenario scripts"],
    ], [5.5, 10.5]),

    ("h", 1, "7. Verification map"),
    ("table", ["Subject", "Test", "Level"], [
        ["Safety predicates and configuration validation", "test_safety_fsm · test_seekerconfig · test_config", "Host"],
        ["Coordinate and size conversion", "test_channelmap", "Host"],
        ["Search scheduling and confirmation window", "test_search", "Host"],
        ["Association, fusion and tracking", "test_assoc · test_fusion · test_seeker", "Host"],
        ["Estimation (EKF / IMM / size-range)", "test_ekf · test_imm · test_sizeekf", "Host"],
        ["Rendezvous geometry and guidance", "test_rendezvous · test_guidance · test_attitude", "Host"],
        ["Worker isolation, deadline and degradation", "test_embedworker", "Host (drives a real thread)"],
        ["Recording policy", "test_recorder · test_blackbox", "Host"],
        ["Bus concurrency", "test_seqslot with TSan", "Host"],
        ["State machine and safety monitors in operation", "Three SITL scenarios", "PX4 SITL"],
        ["Moving-target tracking and patrol", "Three Gazebo scenarios", "Gazebo"],
        ["Drivers, formats, latency, thermals", "BRINGUP-001 B0..B8", "Hardware"],
    ], [5.0, 6.5, 4.5]),

    ("h", 1, "8. Open items"),
    ("table", ["Type", "Content"], [
        ["Open", "End-to-end zero copy (DMA fd sharing) — driver, 2D accelerator and detection runtime support to be measured (S-14)"],
        ["Open", "R-11 relaxation width and R-12 weighting — to be tuned from real-model false-positive rates and residual statistics"],
        ["Open", "Wide/narrow alignment offset calibration"],
        ["Open", "AI model bring-up (detection format, assisted-tracking latency)"],
        ["Open", "Flight tuning of guidance gains and the terminal strategy"],
    ], [2.5, 13.5]),

    ("p_note", "This document is a consolidation of the individual module SDDs. Algorithm derivation, "
               "alternatives considered and revision history live in those documents, which take precedence "
               "where the two disagree."),
]

if __name__ == "__main__":
    docgen.check_parallel(BLOCKS, BLOCKS_EN)
    _d = docgen.out_dir()
    docgen.emit(BLOCKS_EN,
                os.path.join(_d, "RIPOSTE-SDD-001_Software_Detailed_Design.md"),
                os.path.join(_d, "RIPOSTE-SDD-001_Software_Detailed_Design.docx"))
