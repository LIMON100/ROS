// Unit tests for OperatorAuthorization (decision D-2 / SI-2: no autonomous engage
// path). authorize() is the single gate that turns an operator ObcCommand into an
// engage; these tests pin the safety-critical properties: ENGAGE-only, exact token
// match, and DEFAULT-DENY when the token is unconfigured (G3).
#include "OperatorAuthorization.h"
#include "TargetGate.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "riposte/Tunables.h"
#include "riposte/Types.h"

using namespace riposte;

namespace {

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
int checks = 0;

#define CHECK(c)                                                    \
    do {                                                            \
        ++checks;                                                   \
        if (!(c)) {                                                 \
            std::printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            return 1;                                               \
        }                                                           \
    } while (0)

// Build a command the way engage_cli does: NUL-padded token. type is set in the
// aggregate init because ObcCommandType has no zero enumerator (value-init = UB).
ObcCommand make_cmd(ObcCommandType type, const char* token) {
    ObcCommand c{};
    c.magic = OBC_COMMAND_MAGIC;
    c.type = type;
    std::strncpy(c.token, token, sizeof(c.token) - 1);
    return c;
}

int test_correct_token_authorizes() {
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(auth.authorize(make_cmd(ObcCommandType::ENGAGE, "s3cr3t-token")));
    return 0;
}

int test_wrong_token_denied() {
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, "wrong-token")));
    return 0;
}

int test_disengage_never_authorized() {
    // DISENGAGE is not an engage authorization even with the right token; the
    // controller handles disengage unconditionally elsewhere (fail-safe direction).
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::DISENGAGE, "s3cr3t-token")));
    return 0;
}

int test_target_requires_token() {
    // TARGET hands the vehicle a mission cue and requests the engage, so it is
    // token-gated exactly like ENGAGE — a valid token authorizes it, a wrong or
    // absent one does not.
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(auth.authorize(make_cmd(ObcCommandType::TARGET, "s3cr3t-token")));
    CHECK(!auth.authorize(make_cmd(ObcCommandType::TARGET, "wrong-token")));
    CHECK(!auth.authorize(make_cmd(ObcCommandType::TARGET, "")));
    const OperatorAuthorization unset("");
    CHECK(!unset.authorize(make_cmd(ObcCommandType::TARGET, "anything")));
    return 0;
}

int test_safe_side_commands_not_token_gated() {
    // HOLD and RETURN_HOME only move the vehicle toward the safe side; they are
    // handled unconditionally by the controller and never authorized here.
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::OPERATOR_HOLD, "s3cr3t-token")));
    CHECK(!auth.authorize(make_cmd(ObcCommandType::RETURN_HOME, "s3cr3t-token")));
    CHECK(!OperatorAuthorization::requires_token(ObcCommandType::DISENGAGE));
    CHECK(OperatorAuthorization::requires_token(ObcCommandType::ENGAGE));
    CHECK(OperatorAuthorization::requires_token(ObcCommandType::TARGET));
    return 0;
}

int test_unconfigured_token_default_deny() {
    // No token configured => deny by default (G3), even for an empty cmd token.
    const OperatorAuthorization auth("");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, "")));
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, "anything")));
    return 0;
}

int test_empty_cmd_token_denied() {
    // Configured token present, but the command carries none.
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, "")));
    return 0;
}

int test_prefix_not_accepted() {
    // A prefix of the token must not authorize (NUL-padded fixed-length compare).
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, "s3cr3t")));
    return 0;
}

int test_case_sensitive() {
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, "S3CR3T-TOKEN")));
    return 0;
}

int test_single_char_difference_denied() {
    const OperatorAuthorization auth("s3cr3t-token");
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, "s3cr3t-tokeN")));
    return 0;
}

int test_max_length_token_roundtrip() {
    // engage_cli caps the token at 31 chars + NUL; a 31-char token must authorize.
    const std::string t31(31, 'x');
    const OperatorAuthorization auth(t31);
    CHECK(auth.authorize(make_cmd(ObcCommandType::ENGAGE, t31.c_str())));
    // Differ in the last (31st) character -> denied.
    std::string t31b = t31;
    t31b.at(30) = 'y';
    CHECK(!auth.authorize(make_cmd(ObcCommandType::ENGAGE, t31b.c_str())));
    return 0;
}

int test_unfilled_token_bytes_are_nul() {
    // Trailing bytes beyond the string must be NUL so the fixed-length compare
    // matches the NUL-padded configured token (guards against garbage tails).
    ObcCommand c{};
    c.magic = OBC_COMMAND_MAGIC;
    c.type = ObcCommandType::ENGAGE;
    std::memcpy(c.token, "abc", 3); // bytes [3..31] remain NUL from zero-init
    const OperatorAuthorization auth("abc");
    CHECK(auth.authorize(c));
    return 0;
}

// ------------------------------------------------------ TargetGate (payload) --
// Authorization proves the sender; the gate proves the numbers. These pin the
// fail-closed payload contract: finite, in-range, non-decreasing sequence.

ObcCommand make_target_cmd() {
    ObcCommand c = make_cmd(ObcCommandType::TARGET, "t");
    c.target_pos_ned_m[0] = 120.F;
    c.target_pos_ned_m[1] = -40.F;
    c.target_pos_ned_m[2] = -30.F;
    c.target_vel_ned_mps[0] = 5.F;
    c.target_heading_rad = 1.2F;
    c.target_seq = 1;
    return c;
}

int test_target_gate_accepts_sane_cue() {
    const ObcCommand c = make_target_cmd();
    CHECK(target_reject_reason(c, 0) == nullptr);
    // Equal sequence passes: the CLI defaults every cue to seq=1, and an
    // equal-seq replay carries the identical payload anyway.
    CHECK(target_reject_reason(c, 1) == nullptr);
    return 0;
}

int test_target_gate_rejects_nonfinite() {
    ObcCommand c = make_target_cmd();
    c.target_pos_ned_m[1] = std::nanf("");
    CHECK(target_reject_reason(c, 0) != nullptr);
    c = make_target_cmd();
    c.target_vel_ned_mps[2] = HUGE_VALF; // inf
    CHECK(target_reject_reason(c, 0) != nullptr);
    c = make_target_cmd();
    c.target_heading_rad = std::nanf("");
    CHECK(target_reject_reason(c, 0) != nullptr);
    return 0;
}

int test_target_gate_rejects_out_of_range() {
    ObcCommand c = make_target_cmd();
    c.target_pos_ned_m[0] = tun::TARGET_POS_MAX_M * 2.F;
    CHECK(target_reject_reason(c, 0) != nullptr);
    c = make_target_cmd();
    c.target_vel_ned_mps[0] = -(tun::TARGET_VEL_MAX_MPS * 2.F);
    CHECK(target_reject_reason(c, 0) != nullptr);
    c = make_target_cmd();
    c.target_heading_rad = tun::TARGET_HEADING_MAX_RAD * 1.5F;
    CHECK(target_reject_reason(c, 0) != nullptr);
    return 0;
}

int test_target_gate_rejects_sequence_decrease() {
    ObcCommand c = make_target_cmd();
    c.target_seq = 3;
    CHECK(target_reject_reason(c, 3) == nullptr); // same seq: pass
    CHECK(target_reject_reason(c, 4) != nullptr); // decreased: stale/replay
    return 0;
}

} // namespace

int main() {
    int rc = 0;
    rc = rc != 0 ? rc : test_correct_token_authorizes();
    rc = rc != 0 ? rc : test_wrong_token_denied();
    rc = rc != 0 ? rc : test_disengage_never_authorized();
    rc = rc != 0 ? rc : test_target_requires_token();
    rc = rc != 0 ? rc : test_safe_side_commands_not_token_gated();
    rc = rc != 0 ? rc : test_unconfigured_token_default_deny();
    rc = rc != 0 ? rc : test_empty_cmd_token_denied();
    rc = rc != 0 ? rc : test_prefix_not_accepted();
    rc = rc != 0 ? rc : test_case_sensitive();
    rc = rc != 0 ? rc : test_single_char_difference_denied();
    rc = rc != 0 ? rc : test_max_length_token_roundtrip();
    rc = rc != 0 ? rc : test_unfilled_token_bytes_are_nul();
    rc = rc != 0 ? rc : test_target_gate_accepts_sane_cue();
    rc = rc != 0 ? rc : test_target_gate_rejects_nonfinite();
    rc = rc != 0 ? rc : test_target_gate_rejects_out_of_range();
    rc = rc != 0 ? rc : test_target_gate_rejects_sequence_decrease();
    if (rc != 0) {
        return rc;
    }
    std::printf("test_authz: %d checks passed\n", checks);
    return 0;
}
