#include "san_role_management/leader_role_manager.hpp"

#include <chrono>
// [C-2 fix v1.5.1] removed <thread> — std::this_thread::sleep_for replaced
// by an rclcpp one-shot wall timer (see watchdogTick).

namespace san_role_management {

LeaderRoleManager::LeaderRoleManager()
    : LeaderRoleManager(rclcpp::NodeOptions())
{}

LeaderRoleManager::LeaderRoleManager(const rclcpp::NodeOptions& options)
    : rclcpp::Node("leader_role_manager", options),
      leader_term_(0)
{
    declareParameters();
    readParameters();
    wireInterfaces();
    RCLCPP_INFO(get_logger(),
        "LeaderRoleManager started: robot_id=%u leader=%u hub=%u deputy=%u",
        robot_id_, leader_robot_id_, hub_robot_id_, deputy_robot_id_);
}

void LeaderRoleManager::declareParameters() {
    declare_parameter<int>("robot_id", 0);
    declare_parameter<int>("leader_robot_id", 1);
    declare_parameter<int>("hub_robot_id", 2);
    declare_parameter<int>("deputy_robot_id", 3);
    declare_parameter<int>("leader_heartbeat_timeout_ms",
                            LEADER_HEARTBEAT_TIMEOUT_MS);
    declare_parameter<int>("watchdog_period_ms", 100);
    declare_parameter<int>("grace_step_ms", SUCCESSION_GRACE_STEP_MS);
    declare_parameter<double>("min_battery_for_leader",
                                static_cast<double>(MIN_BATTERY_FOR_LEADER));
    declare_parameter<double>("min_battery_follower",
                                static_cast<double>(MIN_BATTERY_FOLLOWER));
}

void LeaderRoleManager::readParameters() {
    robot_id_         = static_cast<uint32_t>(get_parameter("robot_id").as_int());
    leader_robot_id_  = static_cast<uint32_t>(
        get_parameter("leader_robot_id").as_int());
    hub_robot_id_     = static_cast<uint32_t>(
        get_parameter("hub_robot_id").as_int());
    deputy_robot_id_  = static_cast<uint32_t>(
        get_parameter("deputy_robot_id").as_int());
    leader_heartbeat_timeout_ms_ =
        get_parameter("leader_heartbeat_timeout_ms").as_int();
    watchdog_period_ms_ = get_parameter("watchdog_period_ms").as_int();
    grace_step_ms_      = get_parameter("grace_step_ms").as_int();
    min_battery_for_leader_ = static_cast<float>(
        get_parameter("min_battery_for_leader").as_double());
    min_battery_follower_ = static_cast<float>(
        get_parameter("min_battery_follower").as_double());

    is_leader_     = (robot_id_ == leader_robot_id_);
    is_deputy_ugv_ = (robot_id_ == deputy_robot_id_);
    is_hub_ugv_    = (robot_id_ == hub_robot_id_);
}

void LeaderRoleManager::wireInterfaces() {
    // [C-1 fix v1.5.1] Single MutuallyExclusive callback group binds
    // every ROS endpoint of this node. Cross-node parallelism is
    // preserved (Leader / Hub / Limp run on separate threads under
    // MTE), but intra-node race conditions are eliminated.
    cb_group_ = create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions sub_opts;
    sub_opts.callback_group = cb_group_;

    rclcpp::QoS qos(10);
    qos.reliable().transient_local();

    announce_pub_ = create_publisher<LeaderAnn>(
        "/swarm/leader/role_announce", qos);
    announce_sub_ = create_subscription<LeaderAnn>(
        "/swarm/leader/role_announce", qos,
        std::bind(&LeaderRoleManager::onLeaderAnnouncement, this,
                  std::placeholders::_1),
        sub_opts);
    status_sub_ = create_subscription<Status>(
        "/swarm/robot_status", 10,
        std::bind(&LeaderRoleManager::onRobotStatus, this,
                  std::placeholders::_1),
        sub_opts);
    watchdog_timer_ = create_wall_timer(
        std::chrono::milliseconds(watchdog_period_ms_),
        std::bind(&LeaderRoleManager::watchdogTick, this),
        cb_group_);
}

void LeaderRoleManager::recordStatus(const Status& s) {
    BatterySnapshot snap;
    snap.robot_id = s.robot_id;
    snap.battery_percent = s.battery_percent;
    snap.sbc1_healthy = s.sbc1_healthy;
    snap.sbc2_healthy = s.sbc2_healthy;
    snap.is_deputy_ugv = s.is_deputy_ugv;
    snap.is_hub_ugv = (s.robot_id == hub_robot_id_);
    snap.timestamp_ms = s.timestamp_ms;
    battery_monitor_.update(snap);

    if (s.robot_id == leader_robot_id_) {
        last_leader_heartbeat_ = now();
    }
}

void LeaderRoleManager::onRobotStatus(Status::SharedPtr msg) {
    if (msg == nullptr) return;
    recordStatus(*msg);
}

void LeaderRoleManager::injectStatusForTest(const Status& s) {
    recordStatus(s);
}

void LeaderRoleManager::injectAnnouncementForTest(const LeaderAnn& msg) {
    auto p = std::make_shared<LeaderAnn>(msg);
    onLeaderAnnouncement(p);
}

void LeaderRoleManager::simulateLeaderHeartbeatLossForTest() {
    // Push the heartbeat far enough into the past that the next
    // watchdog tick treats it as a timeout.
    last_leader_heartbeat_ =
        now() - rclcpp::Duration(
            std::chrono::milliseconds(leader_heartbeat_timeout_ms_ + 500));
}

void LeaderRoleManager::onLeaderAnnouncement(LeaderAnn::SharedPtr msg) {
    if (msg == nullptr) return;
    if (msg->robot_id == robot_id_) {
        if (msg->leader_term > leader_term_.load()) {
            leader_term_.store(msg->leader_term);
        }
        return;
    }

    // Stale - ignore.
    if (msg->leader_term < leader_term_.load()) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
            "Stale LeaderRoleAnnouncement from %u (term=%u local=%u)",
            msg->robot_id, msg->leader_term, leader_term_.load());
        return;
    }
    leader_term_.store(msg->leader_term);

    if (msg->role == LeaderAnn::LEADER_PROMOTED) {
        // Someone else won the succession - we step down if we were
        // mid-grace or already PROMOTED with a lower term.
        if (role_ == LeaderRole::PROMOTED || role_ == LeaderRole::CANDIDATE) {
            RCLCPP_INFO(get_logger(),
                "Yielding Leader role: robot %u took over (term=%u)",
                msg->robot_id, msg->leader_term);
            demoteToFollower("higher_priority_or_higher_term_peer");
        }
        // Original Leader recovering: refresh heartbeat so we stop
        // counting toward a timeout.
        if (msg->robot_id == leader_robot_id_) {
            last_leader_heartbeat_ = now();
        }
    }
}

SuccessionPriority LeaderRoleManager::determineMyPriority() const {
    auto self = battery_monitor_.get(robot_id_);
    if (self.robot_id == 0) {
        // No status snapshot for ourselves yet.
        return SuccessionPriority::LIMP_MODE;
    }

    // 1순위: Deputy UGV
    if (is_deputy_ugv_
        && self.battery_percent >= min_battery_for_leader_
        && self.sbc1_healthy && self.sbc2_healthy)
    {
        return SuccessionPriority::DEPUTY;
    }

    // 2순위: Hub UGV (Deputy 불능 시)
    if (is_hub_ugv_
        && self.battery_percent >= min_battery_for_leader_
        && self.sbc1_healthy && self.sbc2_healthy)
    {
        if (battery_monitor_.isDeputyFailed(deputy_robot_id_)) {
            return SuccessionPriority::HUB;
        }
    }

    // 3순위: 배터리 최대 follower (Deputy + Hub 모두 불능 시)
    if (battery_monitor_.isDeputyFailed(deputy_robot_id_)
        && battery_monitor_.isHubFailed(hub_robot_id_))
    {
        const uint32_t winner = battery_monitor_.pickMaxBatteryFollower(
            leader_robot_id_, hub_robot_id_, deputy_robot_id_,
            min_battery_follower_);
        if (winner == robot_id_ && self.sbc1_healthy
            && self.battery_percent >= min_battery_follower_)
        {
            return SuccessionPriority::BATTERY_MAX;
        }
    }

    return SuccessionPriority::LIMP_MODE;
}

SuccessionPriority LeaderRoleManager::evaluateSuccessionForTest() {
    last_priority_ = determineMyPriority();
    return last_priority_;
}

void LeaderRoleManager::watchdogTick() {
    if (is_leader_) return;                       // original Leader skip
    if (role_ == LeaderRole::PROMOTED) return;    // already serving
    if (!last_leader_heartbeat_.has_value()) return;

    const auto elapsed_ms =
        (now() - *last_leader_heartbeat_).nanoseconds() / 1'000'000;
    if (elapsed_ms < leader_heartbeat_timeout_ms_) return;

    SuccessionPriority my_priority = determineMyPriority();
    last_priority_ = my_priority;

    if (my_priority == SuccessionPriority::LIMP_MODE) {
        // Nobody eligible - LimpModeManager picks up from here.
        return;
    }

    // [C-2 fix v1.5.1 — DCN-2026-003 D-005]
    // Grace period: 200 ms × priority. Higher-priority candidate
    // (lower numeric) self-promotes first; if it does, the
    // LEADER_PROMOTED announcement will arrive during the grace
    // window and onLeaderAnnouncement will move us back to NORMAL
    // BEFORE the deferred promotion fires.
    //
    // Previous implementation: std::this_thread::sleep_for() blocked
    // the watchdog callback for up to 600 ms; under single-thread
    // executor that froze every other timer / subscription in this
    // process, under MTE it blocked the same thread that needs to
    // service onLeaderAnnouncement — so the grace "yield to higher
    // priority" mechanism was structurally broken.
    //
    // New: schedule promotion via a one-shot wall timer. The
    // executor remains free to dispatch onLeaderAnnouncement during
    // the grace window; if the announcement demotes us, the timer
    // fires with role_ ≠ CANDIDATE and is a no-op.
    if (!grace_in_progress_ && !grace_timer_) {
        grace_in_progress_ = true;
        role_ = LeaderRole::CANDIDATE;
        const int grace_ms =
            static_cast<int>(my_priority) * grace_step_ms_;

        grace_timer_ = create_wall_timer(
            std::chrono::milliseconds(grace_ms),
            [this, my_priority]() {
                // One-shot: cancel + drop the timer first so it never
                // re-fires (rclcpp wall timers are periodic by default).
                if (grace_timer_) {
                    grace_timer_->cancel();
                    grace_timer_.reset();
                }
                grace_in_progress_ = false;
                if (role_ != LeaderRole::CANDIDATE) {
                    // A peer with higher priority or term took the
                    // role during the grace window (or we were
                    // demoted for another reason).
                    return;
                }
                promoteToLeader(my_priority);
            },
            cb_group_);
    }
}

void LeaderRoleManager::promoteToLeader(SuccessionPriority priority) {
    role_ = LeaderRole::PROMOTED;
    last_priority_ = priority;
    leader_term_.fetch_add(1);

    LeaderAnn msg;
    msg.header.stamp = now();
    msg.header.frame_id = "swarm";
    msg.robot_id = robot_id_;
    msg.leader_term = leader_term_.load();
    msg.role = LeaderAnn::LEADER_PROMOTED;
    msg.succession_priority = static_cast<uint8_t>(priority);
    msg.reason = "leader_heartbeat_timeout";
    msg.timestamp_ms = nowMs();

    auto self = battery_monitor_.get(robot_id_);
    msg.battery_percent = self.battery_percent;

    if (announce_pub_) announce_pub_->publish(msg);

    RCLCPP_INFO(get_logger(),
        "Promoted to Leader (priority=%u, leader_term=%u, battery=%.1f%%)",
        msg.succession_priority, leader_term_.load(), msg.battery_percent);
}

void LeaderRoleManager::demoteToFollower(const std::string& reason) {
    role_ = LeaderRole::DEMOTED;
    LeaderAnn msg;
    msg.header.stamp = now();
    msg.header.frame_id = "swarm";
    msg.robot_id = robot_id_;
    msg.leader_term = leader_term_.load();
    msg.role = LeaderAnn::LEADER_DEMOTED;
    msg.reason = reason;
    msg.timestamp_ms = nowMs();
    if (announce_pub_) announce_pub_->publish(msg);
}

uint64_t LeaderRoleManager::nowMs() const {
    return static_cast<uint64_t>(now().nanoseconds() / 1'000'000ll);
}

}  // namespace san_role_management
