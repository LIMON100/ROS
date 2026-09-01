#pragma once
#include <cstring>
#include <string>

#include "riposte/Types.h"

namespace riposte {

// SI-2 / decision D-2: there is NO autonomous engage path. An control session only
// starts when an operator command carries the configured token. A track being
// present is a precondition for a useful engage, never a trigger for one.
//
// Two command types are token-gated because both start or redirect an
// control session: ENGAGE, and TARGET (an external cue that hands the vehicle a
// mission point and requests the engage). DISENGAGE / OPERATOR_HOLD /
// RETURN_HOME are deliberately NOT authorized here — they only ever move the
// vehicle toward the safe side, and requiring a token to stop an control session
// would be a failure mode, not a safeguard.
class OperatorAuthorization {
public:
    explicit OperatorAuthorization(std::string token) : token_(std::move(token)) {}

    static bool requires_token(ObcCommandType type) {
        return type == ObcCommandType::ENGAGE || type == ObcCommandType::TARGET;
    }

    bool authorize(const ObcCommand& cmd) const {
        if (!requires_token(cmd.type)) {
            return false;
        }
        if (token_.empty()) {
            return false; // unconfigured => deny by default (G3)
        }
        // Fixed-length compare over the 32-byte token field.
        return std::strncmp(cmd.token, token_.c_str(), sizeof(cmd.token)) == 0;
    }

private:
    std::string token_;
};

} // namespace riposte
