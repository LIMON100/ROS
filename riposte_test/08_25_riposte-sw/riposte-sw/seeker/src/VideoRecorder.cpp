#include "VideoRecorder.h"

#include "IDetector.h" // Frame

#include <dirent.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h> // kill; sigaction via csignal is not enough for SIG_IGN setup
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "riposte/Log.h"

// SEEKER-SDD §8.1/§8.2: H.264/MP4 30 s segments through an ffmpeg subprocess
// fed raw NV12 over a pipe. The perception loop is never blocked: a frame is
// written only when the WHOLE frame fits in the pipe right now (can_write),
// otherwise it is dropped; a dead encoder is detected, reaped and restarted on
// the next frame; MP4 finalization of a rotated-out segment happens in the
// background (async reap) so a moov rewrite cannot stall a control-rate loop.

namespace riposte {

namespace {

constexpr uint64_t OVERLAY_PERIOD_NS = 100'000'000ULL; // ~10 Hz sidecar refresh
constexpr int SPAWN_FAIL_MAX = 5; // consecutive spawn failures -> recording off

// fs.pipe-max-size: an F_SETPIPE_SZ request ABOVE this fails whole (leaving the
// 64 KiB default), so requests must be clamped to it (SDD §8.1).
size_t read_pipe_max() {
    size_t v = 1'048'576U; // kernel default
    const int fd = ::open("/proc/sys/fs/pipe-max-size", O_RDONLY | O_CLOEXEC);
    if (fd >= 0) {
        char buf[32] = {};
        const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
        ::close(fd);
        if (n > 0) {
            char* end = nullptr;
            const unsigned long long x = std::strtoull(buf, &end, 10);
            if (end != buf && x > 0) {
                v = static_cast<size_t>(x);
            }
        }
    }
    return v;
}

// mkdir -p (0755); true if the full path exists as a directory afterwards.
bool make_dirs(const std::string& path) {
    std::string cur;
    for (std::size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] != '/' && i + 1 < path.size()) {
            continue;
        }
        if (cur == "/" || cur.empty()) {
            continue;
        }
        if (::mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST) {
            return false;
        }
    }
    struct stat st {};
    return ::stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// fork+exec with stdin wired to `stdin_fd` and stdout/stderr silenced (ffmpeg
// runs -loglevel error; anything else is noise on the seeker's console).
// `extra_fd`, when >= 0, is installed as the child's fd 3 (ffmpeg `pipe:3` —
// the dual path's narrow input travels over a plain inherited pipe, so there
// is no filesystem rendezvous to race and EOF works exactly like stdin's).
long spawn(const std::vector<std::string>& args, int stdin_fd, int extra_fd = -1) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) {
        // execvp's `char* const argv[]` is a historical signature; it never
        // modifies the strings.
        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);
    const pid_t pid = ::fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        // Move the extra fd OUT of the 0..3 range first: it must survive the
        // stdin/stdout/stderr rewiring below, and if the parent had a closed
        // standard descriptor it can itself have been allocated as fd 0/1/2.
        // fcntl(F_DUPFD) picks the lowest free fd at or above 4.
        if (extra_fd >= 0 && extra_fd < 4) {
            const int moved = ::fcntl(extra_fd, F_DUPFD, 4);
            if (moved < 0) {
                ::_exit(127); // cannot place the narrow input; fail the spawn
            }
            extra_fd = moved;
        }
        if (stdin_fd >= 0) {
            (void)::dup2(stdin_fd, STDIN_FILENO);
        }
        if (extra_fd >= 0) {
            // dup2 clears CLOEXEC on the copy, so fd 3 survives execvp.
            if (::dup2(extra_fd, 3) < 0) {
                ::_exit(127);
            }
        }
        const int devnull = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (devnull >= 0) {
            (void)::dup2(devnull, STDOUT_FILENO);
            (void)::dup2(devnull, STDERR_FILENO);
        }
        (void)::execvp(argv[0], argv.data());
        ::_exit(127); // exec failed; the parent sees the status via waitpid
    }
    return pid;
}

// Blocking run of `binary -version`, true iff it executed and exited 0 — the
// open()-time proof the encoder exists, so record=true on a machine without
// ffmpeg is a refused startup (P1-07 fail-closed), not a silent no-op.
bool encoder_available(const std::string& binary) {
    const long pid = spawn({binary, "-version"}, -1);
    if (pid < 0) {
        return false;
    }
    int status = 0;
    if (::waitpid(static_cast<pid_t>(pid), &status, 0) != static_cast<pid_t>(pid)) {
        return false;
    }
    // NOLINTNEXTLINE(misc-include-cleaner) — WIFEXITED/WEXITSTATUS: <sys/wait.h>
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

// True iff `ffmpeg -filters` lists `name` — builds vary (static builds often
// lack drawtext), and a missing filter kills the encoder at spawn, so the
// capability is probed ONCE at open() instead of discovered as a death loop.
bool encoder_has_filter(const std::string& binary, const std::string& name) {
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        return false;
    }
    const pid_t pid = ::fork();
    if (pid < 0) {
        ::close(fds[0]);
        ::close(fds[1]);
        return false;
    }
    if (pid == 0) {
        (void)::dup2(fds[1], STDOUT_FILENO);
        const int devnull = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (devnull >= 0) {
            (void)::dup2(devnull, STDERR_FILENO);
        }
        ::execlp(binary.c_str(), binary.c_str(), "-hide_banner", "-filters",
                 static_cast<char*>(nullptr));
        ::_exit(127);
    }
    ::close(fds[1]);
    std::string out;
    char buf[4096];
    ssize_t n = 0;
    while ((n = ::read(fds[0], buf, sizeof(buf))) > 0) {
        out.append(buf, static_cast<size_t>(n));
    }
    ::close(fds[0]);
    int status = 0;
    (void)::waitpid(pid, &status, 0);
    // NOLINTNEXTLINE(misc-include-cleaner) — WIFEXITED/WEXITSTATUS: <sys/wait.h>
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 &&
           out.find(" " + name + " ") != std::string::npos;
}

// riposte_YYYYMMDD_HHMMSS_NNN.mp4 (UTC wall clock — log-stamp use only, never
// a control input). The per-instance sequence keeps names unique even when two
// segments start within one second (death-restart): -y would otherwise
// silently overwrite the earlier file's evidence.
std::string segment_name(uint64_t seq) {
    const time_t now = ::time(nullptr);
    struct tm tmv {};
    (void)::gmtime_r(&now, &tmv);
    char ts[32];
    (void)std::strftime(ts, sizeof(ts), "riposte_%Y%m%d_%H%M%S", &tmv);
    char buf[48];
    (void)std::snprintf(buf, sizeof(buf), "%s_%03llu.mp4", ts,
                        static_cast<unsigned long long>(seq));
    return buf;
}

// Grow a pipe toward `want` bytes, clamped to fs.pipe-max-size (an over-limit
// request fails WHOLE, not partially — SDD §8.1). Returns the granted size.
size_t grow_pipe(int fd, size_t want) {
    const size_t limit = read_pipe_max();
    const size_t ask = std::min(want, limit);
    (void)::fcntl(fd, F_SETPIPE_SZ, static_cast<int>(ask));
    const int got = ::fcntl(fd, F_GETPIPE_SZ);
    return got > 0 ? static_cast<size_t>(got) : 0U;
}

} // namespace

std::string format_overlay_text(const OverlayMeta& meta, const char* utc, int width,
                                int height) {
    char buf[192];
    char gps[64];
    if (meta.gps_ok) {
        (void)std::snprintf(gps, sizeof(gps), "GPS %.6f,%.6f alt %.1fm", meta.lat_deg,
                            meta.lon_deg, static_cast<double>(meta.alt_m));
    } else {
        (void)std::snprintf(gps, sizeof(gps), "GPS NO-FIX");
    }
    char wide[40];
    if (meta.wide_valid) {
        (void)std::snprintf(wide, sizeof(wide), "WIDE (%d,%d)",
                            static_cast<int>(meta.wide_cx * static_cast<float>(width)),
                            static_cast<int>(meta.wide_cy * static_cast<float>(height)));
    } else {
        (void)std::snprintf(wide, sizeof(wide), "WIDE --");
    }
    char nar[40];
    if (meta.nar_valid) {
        (void)std::snprintf(nar, sizeof(nar), "NARROW (%d,%d)",
                            static_cast<int>(meta.nar_cx * static_cast<float>(width)),
                            static_cast<int>(meta.nar_cy * static_cast<float>(height)));
    } else {
        (void)std::snprintf(nar, sizeof(nar), "NARROW --");
    }
    (void)std::snprintf(buf, sizeof(buf), "%s | %s | %s | %s", utc, gps, wide, nar);
    return buf;
}

VideoRecorder::~VideoRecorder() {
    close();
}

bool VideoRecorder::open() {
    frame_bytes_ =
        static_cast<size_t>(p_.width) * static_cast<size_t>(p_.height) * 3U / 2U;
    // A dead encoder mid-write raises SIGPIPE, which would kill the seeker
    // before can_write()'s POLLERR check ever ran again. The seeker writes to
    // no other pipes; datagram sockets (CommandBus) do not raise SIGPIPE.
    (void)::signal(SIGPIPE, SIG_IGN);
    if (!make_dirs(p_.dir)) {
        RLOG_ERROR("recorder", "cannot create %s", p_.dir.c_str());
        return false;
    }
    if (!encoder_available(p_.ffmpeg)) {
        RLOG_ERROR("recorder", "encoder '%s' not runnable", p_.ffmpeg.c_str());
        return false;
    }
    if (p_.dual) {
        // hstack is what makes dual DUAL: without it the composite cannot be
        // built at all, and every spawn would die — fail open() instead.
        if (!encoder_has_filter(p_.ffmpeg, "hstack")) {
            RLOG_ERROR("recorder", "'%s' lacks the hstack filter — dual unusable",
                       p_.ffmpeg.c_str());
            return false;
        }
        // The narrow input travels over a plain anonymous pipe inherited as the
        // encoder's fd 3 (see start_segment) — earlier builds used a named
        // FIFO; remove any such artifact a crashed run left in the directory.
        sweep_stale_fifos();
        if (p_.overlay) {
            struct stat st {};
            if (::stat(p_.font.c_str(), &st) != 0) {
                RLOG_WARN("recorder", "font %s missing — overlay disabled",
                          p_.font.c_str());
                p_.overlay = false;
            } else if (!encoder_has_filter(p_.ffmpeg, "drawtext")) {
                // Static ffmpeg builds commonly ship without drawtext; the
                // subtitle degrades away rather than death-looping the encoder.
                RLOG_WARN("recorder",
                          "'%s' lacks the drawtext filter — overlay "
                          "disabled (recording continues)",
                          p_.ffmpeg.c_str());
                p_.overlay = false;
            } else {
                overlay_path_ = p_.dir + "/.riposte_overlay.txt";
                update_overlay(OverlayMeta{}); // placeholder before the first meta
            }
        }
    }
    enabled_ = true;
    RLOG_INFO("recorder", "recording to %s (%dx%d%s @ %.0f fps, %s)", p_.dir.c_str(),
              p_.width, p_.height, p_.dual ? " x2 wide|narrow" : "", p_.fps,
              p_.codec.c_str());
    return true;
}

bool VideoRecorder::start_segment(uint64_t mono_ns) {
    if (pipe_too_small_ || consec_spawn_fail_ >= SPAWN_FAIL_MAX) {
        return false;
    }
    int fds[2] = {-1, -1};
    if (::pipe2(fds, O_CLOEXEC) != 0) {
        ++consec_spawn_fail_;
        return false;
    }
    // Room for a few whole frames so can_write() has slack; a grant below ONE
    // frame means every whole-frame check would fail forever — disable with a
    // deployment hint instead of silently recording nothing (SDD §8.1).
    if (grow_pipe(fds[1], frame_bytes_ * 4U) < frame_bytes_) {
        RLOG_ERROR("recorder",
                   "pipe capacity below one frame (%zu B) — recording disabled; "
                   "raise fs.pipe-max-size (deploy/sysctl.d)",
                   frame_bytes_);
        pipe_too_small_ = true;
        ::close(fds[0]);
        ::close(fds[1]);
        return false;
    }

    const uint64_t seq = segment_seq_++;
    const std::string out = p_.dir + "/" + segment_name(seq);
    const std::string size = std::to_string(p_.width) + "x" + std::to_string(p_.height);
    const std::string rate = std::to_string(p_.fps);
    // Dual: the narrow input is a SECOND anonymous pipe, inherited by the
    // encoder as fd 3 (`-i pipe:3`) — the same fresh-pipe-per-segment shape as
    // stdin. Earlier builds used one shared named FIFO reopened O_RDWR each
    // segment; that revived a writer on the pipe object the PREVIOUS segment's
    // encoder was still draining (it never saw EOF, never exited, and old and
    // new encoders byte-split one stream), and a fresh FIFO per segment traded
    // that for an open() rendezvous race against encoder startup. An inherited
    // pipe has neither failure mode: no filesystem rendezvous exists, and EOF
    // arrives the moment our write end closes.
    int nfds[2] = {-1, -1};
    if (p_.dual) {
        if (::pipe2(nfds, O_CLOEXEC) != 0 ||
            grow_pipe(nfds[1], frame_bytes_ * 4U) < frame_bytes_) {
            RLOG_ERROR("recorder", "narrow pipe unusable — recording disabled");
            pipe_too_small_ = true;
            if (nfds[0] >= 0) {
                ::close(nfds[0]);
                ::close(nfds[1]);
            }
            ::close(fds[0]);
            ::close(fds[1]);
            return false;
        }
    }
    std::vector<std::string> args = {
        p_.ffmpeg,     "-hide_banner", "-loglevel",     "error",
        "-f",          "rawvideo",     "-pixel_format", "nv12",
        "-video_size", size,           "-framerate",    rate,
        "-i",          "pipe:0"};
    if (p_.dual) {
        const std::vector<std::string> nar_in = {
            "-f", "rawvideo", "-pixel_format", "nv12", "-video_size", size, "-framerate",
            rate, "-i",       "pipe:3"};
        args.insert(args.end(), nar_in.begin(), nar_in.end());
        std::string filter = "[0:v][1:v]hstack=inputs=2";
        if (p_.overlay) {
            filter += "[s];[s]drawtext=fontfile=" + p_.font +
                      ":textfile=" + overlay_path_ +
                      ":reload=1:expansion=none:x=8:y=8:fontsize=24:fontcolor=yellow:"
                      "box=1:boxcolor=black@0.4";
        }
        filter += "[v]";
        args.insert(args.end(), {"-filter_complex", filter, "-map", "[v]"});
    }
    if (p_.codec == "libx264") {
        args.insert(args.end(), {"-preset", p_.preset});
    }
    args.insert(args.end(),
                {"-an", "-c:v", p_.codec, "-movflags", "+faststart", "-y", out});

    child_pid_ = spawn(args, fds[0], nfds[0]);
    ::close(fds[0]); // child owns its copies of the read ends
    if (nfds[0] >= 0) {
        ::close(nfds[0]);
    }
    if (child_pid_ < 0) {
        ::close(fds[1]);
        if (nfds[1] >= 0) {
            ::close(nfds[1]);
        }
        ++consec_spawn_fail_;
        RLOG_WARN("recorder", "encoder spawn failed (%d/%d)", consec_spawn_fail_,
                  SPAWN_FAIL_MAX);
        return false;
    }
    consec_spawn_fail_ = 0;
    pipe_fd_ = fds[1];
    pipe_fd_nar_ = nfds[1]; // -1 in single-channel mode
    segment_start_ns_ = mono_ns;
    return true;
}

void VideoRecorder::finish_segment() {
    if (pipe_fd_ >= 0) {
        ::close(pipe_fd_); // EOF -> ffmpeg drains, finalizes the MP4, exits
        pipe_fd_ = -1;
    }
    if (pipe_fd_nar_ >= 0) {
        // Same EOF contract as stdin: the write ends are the only writers, so
        // the (possibly still-draining) encoder sees EOF on BOTH inputs once
        // the queues empty, finalizes and exits — the next segment's pipes are
        // fresh objects no old reader shares.
        ::close(pipe_fd_nar_);
        pipe_fd_nar_ = -1;
    }
    if (child_pid_ >= 0) {
        // The finalize (moov rewrite for +faststart) can take 100s of ms — do
        // NOT wait here on the perception path; park the pid and reap later.
        int status = 0;
        // NOLINTNEXTLINE(misc-include-cleaner) — WNOHANG: <sys/wait.h>
        const pid_t r = ::waitpid(static_cast<pid_t>(child_pid_), &status, WNOHANG);
        if (r == 0) {
            reap_.push_back(child_pid_);
        }
        child_pid_ = -1;
    }
}

// Removes narrow-input FIFO artifacts older builds (which fed the encoder via
// a named FIFO) left in the directory. Recordings are untouched.
void VideoRecorder::sweep_stale_fifos() const {
    DIR* dir = ::opendir(p_.dir.c_str());
    if (dir == nullptr) {
        return;
    }
    // Single-threaded scan of a private DIR stream (same reasoning as
    // reclaim_disk).
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    while (const dirent* de = ::readdir(dir)) {
        const std::string name = de->d_name;
        if (name.rfind(".riposte_nar_", 0) == 0 || name == ".riposte_ir.fifo") {
            (void)::unlink((p_.dir + "/" + name).c_str());
        }
    }
    (void)::closedir(dir);
}

void VideoRecorder::reap_pending() {
    for (auto it = reap_.begin(); it != reap_.end();) {
        int status = 0;
        // NOLINTNEXTLINE(misc-include-cleaner) — WNOHANG: <sys/wait.h>
        const pid_t r = ::waitpid(static_cast<pid_t>(*it), &status, WNOHANG);
        it = (r != 0) ? reap_.erase(it) : it + 1;
    }
}

void VideoRecorder::reclaim_disk() const {
    struct statvfs vs {};
    if (::statvfs(p_.dir.c_str(), &vs) != 0 || vs.f_blocks == 0) {
        return; // cannot measure: do nothing rather than delete blind
    }
    const uint64_t frsize = vs.f_frsize != 0 ? vs.f_frsize : vs.f_bsize;
    const uint64_t total = static_cast<uint64_t>(vs.f_blocks) * frsize;
    const uint64_t used = total - (static_cast<uint64_t>(vs.f_bfree) * frsize);

    struct Entry {
        std::string name;
        uint64_t bytes;
        time_t mtime;
    };
    std::vector<Entry> entries;
    DIR* dir = ::opendir(p_.dir.c_str());
    if (dir == nullptr) {
        return;
    }
    // Single-threaded scan of a private DIR stream; the "safe" readdir_r is
    // deprecated by POSIX/glibc.
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    while (const dirent* de = ::readdir(dir)) {
        const std::string name = de->d_name;
        if (name.size() < 4 || name.compare(name.size() - 4, 4, ".mp4") != 0) {
            continue; // recordings only — never another file class (S-5)
        }
        struct stat st {};
        const std::string full = p_.dir + "/" + name;
        if (::stat(full.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;
        }
        entries.push_back({name, static_cast<uint64_t>(st.st_size), st.st_mtime});
    }
    (void)::closedir(dir);
    std::sort(entries.begin(), entries.end(),
              [](const Entry& a, const Entry& b) { return a.mtime < b.mtime; });

    std::vector<RecFile> files;
    files.reserve(entries.size());
    for (const auto& e : entries) {
        files.push_back({e.name, e.bytes});
    }
    const size_t evict =
        plan_eviction(files, total, used, p_.disk_high_frac, p_.disk_free_frac);
    for (size_t i = 0; i < evict; ++i) {
        const std::string full = p_.dir + "/" + entries[i].name;
        if (::unlink(full.c_str()) == 0) {
            RLOG_WARN("recorder", "disk high — evicted %s (%llu B)",
                      entries[i].name.c_str(),
                      static_cast<unsigned long long>(entries[i].bytes));
        }
    }
}

bool VideoRecorder::rotate(uint64_t mono_ns) {
    reap_pending();
    if (child_pid_ < 0) {
        reclaim_disk();
        return start_segment(mono_ns);
    }
    if (mono_ns - segment_start_ns_ >= p_.segment_ns) {
        finish_segment();
        reclaim_disk();
        return start_segment(mono_ns);
    }
    return true;
}

bool VideoRecorder::can_write(int fd, bool& dead) const {
    // NOLINTBEGIN(misc-include-cleaner) — pollfd/POLLOUT/poll: <poll.h>;
    // FIONREAD: <sys/ioctl.h>
    pollfd pf{fd, POLLOUT, 0};
    if (::poll(&pf, 1, 0) < 0) {
        return false;
    }
    if ((pf.revents & (POLLERR | POLLHUP)) != 0) {
        dead = true; // reader (encoder) is gone
        return false;
    }
    // POLLOUT guarantees only ONE writable byte; the no-block contract needs
    // room for the WHOLE frame: capacity - queued (single writer, so free
    // space can only grow between this check and the write). SDD §8.1.
    const int capacity = ::fcntl(fd, F_GETPIPE_SZ);
    int queued = 0;
    if (capacity <= 0 || ::ioctl(fd, FIONREAD, &queued) != 0) {
        return false;
    }
    return static_cast<size_t>(capacity) - static_cast<size_t>(queued) >= frame_bytes_;
    // NOLINTEND(misc-include-cleaner)
}

bool VideoRecorder::write_all(int fd, const uint8_t* buf, size_t n) {
    size_t off = 0;
    while (off < n) {
        const ssize_t w = ::write(fd, buf + off, n - off);
        if (w < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false; // EPIPE (encoder died mid-frame) or a real error
        }
        off += static_cast<size_t>(w);
    }
    return true;
}

void VideoRecorder::update_overlay(const OverlayMeta& meta) {
    if (overlay_path_.empty()) {
        return;
    }
    const time_t now = ::time(nullptr);
    struct tm tmv {};
    (void)::gmtime_r(&now, &tmv);
    char utc[24];
    (void)std::strftime(utc, sizeof(utc), "%Y-%m-%d %H:%M:%SZ", &tmv);
    const std::string text = format_overlay_text(meta, utc, p_.width, p_.height);
    // Atomic replace (SDD §8.2): drawtext reload=1 re-reads the file every
    // frame, so an in-place write could burn a truncated half-line into video.
    const std::string tmp = overlay_path_ + ".tmp";
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        return;
    }
    const ssize_t w = ::write(fd, text.data(), text.size());
    ::close(fd);
    if (w == static_cast<ssize_t>(text.size())) {
        (void)::rename(tmp.c_str(), overlay_path_.c_str());
    }
}

void VideoRecorder::write(const Frame& f, uint64_t mono_ns) {
    if (!enabled_ || !frame_matches(f)) {
        ++frames_dropped_;
        return;
    }
    // Pace 60 Hz capture down to the configured encoder rate; early frames are
    // expected decimation, not drops.
    const auto period = static_cast<uint64_t>(1e9 / p_.fps);
    if (next_frame_due_ns_ != 0 && mono_ns < next_frame_due_ns_) {
        return;
    }
    next_frame_due_ns_ =
        (next_frame_due_ns_ == 0 || mono_ns > next_frame_due_ns_ + period)
            ? mono_ns + period
            : next_frame_due_ns_ + period;
    if (!rotate(mono_ns)) {
        ++frames_dropped_;
        return;
    }
    bool dead = false;
    if (!can_write(pipe_fd_, dead)) {
        if (dead) {
            // Restart on the NEXT frame, not mid-frame: rotate() respawns.
            RLOG_WARN("recorder", "encoder died — restarting next frame");
            finish_segment();
        }
        ++frames_dropped_;
        return;
    }
    if (!write_all(pipe_fd_, contiguous(f), frame_bytes_)) {
        ++frames_dropped_;
        finish_segment();
        return;
    }
    ++frames_written_;
}

void VideoRecorder::write_dual(const Frame& wide, const Frame& nar,
                               const OverlayMeta& meta, uint64_t mono_ns) {
    if (!enabled_ || !frame_matches(wide) || !frame_matches(nar)) {
        ++frames_dropped_;
        return;
    }
    const auto period = static_cast<uint64_t>(1e9 / p_.fps);
    if (next_frame_due_ns_ != 0 && mono_ns < next_frame_due_ns_) {
        return;
    }
    next_frame_due_ns_ =
        (next_frame_due_ns_ == 0 || mono_ns > next_frame_due_ns_ + period)
            ? mono_ns + period
            : next_frame_due_ns_ + period;
    if (!rotate(mono_ns)) {
        ++frames_dropped_;
        return;
    }
    if (p_.overlay && mono_ns - last_overlay_ns_ >= OVERLAY_PERIOD_NS) {
        last_overlay_ns_ = mono_ns;
        update_overlay(meta);
    }
    // BOTH pipes must take a whole frame or the pair is dropped as a unit —
    // writing one side alone would shift the wide|narrow composite forever.
    bool dead = false;
    const bool wide_ok = can_write(pipe_fd_, dead);
    const bool nar_ok = !dead && can_write(pipe_fd_nar_, dead);
    if (!wide_ok || !nar_ok) {
        if (dead) {
            RLOG_WARN("recorder", "encoder died — restarting next frame");
            finish_segment();
        }
        ++frames_dropped_;
        return;
    }
    // Each side is repacked (if strided) and written before the other side's
    // contiguous() call: the scratch buffer is shared, so interleaving keeps
    // each channel's bytes intact.
    if (!write_all(pipe_fd_, contiguous(wide), frame_bytes_) ||
        !write_all(pipe_fd_nar_, contiguous(nar), frame_bytes_)) {
        ++frames_dropped_;
        finish_segment();
        return;
    }
    ++frames_written_;
}

void VideoRecorder::close() {
    if (!enabled_) {
        return;
    }
    finish_segment();
    // Shutdown may block briefly: every parked encoder gets a bounded window
    // to finalize its MP4 (evidence beats a fast exit), then SIGKILL.
    if (child_pid_ >= 0) {
        reap_.push_back(child_pid_);
        child_pid_ = -1;
    }
    for (const long pid : reap_) {
        int status = 0;
        bool reaped = false;
        for (int i = 0; i < 500; ++i) { // <= 5 s per encoder
            // NOLINTNEXTLINE(misc-include-cleaner) — WNOHANG: <sys/wait.h>
            if (::waitpid(static_cast<pid_t>(pid), &status, WNOHANG) != 0) {
                reaped = true;
                break;
            }
            ::usleep(10'000);
        }
        if (!reaped) {
            RLOG_WARN("recorder", "encoder %ld stuck — SIGKILL", pid);
            (void)::kill(static_cast<pid_t>(pid), SIGKILL);
            (void)::waitpid(static_cast<pid_t>(pid), &status, 0);
        }
    }
    reap_.clear();
    if (!overlay_path_.empty()) {
        (void)::unlink(overlay_path_.c_str());
    }
    enabled_ = false;
}

bool VideoRecorder::frame_matches(const Frame& f) {
    const bool ok = (f.width == p_.width) && (f.height == p_.height) &&
                    (f.data != nullptr) && (f.stride >= static_cast<size_t>(p_.width));
    if (!ok) {
        ++dim_mismatch_;
    }
    return ok;
}

const uint8_t* VideoRecorder::contiguous(const Frame& f) {
    if (f.stride == static_cast<size_t>(p_.width)) {
        return f.data;
    }
    pack_.resize(frame_bytes_);
    compact_nv12(f.data, static_cast<size_t>(f.stride), p_.width, p_.height,
                 pack_.data());
    return pack_.data();
}

void compact_nv12(const uint8_t* src, size_t stride, int width, int height,
                  uint8_t* dst) {
    const size_t w = static_cast<size_t>(width);
    const size_t h = static_cast<size_t>(height);
    for (size_t y = 0; y < h; ++y) {
        std::memcpy(dst + (y * w), src + (y * stride), w);
    }
    const uint8_t* uv_src = src + (stride * h);
    uint8_t* uv_dst = dst + (w * h);
    for (size_t y = 0; y < h / 2U; ++y) {
        std::memcpy(uv_dst + (y * w), uv_src + (y * stride), w);
    }
}

size_t plan_eviction(const std::vector<RecFile>& oldest_first, uint64_t total_bytes,
                     uint64_t used_bytes, double high_frac, double free_frac) {
    if (total_bytes == 0U ||
        static_cast<double>(used_bytes) < high_frac * static_cast<double>(total_bytes)) {
        return 0;
    }
    const uint64_t target =
        static_cast<uint64_t>(free_frac * static_cast<double>(total_bytes));
    uint64_t reclaimed = 0;
    size_t count = 0;
    for (const auto& f : oldest_first) {
        reclaimed += f.bytes;
        ++count;
        if (reclaimed >= target) {
            break;
        }
    }
    return count;
}

} // namespace riposte
