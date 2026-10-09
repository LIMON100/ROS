#ifndef COMMAND_SERVER_PROTOCOL_HPP
#define COMMAND_SERVER_PROTOCOL_HPP

#include <cstddef>
#include <cstdint>
#include <unistd.h>

namespace command_server {

constexpr uint16_t COMMAND_PORT = 65432;
constexpr uint16_t TOUCH_PORT = 65433;
constexpr uint16_t DRIVING_PORT = 65434;
constexpr uint16_t STATUS_PORT = 65435;
constexpr uint16_t PATH_PORT = 65436;

constexpr std::size_t MAX_SWARM_ROBOTS = 8;
constexpr std::size_t MAX_LOG_ENTRIES = 16;
constexpr std::size_t LOG_MESSAGE_SIZE = 64;
constexpr uint8_t MAX_FORMATION_PRESET_NUMBER = 4;

enum class AssaultCmdType : uint8_t {
  CMD_NONE = 0,
  CMD_START = 1,
  CMD_STOP = 2,
  CMD_PAUSE = 3,
  CMD_RESUME = 4,
  CMD_LOAD_PATH = 5
};

enum class StreamCommandType : uint8_t {
  NONE = 0,
  START = 1,
  STOP = 2
};

enum class MissionStatus : uint8_t {
  NONE = 0,
  READY = 1,
  MOVING = 2,
  PAUSED = 3,
  REACHED = 4,
  SURVEILLING = 5,
  ERROR = 6
};

enum class MissionErrorCode : uint8_t {
  NONE = 0,
  INVALID_PATH_PAYLOAD = 1,
  WAYPOINT_COUNT_MISMATCH = 2,
  PATH_NOT_LOADED = 3,
  INVALID_PATH_COMMAND = 4,
  RETURN_HOME_UNAVAILABLE = 5,
  INVALID_STREAM_TARGET = 6
};

enum class AttackPermission : uint8_t {
  NONE = 0,
  APPROVE = 1,
  DENY = 2
};

enum class FormationType : uint8_t {
  NONE = 0,
  RECON = 1,
  PROTECT = 2,
  ASSAULT = 3
};

enum class SwarmRole : uint8_t {
  STANDALONE = 0,
  LEADER = 1,
  FOLLOWER = 2
};

enum class LinkStatus : uint8_t {
  DISCONNECTED = 0,
  CONNECTING = 1,
  CONNECTED = 2
};

enum class CommQualityLevel : uint8_t {
  NONE = 0,
  POOR = 1,
  FAIR = 2,
  GOOD = 3,
  EXCELLENT = 4
};

enum class SwarmMovementType : uint8_t {
  HOLD = 0,
  FOLLOW_LEADER = 1,
  RETURN_HOME = 2,
  ESTOP = 3
};

enum class RobotLogSeverity : uint8_t {
  INFO = 0,
  WARN = 1,
  ERROR = 2
};

enum class RobotLogEvent : uint8_t {
  NONE = 0,
  MODE_CHANGED = 1,
  STREAM_STARTED = 2,
  STREAM_STOPPED = 3,
  ESTOP_TRIGGERED = 4,
  TARGET_DETECTED = 5,
  SWARM_UPDATE = 6
};

#pragma pack(push, 1)
struct StateCommand {
  uint8_t command_id;
  uint8_t e_stop_command;
  uint8_t attack_permission;
  uint32_t approval_request_id;
  int8_t pan_speed;
  int8_t tilt_speed;
  int8_t zoom_command;
  float lateral_wind_speed;
  uint8_t stream_command;
  uint32_t stream_target_robot_id;
  uint8_t formation_type;
  uint8_t formation_number;
  uint8_t grouping_index;
  uint8_t selected_robot_count;
  uint32_t selected_robot_ids[MAX_SWARM_ROBOTS];
  double drone_target_lat;
  double drone_target_lon;
  uint8_t drone_target_valid;
};

struct DrivingCommand {
  int8_t move_speed;
  int8_t turn_angle;
};

struct TouchCoordinate {
  float x;
  float y;
};

struct NavigationStatePacket {
  double latitude;
  double longitude;
  float heading;
  float current_speed_mps;
};

struct MissionPacket {
  uint8_t mission_status;
  uint8_t error_code;
  uint16_t current_waypoint_index;
  uint16_t total_waypoints;
  float progress_ratio;
  float distance_to_next_wp_m;
  float distance_to_goal_m;
};

struct StatusPacket {
  uint8_t rtsp_server_status;
  uint8_t active_mode_id;
  uint8_t last_tablet_command_id;
  uint8_t estop_active;
  uint8_t permission_request_active;
  uint8_t effective_attack_permission;
  uint8_t path_loaded;
  uint8_t home_position_valid;
  uint8_t return_home_available;
  float crosshair_x;
  float crosshair_y;
  float current_zoom_level;
  uint8_t mission_status;
  uint8_t swarm_role;
  uint8_t formation_type;
  uint8_t formation_number;
  uint8_t grouping_index;
  uint8_t slot_index;
  uint32_t robot_id;
  uint32_t leader_robot_id;
  uint32_t active_stream_robot_id;
  double home_lat;
  double home_lon;
  char active_path_id[LOG_MESSAGE_SIZE];
  NavigationStatePacket nav_state;
  MissionPacket mission;
};

struct RobotAggregateStatus {
  uint32_t robot_id;
  uint8_t role;
  uint8_t link_status;
  uint8_t comm_quality_level;
  uint8_t battery_pct;
  uint8_t active_mode_id;
  uint8_t mission_status;
  uint8_t estop_active;
  uint8_t formation_type;
  uint8_t formation_number;
  uint8_t grouping_index;
  uint8_t slot_index;
  uint8_t movement_type;
  uint8_t error_code;
  uint16_t status_flags;
  double latitude;
  double longitude;
  float heading;
  float speed_mps;
  float zoom_level;
};

struct RobotLogEntry {
  uint32_t robot_id;
  uint32_t timestamp_sec;
  uint8_t severity;
  uint8_t event_code;
  char message[LOG_MESSAGE_SIZE];
};

struct ApprovalRequestStatus {
  uint8_t active;
  uint8_t target_type;
  uint16_t reserved;
  uint32_t request_id;
  float confidence;
  char summary[LOG_MESSAGE_SIZE];
};

struct SwarmStatusPacket {
  StatusPacket leader_status;
  uint8_t selected_robot_count;
  uint32_t selected_robot_ids[MAX_SWARM_ROBOTS];
  uint8_t robot_count;
  RobotAggregateStatus robots[MAX_SWARM_ROBOTS];
  uint8_t log_count;
  RobotLogEntry logs[MAX_LOG_ENTRIES];
  ApprovalRequestStatus approval_request;
};

struct AssaultCommandHeader {
  uint8_t command;
  uint16_t num_waypoints;
  uint32_t data_length;
};
#pragma pack(pop)

static_assert(sizeof(StateCommand) == 72, "StateCommand must remain 72 bytes");
static_assert(sizeof(StatusPacket) == 161, "StatusPacket must remain 161 bytes");
static_assert(sizeof(SwarmStatusPacket) == 1832, "SwarmStatusPacket must remain 1832 bytes");

class SocketGuard {
public:
  explicit SocketGuard(int t_fd) : fd_(t_fd) {}
  ~SocketGuard()
  {
    if (fd_ >= 0) {
      ::close(fd_);
    }
  }

  SocketGuard(const SocketGuard&) = delete;
  SocketGuard& operator=(const SocketGuard&) = delete;

  SocketGuard(SocketGuard&& other) noexcept : fd_(other.fd_)
  {
    other.fd_ = -1;
  }

  SocketGuard& operator=(SocketGuard&& other) noexcept
  {
    if (this != &other) {
      if (fd_ >= 0) {
        ::close(fd_);
      }
      fd_ = other.fd_;
      other.fd_ = -1;
    }
    return *this;
  }

  int get() const { return fd_; }

  int release()
  {
    const int fd = fd_;
    fd_ = -1;
    return fd;
  }

  void reset(int t_fd = -1)
  {
    if (fd_ >= 0) {
      ::close(fd_);
    }
    fd_ = t_fd;
  }

private:
  int fd_;
};

}  // namespace command_server

#endif  // COMMAND_SERVER_PROTOCOL_HPP
