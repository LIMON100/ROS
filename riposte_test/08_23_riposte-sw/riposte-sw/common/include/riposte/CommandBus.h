#pragma once
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <optional>
#include <string>

#include "riposte/Types.h"

// Low-rate command channel (engage/disengage) over a Unix datagram socket.
// Deliberately separate from the hot path: commands are events, tracks are
// state (RIPOSTE-SAD-001 §5.4).

namespace riposte {

class CommandServer {
public:
    CommandServer() = default;
    ~CommandServer() { close(); }

    // Owns a socket fd; copy/move would double-close (G5.2 RAII).
    CommandServer(const CommandServer&) = delete;
    CommandServer& operator=(const CommandServer&) = delete;
    CommandServer(CommandServer&&) = delete;
    CommandServer& operator=(CommandServer&&) = delete;

    bool open(const std::string& path) {
        path_ = path;
        fd_ = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
        if (fd_ < 0) {
            return false;
        }
        ::unlink(path.c_str());
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
        // umask around bind(): the socket inode must never be created wider
        // than 0660, even under a permissive process umask (the chmod below
        // is belt-and-braces, but it leaves a window otherwise).
        const mode_t prev_umask = ::umask(0117);
        const int rc = ::bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
        ::umask(prev_umask);
        if (rc != 0) {
            close();
            return false;
        }
        ::chmod(path.c_str(), 0660); // group-restricted: operator console only
        return true;
    }

    void close() {
        if (fd_ >= 0) {
            ::close(fd_);
            fd_ = -1;
        }
        if (!path_.empty()) {
            ::unlink(path_.c_str());
            path_.clear();
        }
    }

    // Non-blocking; call once per control tick. Malformed datagrams are dropped.
    // Drains the queue (bounded) so a garbage flood cannot delay a valid
    // DISENGAGE by one tick per datagram; returns the first valid command.
    std::optional<ObcCommand> poll() const {
        if (fd_ < 0) {
            return std::nullopt;
        }
        for (int i = 0; i < MAX_DRAIN_PER_POLL; ++i) {
            // Zero-init is a placeholder only: recv() overwrites the full struct and
            // magic/type are validated below (G15.4 defensive parsing).
            // NOLINTNEXTLINE(bugprone-invalid-enum-default-initialization)
            ObcCommand cmd{};
            // MSG_TRUNC: n is the true datagram length, so oversized datagrams
            // fail the exact-size check instead of being silently clipped.
            const ssize_t n = ::recv(fd_, &cmd, sizeof(cmd), MSG_TRUNC);
            if (n < 0) {
                return std::nullopt; // EAGAIN/EWOULDBLOCK: queue drained
            }
            if (n != static_cast<ssize_t>(sizeof(cmd))) {
                continue; // wrong size: drop, keep draining
            }
            if (cmd.magic != OBC_COMMAND_MAGIC) {
                continue;
            }
            if (cmd.type != ObcCommandType::ENGAGE &&
                cmd.type != ObcCommandType::DISENGAGE &&
                cmd.type != ObcCommandType::TARGET &&
                cmd.type != ObcCommandType::OPERATOR_HOLD &&
                cmd.type != ObcCommandType::RETURN_HOME) {
                continue; // unknown opcode from an untrusted peer (G15.4)
            }
            cmd.token[sizeof(cmd.token) - 1] = '\0';
            return cmd;
        }
        return std::nullopt;
    }

    // Bounds the per-call drain so a flood cannot stall the control loop. Also
    // the caller's per-tick dispatch bound (P1-01): the control tick calls
    // poll() in a loop and dispatches EVERY valid command up to this many, so
    // a >20 Hz bad-token flood cannot starve a queued DISENGAGE down to one
    // command per tick. Worst case under a mixed flood is MAX^2 non-blocking
    // recvs per tick (~ms) — bounded, and visible to SM-5 if it ever matters.
    static constexpr int MAX_DRAIN_PER_POLL = 64;

private:
    int fd_ = -1;
    std::string path_;
};

inline bool send_command(const std::string& path, const ObcCommand& cmd) {
    // SOCK_NONBLOCK: if the OBC's queue is full, sendto() fails with EAGAIN
    // (returned as false) — the operator console must never block here.
    const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        return false;
    }
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);
    const ssize_t n = ::sendto(fd, &cmd, sizeof(cmd), 0,
                               reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    ::close(fd);
    return n == static_cast<ssize_t>(sizeof(cmd));
}

} // namespace riposte
