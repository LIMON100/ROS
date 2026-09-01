// gz_track_bridge — Gazebo truth -> TrackBus (SITL only, RIPOSTE_WITH_GZ).
//
// Subscribes to the Gazebo world pose feed, computes the target's position and
// velocity RELATIVE to the ownship in the ownship's body FRD frame (exactly the
// TrackState contract the seeker would publish), and writes it to the TrackBus
// shm. This lets the OBC's GuidanceSource + SafetyMonitor chase a real, moving,
// physically-simulated target in Gazebo without needing the perception stack.
//
// It is a TEST harness — it stands in for riposte-seeker. Never built into a
// flight image (gated behind RIPOSTE_WITH_GZ, off by default).
//
// Usage: gz_track_bridge <world> [ownship_model] [target]
//   world           gz world name (e.g. riposte_closure)
//   ownship_model  PX4 vehicle model name (default x500_0)
//   target          target model/actor name (default target), OR a name ending
//                   in '*' to track EVERY matching model (e.g. "balloon_*")
//
// MULTI-TARGET MODE (target ends in '*') stands in for the seeker during the
// balloon trials: several targets are scattered in the world, and to be a useful
// stand-in the bridge has to behave like perception does — which means modelling
// what the camera can actually SEE. A target counts as detected only inside the
// sensor's range and field of view; the nearest visible one becomes the primary
// (largest apparent target, REQ-001 R-8) and the visible count is published.
// Without the FOV model every balloon would be permanently visible and the
// patrol's search sweep would never be exercised at all.
//   GZ_BRIDGE_MAX_RANGE_M  detection range, metres (default 60 — the wide
//                          camera's tiled range on a 25 cm balloon, REQ-001 §3)
//   GZ_BRIDGE_HFOV_DEG     horizontal field of view (default 60)
//   GZ_BRIDGE_VFOV_DEG     vertical field of view (default 34)
//
// SINGLE-TARGET MODE is unchanged and deliberately has NO visibility gate: the
// closure scenarios inject target truth to exercise guidance, and adding a
// FOV there would silently change what those tests measure.

// NOLINTNEXTLINE(misc-include-cleaner) — generated header providing gz::msgs::Pose_V
#include <gz/msgs/pose_v.pb.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

#include "riposte/Clock.h"
#include "riposte/SeqSlot.h"
#include "riposte/Tunables.h"
#include "riposte/Types.h"
#include <gz/math/Pose3.hh>
#include <gz/math/Vector3.hh>
#include <gz/transport/Node.hh>

namespace {
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
std::atomic<bool> g_run{true};
void on_signal(int /*sig*/) {
    g_run.store(false);
}

struct Shared {
    std::mutex mtx;
    bool have_def = false;
    bool have_int = false;
    uint64_t msg_count = 0;    // pose messages that carried a tracked model
    uint64_t last_rx_ns = 0;   // mono arrival time of the newest such message
    gz::math::Pose3d def_pose; // ownship world pose (ENU / FLU body)
    gz::math::Pose3d int_pose; // target world pose
    // Multi-target mode: every model matching the prefix, by name. Ids are
    // assigned on first sight and never reused, so a target keeps its identity
    // across frames — which is what the patrol's "already visited this one"
    // bookkeeping is keyed on.
    std::map<std::string, gz::math::Pose3d> targets;
    std::map<std::string, uint32_t> target_ids;
    uint32_t next_id = 1;
};

constexpr double PI = 3.14159265358979323846;

// Sensor model for multi-target mode (see the file header).
struct Visibility {
    double max_range_m = 60.0;
    double hfov_rad = 60.0 * PI / 180.0;
    double vfov_rad = 34.0 * PI / 180.0;
};

// Called only from main() before any transport thread exists. getenv is unsafe
// solely against a concurrent setenv, and this tool never writes the
// environment, so the sequencing — not a lock — is what makes it safe here.
double env_double(const char* name, double fallback) {
    // NOLINTNEXTLINE(concurrency-mt-unsafe)
    const char* v = std::getenv(name);
    if (v == nullptr) {
        return fallback;
    }
    try {
        return std::stod(v);
    } catch (const std::exception&) {
        return fallback;
    }
}

// Which model names the pose feed callback cares about.
struct ModelFilter {
    std::string ownship;
    std::string target; // exact model name, single-target mode
    std::string prefix; // model name prefix, multi-target mode
    bool multi = false;
};

// Absorb one pose-feed sample into `shared`. Split out of main() so the
// subscription stays a one-liner and this stays testable by reading.
// NOLINTNEXTLINE(misc-include-cleaner) — Pose_V is from <gz/msgs/pose_v.pb.h>
void absorb_poses(Shared& shared, const ModelFilter& f, const gz::msgs::Pose_V& msg) {
    const uint64_t rx_ns = riposte::mono_now_ns();
    const std::lock_guard<std::mutex> lock(shared.mtx);
    bool seen = false;
    for (int i = 0; i < msg.pose_size(); ++i) {
        const auto& p = msg.pose(i);
        const gz::math::Pose3d pose(p.position().x(), p.position().y(), p.position().z(),
                                    p.orientation().w(), p.orientation().x(),
                                    p.orientation().y(), p.orientation().z());
        if (p.name() == f.ownship) {
            shared.def_pose = pose;
            shared.have_def = true;
            seen = true;
        } else if (f.multi) {
            if (p.name().rfind(f.prefix, 0) == 0) {
                shared.targets[p.name()] = pose;
                if (shared.target_ids.find(p.name()) == shared.target_ids.end()) {
                    shared.target_ids[p.name()] = shared.next_id++;
                }
                seen = true;
            }
        } else if (p.name() == f.target) {
            shared.int_pose = pose;
            shared.have_int = true;
            seen = true;
        }
    }
    if (seen) {
        shared.last_rx_ns = rx_ns;
        ++shared.msg_count;
    }
}

// Rotate a world-frame ENU offset into the ownship's body FRD frame.
gz::math::Vector3d to_body_frd(const gz::math::Pose3d& def,
                               const gz::math::Vector3d& world_offset) {
    // Body FLU (Gazebo convention) = inverse-rotate the world offset.
    const gz::math::Vector3d flu = def.Rot().RotateVectorReverse(world_offset);
    return {flu.X(), -flu.Y(), -flu.Z()}; // FLU -> FRD (right = -left, down = -up)
}

// Picks the primary target among those the sensor can see, and counts them.
// Returns false when nothing is visible. `primary_frd` / `primary_id` describe
// the NEAREST visible target — for equally-sized balloons the nearest is the
// largest apparent one, which is the R-8 selection rule.
bool select_primary(const gz::math::Pose3d& def,
                    const std::map<std::string, gz::math::Pose3d>& targets,
                    const std::map<std::string, uint32_t>& ids, const Visibility& vis,
                    gz::math::Vector3d& primary_frd, uint32_t& primary_id,
                    unsigned& visible_count) {
    bool found = false;
    double best_range = 0.0;
    visible_count = 0;
    for (const auto& [name, pose] : targets) {
        const gz::math::Vector3d frd = to_body_frd(def, pose.Pos() - def.Pos());
        const double range = frd.Length();
        if (range > vis.max_range_m || range < 1e-6) {
            continue;
        }
        if (frd.X() <= 0.0) {
            continue; // behind the camera
        }
        const double az = std::atan2(frd.Y(), frd.X());
        const double horiz = std::hypot(frd.X(), frd.Y());
        const double el = std::atan2(-frd.Z(), horiz); // FRD z is DOWN
        if (std::fabs(az) > vis.hfov_rad * 0.5 || std::fabs(el) > vis.vfov_rad * 0.5) {
            continue; // outside the field of view
        }
        ++visible_count;
        if (!found || range < best_range) {
            found = true;
            best_range = range;
            primary_frd = frd;
            const auto it = ids.find(name);
            primary_id = (it != ids.end()) ? it->second : 0U;
        }
    }
    return found;
}

// Multi-target publish loop: the seeker stand-in for the balloon trials.
void publish_loop_multi(Shared& shared, riposte::ShmSeqSlot<riposte::TrackState>& bus,
                        const Visibility& vis) {
    constexpr uint64_t STALE_AFTER_NS = 300'000'000ULL;
    uint32_t seq = 0;
    uint32_t prev_id = 0;
    bool have_prev = false;
    gz::math::Vector3d prev_frd;
    uint64_t prev_ns = 0;
    uint64_t last_log_ns = 0;
    while (g_run.load()) {
        gz::math::Pose3d def;
        std::map<std::string, gz::math::Pose3d> targets;
        std::map<std::string, uint32_t> ids;
        bool ready = false;
        uint64_t rx_ns = 0;
        {
            const std::lock_guard<std::mutex> lock(shared.mtx);
            ready = shared.have_def && !shared.targets.empty();
            def = shared.def_pose;
            targets = shared.targets;
            ids = shared.target_ids;
            rx_ns = shared.last_rx_ns;
        }
        const uint64_t now = riposte::mono_now_ns();
        const bool stale = ready && (now - rx_ns > STALE_AFTER_NS);

        riposte::TrackState ts{};
        ts.mono_ns = now;
        ts.seq = ++seq;
        gz::math::Vector3d frd;
        uint32_t id = 0;
        unsigned visible = 0;
        if (ready && !stale && select_primary(def, targets, ids, vis, frd, id, visible)) {
            ts.track_id = id;
            ts.rel_pos_frd_m[0] = static_cast<float>(frd.X());
            ts.rel_pos_frd_m[1] = static_cast<float>(frd.Y());
            ts.rel_pos_frd_m[2] = static_cast<float>(frd.Z());
            // Finite-difference only against the SAME target: a primary handoff
            // would otherwise difference two different balloons into a one-tick
            // velocity spike (the same rule TargetEstimator follows).
            if (have_prev && id == prev_id && now > prev_ns) {
                const double dt = static_cast<double>(now - prev_ns) * 1e-9;
                const gz::math::Vector3d v = (frd - prev_frd) / dt;
                ts.rel_vel_frd_mps[0] = static_cast<float>(v.X());
                ts.rel_vel_frd_mps[1] = static_cast<float>(v.Y());
                ts.rel_vel_frd_mps[2] = static_cast<float>(v.Z());
            }
            prev_frd = frd;
            prev_ns = now;
            prev_id = id;
            have_prev = true;
            ts.quality = 0.9F;
            ts.valid = 1;
            ts.num_targets = static_cast<uint8_t>(visible > 255U ? 255U : visible);
            if (now - last_log_ns >= 1'000'000'000ULL) {
                last_log_ns = now;
                std::printf("targets=%u primary=%u range=%.1f frd=[%.1f %.1f %.1f]\n",
                            visible, id, frd.Length(), frd.X(), frd.Y(), frd.Z());
                (void)std::fflush(stdout);
            }
        } else {
            have_prev = false; // nothing in view: reseed the difference on recovery
            if (now - last_log_ns >= 2'000'000'000ULL) {
                last_log_ns = now;
                // Say WHICH of the three ways this can happen, or a failed run
                // is indistinguishable from a world where nothing is in frame.
                const char* why = nullptr;
                if (!ready) {
                    why = targets.empty() ? "no target poses yet (name prefix?)"
                                          : "no ownship pose yet (model name?)";
                } else if (stale) {
                    why = "pose feed stale (sim paused/stopped?)";
                } else {
                    why = "all targets out of range/FOV";
                }
                std::printf("targets=0 (%s)\n", why);
                (void)std::fflush(stdout);
            }
        }
        bus.write(ts);
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}

// Publish TrackBus at ~30 Hz (seeker cadence) until g_run clears. If the gz
// pose feed stops (sim paused/crashed/model removed) the cached pose must NOT
// keep going out stamped fresh+valid — that would blind the OBC's SM-7
// staleness gate. After STALE_AFTER_NS of pose silence we keep publishing but
// with valid=0 so consumers see the invalidation.
void publish_loop(Shared& shared, riposte::ShmSeqSlot<riposte::TrackState>& bus) {
    constexpr uint64_t STALE_AFTER_NS = 300'000'000ULL;
    uint32_t seq = 0;
    bool have_prev = false;
    gz::math::Vector3d prev_rel_world; // world-frame relative position at prev msg
    uint64_t prev_rx_ns = 0;
    uint64_t last_fd_count = 0;
    gz::math::Vector3d vel_frd = gz::math::Vector3d::Zero;
    bool was_stale = false;
    uint64_t last_log_ns = 0;
    while (g_run.load()) {
        gz::math::Pose3d def;
        gz::math::Pose3d intr;
        bool ready = false;
        uint64_t rx_ns = 0;
        uint64_t msg_count = 0;
        {
            const std::lock_guard<std::mutex> lock(shared.mtx);
            ready = shared.have_def && shared.have_int;
            def = shared.def_pose;
            intr = shared.int_pose;
            rx_ns = shared.last_rx_ns;
            msg_count = shared.msg_count;
        }
        const uint64_t now = riposte::mono_now_ns();
        const bool stale = ready && (now - rx_ns > STALE_AFTER_NS);
        if (stale && !was_stale) {
            (void)std::fprintf(
                stderr,
                "gz_track_bridge: WRN pose feed stale (>%llu ms), publishing "
                "valid=0\n",
                static_cast<unsigned long long>(STALE_AFTER_NS / 1'000'000ULL));
        } else if (!stale && was_stale) {
            std::printf("gz_track_bridge: INFO pose feed recovered\n");
            (void)std::fflush(stdout);
        }
        was_stale = stale;

        riposte::TrackState ts{};
        ts.mono_ns = now;
        ts.seq = ++seq;
        ts.track_id = 1;
        if (ready && !stale) {
            const gz::math::Vector3d rel_world = intr.Pos() - def.Pos();
            const gz::math::Vector3d frd = to_body_frd(def, rel_world);
            // Finite-difference velocity only across real message arrivals:
            // re-sampling the cached pose against wall-clock dt would alias the
            // estimate between 0 and 2x. Difference the WORLD-frame relative
            // position over the wall time between the two arrivals (body-frame
            // differencing would fold ownship rotation into the velocity),
            // then rotate the result into body FRD with the current attitude.
            if (msg_count != last_fd_count) {
                if (have_prev && rx_ns > prev_rx_ns) {
                    const double dt = static_cast<double>(rx_ns - prev_rx_ns) * 1e-9;
                    vel_frd = to_body_frd(def, (rel_world - prev_rel_world) / dt);
                }
                prev_rel_world = rel_world;
                prev_rx_ns = rx_ns;
                have_prev = true;
                last_fd_count = msg_count;
            }
            ts.rel_pos_frd_m[0] = static_cast<float>(frd.X());
            ts.rel_pos_frd_m[1] = static_cast<float>(frd.Y());
            ts.rel_pos_frd_m[2] = static_cast<float>(frd.Z());
            ts.rel_vel_frd_mps[0] = static_cast<float>(vel_frd.X());
            ts.rel_vel_frd_mps[1] = static_cast<float>(vel_frd.Y());
            ts.rel_vel_frd_mps[2] = static_cast<float>(vel_frd.Z());
            ts.quality = 0.9F;
            ts.valid = 1;
            if (now - last_log_ns >= 1'000'000'000ULL) {
                last_log_ns = now;
                std::printf("range=%.1f frd=[%.1f %.1f %.1f]\n", frd.Length(), frd.X(),
                            frd.Y(), frd.Z());
                (void)std::fflush(stdout);
            }
        } else {
            // Stale or not yet tracking: publish valid=0 with zero velocity and
            // restart the FD chain so recovery does not difference across the gap.
            vel_frd = gz::math::Vector3d::Zero;
            have_prev = false;
        }
        bus.write(ts); // valid=0 until both poses seen / while pose feed stale
        std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }
}
} // namespace

int main(int argc, char** argv) {
    (void)std::signal(SIGINT, on_signal);
    (void)std::signal(SIGTERM, on_signal);
    const std::string world = (argc > 1) ? argv[1] : "riposte_closure";
    const std::string ownship = (argc > 2) ? argv[2] : "x500_0";
    const std::string target = (argc > 3) ? argv[3] : "target";
    // A trailing '*' selects multi-target mode and makes the rest a name prefix.
    const bool multi = !target.empty() && target.back() == '*';
    const std::string prefix = multi ? target.substr(0, target.size() - 1) : target;

    Visibility vis;
    vis.max_range_m = env_double("GZ_BRIDGE_MAX_RANGE_M", 60.0);
    vis.hfov_rad = env_double("GZ_BRIDGE_HFOV_DEG", 60.0) * PI / 180.0;
    vis.vfov_rad = env_double("GZ_BRIDGE_VFOV_DEG", 34.0) * PI / 180.0;

    Shared shared;
    gz::transport::Node node;
    const std::string topic = "/world/" + world + "/pose/info";
    // NOLINTNEXTLINE(misc-include-cleaner) — Pose_V is from <gz/msgs/pose_v.pb.h>
    const ModelFilter filter{ownship, target, prefix, multi};
    const std::function<void(const gz::msgs::Pose_V&)> on_poses =
        [&](const gz::msgs::Pose_V& msg) { absorb_poses(shared, filter, msg); };
    if (!node.Subscribe(topic, on_poses)) {
        (void)std::fprintf(stderr, "gz_track_bridge: cannot subscribe %s\n",
                           topic.c_str());
        return 1;
    }
    if (multi) {
        std::printf(
            "gz_track_bridge: %s  ownship=%s targets=%s* (multi) -> TrackBus\n"
            "  sensor: range<=%.0fm hfov=%.0fdeg vfov=%.0fdeg\n",
            topic.c_str(), ownship.c_str(), prefix.c_str(), vis.max_range_m,
            vis.hfov_rad * 180.0 / PI, vis.vfov_rad * 180.0 / PI);
    } else {
        std::printf("gz_track_bridge: %s  ownship=%s target=%s -> TrackBus\n",
                    topic.c_str(), ownship.c_str(), target.c_str());
    }

    riposte::ShmSeqSlot<riposte::TrackState> bus;
    if (!bus.open(riposte::tun::SHM_TRACK,
                  riposte::ShmSeqSlot<riposte::TrackState>::Role::WRITER)) {
        (void)std::fprintf(stderr, "gz_track_bridge: cannot open TrackBus shm\n");
        return 1;
    }

    if (multi) {
        publish_loop_multi(shared, bus, vis);
    } else {
        publish_loop(shared, bus);
    }
    std::printf("gz_track_bridge: shutdown\n");
    return 0;
}
