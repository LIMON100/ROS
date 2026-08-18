#include "command_server.hpp"
#include "command_server_path_payload_parser.hpp"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <sys/select.h>
#include <sys/socket.h>
#include <vector>

namespace command_server {
namespace {

bool read_full(int t_sock, void* t_buffer, size_t t_size)
{
  char* ptr = static_cast<char*>(t_buffer);
  size_t left = t_size;
  while (left > 0) {
    const ssize_t n = ::read(t_sock, ptr, left);
    if (n <= 0) {
      return false;
    }
    ptr += n;
    left -= static_cast<size_t>(n);
  }
  return true;
}

bool isPathCapableMode(uint8_t t_active_mode_id)
{
  // PROTECT modes also accept path missions — the queue-based engagement
  // can run alongside a waypoint route (drive to waypoint, engage queued
  // targets there, continue). Only IDLE / E-STOP / RTH are rejected.
  return t_active_mode_id == OperationState::ACTIVE_MODE_RECON ||
         t_active_mode_id == OperationState::ACTIVE_MODE_ASSAULT ||
         t_active_mode_id == OperationState::ACTIVE_MODE_PROTECT_GENERAL ||
         t_active_mode_id == OperationState::ACTIVE_MODE_PROTECT_DRONE;
}

}  // namespace

void CommandServerNode::commandServerThread()
{
  int server_fd;
  struct sockaddr_in address{};
  socklen_t addrlen = sizeof(address);

  if ((server_fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Command TCP] Socket creation failed");
    return;
  }

  int opt = 1;
  if (::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
    RCLCPP_ERROR(this->get_logger(), "[Command TCP] Setsockopt failed");
    return;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(COMMAND_PORT);

  if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Command TCP] Bind failed");
    return;
  }

  if (::listen(server_fd, 5) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Command TCP] Listen failed");
    return;
  }

  RCLCPP_INFO(this->get_logger(), "[Command TCP] Listening on %d", COMMAND_PORT);

  SocketGuard server_guard(server_fd);

  while (rclcpp::ok() && !stop_threads_.load()) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_guard.get(), &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    const int activity = select(server_guard.get() + 1, &read_fds, nullptr, nullptr, &timeout);

    if (activity < 0) {
      if (errno == EINTR) {
        continue;
      }
      RCLCPP_ERROR(this->get_logger(), "[Command TCP] Select error");
      break;
    }

    if (activity == 0) {
      continue;
    }

    if (FD_ISSET(server_guard.get(), &read_fds)) {
      const int client_socket = ::accept(
        server_guard.get(), (struct sockaddr *)&address, &addrlen);

      if (client_socket < 0) {
        RCLCPP_WARN(this->get_logger(), "[Command TCP] Accept failed");
        continue;
      }

      RCLCPP_INFO(this->get_logger(), "[Command TCP] Client connected.");

      SocketGuard client_guard(client_socket);

      while (rclcpp::ok() && !stop_threads_.load()) {
        StateCommand received_state{};
        if (!read_full(client_guard.get(), &received_state, sizeof(StateCommand))) {
          RCLCPP_INFO(this->get_logger(), "[Command TCP] Client disconnected.");
          break;
        }

        const GenericCommand cmd = {CommandType::STATE_CHANGE, {.state = received_state}};
        {
          std::lock_guard<std::mutex> lock(command_queue_mutex_);
          command_queue_.push(cmd);
        }
        publish_command();
      }
    }
  }

  RCLCPP_INFO(this->get_logger(), "[Command TCP] Thread exit.");
}

void CommandServerNode::touchServerThread()
{
  int server_fd;
  if ((server_fd = ::socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Touch UDP] Socket failed");
    return;
  }

  struct sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(TOUCH_PORT);

  if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Touch UDP] Bind failed");
    return;
  }

  SocketGuard server_guard(server_fd);

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  ::setsockopt(server_guard.get(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

  RCLCPP_INFO(this->get_logger(), "[Touch UDP] Listening on %d", TOUCH_PORT);

  while (rclcpp::ok() && !stop_threads_.load()) {
    TouchCoordinate received_touch{};
    const ssize_t len = ::recvfrom(
      server_guard.get(), &received_touch, sizeof(TouchCoordinate), 0, nullptr, nullptr);

    if (len != static_cast<ssize_t>(sizeof(TouchCoordinate))) {
      continue;
    }

    const GenericCommand cmd = {CommandType::TOUCH_INPUT, {.touch = received_touch}};
    {
      std::lock_guard<std::mutex> lock(command_queue_mutex_);
      command_queue_.push(cmd);
    }
    publish_command();
  }

  RCLCPP_INFO(this->get_logger(), "[Touch UDP] Thread exit.");
}

void CommandServerNode::drivingServerThread()
{
  int server_fd;
  if ((server_fd = ::socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Driving UDP] Socket failed");
    return;
  }

  struct sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(DRIVING_PORT);

  if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Driving UDP] Bind failed");
    return;
  }

  SocketGuard server_guard(server_fd);

  struct timeval tv;
  tv.tv_sec = 1;
  tv.tv_usec = 0;
  ::setsockopt(server_guard.get(), SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));

  RCLCPP_INFO(this->get_logger(), "[Driving UDP] Listening on %d", DRIVING_PORT);

  while (rclcpp::ok() && !stop_threads_.load()) {
    DrivingCommand received_drive{};
    const ssize_t len = ::recvfrom(
      server_guard.get(), &received_drive, sizeof(DrivingCommand), 0, nullptr, nullptr);

    if (len != static_cast<ssize_t>(sizeof(DrivingCommand))) {
      continue;
    }

    const GenericCommand cmd = {CommandType::DRIVING_INPUT, {.drive = received_drive}};
    {
      std::lock_guard<std::mutex> lock(command_queue_mutex_);
      command_queue_.push(cmd);
    }
  }

  RCLCPP_INFO(this->get_logger(), "[Driving UDP] Thread exit.");
}

void CommandServerNode::pathServerThread()
{
  int server_fd;
  struct sockaddr_in address{};
  socklen_t addrlen = sizeof(address);

  if ((server_fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Path TCP] Socket creation failed");
    return;
  }

  int opt = 1;
  ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(PATH_PORT);

  if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Path TCP] Bind failed");
    return;
  }

  if (::listen(server_fd, 1) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Path TCP] Listen failed");
    return;
  }

  SocketGuard server_guard(server_fd);

  RCLCPP_INFO(this->get_logger(), "[Path TCP] Listening on %d", PATH_PORT);

  while (rclcpp::ok() && !stop_threads_.load()) {
    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(server_guard.get(), &read_fds);

    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    const int activity = select(server_guard.get() + 1, &read_fds, nullptr, nullptr, &timeout);

    if (activity < 0) {
      if (errno == EINTR) {
        continue;
      }
      break;
    }

    if (activity == 0) {
      continue;
    }

    if (!FD_ISSET(server_guard.get(), &read_fds)) {
      continue;
    }

    const int client_socket = ::accept(
      server_guard.get(), (struct sockaddr *)&address, &addrlen);
    if (client_socket < 0) {
      continue;
    }

    RCLCPP_INFO(this->get_logger(), "[Path TCP] Client connected.");

    SocketGuard client_guard(client_socket);

    while (rclcpp::ok() && !stop_threads_.load()) {
      AssaultCommandHeader header{};
      std::string json_data;
      if (!read_full(client_guard.get(), &header, sizeof(AssaultCommandHeader))) {
        RCLCPP_INFO(this->get_logger(), "[Path TCP] Client disconnected (Header).");
        break;
      }

      const AssaultCmdType cmd_type = static_cast<AssaultCmdType>(header.command);
      std::string cmd_str;
      switch (cmd_type) {
        case AssaultCmdType::CMD_START:
          cmd_str = "START";
          break;
        case AssaultCmdType::CMD_STOP:
          cmd_str = "STOP";
          break;
        case AssaultCmdType::CMD_PAUSE:
          cmd_str = "PAUSE";
          break;
        case AssaultCmdType::CMD_RESUME:
          cmd_str = "RESUME";
          break;
        case AssaultCmdType::CMD_LOAD_PATH:
          cmd_str = "LOAD_PATH";
          break;
        default:
          cmd_str = "UNKNOWN (" + std::to_string(static_cast<int>(header.command)) + ")";
          break;
      }

      bool should_publish_path_command = false;
      uint16_t published_num_waypoints = header.num_waypoints;
      std::string published_path_json;

      if (header.data_length > 0) {
        if (header.data_length > 1024 * 1024) {
          RCLCPP_ERROR(
            this->get_logger(),
            "[Path TCP] Data too large (%u bytes). Dropping.",
            header.data_length);
          break;
        }

        std::vector<char> buffer(header.data_length);
        if (!read_full(client_guard.get(), buffer.data(), header.data_length)) {
          RCLCPP_INFO(this->get_logger(), "[Path TCP] Client disconnected (Payload).");
          break;
        }

        json_data.assign(buffer.begin(), buffer.end());
      }

      if (header.data_length > 0 && cmd_type != AssaultCmdType::CMD_LOAD_PATH) {
        appendRobotLog(
          robot_id_.load(),
          RobotLogSeverity::WARN,
          RobotLogEvent::SWARM_UPDATE,
          "Unexpected payload ignored for non-LoadPath command");
        RCLCPP_WARN(
          this->get_logger(),
          "[Path TCP] Cmd: %s | Ignored unexpected payload (%u bytes)",
          cmd_str.c_str(),
          static_cast<unsigned>(header.data_length));
      }

      switch (cmd_type) {
        case AssaultCmdType::CMD_LOAD_PATH:
        {
          if (json_data.empty()) {
            setMissionError(
              MissionErrorCode::INVALID_PATH_PAYLOAD,
              "LoadPath rejected: payload missing");
            RCLCPP_WARN(
              this->get_logger(),
              "[Path TCP] Rejected LOAD_PATH because payload was missing.");
            break;
          }

          const std::size_t parsed_waypoint_count = extractWaypointCountFromPayload(json_data);
          if (parsed_waypoint_count == 0) {
            setMissionError(
              MissionErrorCode::INVALID_PATH_PAYLOAD,
              "LoadPath rejected: invalid waypoint payload");
            RCLCPP_WARN(
              this->get_logger(),
              "[Path TCP] Rejected LOAD_PATH because payload did not contain valid waypoints.");
            break;
          }

          if (parsed_waypoint_count != header.num_waypoints) {
            setMissionError(
              MissionErrorCode::WAYPOINT_COUNT_MISMATCH,
              "LoadPath rejected: waypoint count mismatch");
            RCLCPP_WARN(
              this->get_logger(),
              "[Path TCP] Rejected LOAD_PATH because header waypoint count %u != payload waypoint count %u.",
              static_cast<unsigned>(header.num_waypoints),
              static_cast<unsigned>(parsed_waypoint_count));
            break;
          }

          {
            std::lock_guard<std::mutex> lock(path_mutex_);
            current_path_json_ = json_data;
            std::memset(active_path_id_, 0, sizeof(active_path_id_));
            std::string active_path_id = extractJsonStringField(json_data, "missionId");
            if (active_path_id.empty()) {
              active_path_id = extractJsonStringField(json_data, "routeId");
            }
            if (!active_path_id.empty()) {
              std::strncpy(active_path_id_, active_path_id.c_str(), LOG_MESSAGE_SIZE - 1);
              active_path_id_[LOG_MESSAGE_SIZE - 1] = '\0';
            }
          }

          path_loaded_.store(1);
          resetMissionStateForLoadedPath(header.num_waypoints);
          appendRobotLog(
            robot_id_.load(),
            RobotLogSeverity::INFO,
            RobotLogEvent::SWARM_UPDATE,
            "Path loaded and mission is ready");
          RCLCPP_INFO(
            this->get_logger(),
            "[Path TCP] Cmd: %s | Received Path (%u WPs, %u bytes)",
            cmd_str.c_str(),
            static_cast<unsigned>(header.num_waypoints),
            static_cast<unsigned>(header.data_length));
          should_publish_path_command = true;
          published_num_waypoints = header.num_waypoints;
          published_path_json = json_data;
          break;
        }
        case AssaultCmdType::CMD_START:
        {
          if (path_loaded_.load() == 0 || total_waypoints_.load() == 0) {
            setMissionError(
              MissionErrorCode::PATH_NOT_LOADED,
              "Mission start rejected: no path loaded");
            RCLCPP_WARN(this->get_logger(), "[Path TCP] Rejected START because no path is loaded.");
            break;
          }

          if (!isPathCapableMode(current_active_mode_id_.load())) {
            setMissionError(
              MissionErrorCode::INVALID_PATH_COMMAND,
              "Mission start rejected: active mode is not mission-capable");
            RCLCPP_WARN(
              this->get_logger(),
              "[Path TCP] Rejected START because active mode is not mission-capable.");
            break;
          }

          const uint8_t current_mission_status = mission_status_.load();
          const bool restart_route =
            current_mission_status != static_cast<uint8_t>(MissionStatus::MOVING) &&
            current_mission_status != static_cast<uint8_t>(MissionStatus::PAUSED);

          if (restart_route || home_position_valid_.load() == 0) {
            captureHomePositionFromCurrentPose("Home position captured for mission start");
          }

          mission_history_available_.store(1);
          assault_state_.store(static_cast<uint8_t>(MissionStatus::MOVING));
          mission_status_.store(static_cast<uint8_t>(MissionStatus::MOVING));
          clearMissionError();
          should_publish_path_command = true;
          RCLCPP_INFO(this->get_logger(), "[Path TCP] Cmd: %s (No Payload)", cmd_str.c_str());
          break;
        }
        case AssaultCmdType::CMD_PAUSE:
          if (mission_status_.load() != static_cast<uint8_t>(MissionStatus::MOVING)) {
            setMissionError(
              MissionErrorCode::INVALID_PATH_COMMAND,
              "Mission pause rejected: mission is not moving");
            RCLCPP_WARN(
              this->get_logger(),
              "[Path TCP] Rejected PAUSE because mission is not moving.");
            break;
          }

          assault_state_.store(static_cast<uint8_t>(MissionStatus::PAUSED));
          mission_status_.store(static_cast<uint8_t>(MissionStatus::PAUSED));
          current_speed_.store(0.0f);
          clearMissionError();
          should_publish_path_command = true;
          RCLCPP_INFO(this->get_logger(), "[Path TCP] Cmd: %s (No Payload)", cmd_str.c_str());
          break;
        case AssaultCmdType::CMD_RESUME:
          if (path_loaded_.load() == 0 || total_waypoints_.load() == 0) {
            setMissionError(
              MissionErrorCode::PATH_NOT_LOADED,
              "Mission resume rejected: no path loaded");
            RCLCPP_WARN(
              this->get_logger(),
              "[Path TCP] Rejected RESUME because no path is loaded.");
            break;
          }

          if (!isPathCapableMode(current_active_mode_id_.load())) {
            setMissionError(
              MissionErrorCode::INVALID_PATH_COMMAND,
              "Mission resume rejected: active mode is not mission-capable");
            RCLCPP_WARN(
              this->get_logger(),
              "[Path TCP] Rejected RESUME because active mode is not mission-capable.");
            break;
          }

          if (home_position_valid_.load() == 0) {
            captureHomePositionFromCurrentPose("Home position captured for mission resume");
          }

          mission_history_available_.store(1);
          assault_state_.store(static_cast<uint8_t>(MissionStatus::MOVING));
          mission_status_.store(static_cast<uint8_t>(MissionStatus::MOVING));
          clearMissionError();
          should_publish_path_command = true;
          RCLCPP_INFO(this->get_logger(), "[Path TCP] Cmd: %s (No Payload)", cmd_str.c_str());
          break;
        case AssaultCmdType::CMD_STOP:
          resetMissionExecution();
          assault_state_.store(static_cast<uint8_t>(MissionStatus::NONE));
          mission_status_.store(static_cast<uint8_t>(MissionStatus::NONE));
          current_waypoint_idx_.store(0);
          assault_progress_.store(0.0f);
          dist_to_next_wp_.store(0.0f);
          dist_to_goal_.store(0.0f);
          clearMissionError();
          should_publish_path_command = true;
          RCLCPP_INFO(this->get_logger(), "[Path TCP] Cmd: %s (No Payload)", cmd_str.c_str());
          break;
        default:
          if (!json_data.empty()) {
            appendRobotLog(
              robot_id_.load(),
              RobotLogSeverity::WARN,
              RobotLogEvent::SWARM_UPDATE,
              "Unexpected payload ignored for invalid path command");
          }
          RCLCPP_WARN(
            this->get_logger(),
            "[Path TCP] Unknown path command: %u",
            static_cast<unsigned>(header.command));
          break;
      }

      if (!should_publish_path_command) {
        continue;
      }

      SwarmPathCommand path_msg;
      path_msg.header.stamp = this->now();
      path_msg.header.frame_id = "tablet_frame";
      path_msg.command = static_cast<uint8_t>(cmd_type);
      path_msg.num_waypoints = published_num_waypoints;
      path_msg.path_json = published_path_json;
      m_pub_swarm_path_command_->publish(path_msg);
      // Local mirror so this (leader) robot's own executor consumes the path too.
      if (m_pub_local_swarm_path_) {
        m_pub_local_swarm_path_->publish(path_msg);
      }
    }
  }

  RCLCPP_INFO(this->get_logger(), "[Path TCP] Thread exit.");
}

}  // namespace command_server
