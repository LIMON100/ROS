// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_lte_redundancy/lte_role_manager.hpp"

#include <algorithm>
#include <chrono>

using namespace std::chrono_literals;

namespace san_lte_redundancy
{

const char * roleToString(LTERole r)
{
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

LTERoleManager::LTERoleManager(const rclcpp::NodeOptions & options)
: rclcpp::Node("lte_role_manager", options),
  lte_term_(0)
{
  declareParameters();
  readParameters();

  uci_ = std::make_unique<Mwan3UciController>(get_logger());
  ubus_ = std::make_unique<Mwan3UbusMonitor>(get_logger());

  wireInterfaces();
  wireUbusCallback();

  RCLCPP_INFO(
    get_logger(),
    "LTERoleManager started: robot_id=%u hub=%s backup=%s chain_size=%zu",
    robot_id_,
    is_hub_ ? "yes" : "no",
    is_backup_designated_ ? "yes" : "no",
    lte_backup_chain_.size());
  publishStatus();
}

LTERoleManager::LTERoleManager(
  const rclcpp::NodeOptions & options,
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

void LTERoleManager::declareParameters()
{
  declare_parameter<int>("robot_id", 0);
  declare_parameter<int>("hub_robot_id", 2);
  declare_parameter<std::vector<int64_t>>(
    "lte_backup_chain", std::vector<int64_t>{3, 4, 5, 6, 7, 8});
  declare_parameter<double>("hub_lte_down_timeout_s", 8.0);
  declare_parameter<double>("ppp_activation_timeout_s", 2.0);
  declare_parameter<double>("watchdog_period_s", 1.0);
}

void LTERoleManager::readParameters()
{
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

  std::lock_guard<std::recursive_mutex> lock(state_mu_);
  if (is_hub_) {
    role_ = LTERole::HUB_NORMAL;
  } else if (is_backup_designated_) {
    role_ = LTERole::BACKUP_STANDBY;
  } else {
    role_ = LTERole::NONE;
  }
}

void LTERoleManager::wireInterfaces()
{
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
    std::bind(
      &LTERoleManager::onAnnouncement, this,
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

  // R-15 (LR1): activation timer ticks every 100 ms while a
  // promotion is in flight. Created up-front; immediately cancelled
  // — promoteAsync() will reset() it.
  activation_timer_ = create_wall_timer(
    100ms, std::bind(&LTERoleManager::activationTick, this),
    cb_group_);
  activation_timer_->cancel();
}

void LTERoleManager::wireUbusCallback()
{
  if (!ubus_) {return;}
  ubus_->onLteStatusChange(
    [this](const std::string & iface, bool is_up) {
      onLocalLteStatusChange(iface, is_up);
    });
}

bool LTERoleManager::isLteActive() const
{
  std::lock_guard<std::recursive_mutex> lock(state_mu_);
  return role_ == LTERole::LTE_ACTIVE ||
         role_ == LTERole::BACKUP_ACTIVATING;
}

bool LTERoleManager::isInBackupChain() const
{
  return std::find(
    lte_backup_chain_.begin(), lte_backup_chain_.end(),
    static_cast<int64_t>(robot_id_)) !=
         lte_backup_chain_.end();
}

bool LTERoleManager::amFirstInBackupChain() const
{
  if (lte_backup_chain_.empty()) {return false;}
  return lte_backup_chain_.front() == static_cast<int64_t>(robot_id_);
}

// ★ PATCH 2026-05-13 (LR3, LR4): equal-term tiebreak.
// Returns true iff the incoming promotion msg should preempt our
// current LTE_ACTIVE role. Lower robot_id wins on equal term.
bool LTERoleManager::incomingPromotionPreempts(
  const combat_robot_msgs::msg::LTERoleAnnouncement & msg) const
{
  const uint32_t local_term = lte_term_.load();
  if (msg.lte_term > local_term) {return true;}
  if (msg.lte_term < local_term) {return false;}
  // Equal term — lower robot_id is the canonical winner.
  return msg.robot_id < robot_id_;
}

void LTERoleManager::onAnnouncement(
  combat_robot_msgs::msg::LTERoleAnnouncement::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  handleHubMessage(*msg);
  handleBackupMessage(*msg);
}

void LTERoleManager::handleHubMessage(
  const combat_robot_msgs::msg::LTERoleAnnouncement & msg)
{
  // Ignore self-broadcasts on the reliable bus loopback (still
  // ratchet our term up).
  if (msg.robot_id == robot_id_) {
    uint32_t cur = lte_term_.load();
    while (msg.lte_term > cur &&
      !lte_term_.compare_exchange_weak(cur, msg.lte_term))
    {}
    return;
  }

  // ★ PATCH 2026-05-13 (LR3): split-brain check uses >= with a
  // wraparound-safe comparison. Pre-patch: `> lte_term - 1` was
  // an underflow on term=0 and an awkward way to say `>=`. The
  // ratchet in handleBackupMessage / incomingPromotionPreempts
  // handles the actual tiebreak.
  if (msg.lte_term < lte_term_.load()) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 1000,
      "stale LTE announcement from robot %u term=%u local=%u - ignoring",
      msg.robot_id, msg.lte_term, lte_term_.load());
    return;
  }
  // Ratchet local term up to received.
  {
    uint32_t cur = lte_term_.load();
    while (msg.lte_term > cur &&
      !lte_term_.compare_exchange_weak(cur, msg.lte_term))
    {}
  }

  if (!is_hub_) {return;}

  using Ann = combat_robot_msgs::msg::LTERoleAnnouncement;
  if (msg.role == Ann::LTE_PROMOTED) {
    RCLCPP_INFO(
      get_logger(),
      "Robot %u took over LTE gateway (term=%u, reason=%s)",
      msg.robot_id, msg.lte_term, msg.reason.c_str());
    std::lock_guard<std::recursive_mutex> lock(state_mu_);
    if (role_ == LTERole::HUB_DEMOTED) {
      role_ = LTERole::HUB_RECOVERING;
      publishStatus();
    }
  }
}

void LTERoleManager::handleBackupMessage(
  const combat_robot_msgs::msg::LTERoleAnnouncement & msg)
{
  if (!is_backup_designated_) {return;}
  if (msg.robot_id == robot_id_) {return;}

  using Ann = combat_robot_msgs::msg::LTERoleAnnouncement;
  std::lock_guard<std::recursive_mutex> lock(state_mu_);

  // Hub announced it lost LTE -> first-in-chain promotes itself.
  if (msg.role == Ann::LTE_DEMOTED && msg.robot_id == hub_robot_id_) {
    if (!amFirstInBackupChain()) {
      RCLCPP_INFO(
        get_logger(),
        "Hub LTE down (term=%u) but I am not first in chain; standby",
        msg.lte_term);
      return;
    }
    if (role_ == LTERole::BACKUP_ACTIVATING ||
      role_ == LTERole::LTE_ACTIVE)
    {
      // Already activating / active — no need to start again.
      return;
    }
    RCLCPP_WARN(
      get_logger(),
      "Hub LTE down (term=%u) - promoting self.", msg.lte_term);
    role_ = LTERole::BACKUP_ACTIVATING;
    promoteAsync("hub_lte_down");
    return;
  }

  // Hub announced recovery -> backup demotes.
  if (msg.role == Ann::LTE_PROMOTED && msg.robot_id == hub_robot_id_ &&
    role_ == LTERole::LTE_ACTIVE)
  {
    RCLCPP_INFO(
      get_logger(),
      "Hub recovered (term=%u) - demoting self.", msg.lte_term);
    role_ = LTERole::LTE_DEACTIVATING;
    demote("hub_recovered");
    return;
  }

  // ★ PATCH 2026-05-13 (LR4): chain-fight — a peer's promotion
  // preempts ours iff incomingPromotionPreempts() says so.
  if (msg.role == Ann::LTE_PROMOTED &&
    (role_ == LTERole::LTE_ACTIVE ||
    role_ == LTERole::BACKUP_ACTIVATING) &&
    incomingPromotionPreempts(msg))
  {
    RCLCPP_WARN(
      get_logger(),
      "Higher-priority promotion from robot %u (term=%u, "
      "local=%u, my_id=%u) - demoting self.",
      msg.robot_id, msg.lte_term, lte_term_.load(), robot_id_);
    // Cancel any in-flight activation FSM.
    if (activation_timer_ && promotion_.in_flight) {
      activation_timer_->cancel();
      promotion_.in_flight = false;
    }
    role_ = LTERole::LTE_DEACTIVATING;
    demote("split_brain_yield");
  }
}

void LTERoleManager::onLocalLteStatusChange(
  const std::string & iface,
  bool is_up)
{
  if (iface != "wan_lte") {return;}
  std::lock_guard<std::recursive_mutex> lock(state_mu_);

  if (!is_hub_) {
    // Backup nodes use this signal to confirm PPP came up during
    // promote(); the activation FSM checks ubus_->isLteUp().
    return;
  }

  // Hub side - track when our own LTE drops so we can self-demote.
  if (!is_up && role_ == LTERole::HUB_NORMAL) {
    hub_lte_down_detected_at_ = now();
    role_ = LTERole::HUB_FAILING;
    RCLCPP_WARN(
      get_logger(),
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
  if (is_up && (role_ == LTERole::HUB_DEMOTED ||
    role_ == LTERole::HUB_RECOVERING))
  {
    const uint32_t new_term = lte_term_.load() + 2;
    lte_term_.store(new_term);
    broadcastRole(
      combat_robot_msgs::msg::LTERoleAnnouncement::LTE_PROMOTED,
      "hub_recovered");
    role_ = LTERole::HUB_NORMAL;
    if (uci_) {uci_->setLteWeight(100);}
    if (ubus_) {ubus_->reloadMwan3Service();}
    publishStatus();
    RCLCPP_INFO(get_logger(), "Hub LTE re-promoted (term=%u)", new_term);
  }
}

void LTERoleManager::watchdogTick()
{
  std::lock_guard<std::recursive_mutex> lock(state_mu_);

  // Hub: if we have been FAILING for longer than the grace timeout,
  // self-demote and broadcast so the chain head takes over.
  if (is_hub_ && role_ == LTERole::HUB_FAILING &&
    hub_lte_down_detected_at_.has_value())
  {
    const auto elapsed =
      (now() - *hub_lte_down_detected_at_).seconds();
    if (elapsed >= hub_lte_down_timeout_s_) {
      RCLCPP_ERROR(
        get_logger(),
        "Hub LTE down %.1fs >= %.1fs timeout - self-demoting",
        elapsed, hub_lte_down_timeout_s_);
      role_ = LTERole::HUB_DEMOTED;
      const uint32_t new_term = lte_term_.load() + 1;
      lte_term_.store(new_term);
      if (uci_) {uci_->setLteWeight(0);}
      if (ubus_) {ubus_->reloadMwan3Service();}
      broadcastRole(
        combat_robot_msgs::msg::LTERoleAnnouncement::LTE_DEMOTED,
        "hub_lte_down");
      publishStatus();
    }
  }

  // Backup: refresh ubus sync state so isLteUp stays current even
  // without hotplug events (some OpenWrt builds throttle them).
  if (ubus_) {ubus_->refreshLteStatus();}
}

// ★ PATCH 2026-05-13 (LR1): async promotion entry point.
// Performs the synchronous setup (UCI / reload) — those are quick C
// API calls that don't block — then schedules an activation_timer_
// to poll for PPP-up until the deadline.
void LTERoleManager::promoteAsync(const std::string & reason)
{
  if (!uci_ || !ubus_) {
    RCLCPP_ERROR(get_logger(), "promoteAsync() without UCI/UBUS bound");
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

  // 3. Schedule async wait. The activation timer ticks every 100 ms;
  //    each tick refreshes ubus and checks isLteUp(). On success it
  //    bumps term + broadcasts + transitions to LTE_ACTIVE. On
  //    deadline it logs and stays in BACKUP_ACTIVATING (caller can
  //    retry).
  promotion_.reason = reason;
  promotion_.deadline = now() + rclcpp::Duration::from_seconds(
    ppp_activation_timeout_s_);
  promotion_.in_flight = true;
  if (activation_timer_) {activation_timer_->reset();}
  RCLCPP_INFO(
    get_logger(),
    "promoteAsync: weight set, mwan3 reloaded, waiting for PPP "
    "(deadline %.1fs)", ppp_activation_timeout_s_);
}

void LTERoleManager::activationTick()
{
  std::lock_guard<std::recursive_mutex> lock(state_mu_);
  if (!promotion_.in_flight) {
    if (activation_timer_) {activation_timer_->cancel();}
    return;
  }
  if (!ubus_) {
    promotion_.in_flight = false;
    activation_timer_->cancel();
    return;
  }
  ubus_->refreshLteStatus();
  if (ubus_->isLteUp()) {
    // PPP came up — finalize promotion.
    const uint32_t new_term = lte_term_.load() + 1;
    lte_term_.store(new_term);
    broadcastRole(
      combat_robot_msgs::msg::LTERoleAnnouncement::LTE_PROMOTED,
      promotion_.reason);
    role_ = LTERole::LTE_ACTIVE;
    publishStatus();
    promotion_.in_flight = false;
    activation_timer_->cancel();
    RCLCPP_INFO(
      get_logger(),
      "Promoted to LTE_ACTIVE (term=%u, reason=%s)",
      new_term, promotion_.reason.c_str());
    return;
  }
  // Deadline check.
  if (now() >= promotion_.deadline) {
    RCLCPP_ERROR(
      get_logger(),
      "LTE PPP did not come up within %.1fs - giving up promote",
      ppp_activation_timeout_s_);
    promotion_.in_flight = false;
    activation_timer_->cancel();
    // Stay BACKUP_ACTIVATING; operator can re-trigger.
  }
}

void LTERoleManager::demote(const std::string & reason)
{
  // Caller holds state_mu_.
  if (uci_) {uci_->setLteWeight(0);}
  if (ubus_) {ubus_->reloadMwan3Service();}
  const uint32_t new_term = lte_term_.load() + 1;
  lte_term_.store(new_term);
  broadcastRole(
    combat_robot_msgs::msg::LTERoleAnnouncement::LTE_DEMOTED,
    reason);
  role_ = LTERole::BACKUP_STANDBY;
  publishStatus();
  RCLCPP_INFO(
    get_logger(), "Demoted to BACKUP_STANDBY (term=%u, reason=%s)",
    new_term, reason.c_str());
}

void LTERoleManager::broadcastRole(
  uint8_t announcement_role,
  const std::string & reason)
{
  if (!announce_pub_) {return;}
  combat_robot_msgs::msg::LTERoleAnnouncement msg;
  msg.header.stamp = now();
  msg.header.frame_id = "swarm";
  msg.sequence = ++announce_seq_;       // ★ PATCH (LR8): atomic
  msg.robot_id = robot_id_;
  msg.lte_term = lte_term_.load();
  msg.role = announcement_role;
  msg.lte_active = (role_ == LTERole::LTE_ACTIVE ||
    role_ == LTERole::BACKUP_ACTIVATING);
  msg.reason = reason;
  msg.timestamp_ms = static_cast<uint64_t>(
    now().nanoseconds() / 1'000'000ll);

  msg.lte_backup_chain_position = 0;
  for (std::size_t i = 0; i < lte_backup_chain_.size(); ++i) {
    if (lte_backup_chain_[i] == static_cast<int64_t>(robot_id_)) {
      msg.lte_backup_chain_position = static_cast<uint32_t>(i + 1);
      break;
    }
  }
  announce_pub_->publish(msg);
}

void LTERoleManager::publishStatus()
{
  if (!status_pub_) {return;}
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
  s.lte_active = (role_ == LTERole::LTE_ACTIVE ||
    role_ == LTERole::BACKUP_ACTIVATING);
  s.is_lte_backup_designated = is_backup_designated_;
  s.timestamp_ms = static_cast<uint64_t>(
    now().nanoseconds() / 1'000'000ll);
  status_pub_->publish(s);
}

void LTERoleManager::injectAnnouncementForTest(
  const combat_robot_msgs::msg::LTERoleAnnouncement & msg)
{
  auto p = std::make_shared<combat_robot_msgs::msg::LTERoleAnnouncement>(msg);
  onAnnouncement(p);
}

void LTERoleManager::injectLocalLteStatusForTest(bool is_up)
{
  onLocalLteStatusChange("wan_lte", is_up);
}

void LTERoleManager::tickActivationForTest()
{
  activationTick();
}

}  // namespace san_lte_redundancy
