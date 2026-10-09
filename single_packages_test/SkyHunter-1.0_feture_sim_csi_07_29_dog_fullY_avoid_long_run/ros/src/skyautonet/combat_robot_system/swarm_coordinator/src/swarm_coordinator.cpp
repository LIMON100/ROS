#include "swarm_coordinator/swarm_coordinator.hpp"

#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <sstream>

#include <rclcpp_components/register_node_macro.hpp>

namespace swarm_coordinator {
namespace {

constexpr uint32_t DEFAULT_MAX_ROBOTS = 8;

std::vector<uint32_t> defaultRobotIds()
{
  std::vector<uint32_t> ids;
  for (uint32_t id = 1; id <= DEFAULT_MAX_ROBOTS; ++id) {
    ids.push_back(id);
  }
  return ids;
}

}  // namespace

SwarmCoordinatorNode::SwarmCoordinatorNode(const rclcpp::NodeOptions& t_options)
: Node("swarm_coordinator", t_options)
{
  m_command_qos.reliable().transient_local();
  m_heartbeat_qos.best_effort();

  initParameters();

  if (m_role == Role::LEADER) {
    initLeader();
  } else {
    initFollower();
  }
}

void SwarmCoordinatorNode::initParameters()
{
  const std::string role = this->declare_parameter<std::string>("role", "leader");
  if (role == "leader") {
    m_role = Role::LEADER;
  } else if (role == "follower") {
    m_role = Role::FOLLOWER;
  } else {
    RCLCPP_WARN(
      this->get_logger(),
      "Invalid role '%s'. Falling back to follower mode.",
      role.c_str());
    m_role = Role::FOLLOWER;
  }

  const int robot_id = this->declare_parameter<int>("robot_id", 1);
  const int leader_robot_id = this->declare_parameter<int>("leader_robot_id", 1);

  m_robot_id = isValidRobotId(static_cast<uint32_t>(robot_id)) ?
    static_cast<uint32_t>(robot_id) : 1u;
  m_leader_robot_id = isValidRobotId(static_cast<uint32_t>(leader_robot_id)) ?
    static_cast<uint32_t>(leader_robot_id) : 1u;

  if (m_robot_id != static_cast<uint32_t>(robot_id)) {
    RCLCPP_WARN(this->get_logger(), "Invalid robot_id=%d. Falling back to S1.", robot_id);
  }
  if (m_leader_robot_id != static_cast<uint32_t>(leader_robot_id)) {
    RCLCPP_WARN(
      this->get_logger(),
      "Invalid leader_robot_id=%d. Falling back to S1.",
      leader_robot_id);
  }

  m_relay_to_self = this->declare_parameter<bool>("relay_to_self", false);
  m_heartbeat_period_ms =
    std::max<int>(50, this->declare_parameter<int>("heartbeat_period_ms", 200));
  m_heartbeat_timeout_ms =
    std::max<int>(
      m_heartbeat_period_ms * 2,
      this->declare_parameter<int>("heartbeat_timeout_ms", 1000));
  m_robot_topic_prefix =
    this->declare_parameter<std::string>("robot_topic_prefix", "/swarm");
  m_follower_output_prefix =
    this->declare_parameter<std::string>("follower_output_prefix", "/swarm/follower");
  m_heartbeat_topic =
    this->declare_parameter<std::string>("heartbeat_topic", "/swarm/leader/heartbeat");

  const std::vector<int64_t> configured_targets =
    this->declare_parameter<std::vector<int64_t>>(
      "default_target_robot_ids",
      std::vector<int64_t>{1, 2, 3, 4, 5, 6, 7, 8});

  m_default_target_robot_ids.clear();
  for (const int64_t id : configured_targets) {
    if (id < 1 || id > static_cast<int64_t>(MAX_ROBOTS)) {
      RCLCPP_WARN(
        this->get_logger(),
        "Ignoring invalid default target robot id: %ld",
        static_cast<long>(id));
      continue;
    }
    const uint32_t robot_id_u32 = static_cast<uint32_t>(id);
    if (std::find(
          m_default_target_robot_ids.begin(),
          m_default_target_robot_ids.end(),
          robot_id_u32) == m_default_target_robot_ids.end())
    {
      m_default_target_robot_ids.push_back(robot_id_u32);
    }
  }

  if (m_default_target_robot_ids.empty()) {
    m_default_target_robot_ids = defaultRobotIds();
  }
}

void SwarmCoordinatorNode::initLeader()
{
  for (uint32_t robot_id = 1; robot_id <= MAX_ROBOTS; ++robot_id) {
    m_pub_robot_commands[robot_id] =
      this->create_publisher<SwarmRobotCommand>(
        robotTopic(robot_id, "command"),
        m_command_qos);
  }

  m_pub_leader_heartbeat =
    this->create_publisher<SwarmLeaderHeartbeat>(m_heartbeat_topic, m_heartbeat_qos);

  m_sub_leader_mission_control =
    this->create_subscription<MissionControlCommand>(
      "/mission_control_command",
      rclcpp::QoS(10),
      std::bind(
        &SwarmCoordinatorNode::onLeaderMissionControlCommand,
        this,
        std::placeholders::_1));
  m_sub_leader_path_command =
    this->create_subscription<SwarmPathCommand>(
      "/swarm/path_command",
      m_command_qos,
      std::bind(&SwarmCoordinatorNode::onLeaderPathCommand, this, std::placeholders::_1));
  m_sub_leader_formation_command =
    this->create_subscription<SwarmControlCommand>(
      "/swarm/control_command",
      m_command_qos,
      std::bind(&SwarmCoordinatorNode::onLeaderFormationCommand, this, std::placeholders::_1));

  m_leader_heartbeat_timer = this->create_wall_timer(
    std::chrono::milliseconds(m_heartbeat_period_ms),
    std::bind(&SwarmCoordinatorNode::publishLeaderHeartbeat, this));

  RCLCPP_INFO(
    this->get_logger(),
    "Swarm coordinator started as leader S%u.",
    static_cast<unsigned>(m_robot_id));
}

void SwarmCoordinatorNode::initFollower()
{
  m_pub_follower_command =
    this->create_publisher<SwarmRobotCommand>(
      followerTopic("command"),
      m_command_qos);
  m_pub_follower_status =
    this->create_publisher<SwarmFollowerStatus>(
      followerTopic("status"),
      rclcpp::QoS(10).reliable());

  m_sub_follower_robot_command =
    this->create_subscription<SwarmRobotCommand>(
      robotTopic(m_robot_id, "command"),
      m_command_qos,
      std::bind(&SwarmCoordinatorNode::onFollowerRobotCommand, this, std::placeholders::_1));
  m_sub_follower_heartbeat =
    this->create_subscription<SwarmLeaderHeartbeat>(
      m_heartbeat_topic,
      m_heartbeat_qos,
      std::bind(&SwarmCoordinatorNode::onFollowerHeartbeat, this, std::placeholders::_1));

  m_follower_status_timer = this->create_wall_timer(
    std::chrono::milliseconds(m_heartbeat_period_ms),
    std::bind(&SwarmCoordinatorNode::publishFollowerStatus, this));

  RCLCPP_INFO(
    this->get_logger(),
    "Swarm coordinator started as follower S%u. Expected leader is S%u.",
    static_cast<unsigned>(m_robot_id),
    static_cast<unsigned>(m_leader_robot_id));
}

void SwarmCoordinatorNode::onLeaderMissionControlCommand(
  const MissionControlCommand::SharedPtr t_msg)
{
  if (!t_msg) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    m_latest_mode_command = *t_msg;
  }

  publishRobotCommand(SwarmRobotCommand::COMMAND_MODE, t_msg->estop_requested);
}

void SwarmCoordinatorNode::onLeaderPathCommand(const SwarmPathCommand::SharedPtr t_msg)
{
  if (!t_msg) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    m_latest_path_command = *t_msg;
  }

  publishRobotCommand(SwarmRobotCommand::COMMAND_PATH, false);
}

void SwarmCoordinatorNode::onLeaderFormationCommand(
  const SwarmControlCommand::SharedPtr t_msg)
{
  if (!t_msg) {
    return;
  }

  updateSelectedRobotIds(*t_msg);
  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    m_latest_formation_command = *t_msg;
  }

  publishRobotCommand(SwarmRobotCommand::COMMAND_FORMATION, false);
}

void SwarmCoordinatorNode::publishLeaderHeartbeat()
{
  m_pub_leader_heartbeat->publish(buildLeaderHeartbeat());
}

void SwarmCoordinatorNode::onFollowerRobotCommand(
  const SwarmRobotCommand::SharedPtr t_msg)
{
  if (!t_msg) {
    return;
  }
  if (t_msg->leader_robot_id != m_leader_robot_id) {
    return;
  }
  if (t_msg->target_robot_id != 0 && t_msg->target_robot_id != m_robot_id) {
    RCLCPP_WARN_THROTTLE(
      this->get_logger(),
      *this->get_clock(),
      2000,
      "Follower S%u ignored command for S%u.",
      static_cast<unsigned>(m_robot_id),
      static_cast<unsigned>(t_msg->target_robot_id));
    return;
  }

  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    m_latest_mode_command.command_id = t_msg->operation_mode;
    m_latest_mode_command.estop_requested = t_msg->estop_requested;
    m_latest_path_command.command = t_msg->path_command;
    m_latest_path_command.num_waypoints = t_msg->num_waypoints;
    m_latest_path_command.path_json = t_msg->path_json;
    m_latest_formation_command.formation_type = t_msg->formation_type;
    m_latest_formation_command.formation_number = t_msg->formation_number;
    m_latest_formation_command.grouping_index = t_msg->grouping_index;
  }

  m_pub_follower_command->publish(*t_msg);

  RCLCPP_INFO(
    this->get_logger(),
    "Follower S%u accepted swarm command seq=%u type=%u from leader S%u.",
    static_cast<unsigned>(m_robot_id),
    static_cast<unsigned>(t_msg->sequence),
    static_cast<unsigned>(t_msg->command_type),
    static_cast<unsigned>(m_leader_robot_id));
}

void SwarmCoordinatorNode::onFollowerHeartbeat(
  const SwarmLeaderHeartbeat::SharedPtr t_msg)
{
  if (!t_msg) {
    return;
  }
  if (t_msg->leader_robot_id != m_leader_robot_id) {
    return;
  }

  m_last_heartbeat_ns.store(this->now().nanoseconds());
  m_last_heartbeat_sequence.store(t_msg->sequence);

  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    m_latest_mode_command.command_id = t_msg->operation_mode;
    m_latest_mode_command.estop_requested = t_msg->estop_active;
    m_latest_formation_command.formation_type = t_msg->formation_type;
    m_latest_formation_command.formation_number = t_msg->formation_number;
    m_latest_formation_command.grouping_index = t_msg->grouping_index;
  }

  if (!m_leader_connected.exchange(true)) {
    RCLCPP_INFO(
      this->get_logger(),
      "Follower S%u connected to leader S%u heartbeat.",
      static_cast<unsigned>(m_robot_id),
      static_cast<unsigned>(m_leader_robot_id));
  }
}

void SwarmCoordinatorNode::publishFollowerStatus()
{
  const int64_t last_heartbeat_ns = m_last_heartbeat_ns.load();
  const int64_t now_ns = this->now().nanoseconds();
  const bool has_heartbeat = last_heartbeat_ns > 0;
  const double heartbeat_age_sec = has_heartbeat ?
    static_cast<double>(now_ns - last_heartbeat_ns) / 1e9 :
    -1.0;
  const bool link_connected =
    has_heartbeat &&
    (now_ns - last_heartbeat_ns) <=
      static_cast<int64_t>(m_heartbeat_timeout_ms) * 1000000LL;

  const bool was_connected = m_leader_connected.exchange(link_connected);
  if (was_connected && !link_connected) {
    RCLCPP_WARN(
      this->get_logger(),
      "Follower S%u lost leader S%u heartbeat.",
      static_cast<unsigned>(m_robot_id),
      static_cast<unsigned>(m_leader_robot_id));
  }

  m_pub_follower_status->publish(buildFollowerStatus(link_connected, heartbeat_age_sec));
}

void SwarmCoordinatorNode::publishRobotCommand(
  uint8_t t_command_type,
  bool t_force_default_targets)
{
  const auto targets = targetRobotIdsForRelay(t_force_default_targets);
  for (const uint32_t robot_id : targets) {
    if (m_pub_robot_commands[robot_id]) {
      m_pub_robot_commands[robot_id]->publish(buildRobotCommand(t_command_type, robot_id));
    }
  }

  RCLCPP_INFO_THROTTLE(
    this->get_logger(),
    *this->get_clock(),
    2000,
    "Relayed swarm command type=%u to %zu follower target(s).",
    static_cast<unsigned>(t_command_type),
    targets.size());
}

SwarmRobotCommand SwarmCoordinatorNode::buildRobotCommand(
  uint8_t t_command_type,
  uint32_t t_target_robot_id)
{
  MissionControlCommand mode{};
  SwarmPathCommand path{};
  SwarmControlCommand formation{};
  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    mode = m_latest_mode_command;
    path = m_latest_path_command;
    formation = m_latest_formation_command;
  }

  std::vector<uint32_t> selected_ids;
  {
    std::lock_guard<std::mutex> lock(m_selected_robot_mutex);
    selected_ids = m_selected_robot_ids;
  }

  SwarmRobotCommand command;
  command.header.stamp = this->now();
  command.header.frame_id = "swarm_leader";
  command.sequence = m_next_command_sequence.fetch_add(1);
  command.command_type = t_command_type;
  command.leader_robot_id = m_robot_id;
  command.target_robot_id = t_target_robot_id;
  command.operation_mode = mode.command_id;
  command.estop_requested = mode.estop_requested;
  command.path_command = path.command;
  command.num_waypoints = path.num_waypoints;
  command.path_json = path.path_json;
  command.formation_type = formation.formation_type;
  command.formation_number = formation.formation_number;
  command.grouping_index = formation.grouping_index;
  command.slot_index = slotIndexForRobot(t_target_robot_id);
  command.selected_robot_count = std::min<uint8_t>(
    static_cast<uint8_t>(selected_ids.size()),
    static_cast<uint8_t>(command.selected_robot_ids.size()));
  command.selected_robot_ids.fill(0);
  for (std::size_t i = 0; i < command.selected_robot_count; ++i) {
    command.selected_robot_ids[i] = selected_ids[i];
  }

  return command;
}

SwarmLeaderHeartbeat SwarmCoordinatorNode::buildLeaderHeartbeat()
{
  MissionControlCommand mode{};
  SwarmControlCommand formation{};
  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    mode = m_latest_mode_command;
    formation = m_latest_formation_command;
  }

  std::vector<uint32_t> selected_ids;
  {
    std::lock_guard<std::mutex> lock(m_selected_robot_mutex);
    selected_ids = m_selected_robot_ids;
  }

  SwarmLeaderHeartbeat heartbeat;
  heartbeat.header.stamp = this->now();
  heartbeat.header.frame_id = "swarm_leader";
  heartbeat.sequence = m_next_heartbeat_sequence.fetch_add(1);
  heartbeat.leader_robot_id = m_robot_id;
  heartbeat.operation_mode = mode.command_id;
  heartbeat.estop_active = mode.estop_requested;
  heartbeat.formation_type = formation.formation_type;
  heartbeat.formation_number = formation.formation_number;
  heartbeat.grouping_index = formation.grouping_index;
  heartbeat.selected_robot_count = std::min<uint8_t>(
    static_cast<uint8_t>(selected_ids.size()),
    static_cast<uint8_t>(heartbeat.selected_robot_ids.size()));
  heartbeat.selected_robot_ids.fill(0);
  for (std::size_t i = 0; i < heartbeat.selected_robot_count; ++i) {
    heartbeat.selected_robot_ids[i] = selected_ids[i];
  }

  return heartbeat;
}

SwarmFollowerStatus SwarmCoordinatorNode::buildFollowerStatus(
  bool t_link_connected,
  double t_heartbeat_age_sec)
{
  MissionControlCommand mode{};
  SwarmControlCommand formation{};
  {
    std::lock_guard<std::mutex> lock(m_command_state_mutex);
    mode = m_latest_mode_command;
    formation = m_latest_formation_command;
  }

  SwarmFollowerStatus status;
  status.header.stamp = this->now();
  status.header.frame_id = "swarm_follower";
  status.robot_id = m_robot_id;
  status.leader_robot_id = m_leader_robot_id;
  status.link_status = t_link_connected ?
    SwarmFollowerStatus::LINK_CONNECTED :
    SwarmFollowerStatus::LINK_DISCONNECTED;
  status.last_heartbeat_sequence = m_last_heartbeat_sequence.load();
  status.heartbeat_age_sec = static_cast<float>(t_heartbeat_age_sec);
  status.last_operation_mode = mode.command_id;
  status.last_formation_type = formation.formation_type;
  status.last_formation_number = formation.formation_number;
  status.last_grouping_index = formation.grouping_index;
  return status;
}

std::vector<uint32_t> SwarmCoordinatorNode::targetRobotIdsForRelay(
  bool t_force_default_targets) const
{
  std::vector<uint32_t> source_ids;
  {
    std::lock_guard<std::mutex> lock(m_selected_robot_mutex);
    source_ids = (t_force_default_targets || m_selected_robot_ids.empty()) ?
      m_default_target_robot_ids :
      m_selected_robot_ids;
  }

  std::vector<uint32_t> targets;
  for (const uint32_t robot_id : source_ids) {
    if (!isValidRobotId(robot_id)) {
      continue;
    }
    if (!m_relay_to_self && robot_id == m_robot_id) {
      continue;
    }
    if (std::find(targets.begin(), targets.end(), robot_id) == targets.end()) {
      targets.push_back(robot_id);
    }
  }

  return targets;
}

void SwarmCoordinatorNode::updateSelectedRobotIds(const SwarmControlCommand& t_msg)
{
  const uint8_t count = std::min<uint8_t>(
    t_msg.selected_robot_count,
    static_cast<uint8_t>(MAX_ROBOTS));

  std::vector<uint32_t> selected_ids;
  for (uint8_t i = 0; i < count; ++i) {
    const uint32_t robot_id = t_msg.selected_robot_ids[i];
    if (!isValidRobotId(robot_id)) {
      continue;
    }
    if (std::find(selected_ids.begin(), selected_ids.end(), robot_id) == selected_ids.end()) {
      selected_ids.push_back(robot_id);
    }
  }

  std::lock_guard<std::mutex> lock(m_selected_robot_mutex);
  m_selected_robot_ids = selected_ids;
}

uint8_t SwarmCoordinatorNode::slotIndexForRobot(uint32_t t_robot_id) const
{
  std::lock_guard<std::mutex> lock(m_selected_robot_mutex);
  const auto it = std::find(
    m_selected_robot_ids.begin(),
    m_selected_robot_ids.end(),
    t_robot_id);
  if (it != m_selected_robot_ids.end()) {
    return static_cast<uint8_t>(
      std::distance(m_selected_robot_ids.begin(), it));
  }

  if (t_robot_id == m_leader_robot_id) {
    return 0;
  }

  return static_cast<uint8_t>(std::min<uint32_t>(t_robot_id - 1u, 255u));
}

bool SwarmCoordinatorNode::isValidRobotId(uint32_t t_robot_id) const
{
  return t_robot_id >= 1 && t_robot_id <= MAX_ROBOTS;
}

std::string SwarmCoordinatorNode::robotTopic(
  uint32_t t_robot_id,
  const std::string& t_suffix) const
{
  std::ostringstream topic;
  topic << m_robot_topic_prefix << "/s" << t_robot_id << "/" << t_suffix;
  return topic.str();
}

std::string SwarmCoordinatorNode::followerTopic(const std::string& t_suffix) const
{
  std::ostringstream topic;
  topic << m_follower_output_prefix << "/s" << m_robot_id << "/" << t_suffix;
  return topic.str();
}

}  // namespace swarm_coordinator

RCLCPP_COMPONENTS_REGISTER_NODE(swarm_coordinator::SwarmCoordinatorNode)
