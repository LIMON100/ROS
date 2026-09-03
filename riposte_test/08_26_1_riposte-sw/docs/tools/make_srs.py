#!/usr/bin/env python3
"""RIPOSTE-SRS-001 요구사항 정의서 — Markdown + Word 생성기.

사용법:
    python3 docs/tools/make_srs.py     # docs/ 에 .md 와 .docx 를 만든다

내용은 여기에만 있고 출력이 둘이다(docgen). .md 는 형상관리 대상이고 .docx 는
산출물이라 커밋하지 않는다(.gitignore) — Word 파일은 zip 바이너리라 diff 도 리뷰도
되지 않기 때문이다. 문서를 고칠 때는 이 파일을 고친 뒤 재생성한다.

출처는 리포의 승인 문서다 — DUALEO-REQ(R-x/T-x), TRACKER-REQ(TR-x),
ESTIMATION-REQ(EST-x), SEEKER-SDD(S-x), OBC-SDD(SM-x), AGENTS(I1~I6),
BRINGUP-001(검증 단계). 여기서 새 요구사항을 만들어내지 않는다: 원 문서가 바뀌면
이 요약도 따라 고쳐야 하고, 충돌 시 원 문서가 이긴다. 상태 표기는 2026-08-17 기준.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import docgen  # noqa: E402

BLOCKS = [
    ("h", 0, "RIPOSTE 무인기 미션 컴퓨터 소프트웨어 요구사항 정의서"),
    ("p", "RIPOSTE-SRS-001 · 버전 0.2 · 2026-08-17"),
    ("table", ["항목", "내용"], [
        ["문서 ID", "RIPOSTE-SRS-001"],
        ["버전 / 작성일", "0.2 (Draft) / 2026-08-17"],
        ["개정", "0.2: R-11·R-12(2채널 검출 정확도), NFR 실시간 데이터 경로(zero-copy/DMA) 반영 / 0.1: 최초 작성"],
        ["대상 시스템", "RK3588 기반 무인기 미션 컴퓨터 소프트웨어 스택 (Riposte POC)"],
        ["하드웨어 구성", "RK3588(시커+제어) · Hailo-8(검출 NPU) · RK NPU(보조 추적) · Pixhawk 6X/PX4(FC) · SiK 433 MHz(GCS 링크)"],
        ["상위 근거 문서", "RIPOSTE-SAD-001, RIPOSTE-DUALEO-REQ-001, RIPOSTE-TRACKER-REQ-001, RIPOSTE-ESTIMATION-REQ-001, RIPOSTE-LENS-REQ-001, RIPOSTE-OBC-SDD-001"],
        ["용어 규약", "소프트웨어 엔지니어링 중립 용어 (RIPOSTE-SAD-001 §0)"],
        ["생성", "docs/tools/make_srs.py (내용은 이 스크립트에만 존재)"],
    ], [3.5, 12.5]),

    ("h", 1, "1. 목적 및 범위"),
    ("p", "본 문서는 Riposte 무인기 미션 컴퓨터에서 동작하는 소프트웨어의 요구사항을 정의한다. "
           "요구사항은 리포지토리의 승인 설계문서(REQ/SDD)에서 도출했으며, 각 항목은 §9 추적성 "
           "표를 통해 원 문서의 결정 ID와 검증 수단에 연결된다."),
    ("h", 2, "1.1 범위"),
    ("ul", [
        "포함: L2 인지(riposte-seeker), L3/L4 제어·유도(riposte-obc), L5 건전성·기록(riposte-supervisor) 소프트웨어.",
        "제외: PX4 비행제어 펌웨어, GCS 애플리케이션, 기구·광학 하드웨어 설계(RIPOSTE-LENS-REQ-001에서 별도 정의).",
        "제외: 서브보드(STM32H7) — POC 범위 외이며 안전 인터록 하드웨어 승격 시 재도입.",
    ]),
    ("h", 2, "1.2 시스템 개요"),
    ("p", "GCS의 인증된 명령을 받아 이륙하고, 이중 EO(광각/협각) 카메라로 대상을 탐지·추적하며, "
           "추정된 상대 상태를 바탕으로 유도 제어를 수행하는 무인기 탑재 소프트웨어다. 인지 계층은 "
           "비행제어장치(FC)에 직접 쓰지 않으며, 모든 제어 출력은 안전 감시 계층을 통과한 뒤에만 "
           "FC로 전달된다."),
    ("table", ["계층", "프로세스", "책임"], [
        ["L2 인지", "riposte-seeker", "카메라 캡처, 전처리, AI 검출, 후처리, 다중대상 추적, 단안 상대상태 추정, TrackBus 발행"],
        ["L3/L4 제어·유도", "riposte-obc", "상태기계, 안전 감시(SM-1~SM-10), 유도 법칙, MAVSDK Offboard 스트리밍"],
        ["L5 건전성", "riposte-supervisor", "버스 신선도 감시, 블랙박스 기록·회전"],
    ], [3.0, 4.0, 9.0]),

    ("h", 1, "2. 안전 불변식 (최상위 제약)"),
    ("p_bold", "다음 불변식은 모든 기능 요구사항에 우선한다. 충돌 시 불변식이 이긴다."),
    ("table", ["ID", "불변식", "의미"], [
        ["I1", "오퍼레이터 인증 게이트", "탐지·AI 판단만으로 상위 임무 명령이 활성화되지 않는다. 인증 토큰을 동반한 오퍼레이터 명령이 필요하다."],
        ["I2", "인지–비행제어 격리", "인지 프로세스는 FC에 절대 쓰지 않는다. 인지 장애는 발행 중단으로만 나타나고 제어 계층이 SM-7로 판단한다."],
        ["I3", "안전 감시 계층 불변", "SM-1~SM-10은 우회·비활성화되지 않는다. 감시가 성립하지 않으면 세션을 시작하지 않는다."],
        ["I4", "비신뢰 장치 데이터", "NPU 출력, 임베딩, V4L2 프레임, MAVLink, 프로세스 간 버스 데이터는 모두 검증 후 사용한다."],
        ["I5", "조용한 기하 오류 방지", "정규화 크기는 단안 거리로 직결되므로 채널·좌표계 변환 시 크기까지 함께 변환한다."],
        ["I6", "AI 계층 무결 강등", "보조 추적 AI가 전부 실패해도 기본 추적으로 강등되어 동작한다."],
    ], [1.3, 4.2, 10.5]),

    ("h", 1, "3. 기능 요구사항 — 임무·탐지 (R 계열)"),
    ("table", ["ID", "요구사항", "상태"], [
        ["R-1", "GCS 명령으로 이륙 및 임무 수행", "구현"],
        ["R-2", "GCS가 대상의 방위·거리(·속도)를 제공하면 예상 위치로 선도 비행(만남점 해석)", "구현"],
        ["R-3", "300 m 거리에서 대상 드론 탐지", "협각 채널 실장 완료, 실기 검증 대기"],
        ["R-4", "IMX568 광각 + IMX568 협각 2채널 EO 구성", "구현"],
        ["R-5", "광각 리사이즈 탐색 → 미탐지 시 3×3 타일 순차 탐색 → 탐지 시 해당 영역 중심 협조 탐지", "구현"],
        ["R-6", "최근 5프레임 중 80% 이상(≥4/5) 동일 대상 탐지 시 추적 모드 전환", "구현"],
        ["R-7", "추적 중 주기적 재탐지로 대상 재확인", "구현"],
        ["R-8", "다중 대상 시 가장 큰(=가장 가까운) 대상 우선 추적, 대상 개수 발행", "구현"],
        ["R-9", "대상이 화면 중심에 오도록 비행 조정(수평 yaw + 수직 pitch)", "구현"],
        ["R-10", "적응형 추론 배분: 탐색 시 광각·협각 교차, 추적 확정 시 협각 우선 + 광각 주기 재탐지. 근거리(≤150 m) 협각 상향", "구현"],
        ["R-11", "2채널 교차 확인으로 검출 정확도 증대 — 두 채널이 같은 대상을 각자 검출하면 확인을 가속하고 신뢰도를 올린다. 누락을 늘리지 않는 방향으로만 작동", "구현"],
        ["R-12", "채널별 정밀도 우선순위 — 추적 확정 후 시선·크기 표본은 각분해능이 약 3.8배인 협각을 우선 사용", "구현"],
    ], [1.3, 12.2, 2.5]),

    ("h", 2, "3.1 이중 EO 채널 요구사항"),
    ("ul", [
        "캡처는 두 채널 모두 60 fps로 고정하고, 추론 스케줄만 적응한다. 비행 중 센서 모드 전환은 금지한다.",
        "공용 좌표계는 광각 정규화 좌표이며, 협각 검출 결과는 중심과 크기를 모두 광각 좌표로 환산한 뒤 추적기·추정기에 입력한다.",
        "협각 시야를 벗어난 대상에 대해서는 해당 슬롯을 광각으로 폴백한다.",
        "협각 카메라가 열리지 않으면 이중 EO 모드로 기동하지 않는다(광각 단독 동작 금지).",
    ]),
    ("p_bold", "정확도 향상의 기전 — 시차(스테레오)가 아니다"),
    ("p", "두 카메라는 네스티드 동축·광축 평행 배치(LENS-REQ §2.2)이므로 유효 기선이 사실상 0이고, "
           "따라서 삼각측량 기반 거리는 성립하지 않는다. R-11/R-12가 노리는 정확도 향상은 "
           "① 협각의 높은 각분해능에 따른 시선·중심 정밀도, ② 두 채널의 교차 확인에 의한 오검출 "
           "억제에서 나온다. 거리는 여전히 단안 크기 기반이며 그 불확실성은 EST-2가 정량화한다."),

    ("h", 1, "4. 기능 요구사항 — 추적 (TR 계열)"),
    ("table", ["ID", "요구사항", "상태"], [
        ["TR-1", "유사 대상 교차 시 트랙 ID 유지(ID 스위치 방지)", "구현(실모델 벤치 대기)"],
        ["TR-2", "탐지 플리커·부재 프레임에서 시선 연속 유지(시각 coast)", "구현"],
        ["TR-3", "AI 추적 계층 장애 시 기본 추적으로 무결 강등", "구현"],
        ["TR-4", "NPU 작업이 60 Hz 파이프라인을 블록하지 않을 것", "구현(기한 격리 워커)"],
        ["TR-5", "템플릿 단독 출력은 품질 강등으로 구분 발행", "구현"],
        ["TR-6", "AI 탐지가 항상 권위 — 템플릿은 대체·교차검증 용도로만 사용", "구현"],
        ["TR-7", "확인 윈도우와 템플릿 앵커는 트랙 정체성에 결합", "구현"],
    ], [1.3, 12.2, 2.5]),

    ("h", 1, "5. 기능 요구사항 — 상태 추정 (EST 계열)"),
    ("table", ["ID", "요구사항", "상태"], [
        ["EST-1", "이동 대상의 상대 위치·속도를 잡음에 강인하게 추정", "구현"],
        ["EST-2", "거리 추정의 불확실성을 정량화(단안 크기 기반의 한계를 은폐하지 않음)", "구현"],
        ["EST-3", "자기 기체 운동을 반영해 대상 고유 운동을 분리", "구현"],
        ["EST-4", "기동 대상에 대한 다중 모델 추정", "구현(IMM)"],
        ["EST-5", "대상 예측 경로 산출(유도 리드·접근점 입력)", "구현"],
        ["EST-6", "추정 실패·저품질을 안전 계층에 전파", "구현"],
    ], [1.3, 12.2, 2.5]),

    ("h", 1, "6. 안전 요구사항 (SM 계열)"),
    ("p", "안전 감시는 제어 주기마다 평가되며, 활성 세션 중 위반이 검출되면 즉시 이탈 절차로 "
           "전이한다. 감시 항목은 비활성화할 수 없다(I3)."),
    ("table", ["ID", "감시 항목", "판정 기준"], [
        ["SM-1", "텔레메트리 신선도(필드별)", "위치·속도 500 ms 초과, 저주기 스트림은 각 스트림 스탬프 기준 초과"],
        ["SM-2", "외부 모드 변경", "진입 후 비행 모드가 Offboard가 아니거나, 활성 중 모드 스트림 정지"],
        ["SM-3", "지오펜스(소프트)", "홈 기준 수평 반경 / 고도 상한 / 고도 하한(이륙 후 무장)"],
        ["SM-4", "속도·자세 클램프", "지령 속도 및 자세각·추력의 상한 강제(최종 방어선)"],
        ["SM-5", "제어 주기 지터", "3회 연속 ±20% 초과"],
        ["SM-6", "disarm 감지", "armed 플래그 해제(신선한 스트림 기준)"],
        ["SM-7", "트랙 신선도·품질", "활성 중 탐지-앵커 트랙이 임계 초과로 노후"],
        ["SM-8", "세션 타임박스", "세션 시작 후 경과 시간이 설정값 초과"],
        ["SM-9", "배터리 게이트", "시작: 잔량 미달·판독 불가·스테일 시 거부 / 비행 중: 하한 미만 시 이탈"],
        ["SM-10", "폴리곤 경계", "설정된 폴리곤 외부. 폴리곤 설정 시 신선한 위치 정보 없으면 시작 거부"],
    ], [1.3, 4.0, 10.7]),
    ("h", 2, "6.1 설정 무결성 요구사항"),
    ("ul", [
        "안전 관련 시간·거리 설정은 형변환 이전 단계에서 부호와 범위를 검증하고, 위반 시 기동을 거부한다(값 보정 금지).",
        "런타임에서 소비하는 모든 설정 키는 검증 구조를 거쳐 한 번만 파싱한다.",
        "요구된 기능을 제공할 수 없는 구성(검출기 부재, 협각 카메라 부재 등)에서는 축소 동작이 아니라 기동 거부를 택한다.",
    ]),

    ("h", 1, "7. 인터페이스 요구사항"),
    ("table", ["인터페이스", "내용"], [
        ["GCS ↔ 미션 컴퓨터", "MAVLink(SiK 433 MHz). 임무 명령은 오퍼레이터 인증 토큰을 동반한다(I1)."],
        ["미션 컴퓨터 ↔ FC", "MAVSDK Offboard(속도/자세 지령), 텔레메트리 구독. 인지 프로세스는 이 경로에 접근하지 않는다(I2)."],
        ["프로세스 간(내부)", "공유메모리 seqlock 버스: 추적 상태·인지 건전성·제어 상태·기록용 좌표. 수신 측은 모든 필드를 검증한다(I4)."],
        ["카메라", "V4L2 MIPI-CSI 2채널(광각/협각), NV12, 60 fps 고정."],
        ["가속기", "Hailo-8(검출), RGA(2D 전처리), RK NPU(보조 추적). 벤더 경로는 컴파일 옵션으로 격리한다."],
    ], [4.0, 12.0]),

    ("h", 1, "8. 비기능 요구사항"),
    ("table", ["구분", "요구사항"], [
        ["성능", "인지 파이프라인 60 fps 유지, 카메라 노출부터 추적 상태 발행까지 종단 지연 100 ms 미만"],
        ["실시간 데이터 경로", "카메라 → 전처리 → AI 검출 → 후처리 경로에서 불필요한 복사를 두지 않는다. 캡처는 드라이버 버퍼 매핑으로 복사 없이 수행하고, 전처리는 2D 가속기 경로를 둔다. 종단 DMA 버퍼 공유는 목표이며 브링업에서 확정한다(§10 참조)"],
        ["실시간성", "제어 주기 20 Hz 고정, 주기 지터 ±20% 이내(SM-5)"],
        ["가용성", "보조 AI 실패 시 기본 추적으로 계속 동작(I6), 인코더·기록 실패가 인지 루프를 정지시키지 않음"],
        ["이식성", "벤더 SDK 부재 환경에서도 호스트 빌드·시험이 성립할 것(옵션 OFF 기본값)"],
        ["검증성", "안전 판정 로직은 순수 함수로 분리해 호스트 단위시험으로 고정"],
        ["관측성", "채널 배분·강등·이탈 사유가 로그와 블랙박스에 남을 것(무증상 실패 금지)"],
    ], [3.0, 13.0]),
    ("p_bold", "실시간 데이터 경로 — 요구의 근거"),
    ("p", "1280×720 NV12 프레임은 약 1.3 MB이고 60 fps이므로 복사 한 번이 초당 약 80 MB의 "
           "메모리 대역을 소모한다. 이 대역은 검출 가속기의 PCIe DMA, 보조 추적 NPU, 2채널 캡처와 "
           "직접 경합하므로, 복사 제거는 성능 최적화가 아니라 실시간성 확보의 전제다."),

    ("h", 1, "9. 검증 방법 및 추적성"),
    ("table", ["검증 단계", "대상", "수단"], [
        ["호스트 단위시험", "순수 로직(안전 판정, 설정 검증, 좌표 사상, 추정 필터)", "ctest 24종 + 새니타이저(ASan/UBSan/TSan)"],
        ["SIL", "인지 파이프라인 전 구간, 이중 EO 배분", "합성 카메라·검출기 프로파일"],
        ["PX4 SITL", "상태기계, 안전 감시 결함 주입, 유도 추종", "SITL 시나리오 3종"],
        ["Gazebo", "물리 시뮬 이동 대상 접근·회피·순찰", "시나리오 3종"],
        ["실기 브링업", "드라이버·장치 포맷·지연 실측·열·전원·비행", "RIPOSTE-BRINGUP-001 체크리스트 B0~B8"],
    ], [3.2, 6.3, 6.5]),
    ("p_bold", "요구사항 ↔ 근거 문서 추적성"),
    ("table", ["요구사항 계열", "근거 문서"], [
        ["R-x, T-x", "RIPOSTE-DUALEO-REQ-001"],
        ["TR-x, K-x", "RIPOSTE-TRACKER-REQ-001"],
        ["EST-x", "RIPOSTE-ESTIMATION-REQ-001"],
        ["S-x (인지 설계)", "modules/RIPOSTE-SEEKER-SDD-001 (실시간 데이터 경로 = S-14)"],
        ["SM-x, I1~I6", "RIPOSTE-OBC-SDD-001, AGENTS.md"],
        ["광학·렌즈", "RIPOSTE-LENS-REQ-001"],
        ["실기 검증", "RIPOSTE-BRINGUP-001"],
    ], [4.5, 11.5]),

    ("h", 1, "10. 미결 사항 및 가정"),
    ("table", ["구분", "내용"], [
        ["미결", "종단 zero-copy(DMA 버퍼 fd 공유) — 드라이버·2D 가속기·검출 런타임 세 계층의 지원 여부에 달려 있어 브링업에서 확정. 현 시점 보장 범위는 드라이버→앱 구간까지"],
        ["미결", "R-11 완화폭·R-12 가중값 실측 튜닝 — 실모델 오검출률과 잔차 통계 확보 후"],
        ["미결", "PX4 및 MAVSDK 버전 확정"],
        ["미결", "AI 실모델 브링업 — 검출 모델 입출력 포맷, 보조 추적 모델 지연 실측"],
        ["미결", "광각↔협각 정렬 오프셋 교정값 확정"],
        ["미결", "300 m 탐지(R-3) 실측 검증"],
        ["가정", "제품 표준 검출 모델은 NMS 내장 컴파일이며 입력은 RGB888/NHWC/uint8"],
        ["가정", "보조 추적 모델의 프레임당 지연은 5 ms 이하"],
        ["보류", "열영상(IR) 채널 — 미래 검토. 파이프라인은 채널 불문 구조를 유지"],
    ], [2.5, 13.5]),

    ("p_note", "본 문서는 리포지토리의 승인 설계문서에서 도출된 요약이며, 상세 설계·근거·개정 "
               "이력은 각 원 문서를 따른다. 상충 시 원 문서가 우선한다."),
]

if __name__ == "__main__":
    d = docgen.out_dir()
    docgen.emit(BLOCKS,
                os.path.join(d, "RIPOSTE-SRS-001_요구사항정의서.md"),
                os.path.join(d, "RIPOSTE-SRS-001_요구사항정의서.docx"))

# --------------------------------------------------------------------------
# English edition. Same document, same structure — docgen.check_parallel below
# fails generation if a section exists in one language only, because a
# translation that silently drifts is worse than no translation.
BLOCKS_EN = [
    ("h", 0, "RIPOSTE UAV Mission Computer — Software Requirements Specification"),
    ("p", "RIPOSTE-SRS-001 · Revision 0.2 · 2026-08-17"),
    ("table", ["Item", "Content"], [
        ["Document ID", "RIPOSTE-SRS-001"],
        ["Revision / date", "0.2 (Draft) / 2026-08-17"],
        ["History", "0.2: R-11/R-12 (dual-channel detection accuracy) and the real-time data path NFR / 0.1: initial issue"],
        ["Target system", "Software stack on the RK3588 UAV mission computer (Riposte POC)"],
        ["Hardware", "RK3588 (perception + control) · Hailo-8 (detection NPU) · RK NPU (assisted tracking) · Pixhawk 6X/PX4 (FC) · SiK 433 MHz (GCS link)"],
        ["Source documents", "RIPOSTE-SAD-001, RIPOSTE-DUALEO-REQ-001, RIPOSTE-TRACKER-REQ-001, RIPOSTE-ESTIMATION-REQ-001, RIPOSTE-LENS-REQ-001, RIPOSTE-OBC-SDD-001"],
        ["Terminology", "Neutral software-engineering vocabulary (RIPOSTE-SAD-001 §0)"],
        ["Generated by", "docs/tools/make_srs.py (the content lives only in that script)"],
    ], [3.5, 12.5]),

    ("h", 1, "1. Purpose and scope"),
    ("p", "This document specifies the software running on the Riposte UAV mission computer. "
           "Every requirement is derived from an approved design document in the repository, and "
           "§9 traces each family back to that source and to the means of verification."),
    ("h", 2, "1.1 Scope"),
    ("ul", [
        "In scope: L2 perception (riposte-seeker), L3/L4 control and guidance (riposte-obc), L5 health and recording (riposte-supervisor).",
        "Out of scope: the PX4 flight-control firmware itself, the GCS application, and the mechanical/optical hardware design (specified separately in RIPOSTE-LENS-REQ-001).",
        "Out of scope: the STM32H7 sub-board — outside the POC, to be reintroduced if the safety interlock is promoted to hardware.",
    ]),
    ("h", 2, "1.2 System overview"),
    ("p", "The software takes off on an authenticated GCS command, detects and tracks a target with a "
           "dual EO (wide/narrow) camera pair, and steers the airframe from the estimated relative state. "
           "The perception layer never writes to the flight controller; every control output reaches the "
           "FC only after passing the safety-monitor layer."),
    ("table", ["Layer", "Process", "Responsibility"], [
        ["L2 perception", "riposte-seeker", "Capture, preprocessing, AI detection, post-processing, multi-target tracking, monocular relative-state estimation, TrackBus publication"],
        ["L3/L4 control", "riposte-obc", "State machine, safety monitors (SM-1..SM-10), guidance law, MAVSDK Offboard streaming"],
        ["L5 health", "riposte-supervisor", "Bus freshness monitoring, blackbox recording and rotation"],
    ], [3.0, 4.0, 9.0]),

    ("h", 1, "2. Safety invariants (top-level constraints)"),
    ("p_bold", "These invariants outrank every functional requirement. Where they conflict, the invariant wins."),
    ("table", ["ID", "Invariant", "Meaning"], [
        ["I1", "Operator authorization gate", "Detection or AI judgement alone never activates a mission command. An operator command carrying the configured token is required."],
        ["I2", "Perception/flight-control isolation", "The perception process never writes to the FC. A perception fault appears only as stalled publication, which the control layer judges through SM-7."],
        ["I3", "Safety-monitor layer is inviolable", "SM-1..SM-10 are never bypassed or disabled. If monitoring cannot be established, the session does not start."],
        ["I4", "Untrusted device data", "NPU outputs, embeddings, V4L2 frames, MAVLink and inter-process bus payloads are validated before use."],
        ["I5", "No silent geometry errors", "Normalized size feeds monocular range directly, so a channel or frame conversion must carry the size with the centre."],
        ["I6", "Graceful AI degradation", "If the assisted-tracking AI fails completely, tracking degrades to the motion-only baseline and keeps working."],
    ], [1.3, 4.2, 10.5]),

    ("h", 1, "3. Functional requirements — mission and detection (R series)"),
    ("table", ["ID", "Requirement", "Status"], [
        ["R-1", "Take off and fly the mission on GCS command", "Implemented"],
        ["R-2", "Given a target bearing/range (and velocity) from the GCS, fly to the predicted meeting point", "Implemented"],
        ["R-3", "Detect a target drone at 300 m", "Narrow channel in place, awaiting hardware verification"],
        ["R-4", "Two EO channels: IMX568 wide + IMX568 narrow", "Implemented"],
        ["R-5", "Wide resized pass first; on no detection sweep a 3x3 tile grid; on detection concentrate both channels on that region", "Implemented"],
        ["R-6", "Enter tracking mode when the same target is detected in at least 80% (>=4/5) of the recent frames", "Implemented"],
        ["R-7", "While tracking, re-detect periodically to re-prove the target", "Implemented"],
        ["R-8", "With multiple targets, track the largest (nearest) first and publish the target count", "Implemented"],
        ["R-9", "Steer so the target stays centred in frame (yaw for bearing, pitch for elevation)", "Implemented"],
        ["R-10", "Adaptive inference allocation: alternate wide/narrow while searching; favour narrow once tracking is confirmed, with periodic wide re-detection; raise the narrow share inside 150 m", "Implemented"],
        ["R-11", "Raise detection accuracy by cross-channel confirmation — when both channels detect the same target, accelerate confirmation and raise confidence. It may only lower the bar, never raise it", "Implemented"],
        ["R-12", "Channel precision priority — once tracking is confirmed, prefer narrow samples for line-of-sight and size (about 3.8x the angular resolution)", "Implemented"],
    ], [1.3, 12.2, 2.5]),

    ("h", 2, "3.1 Dual EO channel requirements"),
    ("ul", [
        "Both channels capture at a fixed 60 fps; only the inference schedule adapts. Switching sensor mode in flight is prohibited.",
        "The shared coordinate frame is wide-channel normalized coordinates; a narrow detection is converted — centre AND size — before it reaches the tracker or estimator.",
        "If the target lies outside the narrow field of view, that slot falls back to the wide frame.",
        "If the narrow camera does not open, dual-EO mode does not start (running wide-only is prohibited).",
    ]),
    ("p_bold", "The accuracy mechanism is not stereo"),
    ("p", "The cameras are nested-coaxial with parallel optical axes (LENS-REQ §2.2), so the effective "
           "baseline is essentially zero and triangulated range does not exist. What R-11 and R-12 exploit "
           "is (a) the narrow channel's higher angular resolution, giving finer bearing and centroid "
           "precision, and (b) cross-channel agreement, which suppresses false positives. Range remains "
           "monocular and size-based, and EST-2 quantifies its uncertainty."),

    ("h", 1, "4. Functional requirements — tracking (TR series)"),
    ("table", ["ID", "Requirement", "Status"], [
        ["TR-1", "Hold track identity when similar targets cross (no ID switch)", "Implemented (bench against the real model pending)"],
        ["TR-2", "Maintain line-of-sight continuity through detection flicker and missing frames (visual coast)", "Implemented"],
        ["TR-3", "Degrade gracefully to the motion-only baseline when the AI tracking layer fails", "Implemented"],
        ["TR-4", "NPU work must not block the 60 Hz pipeline", "Implemented (deadline-isolated worker)"],
        ["TR-5", "Publish template-only output distinctly, with degraded quality", "Implemented"],
        ["TR-6", "AI detection is always authoritative; the template is only a substitute and cross-check", "Implemented"],
        ["TR-7", "Bind the confirmation window and the template anchor to the track identity", "Implemented"],
    ], [1.3, 12.2, 2.5]),

    ("h", 1, "5. Functional requirements — state estimation (EST series)"),
    ("table", ["ID", "Requirement", "Status"], [
        ["EST-1", "Estimate relative position and velocity of a moving target robustly against noise", "Implemented"],
        ["EST-2", "Quantify range uncertainty — do not hide the limits of monocular size-based range", "Implemented"],
        ["EST-3", "Account for own-vehicle motion so the target's own motion is separated", "Implemented"],
        ["EST-4", "Multiple-model estimation for a manoeuvring target", "Implemented (IMM)"],
        ["EST-5", "Produce a predicted target path for the guidance lead / meeting point", "Implemented"],
        ["EST-6", "Propagate estimation failure and low confidence to the safety layer", "Implemented"],
    ], [1.3, 12.2, 2.5]),

    ("h", 1, "6. Safety requirements (SM series)"),
    ("p", "The monitors are evaluated every control tick. Any violation during an active session transitions "
           "immediately to the exit sequence. No monitor can be disabled (I3)."),
    ("table", ["ID", "Monitor", "Criterion"], [
        ["SM-1", "Telemetry freshness (per field)", "Position/velocity older than 500 ms; each low-rate stream judged against its own arrival stamp"],
        ["SM-2", "External mode change", "Flight mode is not Offboard after entry, or the mode stream stalls while active"],
        ["SM-3", "Soft geofence", "Horizontal radius from home, altitude ceiling, altitude floor (armed after take-off)"],
        ["SM-4", "Velocity and attitude clamp", "Hard bounds on commanded velocity, attitude angle and thrust (final defensive line)"],
        ["SM-5", "Control period jitter", "Three consecutive periods beyond ±20%"],
        ["SM-6", "Disarm detection", "Armed flag cleared (judged on a fresh stream)"],
        ["SM-7", "Track freshness and quality", "The detection-anchored track ages past the threshold while active"],
        ["SM-8", "Session timebox", "Elapsed time since session start exceeds the configured bound"],
        ["SM-9", "Battery gate", "Start: refused when remaining capacity is low, unreadable or stale. In flight: exit below the floor"],
        ["SM-10", "Polygon boundary", "Outside the configured polygon. With a polygon configured, entry is refused without a fresh position fix"],
    ], [1.3, 4.0, 10.7]),
    ("h", 2, "6.1 Configuration integrity requirements"),
    ("ul", [
        "Safety-related time and distance settings are validated for sign and range BEFORE conversion, and a violation refuses startup (values are never silently corrected).",
        "Every configuration key consumed at runtime is parsed once, through the validated structure.",
        "Where a configuration cannot deliver what it asks for (missing detector, missing narrow camera), startup is refused rather than reduced.",
    ]),

    ("h", 1, "7. Interface requirements"),
    ("table", ["Interface", "Content"], [
        ["GCS <-> mission computer", "MAVLink over SiK 433 MHz. Mission commands carry the operator authorization token (I1)."],
        ["Mission computer <-> FC", "MAVSDK Offboard (velocity/attitude setpoints) and telemetry subscriptions. The perception process has no access to this path (I2)."],
        ["Inter-process (internal)", "Shared-memory seqlock buses: track state, perception health, control status, recording coordinates. Every field is validated by the reader (I4)."],
        ["Cameras", "Two V4L2 MIPI-CSI channels (wide/narrow), NV12, fixed 60 fps."],
        ["Accelerators", "Hailo-8 (detection), RGA (2D preprocessing), RK NPU (assisted tracking). Vendor paths are isolated behind compile options."],
    ], [4.0, 12.0]),

    ("h", 1, "8. Non-functional requirements"),
    ("table", ["Category", "Requirement"], [
        ["Performance", "Sustain 60 fps in the perception pipeline; under 100 ms end to end from camera exposure to track publication"],
        ["Real-time data path", "No unnecessary copies between camera, preprocessing, AI detection and post-processing. Capture maps the driver's buffers without copying and preprocessing has a 2D-accelerator path; end-to-end DMA buffer sharing is the target and is settled at bring-up (§10)"],
        ["Timing", "Fixed 20 Hz control period with jitter within ±20% (SM-5)"],
        ["Availability", "Assisted-AI failure leaves the baseline tracker running (I6); encoder or recording failure never stalls the perception loop"],
        ["Portability", "Host build and test must succeed without any vendor SDK present (options default off)"],
        ["Verifiability", "Safety decisions are isolated as pure functions and pinned by host unit tests"],
        ["Observability", "Channel allocation, degradation and exit causes appear in the log and blackbox (no symptomless failure)"],
    ], [3.0, 13.0]),
    ("p_bold", "Why the real-time data path is a requirement"),
    ("p", "A 1280x720 NV12 frame is about 1.3 MB, so at 60 fps a single copy costs roughly 80 MB/s of memory "
           "bandwidth. That bandwidth is contended by the detection accelerator's PCIe DMA, the assisted-tracking "
           "NPU and two capture streams, which makes copy removal a precondition for real-time behaviour rather "
           "than an optimization."),

    ("h", 1, "9. Verification and traceability"),
    ("table", ["Stage", "Subject", "Means"], [
        ["Host unit tests", "Pure logic: safety decisions, configuration validation, coordinate mapping, estimation filters", "24 ctest suites plus ASan/UBSan/TSan"],
        ["SIL", "The whole perception pipeline and dual-EO allocation", "Synthetic camera and detector profile"],
        ["PX4 SITL", "State machine, safety-monitor fault injection, guidance tracking", "Three SITL scenarios"],
        ["Gazebo", "Closure, evasion and patrol against a physically simulated moving target", "Three scenarios"],
        ["Hardware bring-up", "Drivers, device formats, measured latency, thermals, power, flight", "RIPOSTE-BRINGUP-001 checklist B0..B8"],
    ], [3.2, 6.3, 6.5]),
    ("p_bold", "Requirement family to source document"),
    ("table", ["Family", "Source document"], [
        ["R-x, T-x", "RIPOSTE-DUALEO-REQ-001"],
        ["TR-x, K-x", "RIPOSTE-TRACKER-REQ-001"],
        ["EST-x", "RIPOSTE-ESTIMATION-REQ-001"],
        ["S-x (perception design)", "modules/RIPOSTE-SEEKER-SDD-001 (real-time data path = S-14)"],
        ["SM-x, I1..I6", "RIPOSTE-OBC-SDD-001, AGENTS.md"],
        ["Optics and lenses", "RIPOSTE-LENS-REQ-001"],
        ["Hardware verification", "RIPOSTE-BRINGUP-001"],
    ], [4.5, 11.5]),

    ("h", 1, "10. Open items and assumptions"),
    ("table", ["Type", "Content"], [
        ["Open", "End-to-end zero copy (DMA buffer sharing) — depends on driver, 2D accelerator and detection runtime all supporting it; settled at bring-up. What is guaranteed today is the driver-to-application segment"],
        ["Open", "R-11 relaxation width and R-12 weighting — to be tuned once real-model false-positive rates and per-channel residual statistics exist"],
        ["Open", "PX4 and MAVSDK versions to be fixed"],
        ["Open", "AI model bring-up — detection model I/O format, assisted-tracking latency measurement"],
        ["Open", "Wide/narrow alignment offset calibration"],
        ["Open", "Verification of 300 m detection (R-3) against real hardware"],
        ["Assumption", "The production detection model is compiled with NMS on device and takes RGB888/NHWC/uint8 input"],
        ["Assumption", "The assisted-tracking model costs no more than 5 ms per frame"],
        ["Deferred", "Thermal (IR) channel — a future consideration; the pipeline stays channel-agnostic"],
    ], [2.5, 13.5]),

    ("p_note", "This document is derived from the approved design documents in the repository. Detailed "
               "design, rationale and revision history live in those documents, and they take precedence "
               "where the two disagree."),
]

if __name__ == "__main__":
    docgen.check_parallel(BLOCKS, BLOCKS_EN)
    _d = docgen.out_dir()
    docgen.emit(BLOCKS_EN,
                os.path.join(_d, "RIPOSTE-SRS-001_Software_Requirements_Specification.md"),
                os.path.join(_d, "RIPOSTE-SRS-001_Software_Requirements_Specification.docx"))
