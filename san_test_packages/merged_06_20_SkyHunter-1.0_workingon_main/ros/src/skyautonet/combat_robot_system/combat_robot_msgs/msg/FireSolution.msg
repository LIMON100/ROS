# SAN v1.5 IDS §5.24 (신규) — 사격 제원 (DCN-2026-026 C-3).
#
# surveillance (위협 클러스터 관리자, DCN-2026-026 C-1) 가 발행. 운용자
# UI 의 조준 제원 표시와 — 선택 파라미터 활성 시 — fire-authorization
# KEY1 사전조건 강화에 사용한다.
#
# 권원:
#   * DCN-2026-026 C-3 (APPROVED 2026-06-10 — k-of-n 합의, k=2)
#   * SDD-SWARM v1.5 §8.6.1 (위협 추적)
#
# 보안 경계 (DCN-2026-026 C-3 필수 명시): engage_ready 는 **advisory** —
# 사격 개시는 Two-key(KEY1→KEY2) + HMAC fire-authorization 체인 단독
# 권한이며 본 메시지는 그 체인을 우회하거나 대체할 수 없다.

std_msgs/Header header

uint32  shooter_robot_id     # 표적 최근접 추적(TRACK) follower
bool    solution_valid

float32 aim_bearing_deg      # world-frame lead 조준 방위
float32 aim_elevation_deg
float32 range_m              # 0 = unknown
float32 lead_time_s
float32 target_rate_dps      # 표적 각속도 추정 (deg/s)

uint8   vote_count           # 검증 통과한 k-of-n 합의 투표 수
uint8   min_votes            # 판정 기준 k (기본 2)
bool    engage_ready         # solution_valid && vote_count >= min_votes

uint64  timestamp_ms
