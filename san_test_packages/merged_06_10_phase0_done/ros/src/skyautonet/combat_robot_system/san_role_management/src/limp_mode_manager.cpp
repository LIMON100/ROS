// Copyright 2026 SkyAutoNet Inc.
//
// Proprietary and confidential. Unauthorized copying, distribution, or use
// of this file, via any medium, is strictly prohibited.

#include "san_role_management/limp_mode_manager.hpp"

#include <chrono>

#include <rcl_interfaces/msg/parameter.hpp>
#include <rcl_interfaces/srv/set_parameters.hpp>

namespace san_role_management
{

LimpModeManager::LimpModeManager()
: LimpModeManager(rclcpp::NodeOptions())
{}

LimpModeManager::LimpModeManager(const rclcpp::NodeOptions & options)
: rclcpp::Node("limp_mode_manager", options),
  in_limp_mode_(false)
{
  declareParameters();
  readParameters();
  wireInterfaces();
  RCLCPP_INFO(
    get_logger(),
    "LimpModeManager started: hub=%u deputy=%u guard=%dms",
    hub_robot_id_, deputy_robot_id_, limp_guard_ms_);
}

void LimpModeManager::declareParameters()
{
  declare_parameter<int>("hub_robot_id", 2);
  declare_parameter<int>("deputy_robot_id", 3);
  declare_parameter<int>("hub_timeout_ms", HUB_HEARTBEAT_TIMEOUT_MS);
  declare_parameter<int>("deputy_timeout_ms", HUB_HEARTBEAT_TIMEOUT_MS);
  declare_parameter<int>("limp_guard_ms", LIMP_MODE_ENTRY_GUARD_MS);
  declare_parameter<int>("watchdog_period_ms", 500);
  declare_parameter<std::string>("android_app_ip_seed", "");
}

void LimpModeManager::readParameters()
{
  hub_robot_id_ = static_cast<uint32_t>(
    get_parameter("hub_robot_id").as_int());
  deputy_robot_id_ = static_cast<uint32_t>(
    get_parameter("deputy_robot_id").as_int());
  hub_timeout_ms_ = get_parameter("hub_timeout_ms").as_int();
  deputy_timeout_ms_ = get_parameter("deputy_timeout_ms").as_int();
  limp_guard_ms_ = get_parameter("limp_guard_ms").as_int();
  watchdog_period_ms_ = get_parameter("watchdog_period_ms").as_int();
  {
    std::lock_guard<std::mutex> lock(endpoint_mutex_);
    android_app_ip_ = get_parameter("android_app_ip_seed").as_string();
  }
}

void LimpModeManager::wireInterfaces()
{
  // [C-1 fix v1.5.1] Single MutuallyExclusive callback group for
  // this node's subs + watchdog. See LeaderRoleManager + HubRoleManager
  // for rationale.
  cb_group_ = create_callback_group(
    rclcpp::CallbackGroupType::MutuallyExclusive);

  rclcpp::SubscriptionOptions sub_opts;
  sub_opts.callback_group = cb_group_;

  rclcpp::QoS qos(10);
  qos.reliable().transient_local();

  hub_sub_ = create_subscription<HubAnn>(
    "/swarm/hub/role_announce", qos,
    std::bind(
      &LimpModeManager::onHubAnnouncement, this,
      std::placeholders::_1),
    sub_opts);
  status_sub_ = create_subscription<Status>(
    "/swarm/robot_status", 10,
    std::bind(
      &LimpModeManager::onRobotStatus, this,
      std::placeholders::_1),
    sub_opts);
  alert_pub_ = create_publisher<std_msgs::msg::String>(
    "/operator/limp_mode_alert", rclcpp::QoS(5).reliable());

  watchdog_timer_ = create_wall_timer(
    std::chrono::milliseconds(watchdog_period_ms_),
    std::bind(&LimpModeManager::watchdogTick, this),
    cb_group_);
}

void LimpModeManager::noteHubAlive()
{
  last_hub_alive_ = now();
}

void LimpModeManager::noteDeputyAlive()
{
  last_deputy_alive_ = now();
}

void LimpModeManager::onHubAnnouncement(HubAnn::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  // Either Hub or Deputy broadcasting that they currently hold the
  // Hub role keeps that lifeline fresh.
  if (msg->robot_id == hub_robot_id_) {
    noteHubAlive();
  } else if (msg->robot_id == deputy_robot_id_ &&
    msg->role == HubAnn::HUB_PROMOTED) {
    noteDeputyAlive();
  }
}

void LimpModeManager::onRobotStatus(Status::SharedPtr msg)
{
  if (msg == nullptr) {return;}
  if (msg->robot_id == hub_robot_id_ &&
    (msg->sbc1_healthy || msg->sbc2_healthy))
  {
    noteHubAlive();
  }
  if (msg->robot_id == deputy_robot_id_ &&
    (msg->sbc1_healthy || msg->sbc2_healthy))
  {
    noteDeputyAlive();
  }
}

void LimpModeManager::injectHubAnnouncementForTest(const HubAnn & msg)
{
  auto p = std::make_shared<HubAnn>(msg);
  onHubAnnouncement(p);
}

void LimpModeManager::injectRobotStatusForTest(const Status & msg)
{
  auto p = std::make_shared<Status>(msg);
  onRobotStatus(p);
}

void LimpModeManager::simulateHubLossForTest()
{
  last_hub_alive_ = now() - rclcpp::Duration(
    std::chrono::milliseconds(hub_timeout_ms_ + limp_guard_ms_ + 1000));
}

void LimpModeManager::simulateDeputyLossForTest()
{
  last_deputy_alive_ = now() - rclcpp::Duration(
    std::chrono::milliseconds(
      deputy_timeout_ms_ + limp_guard_ms_ + 1000));
}

void LimpModeManager::simulateHubRecoveryForTest() {noteHubAlive();}
void LimpModeManager::simulateDeputyRecoveryForTest() {noteDeputyAlive();}

bool LimpModeManager::isHubAlive(const rclcpp::Time & now_t) const
{
  if (!last_hub_alive_.has_value()) {return false;}
  return (now_t - *last_hub_alive_).nanoseconds() / 1'000'000 <
         hub_timeout_ms_;
}

bool LimpModeManager::isDeputyAlive(const rclcpp::Time & now_t) const
{
  if (!last_deputy_alive_.has_value()) {return false;}
  return (now_t - *last_deputy_alive_).nanoseconds() / 1'000'000 <
         deputy_timeout_ms_;
}

void LimpModeManager::watchdogTick()
{
  const auto t = now();
  const bool hub_alive = isHubAlive(t);
  const bool deputy_alive = isDeputyAlive(t);

  if (!hub_alive && !deputy_alive) {
    // 7 s grace from the more recent loss (the LATER of the two
    // last-alive stamps), so the Deputy gets its full takeover window
    // after whichever of Hub/Deputy failed second. Timing from the
    // earlier failure would enter Limp Mode prematurely.
    std::optional<rclcpp::Time> recent_loss;
    if (last_hub_alive_.has_value()) {
      recent_loss = *last_hub_alive_;
    }
    if (last_deputy_alive_.has_value() &&
      (!recent_loss.has_value() || *last_deputy_alive_ > *recent_loss))
    {
      recent_loss = *last_deputy_alive_;
    }
    const auto since_loss_ms = recent_loss.has_value() ?
      (t - *recent_loss).nanoseconds() / 1'000'000 : 0;
    if (since_loss_ms >= limp_guard_ms_ && !in_limp_mode_.load()) {
      enterLimpMode();
    }
  } else if (in_limp_mode_.load() && (hub_alive || deputy_alive)) {
    exitLimpMode();
  }
}

void LimpModeManager::enterLimpMode()
{
  in_limp_mode_.store(true);
  RCLCPP_WARN(
    get_logger(),
    "LIMP MODE ACTIVATED - Hub + Deputy both lost. "
    "Android mesh-direct control armed (fire + video).");

  enableMeshAuthForFire();
  redirectVideoToAndroidDirect();
  notifyAndroidMeshDirectMode();
  pauseComplexMissions();
  publishLimpModeAlert(
    "LIMP_MODE_ACTIVE: Hub+Deputy 모두 불능 - Wi-Fi 6 mesh only. "
    "Android 직접 인증 사격/타격 + 영상 직접 송신 운용.");
}

void LimpModeManager::exitLimpMode()
{
  in_limp_mode_.store(false);
  fire_auth_mesh_direct_ = false;
  {
    std::lock_guard<std::mutex> lock(video_mode_mutex_);
    video_mode_ = VideoSenderMode{};       // back to defaults
  }
  resumeComplexMissions();
  publishLimpModeAlert("LIMP_MODE_EXITED: 정상 운용 복귀");
  RCLCPP_INFO(
    get_logger(),
    "Limp Mode EXITED - Hub or Deputy back online");
}

void LimpModeManager::enableMeshAuthForFire()
{
  fire_auth_mesh_direct_ = true;

  auto client = create_client<rcl_interfaces::srv::SetParameters>(
    "/gun_trigger/set_parameters");
  // [Tier1 audit 2026-05-24 P3-1] non-blocking readiness check.
  // The cb_group is MutuallyExclusive, so a wait_for_service(1s)
  // would stall the watchdog. The local fire_auth_mesh_direct_ flag
  // is already set above, which is the durable degraded-mode signal —
  // missing the parameter push is acceptable per the original
  // "set anyway" semantics.
  if (!client->service_is_ready()) {
    RCLCPP_WARN(
      get_logger(),
      "/gun_trigger/set_parameters not ready - "
      "local fire_auth_mesh_direct flag set anyway");
    return;
  }
  auto req = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
  rcl_interfaces::msg::Parameter p1;
  p1.name = "fire_auth_source";
  p1.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
  p1.value.string_value = "mesh_direct";
  req->parameters.push_back(p1);

  rcl_interfaces::msg::Parameter p2;
  p2.name = "mesh_auth_required";
  p2.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_BOOL;
  p2.value.bool_value = true;
  req->parameters.push_back(p2);

  client->async_send_request(req);
  RCLCPP_INFO(
    get_logger(),
    "Fire commands ENABLED via mesh direct auth");
}

void LimpModeManager::redirectVideoToAndroidDirect()
{
  const std::string android_ip = discoverAndroidAppIp();
  if (android_ip.empty()) {
    RCLCPP_WARN(
      get_logger(),
      "Android app IP not yet known - retrying next tick");
    return;
  }
  {
    std::lock_guard<std::mutex> lock(video_mode_mutex_);
    video_mode_.stream_target = "android_direct";
    video_mode_.android_app_ip = android_ip;
    video_mode_.transport_mode = "srt_direct_with_udp_fallback";
  }

  auto client = create_client<rcl_interfaces::srv::SetParameters>(
    "/video_sender_node/set_parameters");
  // [Tier1 audit 2026-05-24 P3-1] non-blocking readiness check —
  // see enableMeshAuthForFire() for rationale.
  if (!client->service_is_ready()) {
    RCLCPP_WARN(
      get_logger(),
      "/video_sender_node/set_parameters not ready - "
      "local video_mode_ updated only");
    return;
  }
  auto req = std::make_shared<rcl_interfaces::srv::SetParameters::Request>();
  rcl_interfaces::msg::Parameter p1;
  p1.name = "stream_target";
  p1.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
  p1.value.string_value = "android_direct";
  req->parameters.push_back(p1);

  rcl_interfaces::msg::Parameter p2;
  p2.name = "android_app_ip";
  p2.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
  p2.value.string_value = android_ip;
  req->parameters.push_back(p2);

  rcl_interfaces::msg::Parameter p3;
  p3.name = "transport_mode";
  p3.value.type = rcl_interfaces::msg::ParameterType::PARAMETER_STRING;
  p3.value.string_value = "srt_direct_with_udp_fallback";
  req->parameters.push_back(p3);

  client->async_send_request(req);
  RCLCPP_INFO(
    get_logger(),
    "Video pipeline redirected to Android at %s", android_ip.c_str());
}

std::string LimpModeManager::discoverAndroidAppIp() const
{
  std::lock_guard<std::mutex> lock(endpoint_mutex_);
  return android_app_ip_;
}

void LimpModeManager::notifyAndroidMeshDirectMode()
{
  std_msgs::msg::String notify;
  notify.data =
    "ANDROID_MESH_DIRECT_MODE: Hub+Deputy 불능 - "
    "Wi-Fi 6 mesh 직접 통제 시작. "
    "사격/타격 명령 + GStreamer UDP/SRT 영상 모두 정상 운용.";
  if (alert_pub_) {alert_pub_->publish(notify);}
}

void LimpModeManager::pauseComplexMissions()
{
  complex_paused_ = true;
  RCLCPP_INFO(
    get_logger(),
    "Complex missions paused (formation, AI inference). "
    "Simple missions (recon, movement, fire) remain operational.");
}

void LimpModeManager::resumeComplexMissions()
{
  complex_paused_ = false;
}

void LimpModeManager::publishLimpModeAlert(const std::string & text)
{
  std_msgs::msg::String alert;
  alert.data = text;
  if (alert_pub_) {alert_pub_->publish(alert);}
}

VideoSenderMode LimpModeManager::videoSenderMode() const
{
  std::lock_guard<std::mutex> lock(video_mode_mutex_);
  return video_mode_;
}

}  // namespace san_role_management
