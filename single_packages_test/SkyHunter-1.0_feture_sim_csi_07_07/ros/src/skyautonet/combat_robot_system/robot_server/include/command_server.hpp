#ifndef command_server_HPP
#define command_server_HPP

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <string>
#include <vector>

#include <rclcpp/rclcpp.hpp>

#include "combat_robot_msgs/msg/mission_control_command.hpp"
#include "combat_robot_msgs/msg/stream_control_command.hpp"
#include "combat_robot_msgs/msg/swarm_control_command.hpp"
#include "combat_robot_msgs/msg/swarm_path_command.hpp"
#include "combat_robot_msgs/msg/gnss_status.hpp"
#include "combat_robot_msgs/msg/chassis_status.hpp"
#include "combat_robot_msgs/msg/lidar_status.hpp"
#include "combat_robot_msgs/msg/drive_command.hpp"
#include "combat_robot_msgs/msg/touch_target_point.hpp"
#include "combat_robot_msgs/msg/operation_state.hpp"
#include "combat_robot_msgs/msg/swarm_follower_status.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "std_msgs/msg/u_int8.hpp"
#include "command_server_internal_utils.hpp"
#include "command_server_protocol.hpp"

namespace command_server {

using combat_robot_msgs::msg::DriveCommand;
using combat_robot_msgs::msg::MissionControlCommand;
using combat_robot_msgs::msg::OperationState;
using combat_robot_msgs::msg::StreamControlCommand;
using combat_robot_msgs::msg::SwarmControlCommand;
using combat_robot_msgs::msg::SwarmPathCommand;
using combat_robot_msgs::msg::GnssStatus;
using combat_robot_msgs::msg::ChassisStatus;
using combat_robot_msgs::msg::LidarStatus;
using combat_robot_msgs::msg::TouchTargetPoint;
using combat_robot_msgs::msg::SwarmFollowerStatus;

enum class CommandType { STATE_CHANGE, TOUCH_INPUT, DRIVING_INPUT };
struct GenericCommand {
    CommandType type;
    union {
        StateCommand    state;
        TouchCoordinate touch;
        DrivingCommand  drive;
    } data;
};

class CommandServerNode : public rclcpp::Node {
public:
    CommandServerNode(const rclcpp::NodeOptions& options);
    ~CommandServerNode();

private:
    static constexpr double DEFAULT_GPS_LAT = 37.5665;
    static constexpr double DEFAULT_GPS_LON = 126.9780;

    void InitRosCommon();
    bool isLeaderRole() const {
        return swarm_role_.load() == static_cast<uint8_t>(SwarmRole::LEADER);
    }
    void onGlobalSwarmControl(const SwarmControlCommand::SharedPtr msg);
    void onGlobalSwarmPath(const SwarmPathCommand::SharedPtr msg);
    void onGlobalMissionControl(const MissionControlCommand::SharedPtr msg);
    void onOperationState(const OperationState::SharedPtr msg);
    void initializeSwarmStatus();
    void loadDummySwarmStatus();
    void appendRobotLog(uint32_t robot_id, RobotLogSeverity severity, RobotLogEvent event_code, const std::string& message);
    void updateAggregatedRobotStatus(
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
      float zoom_level);
    void syncLeaderStatusToAggregate();
    uint32_t allocateApprovalRequestId();
    uint32_t currentApprovalRequestId();
    void clearApprovalRequestSession();
    uint32_t activateApprovalRequestSession(const char* summary, float confidence);
    void clearMissionError();
    void setMissionError(MissionErrorCode code, const std::string& message);
    bool captureHomePositionFromCurrentPose(const std::string& reason);
    void resetMissionExecution();
    void resetMissionStateForLoadedPath(uint16_t waypoint_count);
    bool canEnterReturnHome() const;
    SwarmStatusPacket buildSwarmStatusPacket();
    void handleStateCommand(const StateCommand& state_command);
    void handleDrivingCommand(
      const DrivingCommand& driving_command,
      double max_linear_speed,
      double max_angular_speed);
    void handleTouchCommand(const TouchCoordinate& touch_coordinate);

    void commandServerThread();
    void touchServerThread();
    void drivingServerThread();
    void statusServerThread();
    void pathServerThread();

    std::thread cmd_thread_;
    std::thread touch_thread_;
    std::thread drive_thread_;
    std::thread status_thread_;
    std::thread path_thread_;

    std::atomic<bool> stop_threads_{false};

    std::queue<GenericCommand> command_queue_;
    std::mutex command_queue_mutex_;

    // Per-robot namespace ("s1".."s8") used to build local mirror command topics.
    // Set once in InitRosCommon from the "robot_namespace" param (default "s{robot_id}").
    std::string robot_namespace_;

    std::string current_path_json_;
    std::mutex  path_mutex_;
    char active_path_id_[LOG_MESSAGE_SIZE]{};

    std::atomic<uint8_t> current_active_mode_id_{0};
    std::atomic<uint8_t> last_tablet_command_id_{0};
    std::atomic<uint8_t> rtsp_server_status_{0};
    std::atomic<uint8_t> mission_status_{static_cast<uint8_t>(MissionStatus::NONE)};
    std::atomic<uint8_t> last_attack_permission_{0};
    std::atomic<bool>    estop_active_{false};
    std::atomic<bool>    permission_request_active_{false};
    std::atomic<float>   current_crosshair_x_{-1.0f};
    std::atomic<float>   current_crosshair_y_{-1.0f};
    std::atomic<float>   current_zoom_level_{1.0f};
    std::atomic<uint8_t> swarm_role_{static_cast<uint8_t>(SwarmRole::LEADER)};
    std::atomic<uint8_t> formation_type_{static_cast<uint8_t>(FormationType::NONE)};
    std::atomic<uint8_t> formation_number_{0};
    std::atomic<uint8_t> grouping_index_{0};
    std::atomic<uint8_t> slot_index_{0};
    std::atomic<uint8_t> leader_link_status_{static_cast<uint8_t>(LinkStatus::CONNECTED)};
    std::atomic<uint8_t> leader_comm_quality_{static_cast<uint8_t>(CommQualityLevel::EXCELLENT)};
    std::atomic<uint8_t> leader_battery_pct_{100};
    std::atomic<uint32_t> robot_id_{1};
    std::atomic<uint32_t> leader_robot_id_{1};
    std::atomic<uint32_t> active_stream_robot_id_{0};
    std::atomic<uint8_t> selected_robot_count_{0};
    std::atomic<uint8_t> robot_count_{static_cast<uint8_t>(MAX_SWARM_ROBOTS)};
    std::atomic<uint8_t> log_count_{0};
    std::atomic<uint32_t> next_approval_request_id_{1002};
    std::atomic<uint8_t> path_loaded_{0};
    std::atomic<uint8_t> home_position_valid_{0};
    std::atomic<uint8_t> mission_history_available_{0};

    std::atomic<double> robot_lat_{DEFAULT_GPS_LAT};
    std::atomic<double> robot_lng_{DEFAULT_GPS_LON};
    std::atomic<double> home_lat_{0.0};
    std::atomic<double> home_lon_{0.0};
    std::atomic<float>  robot_heading_{0.0f};
    std::atomic<float>  current_speed_{0.0f};

    std::atomic<uint8_t>  assault_state_{static_cast<uint8_t>(MissionStatus::NONE)};
    std::atomic<uint16_t> current_waypoint_idx_{0};
    std::atomic<uint16_t> total_waypoints_{0};
    std::atomic<float>    assault_progress_{0.0f};
    std::atomic<float>    dist_to_next_wp_{0.0f};
    std::atomic<float>    dist_to_goal_{0.0f};
    std::atomic<uint8_t>  assault_error_{0};
    std::atomic<int64_t>  swarm_mission_state_last_update_ns_{0};

    std::atomic<uint8_t> operation_state_{0};
    std::atomic<int64_t> operation_state_last_update_ns_{0};

    std::atomic<uint8_t>  gnss_fix_status_{0};
    std::atomic<uint8_t>  gnss_num_satellites_{0};
    std::atomic<double>   gnss_altitude_m_{0.0};
    std::atomic<float>    gnss_horizontal_accuracy_m_{-1.0f};
    std::atomic<float>    gnss_vertical_accuracy_m_{-1.0f};
    std::atomic<int64_t>  gnss_last_update_ns_{0};

    std::atomic<uint8_t>  chassis_drive_state_{0};
    std::atomic<float>    chassis_voltage_v_{0.0f};
    std::atomic<float>    chassis_current_a_{0.0f};
    std::atomic<float>    chassis_linear_velocity_mps_{0.0f};
    std::atomic<float>    chassis_angular_velocity_rps_{0.0f};
    std::atomic<uint32_t> chassis_fault_flags_{0};
    std::atomic<float>    chassis_motor_temp_c_{0.0f};
    std::atomic<int64_t>  chassis_last_update_ns_{0};

    std::atomic<uint8_t>  lidar_status_{0};
    std::atomic<uint32_t> lidar_last_scan_point_count_{0};
    std::atomic<float>    lidar_scan_rate_hz_{0.0f};
    std::atomic<bool>     lidar_obstacle_detected_{false};
    std::atomic<float>    lidar_min_obstacle_distance_m_{0.0f};
    std::atomic<int64_t>  lidar_last_update_ns_{0};

    std::array<RobotAggregateStatus, MAX_SWARM_ROBOTS> robot_statuses_{};
    std::array<RobotLogEntry, MAX_LOG_ENTRIES> recent_logs_{};
    std::array<uint32_t, MAX_SWARM_ROBOTS> selected_robot_ids_{};
    ApprovalRequestStatus approval_request_{};
    std::mutex swarm_status_mutex_;
    std::mutex log_mutex_;
    std::mutex selected_robot_mutex_;
    std::mutex approval_request_mutex_;

    void publish_command();

    rclcpp::Publisher<MissionControlCommand>::SharedPtr m_pub_mission_control_command_;
    rclcpp::Publisher<StreamControlCommand>::SharedPtr m_pub_stream_control_command_;
    rclcpp::Publisher<SwarmControlCommand>::SharedPtr m_pub_swarm_control_command_;
    rclcpp::Publisher<SwarmPathCommand>::SharedPtr m_pub_swarm_path_command_;
    // Local per-robot mirrors (/{ns}/...): both roles publish here so this robot's
    // downstream (swarm_path_executor / FSM) consumes commands without knowing the
    // source (tablet vs swarm bus). Leader mirrors what it publishes globally;
    // follower mirrors what it receives on the global bus.
    rclcpp::Publisher<SwarmControlCommand>::SharedPtr m_pub_local_swarm_control_;
    rclcpp::Publisher<SwarmPathCommand>::SharedPtr m_pub_local_swarm_path_;
    rclcpp::Publisher<MissionControlCommand>::SharedPtr m_pub_local_mission_control_;
    rclcpp::Publisher<DriveCommand>::SharedPtr m_pub_drivecommand_;
    rclcpp::Publisher<TouchTargetPoint>::SharedPtr m_pub_touchcommand_;
    rclcpp::TimerBase::SharedPtr timer_;

    rclcpp::Subscription<OperationState>::SharedPtr  m_sub_operationstate_;
    rclcpp::Subscription<GnssStatus>::SharedPtr    m_sub_gnss_status_;
    rclcpp::Subscription<ChassisStatus>::SharedPtr m_sub_chassis_status_;
    rclcpp::Subscription<LidarStatus>::SharedPtr   m_sub_lidar_status_;
    rclcpp::Subscription<std_msgs::msg::UInt8>::SharedPtr m_sub_rtsp_status_;
    // Own NavSatFix (/{ns}/fix) -> robot_lat_/robot_lng_ for both roles. In sim the
    // GnssStatus (/gnss/status) source has no publisher, so this is the live position.
    rclcpp::Subscription<sensor_msgs::msg::NavSatFix>::SharedPtr m_sub_fix_;
    // Leader role: one subscription per OTHER robot's follower status topic so the
    // aggregate (robot_statuses_) reflects every follower, not just one.
    std::vector<rclcpp::Subscription<SwarmFollowerStatus>::SharedPtr>
      m_sub_follower_statuses_;
    // Follower role: publisher + timer for this robot's own follower status.
    rclcpp::Publisher<SwarmFollowerStatus>::SharedPtr m_pub_own_follower_status_;
    rclcpp::TimerBase::SharedPtr m_follower_status_timer_;
    // Follower role only: subscribe the global swarm bus and re-publish to local mirrors.
    rclcpp::Subscription<SwarmControlCommand>::SharedPtr m_sub_global_swarm_control_;
    rclcpp::Subscription<SwarmPathCommand>::SharedPtr m_sub_global_swarm_path_;
    rclcpp::Subscription<MissionControlCommand>::SharedPtr m_sub_global_mission_control_;

    void onRTSPStatus(const std_msgs::msg::UInt8::SharedPtr msg);
    void onGnssStatus(const GnssStatus::SharedPtr msg);
    void onChassisStatus(const ChassisStatus::SharedPtr msg);
    void onLidarStatus(const LidarStatus::SharedPtr msg);
    void onSwarmFollowerStatus(const SwarmFollowerStatus::SharedPtr msg);
    void onOwnFix(const sensor_msgs::msg::NavSatFix::SharedPtr msg);
    // Follower role: periodically publish this robot's own status (incl. GPS) on
    // /swarm/follower/s{robot_id}/status so the leader can aggregate it for the tablet.
    void publishOwnFollowerStatus();
};

}  // namespace command_server

#endif // command_server_HPP
