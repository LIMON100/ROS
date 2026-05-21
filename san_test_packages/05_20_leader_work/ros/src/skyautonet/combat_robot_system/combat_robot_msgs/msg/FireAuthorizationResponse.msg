# SAN v1.5 IDS §3.5 — Per-trigger fire authorization response.
#
# Returned by the san_fire_authorization gate to the operator after
# evaluating a FireAuthorizationRequest. The decision is final; the
# operator must re-arm (new request_id, new Two-key sequence) for the
# next shot.
#
# Audit trail: every response is written to /var/log/san/fire_audit.log
# as one JSON Lines entry with the audit_log_uuid below. Limp Mode
# fires (limp_mode_fire == true) are flagged explicitly per
# DCN-2026-001 D-004.

# Decision reason codes.
uint8 REASON_GRANTED               = 0   # OK, fire authorized
uint8 REASON_DENIED_HMAC_FAIL      = 1   # signature mismatch
uint8 REASON_DENIED_NONCE_REUSE    = 2   # nonce already seen in 64-window
uint8 REASON_DENIED_TIMESTAMP_DRIFT = 3  # timestamp drift > 1000 ms
uint8 REASON_DENIED_TWO_KEY_INCOMPLETE = 4  # KEY2 received without KEY1
uint8 REASON_DENIED_TWO_KEY_TIMEOUT    = 5  # KEY1 → KEY2 gap > 5000 ms
uint8 REASON_DENIED_AUTONOMOUS_ENGAGEMENT = 6  # autonomous mode conflict with Limp Mode
uint8 REASON_DENIED_OTHER          = 99

std_msgs/Header header
uint32 request_id                  # echoes the request that triggered this response
uint32 sequence
uint64 response_timestamp_ms       # gate clock; for round-trip measurement

# Decision payload.
bool   granted                     # true iff REASON_GRANTED
uint8  reason                      # REASON_*
string reason_detail               # short human-readable explanation (≤ 80 chars)

# DCN-2026-001 D-004 audit flag. True if this grant occurred while
# OperationState.in_limp_mode was true at the moment of decision.
bool   limp_mode_fire

# UUIDv4 of the audit-log entry. Lets external systems join this
# response back to the persisted audit row.
string audit_log_uuid
