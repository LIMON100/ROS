# SAN v1.5 IDS §3.5 — Per-trigger fire authorization request.
#
# DCN-2026-001 D-004: Limp Mode 발사 정책 Option A 확정에 따라, 모든
# 발사 명령은 HMAC-SHA256 인증 + Two-key arming 시퀀스를 요구함. 본
# 메시지는 운용자 단말이 발사 게이트 (san_fire_authorization) 에 보내는
# 단일 trigger 인증 요청.
#
# 권원:
#   * SDD-SWARM v1.5 §5.7.2.1 (Limp Mode 발사 정책 박스)
#   * OPS-SOP v1.5 §7.11 (Limp Mode FAQ — Option A)
#   * SDD-SWARM v1.5 §10.1.1 (HMAC + Two-key + audit 의무화)
#
# Two-key arming sequence:
#   1) operator taps target on map → command_type = TWO_KEY_KEY1_TARGET_TAP
#   2) operator presses red "Confirm Fire" button → command_type = TWO_KEY_KEY2_CONFIRM
#   3) gate produces FireAuthorizationResponse with granted=true after both keys
#
# Mesh-direct path: LTE 단절 (Limp Mode) 시 본 메시지는 Wi-Fi 6 Mesh
# 로 우회 전송됨. 각 robot 의 게이트가 HMAC 을 직접 검증하므로 LTE
# Hub 의 중계가 필요 없음.

# Two-key state machine command types.
uint8 TWO_KEY_KEY1_TARGET_TAP = 1   # 1차 — 표적 탭
uint8 TWO_KEY_KEY2_CONFIRM    = 2   # 2차 — 적색 Confirm Fire 버튼
uint8 TWO_KEY_CANCEL          = 3   # 운용자 취소 (state machine reset)

std_msgs/Header header
uint32 request_id                  # 32-bit, monotonically increasing per operator session
uint32 sequence
string operator_id                 # accountability trail (LDAP/HRID)
uint64 nonce                       # 64-bit random; replay-protection sliding window
uint64 request_timestamp_ms        # operator-side clock; gate checks drift ≤ 1000 ms

# Two-key sequence position
uint8  command_type                # TWO_KEY_*

# Target geometry (valid only when command_type == TWO_KEY_KEY1_TARGET_TAP).
# Coordinates are in WGS84 with int representations (avoid float drift across mesh).
int32  target_lat_e7               # decimal-degrees × 10^7
int32  target_lon_e7
int32  target_alt_mm               # millimetres above ground level

# HMAC-SHA256 signature over the canonical serialization of:
#   request_id || sequence || operator_id || nonce || request_timestamp_ms
#   || command_type || target_lat_e7 || target_lon_e7 || target_alt_mm
# Encoded as lowercase hex (64 chars). Computed with the mesh shared
# secret loaded from /etc/san/mesh_secret.bin (32 bytes, mode 0400).
string hmac_signature
