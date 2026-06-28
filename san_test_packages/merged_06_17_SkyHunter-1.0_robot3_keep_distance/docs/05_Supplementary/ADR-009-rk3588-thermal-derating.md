# ADR-009 — RK3588 열 등급 derating 대응 (EM3588 일반등급 채택)

> **Status**: **Proposed** — 열 시험 데이터 + PM 결정 대기
> **Date**: 2026-06-15
> **Decider**: 김태근 (PM, ㈜스카이오토넷)
> **Consulted**: 공기종 (보드 교체 공지 2026-06-15)
> **Related**: DCN-2026-027 (플랫폼 이관 — O-1), [[ADR-001]] (Hub dual-SBC), BOM v1.1
> **Trigger**: DCN-2026-027 O-1 — 메인보드가 Custom RK3588J → Boardcon EM3588 으로
> 교체되며 탑재 실리콘이 **일반등급 RK3588 (비-J)** 로 확정됨.

---

## 1. 컨텍스트 (Context)

DCN-2026-027 O-1 에서 Boardcon EM3588 의 SoC 가 **일반등급 RK3588** 임이 확정
(PM 결정). 기존 Custom 보드는 산업용 **RK3588J** 를 가정했으므로, 운용 온도
등급이 derating 된다:

| | 동작 온도(권장) | 비고 |
|---|---|---|
| RK3588J (산업용, 기존 가정) | 약 **−40 ~ +85 ℃** | 옥외 전장 envelope 포함 |
| RK3588 (일반, EM3588 실탑재) | 약 **0 ~ +80 ℃** | **저온측이 결정적 리스크** |

> ⚠ 정확한 수치는 Boardcon EM3588 / Rockchip RK3588 데이터시트로 확정 필요
> (junction vs ambient 기준 구분 포함).

### 1.1 운용 리스크

- **저온 cold-start (−20 ℃ 동계 등)**: 일반등급 하한 0 ℃ 미만에서 boot/안정성
  미보증. SkyHunter 는 옥외 무인 군집이라 동계·고지대 배치 시 직접 노출.
- **고온 + 부하**: RK3588 풀로드(8C + NPU, ~10–15 W)는 밀폐 enclosure 내부를
  주변보다 15–25 ℃ 상승시킴 → 주변 +50 ℃ 면 내부 65–75 ℃ 로 80 ℃ 상한에 근접,
  thermal throttling/재부팅 위험.
- **영향 범위**: Leader(Go2 내장 제외) 외 **Hub ×2 SBC + Deputy ×2 + Follower ×5**
  전 노드. Hub 듀얼 SBC 는 좁은 공간 2-보드 동거라 고온측이 특히 취약([[ADR-001]]).

## 2. 결정 (Decision) — *제안*

**열 시험으로 운용 envelope 를 실측한 뒤 확정한다.** 시험 전 잠정 권고는
대안 D(Hybrid). 시험 결과가 envelope 요구를 미충족하면 대안 A 로 에스컬레이트.

핵심: **데이터 없이 등급을 확정하지 않는다** — EM3588 실보드 thermal 챔버
시험(§5)이 결정의 전제.

## 3. 검토한 대안 (Considered Alternatives)

### A. 산업용 SKU 채택 (RK3588J) ⚠️ (비용/납기)
- EM3588 의 RK3588J SKU(Boardcon 제공 여부 확인) 또는 동급 산업용 RK3588J 보드로 교체.
- **장점**: −40~85 ℃ 복원, enclosure 단순. **단점**: 단가↑, 납기(B2B) 리스크,
  보드 재선정 시 DCN-2026-027 일부 재작업.

### B. 능동 열관리 enclosure (RK3588 유지)
- IP67 enclosure + **저온 히터**(cold-start) + 방열(히트싱크/팬) + 서모스탯.
- **장점**: 보드 유지, 기존 BOM 보드선정 불변. **단점**: enclosure BOM·전력 예산↑,
  팬=고장점/IP등급 도전, 히터 워밍업 지연.

### C. 운용 envelope derate (소프트 한정)
- 배치 온도를 **0~50 ℃ 주변**으로 제한 + SW thermal throttling 가드 + 운용교범 명시.
- **장점**: 무비용. **단점**: 동계/혹서 임무 제약 — 방산 요구사항과 충돌 가능.

### D. **Hybrid (잠정 권고)** ✅
- RK3588 유지 + **passive 히트싱크 표준** + **cold-start 히터(저온 지역 한정 옵션)** +
  **SW thermal 가드**(`thermal_zone` 감시 → throttle/안전정지) 를 **시험으로 검증된
  envelope 내에서** 운용. 시험 미충족 영역만 대안 A 로 국소 에스컬레이트.
- **장점**: 비용/리스크 균형, 데이터 기반 단계 확장. **단점**: 시험·검증 공수.

## 4. 결과 (Consequences)

- BOM v1.1 SBC 항목은 DCN-2026-027 상단 주석으로 이미 RK3588 적용 명시. 본 ADR
  확정 시 enclosure/열관리 부품(히터·서모스탯·히트싱크)을 BOM 에 추가.
- ADR-001(Hub dual-SBC) 의 열 budget 재검토 필요(2-보드 동거 고온측).
- 옥외 운용 온도 사양은 §5 시험 결과로 SDD/OPS 문서에 정식 기재.

## 5. 검증 (Verification) — *결정 게이트*

EM3588 실보드 **thermal 챔버 시험**(결정 전제):

1. **부하**: idle / 풀로드(8C stress-ng + NPU 추론 + 영상 인코딩 동시).
2. **주변 온도 스텝**: −20 / 0 / +25 / +50 / +60 ℃, 각 1 h soak.
3. **계측**: `cat /sys/class/thermal/thermal_zone*/temp` 60 s 주기 로깅, throttle
   이벤트(`dmesg | grep -i thermal`), boot 성공률(저온 cold-start 10회).
4. **합격 기준**: 목표 운용 envelope(요구사항 확정 후) 전 구간에서 Tj < 85 ℃,
   throttle 미발생, cold-start 10/10.
5. Hub 듀얼 SBC 는 실제 enclosure 동거 구성으로 재측정(고온측 worst-case).

## 6. 참조 (References)

- DCN-2026-027 §5 O-1 (RK3588 확정) + BOM v1.1 상단 주석.
- Rockchip RK3588 / RK3588J 데이터시트 (동작 온도 등급) — *확보 필요*.
- Boardcon EM3588 제품 사양 — RK3588 SKU 등급 확인.
- [[ADR-001]] Hub dual-SBC 열 budget.
