// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

// SAN v1.5 PHASE 8 - LeaderRoleManager (PATCHED 2026-05-13).

#include "san_role_management/leader_role_manager.hpp"

#include <chrono>
// [R-10 + main C-2 fix v1.5.1] removed <thread> — std::this_thread::sleep_for
// replaced by an rclcpp one-shot wall timer (see watchdogTick).

namespace san_role_management
{

LeaderRoleManager::LeaderRoleManager()
: LeaderRoleManager(rclcpp::NodeOptions())
{}

LeaderRoleManager::LeaderRoleManager(const rclcpp::NodeOptions & options)
: rclcpp::Node("leader_role_manager", options),
  leader_term_(0)
{
  declareParameters();
  readParameters();
  wireInterfaces();
  RCLCPP_INFO(
    get_logger(),
    "LeaderRoleManager started: robot_id=%u leader=%u hub=%u deputy=%u",
    robot_id_, leader_robot_id_, hub_robot_id_, deputy_robot_id_);
}

void LeaderRoleManager::declareParameters()
{
  declare_parameter<int>("robot_id", 0);
  declare_parameter<int>("leader_robot_id", 1);
  declare_parameter<int>("hub_robot_id", 2);
  declare_parameter<int>("deputy_robot_id", 3);
  declare_parameter<int>(
    "leader_heartbeat_timeout_ms",
    LEADER_HEARTBEAT_TIMEOUT_MS);
  declare_parameter<int>("watchdog_period_ms", 100);
  declare_parameter<int>("grace_step_ms", SUCCESSION_GRACE_STEP_MS);
  declare_parameter<double>(
    "min_battery_for_leader",
    static_cast<double>(MIN_BATTERY_FOR_LEADER));
  declare_parameter<double>(
    "min_battery_follower",
    static_cast<double>(MIN_BATTERY_FOLLOWER));
  // ★ PATCH 2026-05-13:
  declare_parameter<int>("demote_cooldown_ms", 2000);
  declare_parameter<int>("status_max_age_ms", 5000);
}

void LeaderRoleManager::readParameters()
{
  robot_id_ = static_cast<uint32_t>(get_parameter("robot_id").as_int());
  leader_robot_id_ = static_cast<uint32_t>(
    get_parameter("leader_robot_id").as_int());
  hub_robot_id_ = static_cast<uint32_t>(
    get_parameter("hub_robot_id").as_int());
  deputy_robot_id_ = static_cast<uint32_t>(
    get_parameter("deputy_robot_id").as_int());
  leader_heartbeat_timeout_ms_ =
    get_parameter("leader_heartbeat_timeout_ms").as_int();
  watchdog_period_ms_ = get_parameter("watchdog_period_ms").as_int();
  grace_step_ms_ = get_parameter("grace_step_ms").as_int();
  min_battery_for_leader_ = static_cast<float>(
    get_parameter("min_battery_for_leader").as_double());
  min_battery_follower_ = static_cast<float>(
    get_parameter("min_battery_follower").as_double());
  demote_cooldown_ms_ = get_parameter("demote_cooldown_ms").as_int();
  status_max_age_ms_ = get_parameter("status_max_age_ms").as_int();

  is_leader_ = (robot_id_ == leader_robot_id_);
  is_deputy_ugv_ = (robot_id_ == deputy_robot_id_);
  is_hub_ugv_ = (robot_id_ == hub_robot_id_);
}

void LeaderRoleManager::wireInterfaces()
{
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
    std::bind(
      &LeaderRoleManager::onLeaderAnnouncement, this,
      std::placeholders::_1),
    sub_opts);
  status_sub_ = create_subscription<Status>(
    "/swarm/robot_status", 10,
    std::bind(
      &LeaderRoleManager::onRobotStatus, this,
      std::placeholders::_1),
    sub_opts);
  watchdog_timer_ = create_wall_timer(
    std::chrono::milliseconds(watchdog_period_ms_),
    std::bind(&LeaderRoleManager::watchdogTick, this),
    cb_group_);
}

// ─── Thread-safe accessors (PATCH 2026-05-13) ───────────────────────────

LeaderRole LeaderRoleManager::getRole() const
{
  std::lock_guard<std::mutex> lock(state_mu_);
  return role_;
}

SuccessionPriority LeaderRoleManager::getSuccessionPriority() const
{
  std::lock_guard<std::mutex> lock(state_mu_);
  return last_priority_;
}

bool LeaderRoleManager::isGraceInProgress() const
{
  std::lock_guard<std::mutex> lock(state_mu_);
  return grace_in_progress_;
}

// ─── Status subscription ────────────────────────────────────────────────

void LeaderRoleManager::recordStatus(const Status & s)
{
  // ★ PATCH 2026-05-13 (M9): freshness check.
  // If status_max_age_ms_ > 0 and the message timestamp is more
  // than that into the past, ignore it. Setting status_max_age_ms
  // to 0 disables the check (back-compat for tests with zero ts).
  if (status_max_age_ms_ > 0 && s.timestamp_ms != 0) {
    const uint64_t now_ms = nowMs();
    if (now_ms > s.timestamp_ms &&
      (now_ms - s.timestamp_ms) >
      static_cast<uint64_t>(status_max_age_ms_))
    {
      RCLCPP_DEBUG_THROTTLE(
        get_logger(), *get_clock(), 5000,
        "Stale RobotStatus from %u ignored (age=%lums > %dms)",
        s.robot_id,
        static_cast<unsigned long>(now_ms - s.timestamp_ms),
        status_max_age_ms_);
      return;
    }
  }

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
    std::lock_guard<std::mutex> lock(state_mu_);
    last_leader_heartbeat_ = now();
  }
}

void LeaderRoleManager::onRobotStatus(Status::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  recordStatus(*msg);
}

void LeaderRoleManager::injectStatusForTest(const Status & s)
{
  recordStatus(s);
}

void LeaderRoleManager::injectAnnouncementForTest(const LeaderAnn & msg)
{
  auto p = std::make_shared<LeaderAnn>(msg);
  onLeaderAnnouncement(p);
}

void LeaderRoleManager::simulateLeaderHeartbeatForTest()
{
  std::lock_guard<std::mutex> lock(state_mu_);
  last_leader_heartbeat_ = now();
}

void LeaderRoleManager::simulateLeaderHeartbeatLossForTest()
{
  std::lock_guard<std::mutex> lock(state_mu_);
  last_leader_heartbeat_ =
    now() - rclcpp::Duration(
    std::chrono::milliseconds(leader_heartbeat_timeout_ms_ + 500));
}

// ─── Announcement handling ──────────────────────────────────────────────

void LeaderRoleManager::onLeaderAnnouncement(LeaderAnn::SharedPtr msg)
{
  if (msg == nullptr) {return;}

  // ★ PATCH 2026-05-13 (M10): loopback rejection.
  // Reliable + transient_local QoS means our own messages arrive on
  // our own subscription. The original code "handled" this by
  // checking msg->robot_id == robot_id_ first, but that path
  // *accepted* a higher term (impersonation risk). New behaviour:
  // if it claims to be us, only re-sync the term IF the message we
  // sent ourselves with that term was actually published recently.
  // Conservative: just ignore self-claims that are higher than ours.
  if (msg->robot_id == robot_id_) {
    // Our own announce coming back. Local term should already be
    // at least msg->leader_term (we just incremented before publish).
    // If msg term > local, it's an impersonation — log & ignore.
    if (msg->leader_term > leader_term_.load()) {
      RCLCPP_ERROR(
        get_logger(),
        "IMPERSONATION suspected: self-claim with term=%u > "
        "local term=%u — ignoring",
        msg->leader_term, leader_term_.load());
    }
    return;
  }

  // ★ PATCH 2026-05-13 (M6/M7): tuple (term, robot_id) ordering.
  // Stale: lower term → ignore.
  if (msg->leader_term < leader_term_.load()) {
    RCLCPP_WARN_THROTTLE(
      get_logger(), *get_clock(), 1000,
      "Stale LeaderRoleAnnouncement from %u (term=%u local=%u)",
      msg->robot_id, msg->leader_term, leader_term_.load());
    return;
  }

  // Equal-term tiebreak: the announcement wins only if its
  // robot_id < our robot_id. Otherwise it's a tie loss and we
  // ignore (our own announce will be authoritative).
  if (msg->leader_term == leader_term_.load()) {
    if (msg->robot_id >= robot_id_) {
      // Tie — we (with lower or equal robot_id) keep our state.
      // The peer will see our announce by symmetric tiebreak.
      return;
    }
    // Peer has lower robot_id → wins; fall through to accept.
  }

  leader_term_.store(msg->leader_term);

  if (msg->role == LeaderAnn::LEADER_PROMOTED) {
    bool need_demote = false;
    {
      std::lock_guard<std::mutex> lock(state_mu_);
      // PATCH 2026-05-13 (C1): also cancels a pending grace
      // by clearing grace_in_progress_; the grace_timer_
      // callback then sees role_ != CANDIDATE and returns.
      if (role_ == LeaderRole::PROMOTED ||
        role_ == LeaderRole::CANDIDATE)
      {
        need_demote = true;
      }
      // Original Leader recovering: refresh heartbeat so we
      // stop counting toward a timeout.
      if (msg->robot_id == leader_robot_id_) {
        last_leader_heartbeat_ = now();
      }
    }
    if (need_demote) {
      RCLCPP_INFO(
        get_logger(),
        "Yielding Leader role: robot %u took over (term=%u)",
        msg->robot_id, msg->leader_term);
      demoteToFollower("higher_priority_or_higher_term_peer");
    }
  }
}

// ─── Priority decision (unchanged logic, locked accessors) ──────────────

SuccessionPriority LeaderRoleManager::determineMyPriority() const
{
  auto self = battery_monitor_.get(robot_id_);
  if (self.robot_id == 0) {
    return SuccessionPriority::LIMP_MODE;
  }

  if (is_deputy_ugv_ &&
    self.battery_percent >= min_battery_for_leader_ &&
    self.sbc1_healthy && self.sbc2_healthy)
  {
    return SuccessionPriority::DEPUTY;
  }

  if (is_hub_ugv_ &&
    self.battery_percent >= min_battery_for_leader_ &&
    self.sbc1_healthy && self.sbc2_healthy)
  {
    if (battery_monitor_.isDeputyFailed(deputy_robot_id_)) {
      return SuccessionPriority::HUB;
    }
  }

  if (battery_monitor_.isDeputyFailed(deputy_robot_id_) &&
    battery_monitor_.isHubFailed(hub_robot_id_))
  {
    const uint32_t winner = battery_monitor_.pickMaxBatteryFollower(
      leader_robot_id_, hub_robot_id_, deputy_robot_id_,
      min_battery_follower_);
    if (winner == robot_id_ && self.sbc1_healthy &&
      self.battery_percent >= min_battery_follower_)
    {
      return SuccessionPriority::BATTERY_MAX;
    }
  }

  return SuccessionPriority::LIMP_MODE;
}

SuccessionPriority LeaderRoleManager::evaluateSuccessionForTest()
{
  auto p = determineMyPriority();
  std::lock_guard<std::mutex> lock(state_mu_);
  last_priority_ = p;
  return p;
}

// ─── Watchdog tick (★ PATCH 2026-05-13 C1: non-blocking) ────────────────

void LeaderRoleManager::watchdogTick()
{
  if (is_leader_) {return;}
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    if (role_ == LeaderRole::PROMOTED) {return;}
    rearmIfCooldownElapsed_locked();       // C5
    if (role_ == LeaderRole::DEMOTED) {return;}
    if (grace_in_progress_) {
      return;                               // C1: grace already pending
    }
    if (!last_leader_heartbeat_.has_value()) {return;}

    const auto elapsed_ms =
      (now() - *last_leader_heartbeat_).nanoseconds() / 1'000'000;
    if (elapsed_ms < leader_heartbeat_timeout_ms_) {return;}
  }

  // [R-10 + main C-2 fix v1.5.1 — DCN-2026-003 D-005]
  // Grace period: 200 ms × priority. Higher-priority candidate
  // (lower numeric) self-promotes first; if it does, the
  // LEADER_PROMOTED announcement will arrive during the grace
  // window and onLeaderAnnouncement will move us back to NORMAL
  // BEFORE the deferred promotion fires.
  //
  // Previous implementation: std::this_thread::sleep_for() blocked
  // the watchdog callback for up to 600 ms; under single-thread
  // executor that froze every other timer / subscription, under
  // MTE it blocked the thread that needs to service
  // onLeaderAnnouncement — so the grace "yield to higher priority"
  // mechanism was structurally broken.
  //
  // New: schedule grace END via a one-shot wall timer. Executor
  // remains free to dispatch announcements during the window; if
  // we get demoted, onGraceComplete observes role_ ≠ CANDIDATE
  // and aborts.
  SuccessionPriority my_priority = determineMyPriority();
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    last_priority_ = my_priority;
    if (my_priority == SuccessionPriority::LIMP_MODE) {
      return;           // LimpModeManager picks up
    }
    role_ = LeaderRole::CANDIDATE;
    grace_in_progress_ = true;
    pending_grace_priority_ = my_priority;
  }

  // R-10 (M11): announce CANDIDATE intent so other candidates
  // see we're about to promote.
  announceCandidate(my_priority);

  const int grace_ms =
    static_cast<int>(my_priority) * grace_step_ms_;
  // cb_group_ binding (from main) keeps the one-shot timer on the
  // dedicated callback group so it can't reenter watchdogTick.
  grace_timer_ = create_wall_timer(
    std::chrono::milliseconds(grace_ms),
    [this]() {
      // One-shot — cancel + drop the timer first so it never
      // re-fires (rclcpp wall timers are periodic by default).
      if (grace_timer_) {
        grace_timer_->cancel();
        grace_timer_.reset();
      }
      onGraceComplete();
    },
    cb_group_);
}

void LeaderRoleManager::onGraceComplete()
{
  SuccessionPriority promote_priority = SuccessionPriority::LIMP_MODE;
  bool should_promote = false;
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    // C1: if an announcement during the grace window demoted us,
    // role_ is no longer CANDIDATE and we silently abort.
    if (role_ == LeaderRole::CANDIDATE && grace_in_progress_) {
      promote_priority = pending_grace_priority_;
      should_promote = true;
    }
    grace_in_progress_ = false;
  }
  if (should_promote) {
    promoteToLeader(promote_priority);
  }
}

// ─── Test helpers for the new non-blocking grace logic ──────────────────

void LeaderRoleManager::watchdogTickForTest()
{
  // Mirror watchdogTick but DON'T schedule the grace timer — leave
  // that to finishGraceForTest. This lets unit tests inject
  // announcement during the grace window deterministically.
  if (is_leader_) {return;}
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    if (role_ == LeaderRole::PROMOTED) {return;}
    rearmIfCooldownElapsed_locked();
    if (role_ == LeaderRole::DEMOTED) {return;}
    if (grace_in_progress_) {return;}
    if (!last_leader_heartbeat_.has_value()) {return;}
    const auto elapsed_ms =
      (now() - *last_leader_heartbeat_).nanoseconds() / 1'000'000;
    if (elapsed_ms < leader_heartbeat_timeout_ms_) {return;}
  }
  SuccessionPriority my_priority = determineMyPriority();
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    last_priority_ = my_priority;
    if (my_priority == SuccessionPriority::LIMP_MODE) {return;}
    role_ = LeaderRole::CANDIDATE;
    grace_in_progress_ = true;
    pending_grace_priority_ = my_priority;
  }
  announceCandidate(my_priority);
}

void LeaderRoleManager::finishGraceForTest()
{
  onGraceComplete();
}

// ─── Re-arming (★ PATCH 2026-05-13 C5) ──────────────────────────────────

void LeaderRoleManager::rearmIfCooldownElapsed_locked()
{
  // Called with state_mu_ held.
  if (role_ != LeaderRole::DEMOTED) {return;}
  if (demote_cooldown_ms_ <= 0) {return;}
  if (demoted_at_ms_ == 0) {return;}
  const uint64_t now_ms = nowMs();
  if (now_ms < demoted_at_ms_) {return;}
  if ((now_ms - demoted_at_ms_) <
    static_cast<uint64_t>(demote_cooldown_ms_)) {return;}

  RCLCPP_INFO(
    get_logger(),
    "Re-arming from DEMOTED to NORMAL after %dms cooldown",
    demote_cooldown_ms_);
  role_ = LeaderRole::NORMAL;
  demoted_at_ms_ = 0;
}

// ─── Promote / demote / announce ────────────────────────────────────────

void LeaderRoleManager::promoteToLeader(SuccessionPriority priority)
{
  // ★ PATCH 2026-05-13 (C4): capture new term BEFORE publish so we
  // know exactly what we're advertising. (fetch_add returns the
  // pre-increment value; we use the result + 1 for the new term.)
  const uint32_t new_term = leader_term_.fetch_add(1) + 1;
  leader_term_.store(new_term);

  {
    std::lock_guard<std::mutex> lock(state_mu_);
    role_ = LeaderRole::PROMOTED;
    last_priority_ = priority;
  }

  LeaderAnn msg;
  msg.header.stamp = now();
  msg.header.frame_id = "swarm";
  msg.robot_id = robot_id_;
  msg.leader_term = new_term;
  msg.role = LeaderAnn::LEADER_PROMOTED;
  msg.succession_priority = static_cast<uint8_t>(priority);
  msg.reason = "leader_heartbeat_timeout";
  msg.timestamp_ms = nowMs();

  auto self = battery_monitor_.get(robot_id_);
  msg.battery_percent = self.battery_percent;

  if (announce_pub_) {announce_pub_->publish(msg);}

  RCLCPP_INFO(
    get_logger(),
    "Promoted to Leader (priority=%u, leader_term=%u, battery=%.1f%%)",
    msg.succession_priority, new_term, msg.battery_percent);
}

void LeaderRoleManager::announceCandidate(SuccessionPriority priority)
{
  // ★ PATCH 2026-05-13 (M11): notify peers we're entering grace.
  // We do NOT increment leader_term here — that happens at PROMOTE.
  LeaderAnn msg;
  msg.header.stamp = now();
  msg.header.frame_id = "swarm";
  msg.robot_id = robot_id_;
  msg.leader_term = leader_term_.load();
  msg.role = LeaderAnn::LEADER_CANDIDATE;
  msg.succession_priority = static_cast<uint8_t>(priority);
  msg.reason = "candidate_grace_started";
  msg.timestamp_ms = nowMs();
  auto self = battery_monitor_.get(robot_id_);
  msg.battery_percent = self.battery_percent;
  if (announce_pub_) {announce_pub_->publish(msg);}
}

void LeaderRoleManager::demoteToFollower(const std::string & reason)
{
  {
    std::lock_guard<std::mutex> lock(state_mu_);
    role_ = LeaderRole::DEMOTED;
    demoted_at_ms_ = nowMs();       // ★ PATCH 2026-05-13 (C5)
    grace_in_progress_ = false;
  }
  LeaderAnn msg;
  msg.header.stamp = now();
  msg.header.frame_id = "swarm";
  msg.robot_id = robot_id_;
  msg.leader_term = leader_term_.load();
  msg.role = LeaderAnn::LEADER_DEMOTED;
  msg.reason = reason;
  msg.timestamp_ms = nowMs();
  if (announce_pub_) {announce_pub_->publish(msg);}
}

uint64_t LeaderRoleManager::nowMs() const
{
  const auto n = now().nanoseconds();
  if (n < 0) {
    return 0;                  // ★ PATCH 2026-05-13: defensive
  }
  return static_cast<uint64_t>(n / 1'000'000ll);
}

}  // namespace san_role_management
