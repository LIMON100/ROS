#include "command_server.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <functional>
#include <cstring>
#include <ctime>

namespace command_server {

constexpr uint16_t STATUS_FLAG_ESTOP = 1u << 6;
constexpr int64_t SWARM_MISSION_STATE_FRESH_NS = 1'000'000'000LL;
// A real GNSS fix (combat_robot_msgs/GnssStatus on /gnss/status) is considered fresh for
// this long. While fresh it wins over the /operation_state GPS fields so the DEFAULT_GPS
// placeholder cannot clobber the real position. Inert until a GnssStatus publisher exists.
constexpr int64_t GNSS_FRESH_NS = 2'000'000'000LL;
// The operation_system FSM (/operation_state) owns operation_state_ while present.
// While a fresh /operation_state is arriving it wins; only when it is absent (e.g. the
// nav2-distributed setup with no FSM) does the vehicle's /swarm/mission_state supply
// operation_state_ so the tablet mode-change gate can leave IDLE. Inert with an FSM present.
constexpr int64_t OPERATION_STATE_FRESH_NS = 1'000'000'000LL;

int64_t steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}

uint32_t CommandServerNode::allocateApprovalRequestId() {
    uint32_t request_id = next_approval_request_id_.fetch_add(1);
    if (request_id == 0) {
        request_id = next_approval_request_id_.fetch_add(1);
    }
    return request_id;
}

uint32_t CommandServerNode::currentApprovalRequestId() {
    std::lock_guard<std::mutex> lock(approval_request_mutex_);
    return (approval_request_.active != 0) ? approval_request_.request_id : 0;
}

void CommandServerNode::clearApprovalRequestSession() {
    std::lock_guard<std::mutex> lock(approval_request_mutex_);
    std::memset(&approval_request_, 0, sizeof(approval_request_));
}

uint32_t CommandServerNode::activateApprovalRequestSession(const char* summary, float confidence) {
    std::lock_guard<std::mutex> lock(approval_request_mutex_);
    const uint32_t request_id =
      (approval_request_.active != 0 && approval_request_.request_id != 0) ?
        approval_request_.request_id :
        allocateApprovalRequestId();

    std::memset(&approval_request_, 0, sizeof(approval_request_));
    approval_request_.active = 1;
    approval_request_.request_id = request_id;
    approval_request_.confidence = confidence;
    std::strncpy(approval_request_.summary, summary, LOG_MESSAGE_SIZE - 1);
    approval_request_.summary[LOG_MESSAGE_SIZE - 1] = '\0';
    return request_id;
}

void CommandServerNode::clearMissionError() {
    assault_error_.store(static_cast<uint8_t>(MissionErrorCode::NONE));
}

void CommandServerNode::setMissionError(MissionErrorCode code, const std::string& message) {
    assault_error_.store(static_cast<uint8_t>(code));
    mission_status_.store(static_cast<uint8_t>(MissionStatus::ERROR));
    assault_state_.store(static_cast<uint8_t>(MissionStatus::ERROR));
    current_speed_.store(0.0f);
    appendRobotLog(
      robot_id_.load(),
      RobotLogSeverity::ERROR,
      RobotLogEvent::SWARM_UPDATE,
      message);
}

bool CommandServerNode::captureHomePositionFromCurrentPose(const std::string& reason) {
    const double lat = robot_lat_.load();
    const double lng = robot_lng_.load();
    if (lat == 0.0 && lng == 0.0) {
        return false;
    }

    home_lat_.store(lat);
    home_lon_.store(lng);
    home_position_valid_.store(1);
    appendRobotLog(
      robot_id_.load(),
      RobotLogSeverity::INFO,
      RobotLogEvent::SWARM_UPDATE,
      reason);
    return true;
}

void CommandServerNode::resetMissionExecution() {
    current_speed_.store(0.0f);
}

void CommandServerNode::resetMissionStateForLoadedPath(uint16_t waypoint_count) {
    resetMissionExecution();
    mission_status_.store(static_cast<uint8_t>(MissionStatus::READY));
    assault_state_.store(static_cast<uint8_t>(MissionStatus::READY));
    current_waypoint_idx_.store(0);
    total_waypoints_.store(waypoint_count);
    assault_progress_.store(0.0f);
    dist_to_next_wp_.store(0.0f);
    dist_to_goal_.store(0.0f);
    mission_history_available_.store(0);
    home_position_valid_.store(0);
    home_lat_.store(0.0);
    home_lon_.store(0.0);
    clearMissionError();
}

bool CommandServerNode::canEnterReturnHome() const {
    return home_position_valid_.load() != 0 && mission_history_available_.load() != 0;
}

CommandServerNode::CommandServerNode(const rclcpp::NodeOptions & options)
 : Node("command_server", options)
{
    InitRosCommon();
    initializeSwarmStatus();

    // 태블릿 transport 스레드는 leader 만 기동. follower 는 태블릿 I/O 없이
    // 글로벌 swarm 버스를 구독해 로컬 미러로 재발행한다(InitRosCommon 의 follower 구독).
    if (isLeaderRole()) {
        cmd_thread_    = std::thread(&CommandServerNode::commandServerThread, this);
        touch_thread_  = std::thread(&CommandServerNode::touchServerThread, this);
        drive_thread_  = std::thread(&CommandServerNode::drivingServerThread, this);
        status_thread_ = std::thread(&CommandServerNode::statusServerThread, this);
        path_thread_   = std::thread(&CommandServerNode::pathServerThread, this);
        RCLCPP_INFO(this->get_logger(),
                    "command_server (leader) initialization (state/driving/touch/status/path)");
    } else {
        RCLCPP_INFO(this->get_logger(),
                    "command_server (follower) initialization (global bus -> /%s mirror)",
                    robot_namespace_.c_str());
    }
}

CommandServerNode::~CommandServerNode()
{
    stop_threads_.store(true);
    
    // 강제 종료를 위해 소켓을 닫거나 해야 하지만, 
    // 여기서는 간단히 join 대기. 실제로는 socket에 shutdown을 걸어야 빨리 깨어남.
    if (status_thread_.joinable()) status_thread_.join();
    if (drive_thread_.joinable())  drive_thread_.join();
    if (touch_thread_.joinable())  touch_thread_.join();
    if (cmd_thread_.joinable())    cmd_thread_.join();
    if (path_thread_.joinable())   path_thread_.join();
}

void CommandServerNode::appendRobotLog(
  uint32_t robot_id,
  RobotLogSeverity severity,
  RobotLogEvent event_code,
  const std::string& message)
{
    std::lock_guard<std::mutex> lock(log_mutex_);
    for (std::size_t i = MAX_LOG_ENTRIES - 1; i > 0; --i) {
        recent_logs_[i] = recent_logs_[i - 1];
    }

    recent_logs_[0] = {};
    recent_logs_[0].robot_id = robot_id;
    recent_logs_[0].timestamp_sec = static_cast<uint32_t>(std::time(nullptr));
    recent_logs_[0].severity = static_cast<uint8_t>(severity);
    recent_logs_[0].event_code = static_cast<uint8_t>(event_code);
    std::strncpy(recent_logs_[0].message, message.c_str(), LOG_MESSAGE_SIZE - 1);
    recent_logs_[0].message[LOG_MESSAGE_SIZE - 1] = '\0';

    const uint8_t next_count = std::min<uint8_t>(
      static_cast<uint8_t>(log_count_.load() + 1),
      static_cast<uint8_t>(MAX_LOG_ENTRIES));
    log_count_.store(next_count);
}

void CommandServerNode::updateAggregatedRobotStatus(
  uint32_t robot_id,
  uint8_t role,
  uint8_t link_status,
  uint8_t comm_quality_level,
  uint8_t battery_pct,
  uint8_t active_mode_id,
  uint8_t mission_status,
  uint8_t estop_active,
  uint8_t formation_type,
  uint8_t formation_number,
  uint8_t grouping_index,
  uint8_t slot_index,
  uint8_t movement_type,
  uint8_t error_code,
  uint16_t status_flags,
  double latitude,
  double longitude,
  float heading,
  float speed_mps,
  float zoom_level)
{
    const int idx = detail::robotIndexFromId(robot_id);
    if (idx < 0) {
        return;
    }

    const uint8_t normalized_formation_type =
      detail::normalizeFormationType(formation_type, formation_number);
    const uint8_t normalized_formation_number =
      detail::normalizeFormationNumber(formation_type, formation_number);

    std::lock_guard<std::mutex> lock(swarm_status_mutex_);
    robot_statuses_[idx] = {
        robot_id,
        role,
        link_status,
        comm_quality_level,
        battery_pct,
        active_mode_id,
        mission_status,
        estop_active,
        normalized_formation_type,
        normalized_formation_number,
        grouping_index,
        slot_index,
        movement_type,
        error_code,
        status_flags,
        latitude,
        longitude,
        heading,
        speed_mps,
        zoom_level
    };
}

void CommandServerNode::initializeSwarmStatus() {
    const int configured_robot_id_param = this->get_parameter("robot_id").as_int();
    const uint32_t configured_robot_id =
      (configured_robot_id_param >= 1 &&
       configured_robot_id_param <= static_cast<int>(MAX_SWARM_ROBOTS)) ?
        static_cast<uint32_t>(configured_robot_id_param) :
        1u;

    if (static_cast<uint32_t>(configured_robot_id_param) != configured_robot_id) {
        RCLCPP_WARN(
          this->get_logger(),
          "Invalid robot_id parameter %d. Falling back to robot_id=1.",
          configured_robot_id_param);
    }

    robot_id_.store(configured_robot_id);
    leader_robot_id_.store(configured_robot_id);
    const bool load_dummy_swarm_data =
      this->get_parameter("load_dummy_swarm_data").as_bool();
    const bool demo_mode =
      this->get_parameter("deployment_mode").as_string() == "demo";

    robot_count_.store(
      static_cast<uint8_t>(
        demo_mode && !load_dummy_swarm_data ? 1u : MAX_SWARM_ROBOTS));
    log_count_.store(0);
    path_loaded_.store(0);
    mission_history_available_.store(0);
    home_position_valid_.store(0);
    home_lat_.store(0.0);
    home_lon_.store(0.0);

    {
        std::lock_guard<std::mutex> lock(swarm_status_mutex_);
        for (std::size_t i = 0; i < MAX_SWARM_ROBOTS; ++i) {
            robot_statuses_[i] = RobotAggregateStatus{
                static_cast<uint32_t>(i + 1),
                static_cast<uint8_t>(i == 0 ? SwarmRole::LEADER : SwarmRole::FOLLOWER),
                static_cast<uint8_t>(i == 0 ? LinkStatus::CONNECTED : LinkStatus::DISCONNECTED),
                static_cast<uint8_t>(i == 0 ? CommQualityLevel::EXCELLENT : CommQualityLevel::NONE),
                static_cast<uint8_t>(i == 0 ? 100 : 0),
                static_cast<uint8_t>(OperationState::ACTIVE_MODE_IDLE),
                static_cast<uint8_t>(MissionStatus::NONE),
                0,
                static_cast<uint8_t>(FormationType::NONE),
                0,
                0,
                static_cast<uint8_t>(i == 0 ? 0 : i),
                static_cast<uint8_t>(SwarmMovementType::HOLD),
                0,
                0,
                0.0,
                0.0,
                0.0f,
                0.0f,
                1.0f
            };
        }
    }

    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        recent_logs_.fill({});
    }

    {
        std::lock_guard<std::mutex> lock(selected_robot_mutex_);
        selected_robot_ids_.fill(0);
    }

    {
        std::lock_guard<std::mutex> lock(path_mutex_);
        current_path_json_.clear();
        std::memset(active_path_id_, 0, sizeof(active_path_id_));
    }

    {
        std::lock_guard<std::mutex> lock(approval_request_mutex_);
        std::memset(&approval_request_, 0, sizeof(approval_request_));
    }

    if (load_dummy_swarm_data) {
        loadDummySwarmStatus();
    } else {
        syncLeaderStatusToAggregate();
    }
}

void CommandServerNode::loadDummySwarmStatus() {
    const uint32_t configured_robot_id =
      detail::isValidRobotId(robot_id_.load()) ? robot_id_.load() : 1u;

    robot_id_.store(configured_robot_id);
    leader_robot_id_.store(configured_robot_id);
    robot_count_.store(static_cast<uint8_t>(MAX_SWARM_ROBOTS));
    // swarm_role_ is set from the "role" parameter in InitRosCommon; do not override here.
    leader_link_status_.store(static_cast<uint8_t>(LinkStatus::CONNECTED));
    leader_comm_quality_.store(static_cast<uint8_t>(CommQualityLevel::EXCELLENT));
    leader_battery_pct_.store(91);
    last_tablet_command_id_.store(0);
    formation_type_.store(static_cast<uint8_t>(FormationType::NONE));
    formation_number_.store(0);
    grouping_index_.store(0);
    slot_index_.store(0);
    robot_lat_.store(37.402152);
    robot_lng_.store(127.108517);
    robot_heading_.store(78.0f);
    current_speed_.store(0.0f);
    current_waypoint_idx_.store(0);
    total_waypoints_.store(0);
    assault_progress_.store(0.0f);
    dist_to_next_wp_.store(0.0f);
    dist_to_goal_.store(0.0f);
    active_stream_robot_id_.store(2);
    rtsp_server_status_.store(1);
    selected_robot_count_.store(2);
    path_loaded_.store(0);
    mission_history_available_.store(0);
    home_position_valid_.store(0);
    home_lat_.store(0.0);
    home_lon_.store(0.0);

    {
        std::lock_guard<std::mutex> lock(selected_robot_mutex_);
        selected_robot_ids_.fill(0);
        selected_robot_ids_[0] = 1;
        selected_robot_ids_[1] = 2;
    }

    {
        std::lock_guard<std::mutex> lock(path_mutex_);
        std::memset(active_path_id_, 0, sizeof(active_path_id_));
    }

    {
        std::lock_guard<std::mutex> lock(approval_request_mutex_);
        std::memset(&approval_request_, 0, sizeof(approval_request_));
        approval_request_.active = 1;
        approval_request_.request_id = 1001;
        approval_request_.confidence = 0.93f;
        std::strncpy(approval_request_.summary, "Tablet target detection request active", LOG_MESSAGE_SIZE - 1);
        approval_request_.summary[LOG_MESSAGE_SIZE - 1] = '\0';
    }

    // The current robot's operation status should come from local ROS inputs,
    // not from the swarm dummy seed used for follower/test data.
    syncLeaderStatusToAggregate();

    updateAggregatedRobotStatus(
      2,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      static_cast<uint8_t>(LinkStatus::CONNECTED),
      static_cast<uint8_t>(CommQualityLevel::GOOD),
      76,
      static_cast<uint8_t>(OperationState::ACTIVE_MODE_RECON),
      static_cast<uint8_t>(MissionStatus::MOVING),
      0,
      static_cast<uint8_t>(FormationType::RECON),
      2,
      1,
      1,
      static_cast<uint8_t>(SwarmMovementType::FOLLOW_LEADER),
      0,
      0x0003u,
      37.402281,
      127.108716,
      77.0f,
      1.5f,
      2.4f);
    updateAggregatedRobotStatus(
      3,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      static_cast<uint8_t>(LinkStatus::CONNECTED),
      static_cast<uint8_t>(CommQualityLevel::FAIR),
      68,
      static_cast<uint8_t>(OperationState::ACTIVE_MODE_PROTECT_GENERAL),
      static_cast<uint8_t>(MissionStatus::READY),
      0,
      static_cast<uint8_t>(FormationType::PROTECT),
      1,
      2,
      2,
      static_cast<uint8_t>(SwarmMovementType::HOLD),
      0,
      0x0001u,
      37.402014,
      127.108302,
      79.0f,
      1.2f,
      1.2f);
    updateAggregatedRobotStatus(
      4,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      static_cast<uint8_t>(LinkStatus::CONNECTING),
      static_cast<uint8_t>(CommQualityLevel::POOR),
      54,
      static_cast<uint8_t>(OperationState::ACTIVE_MODE_PROTECT_DRONE),
      static_cast<uint8_t>(MissionStatus::READY),
      0,
      static_cast<uint8_t>(FormationType::PROTECT),
      4,
      2,
      3,
      static_cast<uint8_t>(SwarmMovementType::HOLD),
      0,
      0x0000u,
      37.401863,
      127.108945,
      80.0f,
      0.2f,
      1.0f);
    updateAggregatedRobotStatus(
      5,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      static_cast<uint8_t>(LinkStatus::DISCONNECTED),
      static_cast<uint8_t>(CommQualityLevel::NONE),
      0,
      static_cast<uint8_t>(OperationState::ACTIVE_MODE_IDLE),
      static_cast<uint8_t>(MissionStatus::ERROR),
      0,
      static_cast<uint8_t>(FormationType::NONE),
      0,
      0,
      4,
      static_cast<uint8_t>(SwarmMovementType::HOLD),
      11,
      0x0000u,
      0.0,
      0.0,
      0.0f,
      0.0f,
      1.0f);
    updateAggregatedRobotStatus(
      6,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      static_cast<uint8_t>(LinkStatus::CONNECTED),
      static_cast<uint8_t>(CommQualityLevel::EXCELLENT),
      87,
      static_cast<uint8_t>(OperationState::ACTIVE_MODE_ASSAULT),
      static_cast<uint8_t>(MissionStatus::MOVING),
      0,
      static_cast<uint8_t>(FormationType::ASSAULT),
      3,
      3,
      1,
      static_cast<uint8_t>(SwarmMovementType::RETURN_HOME),
      0,
      0x0005u,
      37.401624,
      127.107990,
      250.0f,
      1.9f,
      1.4f);
    updateAggregatedRobotStatus(
      7,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      static_cast<uint8_t>(LinkStatus::DISCONNECTED),
      static_cast<uint8_t>(CommQualityLevel::NONE),
      23,
      static_cast<uint8_t>(OperationState::ACTIVE_MODE_RETURN_TO_HOME),
      static_cast<uint8_t>(MissionStatus::PAUSED),
      0,
      static_cast<uint8_t>(FormationType::NONE),
      0,
      0,
      2,
      static_cast<uint8_t>(SwarmMovementType::RETURN_HOME),
      7,
      0x0000u,
      37.401112,
      127.107421,
      245.0f,
      0.0f,
      1.1f);
    updateAggregatedRobotStatus(
      8,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      static_cast<uint8_t>(LinkStatus::CONNECTED),
      static_cast<uint8_t>(CommQualityLevel::POOR),
      18,
      static_cast<uint8_t>(OperationState::ACTIVE_MODE_ESTOP),
      static_cast<uint8_t>(MissionStatus::ERROR),
      1,
      static_cast<uint8_t>(FormationType::NONE),
      0,
      0,
      5,
      static_cast<uint8_t>(SwarmMovementType::ESTOP),
      3,
      0x0040u,
      37.402608,
      127.109248,
      73.0f,
      0.0f,
      1.0f);

    {
        std::lock_guard<std::mutex> lock(log_mutex_);
        recent_logs_.fill({});
        log_count_.store(0);
    }

    struct DummyLogSeed {
        uint32_t robot_id;
        RobotLogSeverity severity;
        RobotLogEvent event_code;
        const char* message;
    };

    const DummyLogSeed dummy_logs[MAX_LOG_ENTRIES] = {
        {1, RobotLogSeverity::INFO,  RobotLogEvent::SWARM_UPDATE,    "Dummy swarm telemetry loaded"},
        {2, RobotLogSeverity::INFO,  RobotLogEvent::TARGET_DETECTED, "Enemy detected near sector B2"},
        {3, RobotLogSeverity::INFO,  RobotLogEvent::MODE_CHANGED,    "Recon follower 3 synced with leader"},
        {4, RobotLogSeverity::WARN,  RobotLogEvent::SWARM_UPDATE,    "Protect formation preset 4 is joining"},
        {5, RobotLogSeverity::ERROR, RobotLogEvent::SWARM_UPDATE,    "Robot disconnected from mesh network"},
        {6, RobotLogSeverity::INFO,  RobotLogEvent::MODE_CHANGED,    "Assault formation preset 3 active"},
        {7, RobotLogSeverity::WARN,  RobotLogEvent::SWARM_UPDATE,    "Return-home follower paused awaiting link"},
        {8, RobotLogSeverity::WARN,  RobotLogEvent::ESTOP_TRIGGERED, "Low battery and E-Stop active"},
        {1, RobotLogSeverity::INFO,  RobotLogEvent::STREAM_STARTED,  "Leader relay stream ready for tablet"},
        {2, RobotLogSeverity::INFO,  RobotLogEvent::SWARM_UPDATE,    "Follower camera stream selected"},
        {3, RobotLogSeverity::INFO,  RobotLogEvent::TARGET_DETECTED, "Thermal signature marked near sector C1"},
        {4, RobotLogSeverity::WARN,  RobotLogEvent::MODE_CHANGED,    "Protect drone profile awaiting confirm"},
        {5, RobotLogSeverity::ERROR, RobotLogEvent::ESTOP_TRIGGERED, "Mesh timeout forced follower hold"},
        {6, RobotLogSeverity::INFO,  RobotLogEvent::SWARM_UPDATE,    "Assault follower aligned with route"},
        {7, RobotLogSeverity::WARN,  RobotLogEvent::STREAM_STOPPED,  "Aux stream released back to leader"},
        {8, RobotLogSeverity::INFO,  RobotLogEvent::MODE_CHANGED,    "Recovery complete after battery swap"}
    };

    for (const DummyLogSeed& log : dummy_logs) {
        appendRobotLog(log.robot_id, log.severity, log.event_code, log.message);
    }
}

void CommandServerNode::syncLeaderStatusToAggregate() {
    formation_type_.store(
      detail::normalizeFormationType(formation_type_.load(), formation_number_.load()));
    formation_number_.store(
      detail::normalizeFormationNumber(formation_type_.load(), formation_number_.load()));

    updateAggregatedRobotStatus(
      robot_id_.load(),
      swarm_role_.load(),
      leader_link_status_.load(),
      leader_comm_quality_.load(),
      leader_battery_pct_.load(),
      current_active_mode_id_.load(),
      mission_status_.load(),
      static_cast<uint8_t>(estop_active_.load() ? 1 : 0),
      formation_type_.load(),
      formation_number_.load(),
      grouping_index_.load(),
      slot_index_.load(),
      static_cast<uint8_t>(estop_active_.load() ? SwarmMovementType::ESTOP : SwarmMovementType::HOLD),
      assault_error_.load(),
      static_cast<uint16_t>(estop_active_.load() ? STATUS_FLAG_ESTOP : 0u),
      robot_lat_.load(),
      robot_lng_.load(),
      robot_heading_.load(),
      current_speed_.load(),
      current_zoom_level_.load());
}

void CommandServerNode::InitRosCommon() {
    const auto swarm_qos = rclcpp::QoS(rclcpp::KeepLast(1)).reliable().transient_local();

    this->declare_parameter<bool>("load_dummy_swarm_data", false);
    this->declare_parameter<int>("robot_id", 1);
    this->declare_parameter<std::string>("deployment_mode", "production");

    // --- Swarm role adapter (target item #1) ---------------------------------
    // role=leader  : tablet TCP/UDP input -> global swarm bus + local mirror.
    // role=follower: subscribe global swarm bus -> local mirror (no tablet I/O).
    // The downstream stack on every robot consumes only the local /{ns}/... mirror,
    // so it is agnostic to whether a command came from the tablet or the swarm bus.
    const std::string role_param =
      this->declare_parameter<std::string>("role", "leader");
    swarm_role_.store(static_cast<uint8_t>(
      (role_param == "follower") ? SwarmRole::FOLLOWER : SwarmRole::LEADER));

    const int ns_robot_id_param = this->get_parameter("robot_id").as_int();
    const int ns_robot_id =
      (ns_robot_id_param >= 1 && ns_robot_id_param <= static_cast<int>(MAX_SWARM_ROBOTS)) ?
        ns_robot_id_param : 1;
    robot_namespace_ = this->declare_parameter<std::string>(
      "robot_namespace", "s" + std::to_string(ns_robot_id));
    const std::string ns_prefix = "/" + robot_namespace_;

    // Leader-only outputs: the global swarm bus + per-robot actuation commands.
    // A follower neither serves the tablet nor publishes to the global bus, so it
    // does not create these (avoids self QoS conflicts with its global subs).
    if (isLeaderRole()) {
      m_pub_mission_control_command_ =
        this->create_publisher<MissionControlCommand>("/mission_control_command", 10);
      m_pub_stream_control_command_ =
        this->create_publisher<StreamControlCommand>("/stream_control_command", 10);
      m_pub_swarm_control_command_ =
        this->create_publisher<SwarmControlCommand>("/swarm/control_command", swarm_qos);
      m_pub_swarm_path_command_ =
        this->create_publisher<SwarmPathCommand>("/swarm/path_command", swarm_qos);
      m_pub_drivecommand_ = this->create_publisher<DriveCommand>("/drive_command", 1);
      m_pub_touchcommand_ = this->create_publisher<TouchTargetPoint>("/touch_command", 1);
    }

    // Local mirrors (both roles). Latched so a late-joining executor gets the last
    // formation/path immediately.
    m_pub_local_mission_control_ =
      this->create_publisher<MissionControlCommand>(ns_prefix + "/mission_control_command", swarm_qos);
    m_pub_local_swarm_control_ =
      this->create_publisher<SwarmControlCommand>(ns_prefix + "/swarm/control_command", swarm_qos);
    m_pub_local_swarm_path_ =
      this->create_publisher<SwarmPathCommand>(ns_prefix + "/swarm/path_command", swarm_qos);

    if (!isLeaderRole()) {
      // Follower: ingest the global bus and re-emit on the local mirror.
      // /mission_control_command is published volatile (QoS 10) by the leader; match it.
      m_sub_global_mission_control_ = this->create_subscription<MissionControlCommand>(
        "/mission_control_command", rclcpp::QoS(10),
        std::bind(&CommandServerNode::onGlobalMissionControl, this, std::placeholders::_1));
      m_sub_global_swarm_control_ = this->create_subscription<SwarmControlCommand>(
        "/swarm/control_command", swarm_qos,
        std::bind(&CommandServerNode::onGlobalSwarmControl, this, std::placeholders::_1));
      m_sub_global_swarm_path_ = this->create_subscription<SwarmPathCommand>(
        "/swarm/path_command", swarm_qos,
        std::bind(&CommandServerNode::onGlobalSwarmPath, this, std::placeholders::_1));
    }
    RCLCPP_INFO(this->get_logger(), "[Swarm] role=%s namespace=%s",
                isLeaderRole() ? "leader" : "follower", robot_namespace_.c_str());

    this->declare_parameter<double>("max_linear_speed", 2.22);
    this->declare_parameter<double>("max_angular_speed", 6.0);

    timer_ = this->create_wall_timer(
        std::chrono::milliseconds(50),
        std::bind(&CommandServerNode::publish_command, this)
    );

    m_sub_operationstate_ = this->create_subscription<OperationState>(
      "/operation_state", rclcpp::QoS(10), std::bind(&CommandServerNode::onOperationState, this, std::placeholders::_1));
    // 차량 상태/GPS/nav 진행률은 FSM 이 /swarm/mission_state 를 통합해 /operation_state
    // 로 단일 전달하므로 여기서 직접 구독하지 않는다 (mission_control 직결 제거).

    const auto sensor_qos = rclcpp::SensorDataQoS();
    m_sub_gnss_status_ = this->create_subscription<GnssStatus>(
      "/gnss/status", sensor_qos,
      std::bind(&CommandServerNode::onGnssStatus, this, std::placeholders::_1));
    m_sub_chassis_status_ = this->create_subscription<ChassisStatus>(
      "/chassis/status", sensor_qos,
      std::bind(&CommandServerNode::onChassisStatus, this, std::placeholders::_1));
    m_sub_lidar_status_ = this->create_subscription<LidarStatus>(
      "/lidar/status", sensor_qos,
      std::bind(&CommandServerNode::onLidarStatus, this, std::placeholders::_1));

    m_sub_rtsp_status_ = this->create_subscription<std_msgs::msg::UInt8>(
      "/rtsp_status", rclcpp::QoS(10), std::bind(&CommandServerNode::onRTSPStatus, this, std::placeholders::_1));

    // Follower status (tablet swarm map). Each robot's command_server publishes its
    // OWN status on /swarm/follower/s{robot_id}/status when role=follower; the leader
    // subscribes to EVERY other robot's topic and aggregates them into robot_statuses_.
    // (Was: a single subscription keyed to the leader's own id, so s2..sN never showed.)
    // robot_id_ is set later in initializeSwarmStatus(), so read the param directly here.
    const int robot_id_param = this->get_parameter("robot_id").as_int();
    const uint32_t own_robot_id =
      (robot_id_param >= 1 && robot_id_param <= static_cast<int>(MAX_SWARM_ROBOTS)) ?
        static_cast<uint32_t>(robot_id_param) : 1u;

    // Own position from NavSatFix (/{ns}/fix). The legacy GnssStatus source
    // (/gnss/status) has no publisher in sim, so this is the live fix for both roles.
    const std::string position_topic =
      this->declare_parameter<std::string>("position_topic", "fix");
    m_sub_fix_ = this->create_subscription<sensor_msgs::msg::NavSatFix>(
      position_topic, rclcpp::SensorDataQoS(),
      std::bind(&CommandServerNode::onOwnFix, this, std::placeholders::_1));

    const auto follower_status_qos = rclcpp::QoS(10).reliable();
    if (isLeaderRole()) {
      for (uint32_t id = 1; id <= MAX_SWARM_ROBOTS; ++id) {
        if (id == own_robot_id) {
          continue;
        }
        const std::string topic = "/swarm/follower/s" + std::to_string(id) + "/status";
        m_sub_follower_statuses_.push_back(
          this->create_subscription<SwarmFollowerStatus>(
            topic, follower_status_qos,
            std::bind(&CommandServerNode::onSwarmFollowerStatus, this, std::placeholders::_1)));
      }
      RCLCPP_INFO(
        this->get_logger(),
        "[Swarm] Leader aggregating %zu follower status topics for the app.",
        m_sub_follower_statuses_.size());
    } else {
      const std::string own_status_topic =
        "/swarm/follower/s" + std::to_string(own_robot_id) + "/status";
      m_pub_own_follower_status_ =
        this->create_publisher<SwarmFollowerStatus>(own_status_topic, follower_status_qos);
      m_follower_status_timer_ = this->create_wall_timer(
        std::chrono::milliseconds(200),
        std::bind(&CommandServerNode::publishOwnFollowerStatus, this));
      RCLCPP_INFO(
        this->get_logger(),
        "[Swarm] Follower publishing own status on %s.",
        own_status_topic.c_str());
    }
}

void CommandServerNode::onRTSPStatus(const std_msgs::msg::UInt8::SharedPtr msg) {
    rtsp_server_status_.store(msg->data);
}

void CommandServerNode::onGnssStatus(const GnssStatus::SharedPtr msg) {
    gnss_fix_status_.store(msg->fix_status);
    gnss_num_satellites_.store(msg->num_satellites);
    gnss_altitude_m_.store(msg->altitude_m);
    gnss_horizontal_accuracy_m_.store(msg->horizontal_accuracy_m);
    gnss_vertical_accuracy_m_.store(msg->vertical_accuracy_m);

    // Only mark the GNSS position "fresh" when this message carries a valid fix.
    // A no-fix message publishes NaN lat/lon (GnssStatus INVALID sentinel); stamping
    // gnss_last_update_ns_ unconditionally would keep the onOperationState freshness
    // guard active and freeze the app at the last good position after a fix loss.
    // Tying the stamp to a valid position lets the guard fall back to DEFAULT_GPS.
    if (std::isfinite(msg->latitude) && std::isfinite(msg->longitude)) {
        robot_lat_.store(msg->latitude);
        robot_lng_.store(msg->longitude);
        gnss_last_update_ns_.store(this->now().nanoseconds());
    }
    if (msg->heading_deg >= 0.0f && msg->heading_deg <= 360.0f) {
        robot_heading_.store(msg->heading_deg);
    }
    if (msg->ground_speed_mps >= 0.0f) {
        current_speed_.store(msg->ground_speed_mps);
    }
}

void CommandServerNode::onChassisStatus(const ChassisStatus::SharedPtr msg) {
    chassis_drive_state_.store(msg->drive_state);
    chassis_voltage_v_.store(msg->battery_voltage_v);
    chassis_current_a_.store(msg->battery_current_a);
    chassis_linear_velocity_mps_.store(msg->linear_velocity_mps);
    chassis_angular_velocity_rps_.store(msg->angular_velocity_rps);
    chassis_fault_flags_.store(msg->fault_flags);
    chassis_motor_temp_c_.store(msg->motor_temp_c);
    chassis_last_update_ns_.store(this->now().nanoseconds());

    if (msg->battery_pct <= 100u) {
        leader_battery_pct_.store(msg->battery_pct);
    }
    if (std::isfinite(msg->linear_velocity_mps)) {
        current_speed_.store(msg->linear_velocity_mps);
    }
}

void CommandServerNode::onLidarStatus(const LidarStatus::SharedPtr msg) {
    lidar_status_.store(msg->status);
    lidar_last_scan_point_count_.store(msg->last_scan_point_count);
    lidar_scan_rate_hz_.store(msg->scan_rate_hz);
    lidar_obstacle_detected_.store(msg->obstacle_detected);
    lidar_min_obstacle_distance_m_.store(msg->min_obstacle_distance_m);
    lidar_last_update_ns_.store(this->now().nanoseconds());
}

void CommandServerNode::onSwarmFollowerStatus(const SwarmFollowerStatus::SharedPtr msg) {
    if (!msg) {
        return;
    }

    const bool link_connected =
      (msg->link_status == SwarmFollowerStatus::LINK_CONNECTED);
    const uint8_t link_status = static_cast<uint8_t>(
      link_connected ? LinkStatus::CONNECTED : LinkStatus::DISCONNECTED);

    // Map heartbeat freshness to a comm quality level. Heartbeat period is 200 ms
    // and the link is declared lost after 1 s (swarm_coordinator defaults).
    uint8_t comm_quality = static_cast<uint8_t>(CommQualityLevel::NONE);
    if (link_connected) {
        const float age_sec = msg->heartbeat_age_sec;
        if (age_sec < 0.4f) {
            comm_quality = static_cast<uint8_t>(CommQualityLevel::EXCELLENT);
        } else if (age_sec < 0.7f) {
            comm_quality = static_cast<uint8_t>(CommQualityLevel::GOOD);
        } else if (age_sec < 1.0f) {
            comm_quality = static_cast<uint8_t>(CommQualityLevel::FAIR);
        } else {
            comm_quality = static_cast<uint8_t>(CommQualityLevel::POOR);
        }
    }

    // SwarmFollowerStatus carries link/heartbeat + last relayed mode/formation and
    // (since the swarm-map fix) the follower's GPS. Fields still not in the message
    // (battery, estop, zoom) keep neutral defaults.
    const uint8_t slot_index =
      static_cast<uint8_t>(msg->robot_id > 0 ? msg->robot_id - 1 : 0);

    updateAggregatedRobotStatus(
      msg->robot_id,
      static_cast<uint8_t>(SwarmRole::FOLLOWER),
      link_status,
      comm_quality,
      0u,  // battery_pct (not reported by follower status)
      msg->last_operation_mode,
      static_cast<uint8_t>(MissionStatus::NONE),
      0u,  // estop_active (not reported)
      msg->last_formation_type,
      msg->last_formation_number,
      msg->last_grouping_index,
      slot_index,
      static_cast<uint8_t>(SwarmMovementType::HOLD),
      0u,    // error_code
      0u,    // status_flags
      msg->latitude,
      msg->longitude,
      msg->heading_deg,
      msg->ground_speed_mps,
      1.0f); // zoom_level
}

void CommandServerNode::onOwnFix(const sensor_msgs::msg::NavSatFix::SharedPtr msg) {
    if (!msg) {
        return;
    }
    if (std::isfinite(msg->latitude) && std::isfinite(msg->longitude) &&
        !(msg->latitude == 0.0 && msg->longitude == 0.0)) {
        robot_lat_.store(msg->latitude);
        robot_lng_.store(msg->longitude);
        gnss_last_update_ns_.store(this->now().nanoseconds());
    }
}

void CommandServerNode::publishOwnFollowerStatus() {
    if (!m_pub_own_follower_status_) {
        return;
    }
    SwarmFollowerStatus status;
    status.header.stamp = this->now();
    status.header.frame_id = robot_namespace_;
    status.robot_id = robot_id_.load();
    status.leader_robot_id = leader_robot_id_.load();
    status.link_status = SwarmFollowerStatus::LINK_CONNECTED;
    status.last_heartbeat_sequence = 0u;
    status.heartbeat_age_sec = 0.0f;
    status.last_operation_mode = current_active_mode_id_.load();
    status.last_formation_type = formation_type_.load();
    status.last_formation_number = formation_number_.load();
    status.last_grouping_index = grouping_index_.load();
    status.latitude = robot_lat_.load();
    status.longitude = robot_lng_.load();
    status.heading_deg = robot_heading_.load();
    status.ground_speed_mps = current_speed_.load();
    m_pub_own_follower_status_->publish(status);
}

void CommandServerNode::onOperationState(const OperationState::SharedPtr msg)
{
    const bool local_mission_error_active =
      mission_status_.load() == static_cast<uint8_t>(MissionStatus::ERROR) &&
      assault_error_.load() != static_cast<uint8_t>(MissionErrorCode::NONE);
    const int64_t now_ns = steadyNowNs();
    const int64_t swarm_mission_last_ns = swarm_mission_state_last_update_ns_.load();
    const bool swarm_mission_state_fresh =
      swarm_mission_last_ns > 0 &&
      now_ns >= swarm_mission_last_ns &&
      (now_ns - swarm_mission_last_ns) < SWARM_MISSION_STATE_FRESH_NS;

    operation_state_.store(msg->state);
    operation_state_last_update_ns_.store(steadyNowNs());
    current_active_mode_id_.store(msg->active_mode_id);
    estop_active_.store(msg->estop_active);

    // Rising edge of FSM-side permission request (PROTECT auto-engage etc.)
    // needs to open an approval session here so the tablet's APPROVE/DENY
    // response carries a matching request_id. Falling edge tears it down.
    const bool prev_perm = permission_request_active_.load();
    const bool new_perm = msg->permission_request_active != 0;
    permission_request_active_.store(new_perm);
    if (!prev_perm && new_perm) {
        if (currentApprovalRequestId() == 0) {
            const uint32_t opened_request_id =
                activateApprovalRequestSession(
                    "FSM autonomous engagement request",
                    1.0f);
            RCLCPP_INFO(
                this->get_logger(),
                "[Approval] FSM-raised request opened request_id=%u",
                static_cast<unsigned>(opened_request_id));
        }
    } else if (prev_perm && !new_perm) {
        clearApprovalRequestSession();
    }
    current_crosshair_x_.store(msg->crosshair_x);
    current_crosshair_y_.store(msg->crosshair_y);
    current_zoom_level_.store(msg->current_zoom_level);

    // Prefer a fresh real GNSS fix (onGnssStatus, fed by a GnssStatus publisher) over the
    // /operation_state GPS fields. In production the FSM publishes gps_lat=gps_lon=0, which
    // would otherwise be replaced by the DEFAULT_GPS placeholder below and clobber the real
    // fix on every tick. gnss_last_update_ns_ is stamped with ROS time in onGnssStatus, so
    // compare against this->now(). With no GnssStatus publisher the guard is inert and the
    // previous DEFAULT_GPS behavior is preserved unchanged.
    const int64_t gnss_last_ns = gnss_last_update_ns_.load();
    const int64_t now_ros_ns = this->now().nanoseconds();
    const bool gnss_fresh =
      gnss_last_ns > 0 &&
      now_ros_ns >= gnss_last_ns &&
      (now_ros_ns - gnss_last_ns) < GNSS_FRESH_NS;

    if (!gnss_fresh) {
        const bool has_valid_gps = msg->gps_lat != 0.0 || msg->gps_lon != 0.0;
        robot_lat_.store(has_valid_gps ? msg->gps_lat : DEFAULT_GPS_LAT);
        robot_lng_.store(has_valid_gps ? msg->gps_lon : DEFAULT_GPS_LON);
        robot_heading_.store(msg->gps_heading);
    }

    if (!local_mission_error_active) {
        if (!gnss_fresh) {
            current_speed_.store(msg->current_speed_mps);
        }
        if (!swarm_mission_state_fresh) {
            mission_status_.store(msg->mission_status);
            assault_state_.store(msg->mission_status);
            current_waypoint_idx_.store(msg->current_waypoint_index);
            total_waypoints_.store(msg->total_waypoints);
            assault_progress_.store(msg->progress_ratio);
            dist_to_next_wp_.store(msg->distance_to_next_wp_m);
            dist_to_goal_.store(msg->distance_to_goal_m);
            assault_error_.store(msg->error_code);
        }
    }

    syncLeaderStatusToAggregate();
}

} // namespace command_server

#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(command_server::CommandServerNode)
