#include "command_server.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <memory>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>

namespace command_server {

SwarmStatusPacket CommandServerNode::buildSwarmStatusPacket()
{
  syncLeaderStatusToAggregate();

  std::array<uint32_t, MAX_SWARM_ROBOTS> selected_robot_ids{};
  {
    std::lock_guard<std::mutex> lock(selected_robot_mutex_);
    selected_robot_ids = selected_robot_ids_;
  }

  ApprovalRequestStatus approval_request{};
  {
    std::lock_guard<std::mutex> lock(approval_request_mutex_);
    approval_request = approval_request_;
  }

  char active_path_id[LOG_MESSAGE_SIZE] = {};
  {
    std::lock_guard<std::mutex> lock(path_mutex_);
    std::memcpy(active_path_id, active_path_id_, sizeof(active_path_id));
  }

  NavigationStatePacket nav_pkt{};
  nav_pkt.latitude = robot_lat_.load();
  nav_pkt.longitude = robot_lng_.load();
  nav_pkt.heading = robot_heading_.load();
  nav_pkt.current_speed_mps = current_speed_.load();

  MissionPacket mission_pkt{};
  mission_pkt.mission_status = mission_status_.load();
  mission_pkt.error_code = assault_error_.load();
  mission_pkt.current_waypoint_index = current_waypoint_idx_.load();
  mission_pkt.total_waypoints = total_waypoints_.load();
  mission_pkt.progress_ratio = assault_progress_.load();
  mission_pkt.distance_to_next_wp_m = dist_to_next_wp_.load();
  mission_pkt.distance_to_goal_m = dist_to_goal_.load();

  StatusPacket leader_status = {};
  leader_status.rtsp_server_status = rtsp_server_status_.load();
  leader_status.active_mode_id = current_active_mode_id_.load();
  leader_status.last_tablet_command_id = last_tablet_command_id_.load();
  leader_status.estop_active = static_cast<uint8_t>(estop_active_.load() ? 1 : 0);
  leader_status.permission_request_active =
    static_cast<uint8_t>(permission_request_active_.load() ? 1 : 0);
  leader_status.effective_attack_permission = last_attack_permission_.load();
  leader_status.path_loaded = path_loaded_.load();
  leader_status.home_position_valid = home_position_valid_.load();
  leader_status.return_home_available = static_cast<uint8_t>(canEnterReturnHome() ? 1 : 0);
  leader_status.crosshair_x = current_crosshair_x_.load();
  leader_status.crosshair_y = current_crosshair_y_.load();
  leader_status.current_zoom_level = current_zoom_level_.load();
  leader_status.mission_status = mission_status_.load();
  leader_status.swarm_role = swarm_role_.load();
  leader_status.formation_type = formation_type_.load();
  leader_status.formation_number = formation_number_.load();
  leader_status.grouping_index = grouping_index_.load();
  leader_status.slot_index = slot_index_.load();
  leader_status.robot_id = robot_id_.load();
  leader_status.leader_robot_id = leader_robot_id_.load();
  leader_status.active_stream_robot_id = active_stream_robot_id_.load();
  leader_status.home_lat = home_position_valid_.load() != 0 ? home_lat_.load() : 0.0;
  leader_status.home_lon = home_position_valid_.load() != 0 ? home_lon_.load() : 0.0;
  leader_status.nav_state = nav_pkt;
  leader_status.mission = mission_pkt;
  std::memcpy(leader_status.active_path_id, active_path_id, sizeof(active_path_id));

  SwarmStatusPacket packet_to_send = {};
  packet_to_send.leader_status = leader_status;
  packet_to_send.selected_robot_count = selected_robot_count_.load();
  std::memcpy(
    packet_to_send.selected_robot_ids,
    selected_robot_ids.data(),
    sizeof(packet_to_send.selected_robot_ids));
  packet_to_send.robot_count =
    std::min<uint8_t>(robot_count_.load(), static_cast<uint8_t>(MAX_SWARM_ROBOTS));
  packet_to_send.log_count =
    std::min<uint8_t>(log_count_.load(), static_cast<uint8_t>(MAX_LOG_ENTRIES));
  {
    std::lock_guard<std::mutex> lock(swarm_status_mutex_);
    std::memcpy(packet_to_send.robots, robot_statuses_.data(), sizeof(packet_to_send.robots));
  }
  // The swarm executes one collective mission, but followers don't track mission
  // progress on their own (they only relay the leader's path). Reflect the leader's
  // mission_status on every connected robot so the tablet shows s2/s3 as
  // MOVING/REACHED alongside s1 instead of a stale NONE.
  {
    const uint8_t swarm_mission_status = mission_status_.load();
    for (std::size_t i = 0; i < MAX_SWARM_ROBOTS; ++i) {
      if (packet_to_send.robots[i].link_status ==
          static_cast<uint8_t>(LinkStatus::CONNECTED)) {
        packet_to_send.robots[i].mission_status = swarm_mission_status;
      }
    }
  }
  {
    std::lock_guard<std::mutex> lock(log_mutex_);
    std::memcpy(packet_to_send.logs, recent_logs_.data(), sizeof(packet_to_send.logs));
  }
  packet_to_send.approval_request = approval_request;

  return packet_to_send;
}

void CommandServerNode::statusServerThread()
{
  int server_fd;
  struct sockaddr_in address{};
  socklen_t addrlen = sizeof(address);

  if ((server_fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Status TCP] Socket failed");
    return;
  }

  int opt = 1;
  ::setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(STATUS_PORT);

  if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Status TCP] Bind failed");
    return;
  }
  if (::listen(server_fd, 1) < 0) {
    RCLCPP_ERROR(this->get_logger(), "[Status TCP] Listen failed");
    return;
  }

  SocketGuard server_guard(server_fd);

  RCLCPP_INFO(this->get_logger(), "[Status TCP] Listening on %d", STATUS_PORT);

  int client_socket = -1;
  std::unique_ptr<SocketGuard> client_guard;

  while (rclcpp::ok() && !stop_threads_.load()) {
    if (client_socket < 0) {
      fd_set read_fds;
      FD_ZERO(&read_fds);
      FD_SET(server_guard.get(), &read_fds);

      struct timeval timeout;
      timeout.tv_sec = 1;
      timeout.tv_usec = 0;

      const int activity =
        select(server_guard.get() + 1, &read_fds, nullptr, nullptr, &timeout);

      if (activity < 0) {
        if (errno == EINTR) {
          continue;
        }
        break;
      }
      if (activity == 0) {
        continue;
      }

      if (FD_ISSET(server_guard.get(), &read_fds)) {
        client_socket = ::accept(server_guard.get(), (struct sockaddr *)&address, &addrlen);
        if (client_socket >= 0) {
          RCLCPP_INFO(this->get_logger(), "[Status TCP] App connected.");
          client_guard = std::make_unique<SocketGuard>(client_socket);
        }
      }
      continue;
    }

    const SwarmStatusPacket packet_to_send = buildSwarmStatusPacket();
    const ssize_t sent =
      ::send(client_guard->get(), &packet_to_send, sizeof(SwarmStatusPacket), MSG_NOSIGNAL);
    if (sent < 0) {
      RCLCPP_INFO(this->get_logger(), "[Status TCP] App disconnected/Error.");
      client_guard.reset();
      client_socket = -1;
      continue;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  RCLCPP_INFO(this->get_logger(), "[Status TCP] Thread exit.");
}

}  // namespace command_server
