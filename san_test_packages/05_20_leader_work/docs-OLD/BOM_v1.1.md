# Bill of Materials — v1.1

**기준**: SAN-IDS-CMD-001 v1.1 / SAN-SDD-CMD-001 v1.1 (2026-05-11)
**범위**: v1.0 → v1.1 사이 변경된 부품. 변경 없는 항목(배터리, Go2 EDU 본체,
LTE 모뎀 등)은 의도적으로 생략. 설계 결정 근거는 `docs/adr/` 참조.

## 1. Robot fleet — SBC / sensor

| 부품 | v1.0 | v1.1 | 비고 |
|---|---|---|---|
| Hub UGV SBC | RK3588J × 1 | Jetson Orin Nano (8GB) 또는 RK3588J × 1 **+** RK3588J × 1 (듀얼) | 듀얼 구성 시 SLAM SBC + Comm/Video SBC 분리. ADR-001 |
| UGV LiDAR (follower) | 미지정 (테스트 시 Mid360 임시) | **Robosense E1** × 8 | Leader는 Unitree L1 내장 그대로. ADR-005 |
| Pan-tilt 짐벌 | (기존 모델 그대로) | (변경 없음) | sweep 모드 추가 사용. 하드웨어 envelope ≤ 60°/s 그대로 |
| Follower SBC | RK3588J | (변경 없음) | — |
| Leader SBC | RK3588J | (변경 없음) | — |

## 2. Network — Wi-Fi mesh / WAN

| 부품 | v1.0 | v1.1 | 비고 |
|---|---|---|---|
| Wi-Fi router (PoC) | 미지정 | **GL.iNet Flint 2 (GL-MT6000)** — Wi-Fi 6, OpenWrt 24.10 호환 | 또는 동급 IPQ8074A 보드 |
| Wi-Fi router (양산) | 미지정 | **Compex WPJ563** — IPQ6000, OpenWrt 24.10 | 양산 BOM 후보. ADR-004 |
| Mesh stack | (벤더 stock firmware) | OpenWrt 24.10 + batman-adv + mwan3 + DAWN + SQM cake | `infra/openwrt/24.10/` |
| LTE 모뎀 | (기존 그대로) | (변경 없음) | WAN failover 경로 |

> 주: 양산용 WPJ563은 antenna 외부화 가능, IP67 enclosure에 맞춤. PoC 단계의
> Flint 2는 internal antenna로 빠른 deploy 용도.

## 3. Bandwidth / payload assumptions

| 항목 | v1.0 | v1.1 | 사유 |
|---|---|---|---|
| SLAM aggregation 주기 | 1 Hz | 30–60s (default 30s, narrow mode 15s) | ADR-002 (~1/100 대역 감소) |
| Per-follower video stream | 단일 RTSP | GStreamer SRT, 최대 3개 동시 FHD (4번째부터 thumbnail 자동 강등) | ADR-003 |
| Pan-tilt 명령 주기 | sector 수신 시 | sector 수신 시 (변경 없음) | 시퀀스 번호 + envelope validation 추가 |

## 4. Compute envelope

| 항목 | v1.0 | v1.1 | 비고 |
|---|---|---|---|
| Hub UGV peak compute | RK3588J 단일 (8C ARM, 6 TOPS NPU) | Orin Nano 또는 RK3588J × 2 | Orin Nano 채택 시 SLAM fusion + GStreamer 인코딩 동거. 듀얼 RK3588J 채택 시 SBC당 부담 분리. |
| Hub SBC 장애 처리 | (없음 — 전기 실패 = 노드 down) | HubHealthMonitor 부분 운용 fallback (`safety/hub_health_monitor.py`) | SLAM 측 SBC 실패 시 comm/video는 계속 운용 가능 |

## 5. Deprecated / removed

| 항목 | 상태 | v1.2 계획 |
|---|---|---|
| `san_hub` 단일 SBC 패키지 | Deprecated (v1.1) | v1.2에서 제거 |
| `SLAMDelta.msg` | Deprecated (v1.1) — `SLAMLocalDelta.msg` 사용 | v1.2에서 제거 |

## 6. Sourcing lead time (참고)

| 부품 | typical lead time | 대체품 |
|---|---|---|
| Robosense E1 | 4–6 weeks | Livox Mid360 (호환 layer 필요) |
| Jetson Orin Nano | 2–4 weeks (NVIDIA Partner) | RK3588J × 2 듀얼 구성으로 대체 가능 |
| Compex WPJ563 | 6–8 weeks (B2B) | GL.iNet Flint 2 (off-the-shelf, PoC 한정) |

부품 변경 영향 검증은 `tests/test_s15_*.py` (S15 시나리오)와
`tests/test_phase5_streaming.py`, `tests/test_aggregated_map.py`에서 수행.
