#include "command_server.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

namespace command_server {

void CommandServerNode::handleStateCommand(const StateCommand& s)
{
  const uint8_t normalized_attack_permission =
    detail::normalizeAttackPermission(s.attack_permission);
  const bool approval_response_provided =
    (normalized_attack_permission == static_cast<uint8_t>(AttackPermission::APPROVE)) ||
    (normalized_attack_permission == static_cast<uint8_t>(AttackPermission::DENY));
  const uint8_t normalized_formation_type =
    detail::normalizeFormationType(s.formation_type, s.formation_number);
  const uint8_t normalized_formation_number =
    detail::normalizeFormationNumber(s.formation_type, s.formation_number);
  const uint8_t selected_count =
    std::min<uint8_t>(s.selected_robot_count, static_cast<uint8_t>(MAX_SWARM_ROBOTS));
  const uint32_t current_robot_id =
    detail::isValidRobotId(robot_id_.load()) ? robot_id_.load() : 1u;
  const bool targets_current_robot =
    detail::isStateCommandTargetingRobot(s, current_robot_id);
  const bool estop_requested = detail::isIncomingEstopCommand(s);
  const uint32_t active_approval_request_id = currentApprovalRequestId();
  const bool had_permission_request_active =
    permission_request_active_.load() && active_approval_request_id != 0;
  const bool incoming_approval_matches_active_request =
    had_permission_request_active &&
    s.approval_request_id != 0 &&
    s.approval_request_id == active_approval_request_id;
  uint8_t effective_attack_permission = last_attack_permission_.load();

  MissionControlCommand mission;
  StreamControlCommand stream;
  SwarmControlCommand swarm;

  mission.header.stamp = this->now();
  mission.header.frame_id = "tablet_frame";
  stream.header = mission.header;
  swarm.header = mission.header;

  selected_robot_count_.store(selected_count);
  {
    std::lock_guard<std::mutex> lock(selected_robot_mutex_);
    selected_robot_ids_.fill(0);
    std::memcpy(selected_robot_ids_.data(), s.selected_robot_ids, sizeof(s.selected_robot_ids));
  }

  if (!targets_current_robot) {
    // RCLCPP_INFO_THROTTLE(
    //   this->get_logger(),
    //   *this->get_clock(),
    //   2000,
    //   "[STATE] Skip tablet cmd=%u for robot=%u; selected list does not include this robot.",
    //   static_cast<unsigned>(s.command_id),
    //   static_cast<unsigned>(current_robot_id));
    return;
  }

  last_tablet_command_id_.store(s.command_id);
  formation_type_.store(normalized_formation_type);
  formation_number_.store(normalized_formation_number);
  grouping_index_.store(s.grouping_index);

  const uint8_t current_operation_state = operation_state_.load();
  const uint8_t requested_mode_command_id =
    detail::mapIncomingCommandToMissionCommand(s.command_id);
  const bool return_home_requested =
    requested_mode_command_id == MissionControlCommand::RETURN_TO_HOME;
  const bool return_home_allowed = !return_home_requested || canEnterReturnHome();
  uint8_t effective_mode_command_id =
    estop_requested ?
      detail::commandIdForCurrentContext(current_operation_state, current_active_mode_id_.load()) :
      requested_mode_command_id;

  if (!estop_requested && return_home_requested && !return_home_allowed) {
    setMissionError(
      MissionErrorCode::RETURN_HOME_UNAVAILABLE,
      "Return Home rejected: home position or mission history missing");
    RCLCPP_WARN(
      this->get_logger(),
      "[STATE] Rejected Return Home because home position or mission history is missing.");
    effective_mode_command_id =
      detail::commandIdForCurrentContext(current_operation_state, current_active_mode_id_.load());
  }

  if (!estop_requested &&
      !detail::isModeChangeAllowed(current_operation_state, effective_mode_command_id))
  {
    RCLCPP_WARN_THROTTLE(
      this->get_logger(),
      *this->get_clock(),
      2000,
      "[STATE] Hold mode change cmd=%u while state=%u active_mode=%u. Keeping current command until the robot returns to IDLE.",
      static_cast<unsigned>(effective_mode_command_id),
      static_cast<unsigned>(current_operation_state),
      static_cast<unsigned>(current_active_mode_id_.load()));
    effective_mode_command_id =
      detail::commandIdForCurrentContext(current_operation_state, current_active_mode_id_.load());
  }

  mission.command_id = effective_mode_command_id;
  current_active_mode_id_.store(effective_mode_command_id);

  const double drone_target_lat = std::clamp(s.drone_target_lat, -90.0, 90.0);
  const double drone_target_lon = std::clamp(s.drone_target_lon, -180.0, 180.0);
  const bool drone_target_valid = (s.drone_target_valid != 0);

  mission.estop_requested = estop_requested;
  mission.pan_speed = s.pan_speed;
  mission.tilt_speed = s.tilt_speed;
  mission.zoom_command = s.zoom_command;
  mission.lateral_wind_speed = s.lateral_wind_speed;
  mission.drone_target_lat = drone_target_lat;
  mission.drone_target_lon = drone_target_lon;
  mission.drone_target_valid = drone_target_valid;

  stream.stream_command = s.stream_command;
  stream.stream_target_robot_id = s.stream_target_robot_id;

  swarm.formation_type = normalized_formation_type;
  swarm.formation_number = normalized_formation_number;
  swarm.grouping_index = s.grouping_index;
  swarm.selected_robot_count = selected_count;
  std::copy(
    s.selected_robot_ids,
    s.selected_robot_ids + MAX_SWARM_ROBOTS,
    swarm.selected_robot_ids.begin());

  if (estop_requested) {
    estop_active_.store(true);
    effective_attack_permission = static_cast<uint8_t>(AttackPermission::NONE);
    permission_request_active_.store(false);
    clearApprovalRequestSession();
    resetMissionExecution();
    appendRobotLog(
      robot_id_.load(),
      RobotLogSeverity::WARN,
      RobotLogEvent::ESTOP_TRIGGERED,
      "Tablet E-Stop requested");
  } else {
    if (s.command_id != 0) {
      estop_active_.store(false);
      appendRobotLog(
        robot_id_.load(),
        RobotLogSeverity::INFO,
        RobotLogEvent::MODE_CHANGED,
        "Tablet mode command received");
    }

    if (effective_mode_command_id == MissionControlCommand::IDLE) {
      resetMissionExecution();
      mission_status_.store(static_cast<uint8_t>(MissionStatus::NONE));
      assault_state_.store(static_cast<uint8_t>(MissionStatus::NONE));
      effective_attack_permission = static_cast<uint8_t>(AttackPermission::NONE);
      permission_request_active_.store(false);
      clearApprovalRequestSession();
    } else if (s.drone_target_valid != 0) {
      effective_attack_permission = static_cast<uint8_t>(AttackPermission::NONE);
      permission_request_active_.store(true);
      if (!had_permission_request_active) {
        const uint32_t opened_request_id =
          activateApprovalRequestSession("Tablet target detection request active", 1.0f);
        appendRobotLog(
          robot_id_.load(),
          RobotLogSeverity::INFO,
          RobotLogEvent::TARGET_DETECTED,
          "Tablet target detection request active");
        RCLCPP_INFO(
          this->get_logger(),
          "[Approval] Opened request_id=%u for a new approval session.",
          static_cast<unsigned>(opened_request_id));
      }
    } else if (effective_mode_command_id != MissionControlCommand::RECON &&
               effective_mode_command_id != MissionControlCommand::ASSAULT &&
               effective_mode_command_id != MissionControlCommand::PROTECT_GENERAL &&
               effective_mode_command_id != MissionControlCommand::PROTECT_DRONE)
    {
      // Modes that have no engagement gate (IDLE-like) — drop any pending
      // approval session. PROTECT_GENERAL / PROTECT_DRONE keep theirs
      // because the FSM auto-raises permission requests in those modes.
      effective_attack_permission = static_cast<uint8_t>(AttackPermission::NONE);
      permission_request_active_.store(false);
      clearApprovalRequestSession();
    } else if (approval_response_provided && incoming_approval_matches_active_request) {
      effective_attack_permission = normalized_attack_permission;
      permission_request_active_.store(false);
      clearApprovalRequestSession();
      RCLCPP_INFO(
        this->get_logger(),
        "[Approval] Accepted response=%u for request_id=%u.",
        static_cast<unsigned>(normalized_attack_permission),
        static_cast<unsigned>(s.approval_request_id));
    } else if (approval_response_provided) {
      RCLCPP_INFO(
        this->get_logger(),
        "[Approval] Ignored stale response=%u for request_id=%u (active_request_id=%u).",
        static_cast<unsigned>(normalized_attack_permission),
        static_cast<unsigned>(s.approval_request_id),
        static_cast<unsigned>(active_approval_request_id));
    }
  }

  mission.attack_permission = effective_attack_permission;
  // Approval responses are one-shot — publish the APPROVE/DENY value to the
  // FSM exactly once, then revert the latch to NONE so subsequent tablet
  // heartbeats don't keep re-applying the same response to later targets.
  if (incoming_approval_matches_active_request) {
    last_attack_permission_.store(static_cast<uint8_t>(AttackPermission::NONE));
  } else {
    last_attack_permission_.store(effective_attack_permission);
  }

  if (s.zoom_command != 0) {
    float zoom = current_zoom_level_.load();
    zoom = std::clamp(zoom + static_cast<float>(s.zoom_command) * 0.1f, 1.0f, 10.0f);
    current_zoom_level_.store(zoom);

    const uint32_t zoom_robot_id =
      (active_stream_robot_id_.load() != 0) ? active_stream_robot_id_.load() : robot_id_.load();
    const int zoom_idx = detail::robotIndexFromId(zoom_robot_id);
    if (zoom_idx >= 0) {
      std::lock_guard<std::mutex> lock(swarm_status_mutex_);
      robot_statuses_[zoom_idx].zoom_level = zoom;
    }
  }

  switch (s.stream_command) {
    case StreamControlCommand::STREAM_START:
    {
      const uint32_t fallback_stream_robot_id =
        (leader_robot_id_.load() != 0) ? leader_robot_id_.load() : robot_id_.load();
      uint32_t resolved_stream_robot_id = fallback_stream_robot_id;

      if (s.stream_target_robot_id != 0) {
        if (detail::isValidRobotId(s.stream_target_robot_id)) {
          resolved_stream_robot_id = s.stream_target_robot_id;
        } else {
          appendRobotLog(
            robot_id_.load(),
            RobotLogSeverity::WARN,
            RobotLogEvent::SWARM_UPDATE,
            "Invalid stream target robot ID; using leader stream");
        }
      }

      stream.stream_target_robot_id = resolved_stream_robot_id;
      active_stream_robot_id_.store(resolved_stream_robot_id);
      rtsp_server_status_.store(1);
      {
        const int stream_idx = detail::robotIndexFromId(active_stream_robot_id_.load());
        if (stream_idx >= 0) {
          std::lock_guard<std::mutex> lock(swarm_status_mutex_);
          current_zoom_level_.store(robot_statuses_[stream_idx].zoom_level);
        }
      }
      appendRobotLog(
        active_stream_robot_id_.load(),
        RobotLogSeverity::INFO,
        RobotLogEvent::STREAM_STARTED,
        "RTSP stream start requested");
      break;
    }
    case StreamControlCommand::STREAM_STOP:
      appendRobotLog(
        active_stream_robot_id_.load(),
        RobotLogSeverity::INFO,
        RobotLogEvent::STREAM_STOPPED,
        "RTSP stream stop requested");
      active_stream_robot_id_.store(0);
      rtsp_server_status_.store(0);
      break;
    default:
      break;
  }

  syncLeaderStatusToAggregate();

  // RCLCPP_INFO_THROTTLE(
  //   this->get_logger(),
  //   *this->get_clock(),
  //   2000,
  //   "[STATE] cmd=%u estop=%u stream=%u target=%u perm=%u formation=%u/%u group=%u selected=%u pan=%d tilt=%d zoom=%d wind=%.2f",
  //   static_cast<unsigned>(s.command_id),
  //   static_cast<unsigned>(s.e_stop_command),
  //   static_cast<unsigned>(s.stream_command),
  //   static_cast<unsigned>(s.stream_target_robot_id),
  //   static_cast<unsigned>(effective_attack_permission),
  //   static_cast<unsigned>(normalized_formation_type),
  //   static_cast<unsigned>(normalized_formation_number),
  //   static_cast<unsigned>(s.grouping_index),
  //   static_cast<unsigned>(selected_count),
  //   static_cast<int>(s.pan_speed),
  //   static_cast<int>(s.tilt_speed),
  //   static_cast<int>(s.zoom_command),
  //   s.lateral_wind_speed);

  m_pub_mission_control_command_->publish(mission);
  if (stream.stream_command != StreamControlCommand::STREAM_NONE) {
    m_pub_stream_control_command_->publish(stream);
  }
  m_pub_swarm_control_command_->publish(swarm);

  // Local mirror so this (leader) robot's own downstream consumes the same command.
  if (m_pub_local_mission_control_) m_pub_local_mission_control_->publish(mission);
  if (m_pub_local_swarm_control_) m_pub_local_swarm_control_->publish(swarm);
}

// ---- Follower role: global swarm bus -> local /{ns} mirror ----
void CommandServerNode::onGlobalMissionControl(const MissionControlCommand::SharedPtr msg)
{
  if (msg && m_pub_local_mission_control_) {
    m_pub_local_mission_control_->publish(*msg);
  }
}

void CommandServerNode::onGlobalSwarmControl(const SwarmControlCommand::SharedPtr msg)
{
  if (!msg) return;
  // Track formation locally so aggregated status reflects the relayed command.
  formation_type_.store(detail::normalizeFormationType(msg->formation_type, msg->formation_number));
  formation_number_.store(detail::normalizeFormationNumber(msg->formation_type, msg->formation_number));
  grouping_index_.store(msg->grouping_index);
  if (m_pub_local_swarm_control_) {
    m_pub_local_swarm_control_->publish(*msg);
  }
}

void CommandServerNode::onGlobalSwarmPath(const SwarmPathCommand::SharedPtr msg)
{
  if (msg && m_pub_local_swarm_path_) {
    m_pub_local_swarm_path_->publish(*msg);
  }
}

void CommandServerNode::handleDrivingCommand(
  const DrivingCommand& d,
  double max_linear_speed,
  double max_angular_speed)
{
  DriveCommand dv;
  dv.header.stamp = this->now();
  dv.header.frame_id = "tablet_frame";
  dv.linear_velocity = static_cast<double>(d.move_speed) / 100.0 * max_linear_speed;
  dv.angular_velocity = -static_cast<double>(d.turn_angle) / 100.0 * max_angular_speed;

  m_pub_drivecommand_->publish(dv);

  // RCLCPP_INFO_THROTTLE(
  //   this->get_logger(),
  //   *this->get_clock(),
  //   500,
  //   "[DRIVE] spd=%d ang=%d",
  //   static_cast<int>(d.move_speed),
  //   static_cast<int>(d.turn_angle));
}

void CommandServerNode::handleTouchCommand(const TouchCoordinate& t)
{
  TouchTargetPoint message;
  message.touch_x = t.x;
  message.touch_y = t.y;

  m_pub_touchcommand_->publish(message);

  // RCLCPP_INFO(this->get_logger(), "[TOUCH] (%.3f, %.3f)", t.x, t.y);
}

void CommandServerNode::publish_command()
{
  const double max_linear_speed = this->get_parameter("max_linear_speed").as_double();
  const double max_angular_speed = this->get_parameter("max_angular_speed").as_double();

  std::vector<GenericCommand> local_cmds;
  {
    std::lock_guard<std::mutex> lock(command_queue_mutex_);
    while (!command_queue_.empty()) {
      local_cmds.push_back(command_queue_.front());
      command_queue_.pop();
    }
  }

  for (const auto& command : local_cmds) {
    switch (command.type) {
      case CommandType::STATE_CHANGE:
        handleStateCommand(command.data.state);
        break;
      case CommandType::DRIVING_INPUT:
        handleDrivingCommand(command.data.drive, max_linear_speed, max_angular_speed);
        break;
      case CommandType::TOUCH_INPUT:
        handleTouchCommand(command.data.touch);
        break;
      default:
        break;
    }
  }
}

}  // namespace command_server
