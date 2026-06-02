# Changelog

본 파일은 [SkyHunter](https://github.com/adasone/SkyHunter-1.0) 의 주요
변경 사항을 정리한다. 형식은 [Keep a Changelog](https://keepachangelog.com/ko/1.1.0/)
관례를 따르며, 버전 체계는 [SemVer](https://semver.org/lang/ko/) 를 따른다.

릴리즈는 GitHub Releases 페이지에서 확인:
https://github.com/adasone/SkyHunter-1.0/releases

---

## [Unreleased]

(v1.5.5 다음 sprint 의 변경분이 여기에 누적.)

---

## [v1.5.5] — 2026-05-24

> **Sprint 결과**: v1.5.4 6-DCN cross-audit 결과 (37 findings, 21
> fixes + 6 historical documented) + Gate-1 CI workflow 도입 +
> DCN-2026-024 WiFi auth gate (DEV_TEST mode) 묶음 minor release.
>
> **GitHub release**:
> https://github.com/adasone/SkyHunter-1.0/releases/tag/v1.5.5

### v1.5.4 6-DCN cross-audit follow-up fixes (P1 + P2 + P3 모두 완료)

- **fix(comm): DCN-019 audit B1+B10** (PR #207) — sliding-window dedup
  correctness (replay vulnerability) + sender pending mutex (MTE race)
- **fix(safety): DCN-016 audit A1+A5** (PR #208) — Gate-1 E-Stop
  non-blocking RTH dispatch + SCOPE_LEADER_ONLY uses authoritative
  `is_leader_role_` flag (was robot_id_==1 proxy)
- **fix(L5): DCN-022 audit C1-C5** (PR #209) — `Outcome::SKIP` enum +
  topic-presence false-positives → SKIP + L5_30 reset_home_pose=false +
  elapsed_ms real measurement + JUnit `<skipped/>` emission
- **chore(docs): audit D1+D5** (PR #210) — `docs/CI_GUIDE.md` BLE row
  cleanup + `DCN-2026-024_WiFi_Auth_Follow_Up.md` proposed DCN
- **fix(thread-safety): P2-1 A2+A3+A7+B2** (PR #215) — DemoSequencer
  isEnabled lock + watchdog CAS reset + FireSim gimbal mutex +
  McProtocol state_mu_
- **fix(comm): P2-2 B5+B7** (PR #218) — receiver crc 중복 제거 +
  sender INVALID_CHECKSUM 즉시 timeout (retransmit 1.5s 낭비 제거)
- **fix(L5): P2-3 B15** (PR #219) — McStressScenario `--seed N`
  deterministic replay 옵션
- **chore(audit): P3 cleanup** (PR #220) — 11 minor refactor
  (FireSim sentinel/주석, MC seq wrap doc, late-ack debug log,
  reorder queue drain, JUnit attr length cap) + 6 historical 문서화

### CI workflow stabilization
- **PR #212 / #213 / #216 / #217** — Gate-1 Regression workflow 도입
  + `xmllint` → `libxml2-utils` + `safe.directory` + dorny
  `fail-on-empty=false` + workflow `permissions: { contents:read,
  checks:write, pull-requests:write }`. Final GREEN PASS verified
  via `gh workflow run` (run 26352795009).

### Added

- **DCN-2026-024 — WiFi auth gate for DEV_TEST (Option C, this PR)** —
  DCN-2026-023 v2 가 BLE PIN auth 를 제거하면서 남긴 DEV_TEST
  진입 인증 공백을 매움. `OperationalModeController` 에
  shared-secret token 검증 추가:
  - ctor 인자 → `SAN_DEV_TEST_SECRET` env → `/etc/san/dev_test_secret`
    파일 → 없으면 fail-closed
  - `request_mode(mode, auth_token="")` — DEV_TEST 시 only,
    `secrets.compare_digest`
  - 다른 모드 backward-compatible — `auth_token` 무시
  - 8 새 pytest cases (gate + resolution priority + fail-closed +
    no-leak)

### Pending

- (없음 — v1.5.4 cycle + audit + DCN-024 모두 완료)

---

## [v1.5.4] — 2026-05-24

> **Sprint 결과**: 154-1 spec v1.5.4 Sprint 6 phase 모두 완료.
> 6개 DCN (016 / 018 / 019 / 020 / 022 / 023 v2) main 적용, Phase 5
> 검증 PASS (38/38 packages build / **381/381 gtest PASS**), tag 발행.
>
> **GitHub release**:
> https://github.com/adasone/SkyHunter-1.0/releases/tag/v1.5.4

### Added

- **DCN-2026-019 — MC ACK/retransmit protocol** (`san_operation_control` +
  `combat_robot_msgs`)
  - `MCMessage.msg` + `MCAck.msg` — wire-level seq + crc32 envelope.
    higher-level MissionStateCommand 등과 분리된 reliability 계층.
  - `mc_protocol_node` (receiver) — `/mc/command` 구독, boost::crc_32_type
    검증, 16-deep sliding window dedup + OUT_OF_ORDER 필터, 합격된 명령
    을 `/mc/command_validated` 로 republish.
  - `mc_sender_node` (sender) — `/mc/raw_command` 구독, seq + crc32
    stamp 후 `/mc/command` publish, 500 ms ack timeout + 3 retry,
    초과 시 `/mc/timeout` 발행.
  - 8 gtest cases (pure-logic via evaluateForTest / stampForTest seam).

- **DCN-2026-020 — MC stress test scenario** (`san_l5_regression`)
  - `McStressScenario` — 1 kHz 부하 + drop/dup/reorder 합성 noise
    injection, RTT p50/95/99 측정, CSV 출력.
  - 합격: p99 < 50 ms (Gate-1 KPP).
  - `regression_main --scenario mc_stress` dispatch.
  - 3 gtest cases — summarize() 결정 로직 (empty / within target /
    over target).

- **DCN-2026-022 — L5_26~L5_33 Gate-1 acceptance suite** (`san_l5_regression`)
  - 8 시나리오: Deputy boot / RTK lock / Costmap rate / Nav2 waypoint
    / RTH accuracy (/rth action) / E-Stop response / Mission BT loop
    / Gate-1 demo E2E (/gate1/start_demo).
  - 각 시나리오는 live dependency 없으면 graceful FAIL (crash 없음).
  - `renderJunitXml()` — pure-logic JUnit XML emitter (jest-junit
    compatible).
  - `regression_main --scenario gate1_suite` 가 8 모두 실행 + JUnit
    출력. `--scenario L5_NN` 로 단일 실행.
  - `.github/workflows/gate1-regression.yml` — PR + workflow_dispatch
    트리거, dorny/test-reporter@v1 로 결과 publish.
  - 6 gtest cases — Gate1Junit emitter pure-logic 검증.

- **DCN-2026-016 — Gate-1 demo ROS integration** (`san_operation_control`)
  - `/gate1/start_demo` (`std_srvs/Trigger`) — operator-facing service.
    deployment_mode ∈ {DEMO, LAB_TEST} 일 때만 accept.
  - `/gate1/demo_status` (`combat_robot_msgs/OperationState`) — 5 s
    publisher. operator_banner 가 현재 DemoPhase 를 carry.
  - `/emergency_stop` (`combat_robot_msgs/EmergencyStop`) subscriber —
    scope (ALL / SINGLE_ROBOT / LEADER_ONLY) 필터 후 DemoSequencer
    disable + `/rth` ActionGoal 발사.
  - 5 s watchdog tick → 60 s 동안 phase 변화 없으면 `/rth` fallback.
  - DemoSequencer phase callback wiring → RTB 진입 시 자동 `/rth`.
  - `gate1_demo.launch.xml` (operation_control_node + san_rth 합성).
  - 6 gtest cases (T1-T6) — service accept/reject + status pub +
    estop scope filter + RTB auto-trigger.

- **DCN-2026-018 — Fire simulator (co-located)** (`san_fire_authorization`)
  - `fire_simulator_node` — `/swarm/fire/authorization_response` 구독,
    `/gimbal/pan_tilt_state` (`sensor_msgs/JointState`) 구독,
    `/swarm/fire/result` (`combat_robot_msgs/FireResult`) 발행.
  - 정렬 평가: HIT iff `|pan_err| < tol AND |tilt_err| < tol` (기본 2°).
  - `FireResult` 의 actual IDS §4.6 schema 사용
    (`result` enum / `impact_point_{x,y}_m` / `rounds_fired` /
    `authorization_chain` / `timestamp_{fire,report}_ms`).
  - 5 gtest cases — granted+aligned → SUCCESS / denied → NO_AUTHORIZATION
    / misaligned → MISS / gimbal cache / FireResult field 완성.
  - 가공된 가짜 무기 발사 없음 — `FireResult` schema 호환만 보장.

### Changed

- **DCN-2026-023 v2 — BLE 완전 제거** (PIN auth + archive 포함)
  - `san_mission/operational_modes.py` — PIN auth dead code 전부 제거:
    `requires_pin` 필드, `_pin_authenticated`, `set_pin_authenticated()`,
    `is_pin_authenticated()`, `request_mode()` 의 PIN gate 블록 모두 삭제.
  - DEV_TEST 진입 시 PIN gate 없어짐 — **보안 영향**: WiFi-기반
    인증은 별도 DCN 으로 추가 예정.
  - `san_mission/__init__.py` + `mission_node.py` — stale BLE 주석 정리.
  - `test_operational_modes{,_patch}.py` — PIN auth 가정 test 제거,
    thread-safe coverage (PO1) 유지.
  - `docs/PDR/{PKG,MSG,ARCH}-001` — `san_ble_control` 행 / `BleCommand`
    + `BlePhaseStatus` 메시지 / `T1_BLE` 노드 / "BLE Controller" 표기
    모두 삭제 + footnote 추가.

### Removed

- `archive/v15_python_prototype/` — 6 BLE 파일 git rm:
  - `control/ble_control_process.py`
  - `control/_aban_reference/ble_sim.{c,h}`
  - `tests/blesim_tcp.py`
  - `tests/test_ble_control.py`
  - `tests/test_ble_gatt_api.py`

### Fixed

- `san_fire_authorization/src/fire_authorization_node.cpp` — `OperationState.msg` 에
  없는 `hub_term` / `leader_term` 필드 참조 build error 수정 (msg schema 에
  실제 존재하는 `hub_robot_id` / `leader_robot_id` 와 혼동). audit slot 0
  으로 처리, msg 가 term 필드를 추가하면 복원 예정.

### Pending

- (없음) — v1.5.4 sprint Phase 4 INTEGRATION 6/6 DCN 완료. Phase 5
  (full validation) + Phase 6 (v1.5.4 tag + GitHub release) 후속.

---

## [Pre-v1.5.4 baseline] — main HEAD `5b45569` 이전 history

상세 변경 history 는 git log 참조:
```bash
git log --oneline 5b45569
```

주요 작업 묶음:

### v1.5 PDR-prep + R-series deep-dives (2026-05-12 ~ 2026-05-24)
- R-3 san_sim_gazebo_helpers (#105) — drone target + GPS disturbance
- R-4 san_formation (#107) — frame transform + heading prediction
- R-5 san_surveillance (#108) — world-frame sectors
- R-6 san_follower_tier (#109) — dt-aware 5-Tier FSM
- R-7 san_fire_authorization (#110/#192/#193) — safety-critical auth + audit
- R-8 san_reroute_planner (#111) — PNG decode + lethal guards
- R-9 comm-modules (#112) — san_comm_link + san_comm deprecates LinkSelector
- R-10 san_role_management (#113) — non-blocking grace + split-brain hardening
- R-13 san_mission (#115) — BT semantic + thread safety
- R-14 san_perception (#116) — Python deep-dive (stub safety, timestamp)
- R-15 san_hub_comm + san_lte_redundancy (#117) — comm deep-dive
- R-16 san_cameras (#119) — parameter override, atomics, timestamp validation

### Phase 0 (operator/sensor/driver safety) — 2026-05-22 ~ 2026-05-24
- PR-A san_fire_authorization (#118) — KEY1↔KEY2 binding, audit fail-closed
- PR-B sensor stub safety (#120) — IMU/cameras/RTK/LRF
- PR-C driver/actuation safety (#121) — Unitree stub, cmd_vel watchdog, pan-tilt clamp
- PR-D operator command auth interim (#122)

### Phase 1 sensor stub surfacing (#123)
- `~/stub_status` latched topic 으로 downstream 이 stub 여부 인지 가능.

### Phase 5 / Phase 6 misc residual fixes
- Phase 5 hub health hysteresis + bounded clock skew (#127)
- Phase 6 threat_aggregator double-publish + tier_node KPP-2 (#128)
- D-018 + D-019 human_detector postprocess hardening (#200)
- R-1 cppcheck cleanup (#196)
- v1.5 PDR sw verification posture (#197) — mission_node B1 wiring +
  coverage/sanitizers CI + S20-7b

### Infrastructure
- DCN-2026-014 v2 (#179) — FastDDS + EasyMesh unification
- DCN-2026-013 (#180) — swarm_monitor_node + Hub-only /swarm/poses gate
- DCN-2026-012 v2 (#178) — multi-robot sim hardening
- DCN-2026-021 (#182) — Aban Android rosbridge schema + mock
- DCN-2026-022 (#183) — san_test Gate-1 regression (L5_26~33)
- ADR-008 (#177) — Tier-based language policy
- DCN-2026-017 (#177) — san_rth RTH action server (Tier 1 C++)

---

## 참조

- **DCN log**: `docs/04_Change_Management/DCN_log.md` (예정)
- **PDR 산출물**: `docs/PDR/SAN-PDR-*.md`
- **ADR**: `docs/05_Supplementary/ADR-*.md`
- **GitHub Issues**: https://github.com/adasone/SkyHunter-1.0/issues
- **Release page**: https://github.com/adasone/SkyHunter-1.0/releases
