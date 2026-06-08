# CI 블록 회복 절차 (Standard Operating Procedure)

**문서번호**: SOP-CI-001
**발행일**: 2026-05-13
**개정 이력**:
- Rev.A (2026-05-13) — 최초 발행
- Rev.B (2026-05-14) — §3 "Infra fail masking 방지" 신규 추가, §8 사례 2 추가 (PR #144 → #151 사건)

**적용 대상**: SkyHunter 프로젝트의 모든 main / develop 브랜치 CI 블록 사건
**최초 발행 사유**: 2026-05-13 DCN-2026-003 sprint 첫날 san_hub_slam 빌드 에러로 PR #137~#140 일시 블록

---

## 1. 적용 범위

본 절차는 다음 상황에서 호출된다:

- main / develop 브랜치의 CI 가 30분 이상 red
- 진행 중 PR 1건 이상이 main CI 블록으로 진행 불가
- hotfix PR (이하 #FIX) 가 발행되어 CI 재확인 대기 중

본 절차의 목적: **대기 시간 낭비 방지 + root cause 검증 + 재실행 흐름 표준화**.

---

## 2. 즉시 액션 (CI 자동 재확인 대기 윈도우 활용)

### Step 1: PR #FIX 의 root cause 검증 (≤ 3분)

```bash
# PR body 확인
gh pr view <FIX_PR> --json title,body

# diff 패턴 확인
gh pr diff <FIX_PR> --patch | head -200

# 실패한 CI run 의 로그
gh run list --branch=main --status=failure --limit=3
gh run view <run_id> --log-failed | grep -A20 '<failing_package>'
```

**판단 매트릭스**:

| Diff 패턴 | 해석 | 조치 |
|---|---|---|
| 패키지 의존성 추가 (`<depend>`) | root cause | ⭕ merge |
| 누락된 헤더 / 코드 import 추가 | root cause | ⭕ merge |
| API 변경 동기 (caller 측 수정) | root cause | ⭕ merge |
| `-Wno-error=*` 추가, 또는 fatal → warning 변경 | workaround | ⚠ merge + tech debt 등록 |
| `if (skip_test) return;` 같은 가드 추가 | workaround | ⚠ merge + tech debt 등록 |
| 패키지 자체 비활성화 (`COLCON_IGNORE`) | escape hatch | ❌ 거부, 재작업 |

### Step 2: 의존 PR 의 영향 분석 (≤ 4분)

```bash
# 블록된 PR 들이 #FIX 와 같은 파일을 건드리는지
for n in <blocked PR ids>; do
    echo "=== PR #$n ==="
    gh pr diff $n --name-only
done | sort -u

# 충돌 잠재성 사전 점검
gh pr view <FIX_PR> --json files --jq '.files[].path' > /tmp/fix_files.txt
for n in <blocked PR ids>; do
    gh pr view $n --json files --jq '.files[].path' | comm -12 - /tmp/fix_files.txt
done
```

**판단**:
- 공통 파일 없음 → rebase 자동 OK
- 공통 파일 있음 → 수동 merge 위험, 사전 분석 필요

### Step 3: 블록된 PR rebase 준비 (≤ 3분)

```bash
# 각 PR 의 head branch 에 main 의 변경 반영 명령 사전 준비
for n in <blocked PR ids>; do
    echo "gh pr update $n   # auto rebase 또는"
    echo "git fetch origin && git checkout pr-$n-branch && git rebase origin/main"
done > /tmp/rebase_commands.sh
```

### Step 4: Standup / 사내 통지 (≤ 1분)

- Slack `#dev-standup` 채널에 1줄 통지: "main CI red, PR #FIX 머지 대기 (예상 +12분). 블록 영향 PR: #137~#140."
- STANDUP_LOG 의 당일 표에 블로커 칸 명시.

---

## 3. Infra fail masking 방지 (신규 — 2026-05-14 추가)

**원칙**: CI 가 RED 인 상태에서는 머지 금지. 인프라 결함처럼 보이는 RED 도 *진짜* 결함을 마스킹하고 있을 가능성이 있다는 전제로 다룬다.

### 3.1 적용 상황

PR 의 canonical CI gate (`colcon build + test (Humble)`, `TST S20 series`) 가 RED 인데 — 로그를 보니 그 RED 가 다음 중 하나로 보이는 경우:

- 시스템 의존성 누락 (`libpython3-dev not found`, `apt-get` 응답 없음)
- 네트워크 타임아웃 (rosdep init, 외부 mirror)
- Runner 환경 결함 (cache miss, runner OOM, fork bomb 등)
- 다른 PR 의 진행 중인 hotfix 와 동일 root cause

표면적으로는 "내 PR 의 content 문제가 아니다" 인 것 같아 보이지만, 실제로 **CI 가 그 결함 이후의 build/test 단계에 도달하지 못한 채 fail 보고**하고 있을 수 있다. 그 build/test 단계에서 PR 의 content-level regression 이 잡혔어야 하는데 못 잡힌 채 PR 이 green 처럼 보이게 된다.

### 3.2 의무 절차

인프라 결함으로 보이는 RED 발견 시:

1. **Issue 등록** — 인프라 결함 자체를 별도 티켓으로 분리 (예: `#142 main CI: san_integration_tests requires libpython3-dev`).
2. **인프라 fix PR 발행** + merge (예: `#145 fix(ci): install libpython3-dev`).
3. **원래 PR 의 CI 재실행** — `gh pr checks <N> --watch`. 같은 시점의 워크플로우 재실행이 필요하면 `gh pr comment <N> --body "/ci"` 또는 manual rerun.
4. **재실행 결과가 green 임을 직접 확인한 후에만 머지.**

3-step 만 충족하고 4-step (green 확인) 을 건너뛰는 것이 본 룰의 핵심 금지사항이다.

### 3.3 절대 금지사항

- ❌ "이 RED 는 infra 결함이라 OK" 라는 사후 추정으로 머지
- ❌ "다른 PR 도 같은 fail 이라 공통 infra 결함이 분명함" 으로 우회
- ❌ CMake / colcon 의 `Could NOT find <package>` 메시지가 *왜* 발생했는지 확인 없이 system apt install 만 추가
- ❌ CI 가 사실은 RED 인데 mergeable status 만 보고 머지

### 3.4 진단 보강 (2026-05-14 incident 의 추가 교훈)

`Could NOT find PythonLibs` 같은 CMake-from-CMake 호출 체인 fail 은 **두 가지 별개 원인**이 똑같은 메시지를 낼 수 있다:

1. 시스템 패키지 누락 (`libpython3-dev` 미설치)
2. CMake 측 결함 (`project(... LANGUAGES NONE)` 같은 호출자 측 misconfig — toolchain 자체가 활성화 안 되어 있어 detect 실패)

원인 (1) 만 고치고 끝내면, 원인 (2) 가 동일 메시지로 재현된다 ("fix 가 안 됐다" 처럼 보임 → SOP 위반의 가장 흔한 패턴). 진단할 때:

- 실패한 `find_package` 호출이 어떤 CMake 모듈에서 시작되었는지 stack trace 확인 (CMake 가 출력해 줌)
- 실패한 package 의 `CMakeLists.txt` 의 `project()` 라인을 직접 확인 — `LANGUAGES NONE` / `LANGUAGES C` 등 toolchain 한정자가 있는지
- runner 상태에 대한 추정 (예: "apt 가 늦었나 보다") 보다 **로그의 정확한 줄 + CMake module path** 를 신뢰

### 3.5 위반 사례

| 사건 | 일자 | 원인 분류 | 영향 |
|---|---|---|---|
| PR #144 (D-006 SwarmAggregator mutex) 머지 | 2026-05-14 | 본 SOP §3.2 step 4 미실행 (CI 가 RED 인데 "infra 결함이라 OK" 추정 → 머지) | main RED, 5개 후속 PR 차단 + hotfix #151 발행 |

추가로 발견된 점 (위 진단 보강 §3.4 의 출처):
- libpython3-dev 부재 (#142) 와 `san_integration_tests/CMakeLists.txt` 의 `project(... NONE)` 가 동일한 `Could NOT find PythonLibs` 메시지로 표시되어 #145 의 부분 fix 후에도 RED 상태가 지속.

---

## 4. CI 재확인 결과별 분기

### 3.1 PASS (PR #FIX 통과)

```
PR #FIX merge   (Tech Lead, hotfix 절차로 PM 사후 승인)
   ↓ main 안정화 ~5분 대기
   ↓
블록된 PR 들 자동 rebase (gh pr update 또는 GitHub 의 update-branch)
   ↓ 각 PR CI 재실행 (병렬 ~15-20분)
   ↓
원래 머지 순서대로 진행
```

### 3.2 FAIL (PR #FIX 통과 못 함)

```
30분 분석 회의 소집 (PM + Tech Lead + #FIX author + 의심되는 코드 owner)
   ↓
2가지 결정:
   1. PR #FIX amend (추가 commit push, 다시 CI 대기)
   2. PR #FIX close + 신규 PR (작업 0.5~1d)
```

**회의 의제**:
- 실제 root cause 가 맞는가? 다른 원인 가능성?
- 다른 잔존 빌드 에러 동시 발견되었나?
- workaround 로 unblock 후 후속 처리할 가치가 있는가?

### 3.3 FLAKY (다른 원인 실패, 환경 문제 등)

```
1회 재실행 (`gh run rerun <run_id>`)
   ↓
재차 실패 시 → 3.2 (FAIL) 진입
PASS 시 → 3.1 (PASS) 진입
```

---

## 5. 머지 후 정리 작업

1. PR #FIX 가 workaround 였다면 → tech debt 티켓 (예: `#tech-debt: san_hub_slam build fragility`) 등록
2. STANDUP_LOG 의 다음 일자 표에 "CI 블록 회복 완료, X분 슬립" 기록
3. 본 사건의 root cause 가 다른 PR (#129 같은) 의 부분 적용에서 비롯되었다면 → retrospective 의제에 추가

---

## 6. 일정 영향 평가

| 시나리오 | 슬립 | Demo Day buffer 영향 |
|---|---|---|
| 3.1 PASS | +30~40분 | 무시 가능 |
| 3.2 FAIL → amend | +1~2시간 | 무시 가능 |
| 3.2 FAIL → 재작업 | +0.5~1d | buffer 흡수 |
| 3.3 FLAKY → PASS | +20분 | 무시 가능 |
| 3.3 FLAKY → FAIL | 3.2 와 동일 | — |

Demo Day buffer 가 30일 이상이라면 모든 시나리오는 일정 risk 가 아님. 침착하게 root cause 검증에 집중.

---

## 7. 회피 권고 (Don't)

| 항목 | 사유 |
|---|---|
| ❌ CI 결과 대기 중 블록된 PR 를 force-push 로 우회 | main CI 깨진 채로 PR 머지 시 도미노 효과 |
| ❌ `--no-verify` 또는 `[skip ci]` 로 hotfix 우회 | 본질 fix 미반영, 다른 PR 까지 오염 |
| ❌ PR #FIX 의 root cause 검증 없이 즉시 merge | workaround 인 경우 동종 사건 재발 |
| ❌ CI 블록 동안 사내 통지 생략 | PM / Tech Lead 가 블로커 인지 못 함 |
| ❌ rebase 결과 검증 없이 자동 머지 | rebase conflict 의 silent resolution 으로 결함 유입 |

---

## 8. 본 절차의 적용 사례

### 8.1 사례 1 — san_hub_slam 빌드 에러

| 항목 | 값 |
|---|---|
| 사건 일자 | 2026-05-13 |
| 사건 명 | san_hub_slam 빌드 에러로 PR #137~#140 블록 |
| Hotfix PR | #141 (`fix(san_hub_slam): expose filterDeltaTopics for test`) |
| 회복 시간 | 약 30분 |
| 시나리오 | A (PASS — PR #141 머지 후 즉시 회복) |
| Root cause | san_hub_slam test 가 internal API 에 접근하는데 해당 함수가 export 되어 있지 않음 |
| 후속 조치 | 없음. 본 SOP 의 §2~§4 절차가 의도대로 작동 |

### 8.2 사례 2 — D-006 SwarmAggregator + libpython3-dev / project(NONE) 복합 (본 SOP §3 위반)

| 항목 | 값 |
|---|---|
| 사건 일자 | 2026-05-14 |
| 사건 명 | PR #144 (D-006 SwarmAggregator mutex) 머지 후 main RED, 5개 후속 PR 블록 |
| Hotfix PR | #151 (`fix: setter + san_integration_tests language`) |
| 회복 시간 | (사후 기재 — 약 1~2시간 추정) |
| 시나리오 | C (SOP §3 위반 — infra-RED CI 인 채로 #144 머지) |
| Root cause | 1) `SwarmAggregator` 에 추가된 `std::mutex` 가 implicit `operator=` 를 deleted 로 만들어 `hub_orchestrator_node.cpp:18` 의 reassign 가 컴파일 안 됨 2) `san_integration_tests/CMakeLists.txt` 의 `project(... NONE)` 가 launch_testing_ament_cmake 의 PythonLibs detect 를 차단해 같은 `Could NOT find PythonLibs` 메시지를 libpython3-dev fix 후에도 재현 |
| 후속 조치 | (a) 본 §3 (Infra fail masking 방지) 신규 추가. (b) Issue #152 retrospective. (c) sprint instruction §4 decision matrix 의 "infra fail OK" 룰 강화 검토 |

— 끝 —
