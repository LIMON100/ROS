# SAN v1.5 IDS §4.6 — system → operator/leader fire result report.
#
# Fire authorization (D-004) 후 실제 사격이 완료되면 발행. Audit log
# 와 함께 사격 결과를 운용병사 및 통제관에게 전달.
#
# 발신: san_fire_authorization (사격 완료 시점)
# 수신: operator UI, Hub UGV (audit aggregation), mission_node
#
# 권원:
#   * IDS-CMD v1.5 §4.6 (PDR-7 신설)
#   * DCN-2026-001 D-004 (Fire Auth HMAC + 2-key + Audit)
#   * SDD-SWARM v1.5 §10 (Fire authorization workflow)
#
# QoS: RELIABLE, KEEP_LAST(20) — 모든 사격 결과 보존

# ─── 사격 결과 enum ─────────────────────────────────────────
uint8 RESULT_SUCCESS         = 0    # 발사 + 표적 명중 (탐지 기준)
uint8 RESULT_MISS            = 1    # 발사했으나 명중 실패
uint8 RESULT_MALFUNCTION     = 2    # 발사 실패 (탄걸림, mechanism 등)
uint8 RESULT_ABORTED         = 3    # 발사 직전 취소 (operator override 등)
uint8 RESULT_OUT_OF_RANGE    = 4    # 사거리 외 표적
uint8 RESULT_NO_AUTHORIZATION = 5   # D-004 인가 실패로 미발사

std_msgs/Header header

uint32 robot_id                       # 사격한 로봇
uint32 command_id                     # FireAuthorization.command_id echo
uint32 sequence

uint8  result                         # RESULT_*

# 사격 통계
uint32 rounds_fired                   # 발사 발수
uint32 target_id                      # 표적 ID (san_perception tracker)
float32 distance_to_target_m
float32 impact_point_x_m              # 추정 명중점 (Local frame)
float32 impact_point_y_m
float32 confidence                    # 결과 confidence (0.0 ~ 1.0)

# Audit trail
string authorization_chain            # D-004 의 HMAC chain ID (audit join key)
string notes                          # operator-facing 메모 (선택)

uint64 timestamp_fire_ms              # 실제 발사 시점 (epoch ms)
uint64 timestamp_report_ms            # 본 메시지 발행 시점
