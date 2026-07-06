# SAN v1.5 IDS §5.23 (신규) — 교전 합의 투표 (DCN-2026-026 C-3).
#
# 위협을 독립 검출한 robot 의 detection_to_threat 가 발행. surveillance
# (FireSolution 판정자) 가 HMAC-SHA256 + nonce sliding-window 로 검증 후
# k-of-n (기본 k=2, 1.5 s 윈도) 합의에 집계한다. 평문 투표(UInt32) 는
# DCN-2026-026 으로 명시적 폐기.
#
# 권원:
#   * DCN-2026-026 C-3 (APPROVED 2026-06-10 — k=2 확정)
#   * DCN-2026-025 (track_id 표적 바인딩)
#   * DCN-2026-001 D-004 (HMAC-SHA256 mesh shared secret 체계 재사용)
#
# 보안 경계: 본 메시지와 FireSolution.engage_ready 는 advisory 다.
# 사격 개시 권한은 Two-key + HMAC 의 fire-authorization 체인 단독.

std_msgs/Header header

uint32  robot_id             # 투표 주체 (검출 로봇)
uint32  track_id             # ByteTrack 트랙 바인딩 (DCN-2026-025)

float32 bearing_deg          # world-frame 표적 방위 (0=+x, CCW+)
float32 elevation_deg        # horizon-frame
float32 range_m              # 0 = unknown

uint64  nonce                # 재전송 방지 (sliding-window 검증)
uint64  timestamp_ms         # 서명 시각 (epoch ms)
string  hmac_hex             # HMAC-SHA256 hex(64) — robot_id|track_id|
                             # bearing|elevation|range|nonce|timestamp
