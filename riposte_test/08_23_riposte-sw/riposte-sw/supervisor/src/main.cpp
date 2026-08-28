// riposte-supervisor (L5 health + blackbox) — RIPOSTE-SAD-001 §6.
//
// Read-only consumer of the three status buses. It aggregates health, writes a
// JSONL blackbox, and pets the systemd watchdog. It has NO authority over flight:
// if it dies, flight is unaffected; if the OBC dies, PX4's offboard failsafe (not
// the supervisor) takes the aircraft. This keeps disk I/O and reporting off the
// deterministic control path (design decision A-4).

#include "Blackbox.h"

#include <array>
#include <atomic>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

#include "riposte/Clock.h"
#include "riposte/Config.h"
#include "riposte/Log.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"

#ifdef RIPOSTE_WITH_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

using namespace riposte;

namespace {
// Cooperative-shutdown flag (G7.7): set by the signal handler, observed by the
// poll loop. Mutable by design — a global is the only channel a signal handler
// can reach.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_run{true};
void on_signal(int /*unused*/) {
    g_run.store(false);
}

// A NaN/Inf in a %f field prints "nan"/"inf" and breaks the JSON line (P2-02):
// substitute 0 so a parser keeps the rest of the record's fields.
double jf(double x) {
    return std::isfinite(x) ? x : 0.0;
}
float jf(float x) {
    return std::isfinite(x) ? x : 0.F;
}

} // namespace

int main(int argc, char** argv) {
    (void)std::signal(SIGINT, on_signal); // prior handler is irrelevant here
    (void)std::signal(SIGTERM, on_signal);

    Config cfg;
    if (argc > 1 && !cfg.load(argv[1])) {
        // Same rule as the seeker/OBC (AGENTS §7.9): an explicitly named config
        // that fails to load is an ops error. Defaults here would silently run
        // without a blackbox — flight evidence lost with a healthy-looking log.
        RLOG_ERROR("sup", "cannot load config %s — refusing to start on defaults",
                   argv[1]);
        return 1;
    }
    const std::string blackbox = cfg.get_str("supervisor.blackbox", "");
    const long bb_max = cfg.get_int("supervisor.blackbox_max_bytes", 16L * 1024 * 1024);
    const int bb_keep = static_cast<int>(cfg.get_int("supervisor.blackbox_keep", 5));

    std::unique_ptr<Blackbox> bb;
    if (!blackbox.empty()) {
        bb = std::make_unique<Blackbox>(blackbox, bb_max, bb_keep);
        if (!bb->ok()) {
            RLOG_WARN("sup", "cannot open blackbox %s", blackbox.c_str());
        }
    }

    ShmSeqSlot<ObcStatus> obc;
    ShmSeqSlot<TrackState> track;
    ShmSeqSlot<SeekerHealth> seeker;
    obc.open(tun::SHM_OBC_STATUS, ShmSeqSlot<ObcStatus>::Role::READER);
    track.open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::READER);
    seeker.open(tun::SHM_SEEKER_HEALTH, ShmSeqSlot<SeekerHealth>::Role::READER);

#ifdef RIPOSTE_WITH_SYSTEMD
    sd_notify(0, "READY=1");
#endif

    uint64_t last_report = 0;
    while (g_run.load()) {
        // Retry attaching to buses that were not up at startup.
        obc.ensure_open(tun::SHM_OBC_STATUS, ShmSeqSlot<ObcStatus>::Role::READER);
        track.ensure_open(tun::SHM_TRACK, ShmSeqSlot<TrackState>::Role::READER);
        seeker.ensure_open(tun::SHM_SEEKER_HEALTH,
                           ShmSeqSlot<SeekerHealth>::Role::READER);

        const uint64_t now = mono_now_ns();

        ObcStatus os{};
        const bool have_obc = obc.read(os);
        TrackState ts{};
        const bool have_trk = track.read(ts);
        SeekerHealth sh{};
        const bool have_shk = seeker.read(sh);

        // Freshness (P2-01): read() only proves a value EXISTS. A dead producer
        // leaves its last sample frozen in shm; a value older than the stale
        // bound is treated as absent for liveness. age_ns() (not a bare
        // subtraction) clamps a producer publish that lands after `now`.
        const bool obc_fresh =
            have_obc && age_ns(now, os.mono_ns) <= tun::SUPERVISOR_STALE_NS;
        const bool trk_fresh =
            have_trk && age_ns(now, ts.mono_ns) <= tun::SUPERVISOR_STALE_NS;
        const bool shk_fresh =
            have_shk && age_ns(now, sh.mono_ns) <= tun::SUPERVISOR_STALE_NS;

        // A FROZEN OFFBOARD_ACTIVE must not read as active — else a dead OBC
        // drives the 20 Hz cadence and logs a stale state as current.
        const bool active = obc_fresh && os.state == ObcState::OFFBOARD_ACTIVE;
        const uint64_t interval = active ? 50'000'000ULL : 1'000'000'000ULL;
        if ((bb != nullptr) && bb->ok() && now - last_report >= interval) {
            last_report = now;
            // Best-effort record; Blackbox bounds on-disk size via rotation (A-4).
            // Per-stream `stale` + age so post-analysis tells "frozen" from
            // "healthy"; every float is finite-guarded so NaN/Inf cannot break
            // the JSON line (P2-01/P2-02).
            std::array<char, 640> line{};
            const int n = std::snprintf(
                line.data(), line.size(),
                "{\"t\":%.6f,"
                "\"obc\":{\"state\":\"%s\",\"stale\":%d,\"viol\":%u,\"jit_ms\":%.2f,"
                "\"eng\":%u},"
                "\"trk\":{\"stale\":%d,\"valid\":%d,\"q\":%.2f,\"age_ms\":%.0f,"
                "\"targets\":%u},"
                "\"seeker\":{\"stale\":%d,\"fps\":%.1f,\"infer_ms\":%.1f,\"det_ok\":%d,"
                "\"synth\":%d}}\n",
                ns_to_s(now), have_obc ? to_string(os.state) : "NA", obc_fresh ? 0 : 1,
                have_obc ? os.violation_mask : 0U, have_obc ? jf(os.loop_jitter_ms) : 0.F,
                have_obc ? os.engage_count : 0U, trk_fresh ? 0 : 1,
                have_trk ? ts.valid : 0, have_trk ? jf(ts.quality) : 0.F,
                // age_ns(), not a bare subtraction: the track is read AFTER now
                // is sampled, so a publish in between would underflow to a
                // nonsense age.
                have_trk ? jf(ns_to_ms(age_ns(now, ts.mono_ns))) : -1.0,
                have_trk ? ts.num_targets : 0U, shk_fresh ? 0 : 1,
                have_shk ? jf(sh.fps) : 0.F, have_shk ? jf(sh.infer_latency_ms) : 0.F,
                have_shk ? sh.detector_ok : 0, have_shk ? sh.synthetic : 0);
            // Clamp to bytes actually written (snprintf returns the untruncated len).
            const int len = (n < static_cast<int>(line.size()))
                                ? n
                                : static_cast<int>(line.size()) - 1;
            if (!bb->write_line(line.data(), len)) {
                RLOG_WARN("sup", "blackbox write failed (disk full?) — logging degraded");
            }
        }

#ifdef RIPOSTE_WITH_SYSTEMD
        sd_notify(0, "WATCHDOG=1");
#endif
        // 20 Hz while the OBC is active so the ~20 Hz blackbox cadence above is
        // actually achievable; 10 Hz supervisory poll when idle.
        struct timespec const req{0, active ? 50'000'000L : 100'000'000L};
        // NOLINTNEXTLINE(misc-include-cleaner) — nanosleep comes from <ctime>/POSIX
        nanosleep(&req, nullptr);
    }

    RLOG_INFO("sup", "shutdown");
    return 0;
}
