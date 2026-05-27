#include "san_lte_redundancy/lte_role_manager.hpp"

#include <algorithm>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

namespace san_lte_redundancy {

const char* roleToString(LTERole r) {
    switch (r) {
        case LTERole::NONE:               return "NONE";
        case LTERole::BACKUP_STANDBY:     return "BACKUP_STANDBY";
        case LTERole::BACKUP_ACTIVATING:  return "BACKUP_ACTIVATING";
        case LTERole::LTE_ACTIVE:         return "LTE_ACTIVE";
        case LTERole::LTE_DEACTIVATING:   return "LTE_DEACTIVATING";
        case LTERole::HUB_NORMAL:         return "HUB_NORMAL";
        case LTERole::HUB_FAILING:        return "HUB_FAILING";
        case LTERole::HUB_DEMOTED:        return "HUB_DEMOTED";
        case LTERole::HUB_RECOVERING:     return "HUB_RECOVERING";
    }
    return "?";
}

LTERoleManager::LTERoleManager()
    : LTERoleManager(rclcpp::NodeOptions())
{}

LTERoleManager::LTERoleManager(const rclcpp::NodeOptions& options)
    : rclcpp::Node("lte_role_manager", options),
      lte_term_(0)
{
    declareParameters();
    readParameters();

    uci_  = std::make_unique<Mwan3UciController>(get_logger());
    ubus_ = std::make_unique<Mwan3UbusMonitor>(get_logger());

    wireInterfaces();
    wireUbusCallback();

    RCLCPP_INFO(get_logger(),
        "LTERoleManager started: robot_id=%u hub=%s backup=%s chain_size=%zu",
        robot_id_,
        is_hub_ ? "yes" : "no",
        is_backup_designated_ ? "yes" : "no",
        lte_backup_chain_.size());
    publishStatus();
}

LTERoleManager::LTERoleManager(const rclcpp::NodeOptions& options,
                               std::unique_ptr<Mwan3UciController> uci,
                               std::unique_ptr<Mwan3UbusMonitor> ubus)
    : rclcpp::Node("lte_role_manager", options),
      lte_term_(0),
      uci_(std::move(uci)),
      ubus_(std::move(ubus))
{
    declareParameters();
    readParameters();
    wireInterfaces();
    wireUbusCallback();
    publishStatus();
}

void LTERoleManager::declareParameters() {
    declare_parameter<int>("robot_id", 0);
    declare_parameter<int>("hub_robot_id", 2);
    // v1.5 (DCN-2026-001 D-001): all UGVs (S2..S8) ship with LTE modem
    // standard. chain extended from [3, 5] (v1.3: Hub + S3 backup) to
    // [3, 4, 5, 6, 7, 8] — Deputy (S3) is 1st backup, followers next.
    declare_parameter<std::vector<int64_t>>(
        "lte_backup_chain", std::vector<int64_t>{3, 4, 5, 6, 7, 8});
    declare_parameter<double>("hub_lte_down_timeout_s", 8.0);
    declare_parameter<double>("ppp_activation_timeout_s", 2.0);
    declare_parameter<double>("watchdog_period_s", 1.0);
}

void LTERoleManager::readParameters() {
    robot_id_ = static_cast<uint32_t>(get_parameter("robot_id").as_int());
    hub_robot_id_ = static_cast<uint32_t>(get_parameter("hub_robot_id").as_int());
    lte_backup_chain_ = get_parameter("lte_backup_chain").as_integer_array();
    hub_lte_down_timeout_s_ =
        get_parameter("hub_lte_down_timeout_s").as_double();
    ppp_activation_timeout_s_ =
        get_parameter("ppp_activation_timeout_s").as_double();
    watchdog_period_s_ = get_parameter("watchdog_period_s").as_double();

    is_hub_ = (robot_id_ == hub_robot_id_);
    is_backup_designated_ = isInBackupChain();

    if (is_hub_) {
        role_ = LTERole::HUB_NORMAL;
    } else if (is_backup_designated_) {
        role_ = LTERole::BACKUP_STANDBY;
    } else {
        role_ = LTERole::NONE;
    }
}

void LTERoleManager::wireInterfaces() {
    // [C-1 fix v1.5.1] MutuallyExclusive cb_group for sub + timer.
    cb_group_ = create_callback_group(
        rclcpp::CallbackGroupType::MutuallyExclusive);

    rclcpp::SubscriptionOptions sub_opts;
    sub_opts.callback_group = cb_group_;

    rclcpp::QoS announce_qos(10);
    announce_qos.reliable().transient_local();

    announce_pub_ = create_publisher<
        combat_robot_msgs::msg::LTERoleAnnouncement>(
            "/swarm/lte/role_announcement", announce_qos);

    announce_sub_ = create_subscription<
        combat_robot_msgs::msg::LTERoleAnnouncement>(
            "/swarm/lte/role_announcement", announce_qos,
            std::bind(&LTERoleManager::onAnnouncement, this,
                      std::placeholders::_1),
            sub_opts);

    // ROS 2 topic-name validator rejects tokens starting with a digit;
    // see operation_control_node.cpp for the matching fix. Keep both
    // producers in lock-step on the "robot_<id>" prefix.
    status_pub_ = create_publisher<combat_robot_msgs::msg::RobotStatus>(
        "/swarm/robot_" + std::to_string(robot_id_) + "/status",
        rclcpp::QoS(5).reliable());

    const auto period = std::chrono::duration<double>(watchdog_period_s_);
    watchdog_timer_ = create_wall_timer(
        std::chrono::duration_cast<std::chrono::nanoseconds>(period),
        std::bind(&LTERoleManager::watchdogTick, this),
        cb_group_);
}

void LTERoleManager::wireUbusCallback() {
    if (!ubus_) return;
    ubus_->onLteStatusChange(
        [this](const std::string& iface, bool is_up) {
            onLocalLteStatusChange(iface, is_up);
        });
}

bool LTERoleManager::isLteActive() const {
    return role_ == LTERole::LTE_ACTIVE
        || role_ == LTERole::BACKUP_ACTIVATING;
}

bool LTERoleManager::isInBackupChain() const {
    return std::find(lte_backup_chain_.begin(), lte_backup_chain_.end(),
                     static_cast<int64_t>(robot_id_))
        != lte_backup_chain_.end();
}

bool LTERoleManager::amFirstInBackupChain() const {
    if (lte_backup_chain_.empty()) return false;
    return lte_backup_chain_.front() == static_cast<int64_t>(robot_id_);
}

void LTERoleManager::onAnnouncement(
    combat_robot_msgs::msg::LTERoleAnnouncement::SharedPtr msg)
{
    if (msg == nullptr) return;
    handleHubMessage(*msg);
    handleBackupMessage(*msg);
}

void LTERoleManager::handleHubMessage(
    const combat_robot_msgs::msg::LTERoleAnnouncement& msg)
{
    // Ignore self-broadcasts on the reliable bus loopback.
    if (msg.robot_id == robot_id_) {
        lte_term_.store(std::max(lte_term_.load(), msg.lte_term));
        return;
    }

    // split-brain check: drop stale terms.
    if (msg.lte_term < lte_term_.load()) {
        RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 1000,
            "stale LTE announcement from robot %u term=%u local=%u - ignoring",
            msg.robot_id, msg.lte_term, lte_term_.load());
        return;
    }
    lte_term_.store(msg.lte_term);

    if (!is_hub_) return;

    // Another robot took over the LTE gateway -> Hub stays demoted
    // until its own modem recovers and we win the next term.
    using Ann = combat_robot_msgs::msg::LTERoleAnnouncement;
    if (msg.role == Ann::LTE_PROMOTED) {
        RCLCPP_INFO(get_logger(),
            "Robot %u took over LTE gateway (term=%u, reason=%s)",
            msg.robot_id, msg.lte_term, msg.reason.c_str());
        if (role_ == LTERole::HUB_DEMOTED) {
            role_ = LTERole::HUB_RECOVERING;
            publishStatus();
        }
    }
}

void LTERoleManager::handleBackupMessage(
    const combat_robot_msgs::msg::LTERoleAnnouncement& msg)
{
    if (!is_backup_designated_) return;
    if (msg.robot_id == robot_id_) return;

    using Ann = combat_robot_msgs::msg::LTERoleAnnouncement;

    // Hub announced it lost LTE -> first-in-chain promotes itself.
    if (msg.role == Ann::LTE_DEMOTED && msg.robot_id == hub_robot_id_) {
        if (!amFirstInBackupChain()) {
            RCLCPP_INFO(get_logger(),
                "Hub LTE down (term=%u) but I am not first in chain; standby",
                msg.lte_term);
            return;
        }
        RCLCPP_WARN(get_logger(),
            "Hub LTE down (term=%u) - promoting self.", msg.lte_term);
        role_ = LTERole::BACKUP_ACTIVATING;
        promote("hub_lte_down");
        return;
    }

    // Hub announced recovery -> backup demotes.
    if (msg.role == Ann::LTE_PROMOTED && msg.robot_id == hub_robot_id_
        && role_ == LTERole::LTE_ACTIVE)
    {
        RCLCPP_INFO(get_logger(),
            "Hub recovered (term=%u) - demoting self.", msg.lte_term);
        role_ = LTERole::LTE_DEACTIVATING;
        demote("hub_recovered");
        return;
    }

    // A peer with a higher term promoted itself -> stand down to avoid
    // split-brain (covers the chain-fight race).
    if (msg.role == Ann::LTE_PROMOTED && role_ == LTERole::LTE_ACTIVE
        && msg.lte_term > lte_term_.load() - 1)
    {
        RCLCPP_WARN(get_logger(),
            "Higher-term promotion from robot %u (term=%u) - demoting self.",
            msg.robot_id, msg.lte_term);
        role_ = LTERole::LTE_DEACTIVATING;
        demote("split_brain_yield");
    }
}

void LTERoleManager::onLocalLteStatusChange(const std::string& iface,
                                            bool is_up)
{
    if (iface != "wan_lte") return;

    if (!is_hub_) {
        // Backup nodes use this signal to confirm PPP came up during
        // promote(); the hot-path is in promote() itself.
        return;
    }

    // Hub side - track when our own LTE drops so we can self-demote.
    if (!is_up && role_ == LTERole::HUB_NORMAL) {
        hub_lte_down_detected_at_ = now();
        role_ = LTERole::HUB_FAILING;
        RCLCPP_WARN(get_logger(),
            "Hub LTE failing - %.1fs grace period before self-demote",
            hub_lte_down_timeout_s_);
        return;
    }
    if (is_up && role_ == LTERole::HUB_FAILING) {
        hub_lte_down_detected_at_.reset();
        role_ = LTERole::HUB_NORMAL;
        RCLCPP_INFO(get_logger(), "Hub LTE recovered within grace period");
        return;
    }
    if (is_up && (role_ == LTERole::HUB_DEMOTED
                  || role_ == LTERole::HUB_RECOVERING))
    {
        // Modem is back; we hold in RECOVERING until the active backup
        // demotes itself, then we re-promote with term+2.
        const uint32_t new_term = lte_term_.load() + 2;
        lte_term_.store(new_term);
        broadcastRole(combat_robot_msgs::msg::LTERoleAnnouncement::LTE_PROMOTED,
                      "hub_recovered");
        role_ = LTERole::HUB_NORMAL;
        if (uci_) uci_->setLteWeight(100);
        if (ubus_) ubus_->reloadMwan3Service();
        publishStatus();
        RCLCPP_INFO(get_logger(), "Hub LTE re-promoted (term=%u)", new_term);
    }
}

void LTERoleManager::watchdogTick() {
    // Hub: if we have been FAILING for longer than the grace timeout,
    // self-demote and broadcast so the chain head takes over.
    if (is_hub_ && role_ == LTERole::HUB_FAILING
        && hub_lte_down_detected_at_.has_value())
    {
        const auto elapsed =
            (now() - *hub_lte_down_detected_at_).seconds();
        if (elapsed >= hub_lte_down_timeout_s_) {
            RCLCPP_ERROR(get_logger(),
                "Hub LTE down %.1fs >= %.1fs timeout - self-demoting",
                elapsed, hub_lte_down_timeout_s_);
            role_ = LTERole::HUB_DEMOTED;
            const uint32_t new_term = lte_term_.load() + 1;
            lte_term_.store(new_term);
            if (uci_) uci_->setLteWeight(0);
            if (ubus_) ubus_->reloadMwan3Service();
            broadcastRole(
                combat_robot_msgs::msg::LTERoleAnnouncement::LTE_DEMOTED,
                "hub_lte_down");
            publishStatus();
        }
    }

    // Backup: refresh ubus sync state so isLteUp stays current even
    // without hotplug events (some OpenWrt builds throttle them).
    if (ubus_) ubus_->refreshLteStatus();
}

void LTERoleManager::promote(const std::string& reason) {
    if (!uci_ || !ubus_) {
        RCLCPP_ERROR(get_logger(), "promote() without UCI/UBUS bound");
        return;
    }

    // 1. Bump weight via libuci - no shell.
    if (!uci_->setLteWeight(100)) {
        RCLCPP_ERROR(get_logger(), "UCI weight=100 failed - promote aborted");
        return;
    }
    // 2. mwan3 reload via libubus - no shell.
    if (!ubus_->reloadMwan3Service()) {
        RCLCPP_ERROR(get_logger(), "mwan3 reload failed - promote aborted");
        return;
    }
    // 3. Wait up to ppp_activation_timeout_s_ for the modem to come up.
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::duration<double>(ppp_activation_timeout_s_));
    while (std::chrono::steady_clock::now() < deadline) {
        ubus_->refreshLteStatus();
        if (ubus_->isLteUp()) break;
        std::this_thread::sleep_for(100ms);
    }
    if (!ubus_->isLteUp()) {
        RCLCPP_ERROR(get_logger(),
            "LTE PPP did not come up within %.1fs - giving up promote",
            ppp_activation_timeout_s_);
        return;
    }

    // 4. Bump term + broadcast.
    const uint32_t new_term = lte_term_.load() + 1;
    lte_term_.store(new_term);
    broadcastRole(combat_robot_msgs::msg::LTERoleAnnouncement::LTE_PROMOTED,
                  reason);
    role_ = LTERole::LTE_ACTIVE;
    publishStatus();
    RCLCPP_INFO(get_logger(), "Promoted to LTE_ACTIVE (term=%u, reason=%s)",
                new_term, reason.c_str());
}

void LTERoleManager::demote(const std::string& reason) {
    if (uci_) uci_->setLteWeight(0);
    if (ubus_) ubus_->reloadMwan3Service();
    const uint32_t new_term = lte_term_.load() + 1;
    lte_term_.store(new_term);
    broadcastRole(combat_robot_msgs::msg::LTERoleAnnouncement::LTE_DEMOTED,
                  reason);
    role_ = LTERole::BACKUP_STANDBY;
    publishStatus();
    RCLCPP_INFO(get_logger(), "Demoted to BACKUP_STANDBY (term=%u, reason=%s)",
                new_term, reason.c_str());
}

void LTERoleManager::broadcastRole(uint8_t announcement_role,
                                   const std::string& reason)
{
    if (!announce_pub_) return;
    combat_robot_msgs::msg::LTERoleAnnouncement msg;
    msg.header.stamp = now();
    msg.header.frame_id = "swarm";
    msg.sequence = ++announce_seq_;
    msg.robot_id = robot_id_;
    msg.lte_term = lte_term_.load();
    msg.role = announcement_role;
    msg.lte_active = isLteActive();
    msg.reason = reason;
    msg.timestamp_ms = static_cast<uint64_t>(
        now().nanoseconds() / 1'000'000ll);

    // Chain position: 0 for hub, 1-based for designated backups,
    // 0 for non-designated robots (they should never broadcast a
    // promoted role anyway).
    msg.lte_backup_chain_position = 0;
    for (std::size_t i = 0; i < lte_backup_chain_.size(); ++i) {
        if (lte_backup_chain_[i] == static_cast<int64_t>(robot_id_)) {
            msg.lte_backup_chain_position = static_cast<uint32_t>(i + 1);
            break;
        }
    }

    announce_pub_->publish(msg);
}

void LTERoleManager::publishStatus() {
    if (!status_pub_) return;
    combat_robot_msgs::msg::RobotStatus s;
    s.header.stamp = now();
    s.robot_id = robot_id_;
    s.robot_role = is_hub_ ? 2 : 0;
    s.battery_soc = 1.0f;
    s.locomotion_mode = 0;
    s.tier = 0;
    s.slam_healthy = true;
    s.perception_healthy = true;
    s.comm_healthy = true;
    s.lte_active = isLteActive();
    s.is_lte_backup_designated = is_backup_designated_;
    s.timestamp_ms = static_cast<uint64_t>(
        now().nanoseconds() / 1'000'000ll);
    status_pub_->publish(s);
}

void LTERoleManager::injectAnnouncementForTest(
    const combat_robot_msgs::msg::LTERoleAnnouncement& msg)
{
    auto p = std::make_shared<combat_robot_msgs::msg::LTERoleAnnouncement>(msg);
    onAnnouncement(p);
}

void LTERoleManager::injectLocalLteStatusForTest(bool is_up) {
    onLocalLteStatusChange("wan_lte", is_up);
}

}  // namespace san_lte_redundancy
