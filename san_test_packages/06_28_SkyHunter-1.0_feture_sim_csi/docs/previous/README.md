# App Integration Docs Guide

## 목적

`docs/app_integration` 안의 문서들은 내용이 일부 중복된다. 이 폴더는 아래 규칙으로 관리한다.

- 기준 문서: 먼저 수정해야 하는 문서
- 파생 문서: 기준 문서 내용을 요약/분리/번역한 문서
- 생성 산출물: 스크립트로 만든 결과물, 직접 수정하지 않는 문서

중복 내용이 충돌하면 `app_unified_spec.md`를 우선한다.

## 문서 분류

### 1. 기준 문서

- `app_unified_spec.md`
  - 앱 연동 전체 기준 문서
  - 모드, 시나리오, 패킷, 상태, 예외 처리까지 한 곳에서 관리
  - 제품 기준 정책이 바뀌면 이 문서를 먼저 수정한다

### 2. 파생 문서

- `app_interface_spec.md`
  - 패킷/채널/상태 DTO만 따로 보는 기술 문서
  - `app_unified_spec.md`의 인터페이스 파트를 분리한 뷰

- `app_fsm.md`
  - 상태 전이와 버튼 매핑만 따로 보는 문서
  - `app_unified_spec.md`의 FSM 파트를 분리한 뷰

- `operation_scenario.md`
  - 운용 절차 중심 문서
  - `app_unified_spec.md`의 운용 시나리오 파트를 분리한 뷰

- `app_dev_spec_en.md`
  - 영문 개발자 가이드
  - 한국어 기준 문서를 영어로 풀어쓴 파생 문서

### 3. 중복이 큰 참고 문서

- `app_integration_scenario.md`
  - `operation_scenario.md`와 `app_unified_spec.md`의 화면/운용 흐름과 많이 겹친다
  - 신규 내용 추가 대상보다는 비교/검토용 참고 문서로 유지한다
  - 같은 항목이 다르면 `app_unified_spec.md`와 `operation_scenario.md` 기준으로 맞춘다

### 4. 생성 산출물

- `app_unified_spec.docx`
- `app_dev_spec_en.docx`

위 두 파일은 직접 수정하지 않는다. 아래 스크립트로 다시 생성한다.

- `generate_app_unified_docx.py`
- `generate_app_dev_spec_en_docx.py`

## 권장 수정 순서

1. `app_unified_spec.md`를 먼저 수정한다.
2. 필요한 경우 `app_interface_spec.md`, `app_fsm.md`, `operation_scenario.md`를 동기화한다.
3. 영문 문서가 필요하면 `app_dev_spec_en.md`를 반영한다.
4. 최종 배포본이 필요할 때 DOCX를 재생성한다.

## 이번 정리 기준

현재 폴더는 아래처럼 이해하면 된다.

- 제품 기준 원문: `app_unified_spec.md`
- 개발용 분리 문서: `app_interface_spec.md`, `app_fsm.md`, `operation_scenario.md`
- 중복 참고 문서: `app_integration_scenario.md`
- 영문 파생 문서: `app_dev_spec_en.md`
- 배포 산출물: `*.docx`
