# CI workflow templates

PR 작업 권한이 `workflow` scope 를 갖지 않을 때, 새 CI workflow 는
일단 본 디렉터리에 template 으로 ship 되고, repo admin 이 직접
`.github/workflows/` 로 복사해야 한다.

## 적용 방법

```bash
# repo admin (workflow scope 있음) 만 실행
cp docs/ci_templates/gate1-regression.yml.template \
   .github/workflows/gate1-regression.yml
git add .github/workflows/gate1-regression.yml
git commit -m "ci: enable Gate-1 regression workflow (DCN-2026-022)"
git push origin main
```

## 현재 template 목록

| Template | DCN | 설명 |
|---|---|---|
| `gate1-regression.yml.template` | DCN-2026-022 | L5_26~L5_33 Gate-1 acceptance suite — PR + workflow_dispatch 트리거, JUnit XML 업로드, dorny/test-reporter publish |
| `coverage.yml.template` | 2026-05-24 | Coverage Measurement workflow `sh→bash` fix — pre-existing `source: not found` (exit 127) 인프라 버그 복구. `defaults.run.shell: bash` top-level 패치만 추가, 나머지 step body 동일. |

## coverage.yml fix (2026-05-24)

기존 `.github/workflows/coverage.yml` 의 cpp-coverage job 이 `source /opt/ros/humble/setup.bash` 단계에서 매 commit `exit code 127` 로 실패하고 있었음. Root cause: GitHub Actions Linux 기본 shell 이 `sh -e {0}` 이고 `source` 는 bash builtin 이라 sh 에서 not found.

Fix: workflow top-level 에 `defaults: { run: { shell: bash } }` 한 줄 추가. 나머지 step body 는 동일.

```bash
# repo admin sync
cp docs/ci_templates/coverage.yml.template .github/workflows/coverage.yml
git add .github/workflows/coverage.yml
git commit -m "ci(coverage): fix sh→bash so source builtin works"
git push origin main
```
