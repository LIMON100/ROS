// riposte-engage: operator console stand-in. Sends mission/engage/disengage
// commands to the OBC command socket.
//
//   riposte-engage engage <token>
//   riposte-engage target <token> <north_m> <east_m> <down_m> <heading_rad> [seq]
//   riposte-engage hold
//   riposte-engage return-home
//   riposte-engage disengage
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "riposte/CommandBus.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"

using namespace riposte;

namespace {
bool copy_token(ObcCommand& cmd, const char* token) {
    const size_t token_len = std::strlen(token);
    if (token_len >= sizeof(cmd.token)) {
        (void)std::fprintf(stderr, "token too long (%zu chars, max %zu)\n", token_len,
                           sizeof(cmd.token) - 1);
        return false;
    }
    std::strncpy(cmd.token, token, sizeof(cmd.token) - 1);
    return true;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf(
            "usage: %s engage <token>\n"
            "       %s target <token> <N> <E> <D> <heading_rad> [seq]\n"
            "       %s target <token> <N> <E> <D> <heading_rad> <vN> <vE> <vD> [seq]\n"
            "       %s hold | return-home | disengage\n",
            argv[0], argv[0], argv[0], argv[0]);
        std::printf("  socket: $RIPOSTE_OBC_SOCKET (default %s)\n", tun::OBC_CMD_SOCKET);
        return 2;
    }
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* env_sock = std::getenv("RIPOSTE_OBC_SOCKET");
    const std::string socket = (env_sock != nullptr) ? env_sock : tun::OBC_CMD_SOCKET;

    ObcCommand cmd{};
    cmd.magic = OBC_COMMAND_MAGIC;

    const std::string verb = argv[1];
    if (verb == "engage") {
        if (argc < 3) {
            std::printf("engage requires a token\n");
            return 2;
        }
        cmd.type = ObcCommandType::ENGAGE;
        if (!copy_token(cmd, argv[2])) {
            return 2;
        }
    } else if (verb == "target") {
        if (argc < 7) {
            std::printf(
                "target requires: <token> <north_m> <east_m> <down_m> <heading_rad> "
                "[seq]\n");
            return 2;
        }
        cmd.type = ObcCommandType::TARGET;
        if (!copy_token(cmd, argv[2])) {
            return 2;
        }
        cmd.target_pos_ned_m[0] = std::strtof(argv[3], nullptr);
        cmd.target_pos_ned_m[1] = std::strtof(argv[4], nullptr);
        cmd.target_pos_ned_m[2] = std::strtof(argv[5], nullptr);
        cmd.target_heading_rad = std::strtof(argv[6], nullptr);
        // Optional target velocity (NED m/s). Omitted => a cue with no reported
        // motion, which the rendezvous solver flies to directly.
        if (argc >= 10) {
            cmd.target_vel_ned_mps[0] = std::strtof(argv[7], nullptr);
            cmd.target_vel_ned_mps[1] = std::strtof(argv[8], nullptr);
            cmd.target_vel_ned_mps[2] = std::strtof(argv[9], nullptr);
            cmd.target_seq =
                (argc >= 11) ? static_cast<uint32_t>(std::strtoul(argv[10], nullptr, 10))
                             : 1U;
        } else {
            cmd.target_seq =
                (argc >= 8) ? static_cast<uint32_t>(std::strtoul(argv[7], nullptr, 10))
                            : 1U;
        }
    } else if (verb == "hold") {
        cmd.type = ObcCommandType::OPERATOR_HOLD;
    } else if (verb == "return-home") {
        cmd.type = ObcCommandType::RETURN_HOME;
    } else if (verb == "disengage") {
        cmd.type = ObcCommandType::DISENGAGE;
    } else {
        std::printf("unknown verb '%s'\n", verb.c_str());
        return 2;
    }

    if (!send_command(socket, cmd)) {
        std::printf("send failed (is riposte-obc running?)\n");
        return 1;
    }
    std::printf("sent %s\n", verb.c_str());
    return 0;
}
