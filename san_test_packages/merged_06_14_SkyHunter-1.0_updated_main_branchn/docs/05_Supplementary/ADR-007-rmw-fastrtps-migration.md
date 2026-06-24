# ADR-007 — RMW Migration from CycloneDDS to FastDDS (DCN-2026-014)

> **Status**: Accepted
> **Date**: 2026-05-26
> **Decider**: 김태근 (PM, ㈜스카이오토넷)
> **Consulted**: Gong (combat_nav2 commit `48b0cef`, 2026-05-20), Limon
> (sim hardening 2026-05-14 report), Choi (Nav2 CPU regression analysis)
> **Supersedes**: 부분적으로 [[ADR-006]] §4 (RMW pick) — IPC substrate
> 자체는 그대로 ROS 2 DDS; RMW 구현체만 교체.
> **Related**: DCN-2026-004 (CycloneDDS LTE-restriction work, 이제
> deprecated 폴더로 이동), DCN-2026-012 v2 (Limon sim hardening, 이 ADR
> 위에서 시작), DCN-2026-015 (combat_nav2 PR-5 C++ 적용).

---

## 1. 컨텍스트 (Context)

### 1.1 트리거

| # | 사건 | 영향 |
|---|---|---|
| 1 | Gong, 커밋 `48b0cef` (2026-05-20) | `combat_nav2`만 단독으로 FastDDS로 전환. 5대 중 1대만 다른 RMW → 군집 내 cross-RMW 토픽 호환성 미보증 상태. |
| 2 | Limon, 2026-05-14 sim 리포트 | 2/3-robot Gazebo 시나리오에서 CycloneDDS SPDP가 Docker 브릿지를 막혀 TF 트리 비어 있음 + lifecycle_manager hang. |
| 3 | DCN-2026-004 D-010 운영 경험 | CycloneDDS의 `<NetworkInterface>` 인터페이스 화이트리스트가 wlan0/eth0 단순 case는 처리 OK 였으나, Docker 환경 + multi-NIC 조합에서 잘못된 인터페이스 선택이 반복됨. 매번 `<Peers>` 수동 갱신. |

이 3건을 함께 보면 "robot마다 RMW가 다른 상태"가 점점 굳어지므로,
한쪽으로 통일이 필요. 두 후보 중 FastDDS로 정렬하는 결정.

### 1.2 현 사용 현황 (이 ADR 작성 시점)

| 위치 | RMW |
|---|---|
| `san_bringup/systemd/san-squadron.service` | `rmw_cyclonedds_cpp` (default) |
| `infra/docker/cyclonedds/cyclonedds_sbc{1,2}.xml` | `rmw_cyclonedds_cpp` (Docker run env) |
| `combat_nav2` (커밋 `48b0cef`) | `rmw_fastrtps_cpp` |
| 나머지 4 robots 코드/launch | 명시 없음 → 시스템 디폴트 = CycloneDDS |

### 1.3 네트워크 환경 (Gong/Choi confirmation, 2026-05-23)

| 항목 | 값 |
|---|---|
| Mesh interface | `mesh0` (Wi-Fi 6 + WPA3, **EasyMesh** standard) |
| Topology | Multi-hop relay (TTL ≤ 3) |
| Multicast | EasyMesh 상 안정 — discovery 가속에 `239.255.0.1:7400` 사용 |
| IP 할당 | 정적, `192.168.50.0/24` (전장 DHCP 없음) |
| Robot IP 매핑 | Leader=.10, Hub#1=.20, Hub#2=.21, Deputy=.30, Follower 1-5=.40~.44, Aban Android=.100 |
| MTU 마진 | 1400 (1500 Ethernet − IP 20 − UDP 8 − RTPS 32 − mesh hdr 40) |

---

## 2. 결정 (Decision)

**5대 robot 전부 `rmw_fastrtps_cpp`로 통일한다.** 추가로:

1. 프로파일 XML 2개를 san_bringup에 신규 작성:
   * `fastrtps_profile.xml` (production — SHM intra-host + UDPv4
     **mesh0** EasyMesh interface + 9-robot initialPeersList +
     metatraffic multicast `239.255.0.1:7400` + leaseDuration 10s +
     maxMessageSize 1400 + TTLBufferSize 3 / 자세한 근거는 §4.2 참조)
   * `fastrtps_sim_profile.xml` (simulation — 127.0.0.1 only)
2. Phase-7 Discovery Server 프로파일은 *준비*만 (`fastrtps_discovery_server.xml`,
   opt-in, 기본 비활성).
3. systemd unit과 ROS 2 XML launch 양쪽에서 `RMW_IMPLEMENTATION`을
   **명시적으로 export** — 시스템 디폴트에 절대 의존하지 않음.
4. CycloneDDS XML 2개는 **이동(보존)**, 삭제하지 않음. 회수 절차는
   `infra/docker/cyclonedds/deprecated/README.md`.
5. **정적 IP 할당 자동화** (Item 8): `infra/network/static_ip_mesh0.sh`
   가 `/etc/skyautonet/{robot_id,sbc_id}`를 읽어 `192.168.50.0/24`
   대역의 결정론적 IP를 mesh0에 할당. 5개 systemd unit 전부
   `ExecStartPre=`로 호출 — 전장에 DHCP 없는 환경 대응.

---

## 3. 검토한 대안 (Considered Alternatives)

### A. 현 상태 유지 (CycloneDDS 기본 + combat_nav2만 FastDDS) ❌
- **장점**: 무작업.
- **단점**: cross-RMW 토픽 호환성이 `rmw_cyclonedds_cpp` ↔
  `rmw_fastrtps_cpp` 간 100% 보증되지 않음 (특히 QoS 매칭, Discovery).
  Limon이 보고한 sim 문제는 그대로. 운영자가 매번 어느 robot이 어느
  RMW인지 추적해야 함.

### B. 전체 CycloneDDS로 정렬 (combat_nav2를 다시 CycloneDDS로 되돌림) ⚠️
- **장점**: 운영 자산(D-010 XML, runbook)이 그대로 살아남음.
- **단점**: Gong이 발견한 combat_nav2 문제(56% CPU, Nav2
  controller_server CRITICAL FAILURE — DCN-2026-015 참조)는
  CycloneDDS 쪽 issue로 의심되는 정황. Limon의 sim 문제도 그대로.
  Apache 2.0 (FastDDS) → EPL 2.0 (CycloneDDS) 라이센스 변경은 매번
  검토 필요.

### C. **FastDDS로 통일 (이 ADR의 결정)** ✅
- **장점**:
  * 라이센스 단일화 (Apache 2.0).
  * Phase-7 Discovery Server 옵션이 standalone 제품으로 제공됨
    (CycloneDDS는 동등 기능이 없음, 직접 broker 작성 필요).
  * SHM intra-host 트랜스포트가 built-in (CycloneDDS는 별도 plugin).
  * Ubuntu apt 패키지로 deploy (이미 `ros-humble-rmw-fastrtps-cpp` 설치 기본).
- **단점**:
  * D-010 XML 자산 폐기 (deprecated/로 이동).
  * 1-host RTT가 CycloneDDS ~50µs → FastDDS ~70µs로 약간 증가 (KPP-1
    baseline은 여전히 KPP 요구 250ms 이내에 큰 여유).

---

## 4. 결과 (Consequences)

### 4.1 긍정

* 5대 robot 전부 동일 RMW + 동일 프로파일 → cross-robot 호환성 보증.
* `combat_nav2`가 다른 robot과 토픽 교환 시 RMW 매핑 issue 자동 해소.
* Discovery Server가 Phase-7 옵션으로 사용 가능 → 군집 확장 시
  multicast 대역 saturation 회피 경로 확보.
* sim 시나리오 (Limon)에서 `avoid_builtin_multicast=true` +
  `metatrafficUnicastLocatorList=127.0.0.1`로 Docker/CI 환경에서도
  TF 트리가 정상 구성됨.

### 4.2 부정 / 비용

* DCN-2026-004 D-010이 만든 CycloneDDS XML 자산은 **즉시 폐기되지
  않고**, deprecated/ 폴더에 emergency rollback 경로로 한 sprint
  보존. 그 기간 동안 두 위치에서 RMW 설정이 존재하므로 운영자
  실수 가능 → README + ADR-007 양쪽 명시.
* KPP-1 1-host RTT가 ~50µs → ~70µs로 약 40% 증가 (실측은
  `san_rtk_gnss/src/measure_kpp1_latency_node.cpp` (D-055)로 측정).
  KPP-2 (300ms E2E budget)와 KPP-1 (250ms 군집 응답)에는 영향 없음 —
  여유의 0.01% 미만.
* `squadron.launch.py` 등 6개 Python launch 파일을 ROS 2 XML로 전환
  필요 (C++ only 정책의 연쇄 영향, 같은 sprint에서 처리).

### 4.3 위험 (Risk)

* **R-1**: FastDDS의 `<interfaceWhiteList>` 동작이 CycloneDDS의
  `<NetworkInterface>`와 같은 의미인지 — Limon이 sim 통합 후 KPP-2
  E2E 시 확인 (DCN-2026-012 v2 의 gtest T8에 포함).
* **R-2**: SHM transport의 `segment_size=16 MiB`가 Hub SBC #2의
  `/dev/shm` 한도와 충돌 가능성 — 초기 boot 시 `df -h /dev/shm`
  monitoring 추가 (DCN-2026-011 D-035 install.sh에 한 줄 추가).
* **R-3**: Discovery Server가 Phase-7에서 활성화될 때까지 그 프로파일
  XML이 "검증 안 됨"으로 남음. opt-in이므로 production에 영향 없으나
  활성화 직전 별도 dry-run 필수.

---

## 5. Rollback Procedure

`infra/docker/cyclonedds/deprecated/README.md` 의 "Rollback" 섹션 참조.
요약:

1. 모든 robot에서 `sudo systemctl stop 'skyautonet-*.service'`.
2. `/etc/skyautonet/<robot>.env` 에서 `RMW_IMPLEMENTATION=rmw_cyclonedds_cpp` +
   `CYCLONEDDS_URI=file:///opt/san/infra/docker/cyclonedds/deprecated/cyclonedds_legacy_sbc<N>.xml`.
3. `unset FASTRTPS_DEFAULT_PROFILES_FILE`.
4. `sudo systemctl daemon-reload && sudo systemctl start 'skyautonet-*.service'`.
5. `ros2 doctor --report | grep -i middleware` → `rmw_cyclonedds_cpp` 확인.

Rollback 기준: 7일 연속 KPP-1 또는 KPP-2 budget 위반이 RMW에
기인하는 것으로 합리적 의심될 때. 그 외에는 fix-forward.

---

## 6. 검증 (Verification)

* 신규 gtest `test_fastrtps_profile_selection.cpp` 의 8 케이스 통과
  (san_bringup `colcon test`).
* `ros2 doctor --report` 5대 robot 모두 `rmw_fastrtps_cpp` 보고.
* `measure_kpp1_latency_node` 60초 측정 시 p95 ≤ 100µs (1-host) /
  p95 ≤ 5ms (cross-robot mesh).
* Limon Scenario A/B 시나리오 (DCN-2026-012 v2 gtest 6 케이스) green.

---

## 7. 참조 (References)

* Gong, commit `48b0cef` (2026-05-20): `combat_nav2`의 CycloneDDS →
  FastDDS 단독 전환. 이 ADR이 다른 4 robot으로 확장.
* DCN-2026-014: 본 ADR의 implementation ticket (D-050 ~ D-055).
* DCN-2026-004 D-010 (PR #148): CycloneDDS LTE multicast 제한 (이제
  deprecated/cyclonedds_legacy_sbc{1,2}.xml).
* [[ADR-006]] IPC Unification on ROS 2: 본 ADR이 그 §4 RMW 선택을
  부분적으로 supersede.
* FastDDS 공식 문서 — XML profile schema:
  https://fast-dds.docs.eprosima.com/en/latest/fastdds/xml_configuration/
