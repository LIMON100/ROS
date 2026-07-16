#include "combat_robot_operation_system.hpp"

namespace combat_robot_system {

void CombatRobotOperationSystem::onTargetPoint(const TargetPoint::ConstSharedPtr msg)
{
    last_detector_update_time_ = this->now();
    if (msg->header.stamp.sec == 0 && msg->header.stamp.nanosec == 0) {
        RCLCPP_WARN(get_logger(), "Received target point with zero timestamp, ignoring.");
        return;
    }

    double target_time = msg->header.stamp.sec + msg->header.stamp.nanosec * 1e-9;

    std::lock_guard<std::mutex> lock(mtx_target_);
    m_prev_target_object_ = target_object_;
    m_prev_target_time_ = last_Target_time_;

    last_target_header_ = msg->header;
    last_Target_time_ = target_time < 0 ? 0.0 : target_time;
    isTargetLocked_.store(static_cast<bool>(msg->is_locked));
    target_object_.x = static_cast<float>(msg->x);
    target_object_.y = static_cast<float>(msg->y);
    target_object_.height = static_cast<float>(msg->height);
    target_object_.track_id = msg->track_id;
    target_object_.class_id = msg->class_id;
    target_object_.bounding_box = msg->box;
}

void CombatRobotOperationSystem::onPanTiltState(const PanTiltState::ConstSharedPtr msg)
{
    std::lock_guard<std::mutex> lock(mtx_actuator_);
    last_pantilt_state_update_time_ = this->now();
    prev_actuator_horizontal_angle_ = current_actuator_horizontal_angle_;
    prev_actuator_vertical_angle_ = current_actuator_vertical_angle_;
    current_actuator_vertical_angle_ = msg->vertical_angle;
    current_actuator_horizontal_angle_ = msg->horizontal_angle;

    is_pan_tilt_moving_ =
        (fabs(current_actuator_horizontal_angle_ - prev_actuator_horizontal_angle_) > 0.01) ||
        (fabs(current_actuator_vertical_angle_ - prev_actuator_vertical_angle_) > 0.01);
}

void CombatRobotOperationSystem::onMissionControlCommand(
    const MissionControlCommand::ConstSharedPtr msg)
{
    std::lock_guard<std::mutex> lock(mtx_cmd_);
    last_mission_control_command_time_ = this->now();

    mission_control_command_ = *msg;
    if (std::abs(mission_control_command_.pan_speed) >= 10 ||
        std::abs(mission_control_command_.tilt_speed) >= 10) {
        new_pan_tilt_command_.store(true);
    } else {
        new_pan_tilt_command_.store(false);
    }
}

void CombatRobotOperationSystem::onSwarmPathCommand(const SwarmPathCommand::ConstSharedPtr msg)
{
    if (!msg || msg->command == SwarmPathCommand::CMD_NONE) {
        return;
    }

    std::lock_guard<std::mutex> lock(mtx_mission_);
    switch (msg->command) {
    case SwarmPathCommand::CMD_LOAD_PATH:
        mission_execution_state_.status = OperationState::MISSION_READY;
        mission_execution_state_.current_waypoint_index = 0;
        mission_execution_state_.total_waypoints = msg->num_waypoints;
        mission_execution_state_.current_speed_mps = 0.0f;
        mission_execution_state_.path_loaded =
            (msg->num_waypoints > 0) || !msg->path_json.empty();
        RCLCPP_INFO(
            this->get_logger(),
            "Loaded mission path metadata: waypoints=%u",
            static_cast<unsigned int>(msg->num_waypoints));
        break;
    case SwarmPathCommand::CMD_START:
    case SwarmPathCommand::CMD_RESUME:
        mission_execution_state_.status = OperationState::MISSION_MOVING;
        RCLCPP_INFO(this->get_logger(), "Mission execution marked moving.");
        break;
    case SwarmPathCommand::CMD_PAUSE:
        mission_execution_state_.status = OperationState::MISSION_PAUSED;
        mission_execution_state_.current_speed_mps = 0.0f;
        RCLCPP_INFO(this->get_logger(), "Mission execution marked paused.");
        break;
    case SwarmPathCommand::CMD_STOP:
        mission_execution_state_ = MissionExecutionState{};
        RCLCPP_INFO(this->get_logger(), "Mission execution stopped and reset.");
        break;
    case SwarmPathCommand::CMD_COMPLETE:
        mission_execution_state_.status = OperationState::MISSION_REACHED;
        mission_execution_state_.current_speed_mps = 0.0f;
        RCLCPP_INFO(
            this->get_logger(),
            "Mission marked REACHED (path follower / operator signal).");
        break;
    default:
        RCLCPP_WARN(
            this->get_logger(),
            "Unknown swarm path command: %u",
            static_cast<unsigned int>(msg->command));
        break;
    }

    // --- 게이트 훅 (후속: 모드/상태별 정책. 다른 FSM 모드 로직과 함께 확정) ---
    // 초기 동작: 투명 전달. 추후 active_mode_id_ / mission_execution_state_ /
    // operation_state 에 따라 LOAD_PATH/START/RESUME 를 차단하거나 STOP 으로
    // 치환하는 정책을 이 지점에 삽입한다.
    // 원본 메시지를 그대로 재발행하여 path_json 등 페이로드를 보존한다
    // (mission_control 이 path_json 파싱에 의존).
    if (m_pub_mission_path_command_) {
        m_pub_mission_path_command_->publish(*msg);
    }
}

void CombatRobotOperationSystem::onSwarmControlCommand(const SwarmControlCommand::ConstSharedPtr msg)
{
    if (!msg) {
        return;
    }
    // 대형(formation) 게이트 훅 — 경로 게이트와 동일 패턴. 초기 동작은 투명 전달:
    // command_server → FSM → path_executor 로 SwarmControlCommand 를 그대로 재발행한다.
    // 추후 active_mode_id_/state 에 따라 대형 변경을 차단·치환하는 정책을 여기 삽입.
    if (m_pub_mission_control_command_swarm_) {
        m_pub_mission_control_command_swarm_->publish(*msg);
    }
}

void CombatRobotOperationSystem::onSwarmMissionState(const OperationState::ConstSharedPtr msg)
{
    if (!msg) {
        return;
    }
    // 차량 mission_control 이 보고하는 실측 nav 텔레메트리를 캐싱한다. FSM 의 state/
    // active_mode_id 권위는 그대로 두고(여기서 채택하지 않음), FSM 이 자체 생산하지
    // 않는 값(실 GPS/속도/waypoint 진행률/거리/nav 에러)만 보관해 on_timer 에서
    // /operation_state 에 통합한다.
    std::lock_guard<std::mutex> lock(mtx_mission_);
    vehicle_nav_telemetry_.valid = true;
    vehicle_nav_telemetry_.mission_status = msg->mission_status;
    vehicle_nav_telemetry_.gps_lat = msg->gps_lat;
    vehicle_nav_telemetry_.gps_lon = msg->gps_lon;
    vehicle_nav_telemetry_.gps_heading = msg->gps_heading;
    vehicle_nav_telemetry_.current_speed_mps = msg->current_speed_mps;
    vehicle_nav_telemetry_.current_waypoint_index = msg->current_waypoint_index;
    vehicle_nav_telemetry_.total_waypoints = msg->total_waypoints;
    vehicle_nav_telemetry_.progress_ratio = msg->progress_ratio;
    vehicle_nav_telemetry_.distance_to_next_wp_m = msg->distance_to_next_wp_m;
    vehicle_nav_telemetry_.distance_to_goal_m = msg->distance_to_goal_m;
    vehicle_nav_telemetry_.error_code = msg->error_code;
    last_swarm_mission_state_time_ = this->now();
}

void CombatRobotOperationSystem::onZoomLevel(const std_msgs::msg::Int32::ConstSharedPtr msg)
{
    current_zoom_level_.store(msg->data);
}

void CombatRobotOperationSystem::onTouchCommand(const TouchTargetPoint::ConstSharedPtr msg)
{
    std::lock_guard<std::mutex> lock(mtx_touch_cmd_);
    touch_command_point_ = *msg;
    new_touch_command_.store(true);
}

void CombatRobotOperationSystem::onLaserDistance(const std_msgs::msg::Float64::ConstSharedPtr msg)
{
    current_laser_distance_.store(msg->data);
}

bool CombatRobotOperationSystem::isCenter(point_t target_object)
{
    return in_offset_range(target_object.x, 0.5f, 0.02) &&
           in_offset_range(target_object.y, 0.5f, 0.02);
}

bool CombatRobotOperationSystem::isOutofMaximumAngle()
{
    return (current_actuator_vertical_angle_ < min_tilt_deg_) ||
           (current_actuator_vertical_angle_ > max_tilt_deg_) ||
           (current_actuator_horizontal_angle_ < min_pan_deg_) ||
           (current_actuator_horizontal_angle_ > max_pan_deg_);
}

bool CombatRobotOperationSystem::isInitPose()
{
    return (fabs(90 - current_actuator_vertical_angle_) < 10.0) &&
           (fabs(180 - current_actuator_horizontal_angle_) < 10.0);
}

bool CombatRobotOperationSystem::InitPanTiltModule()
{
    if (!check_pantilt_status_) {
        std::lock_guard<std::mutex> lock(mtx_actuator_);
        last_pantilt_state_update_time_ = this->now();
        prev_actuator_horizontal_angle_ = 0.0f;
        prev_actuator_vertical_angle_ = 0.0f;
        current_actuator_horizontal_angle_ = 0.0f;
        current_actuator_vertical_angle_ = 0.0f;
        is_pan_tilt_moving_ = false;
        prev_target_pan_deg_ = 0.0f;
        prev_target_tilt_deg_ = 0.0f;
        init_command_sent_ = true;
        RCLCPP_INFO_ONCE(
            this->get_logger(),
            "Pan/Tilt check disabled. Skipping pan/tilt initialization for test mode.");
        return true;
    }

    {
        float init_pan_angle = 0.0f;
        float init_tilt_angle = 0.0f;
        prev_target_pan_deg_ = init_pan_angle;
        prev_target_tilt_deg_ = init_tilt_angle;
        PublishDegControl(init_pan_angle, init_tilt_angle);
        init_command_sent_ = true;
        RCLCPP_INFO(get_logger(), "Sent command to move Pan/Tilt to initial pose.");
    }

    if (last_pantilt_state_update_time_.seconds() == 0) {
        RCLCPP_INFO_THROTTLE(
            this->get_logger(),
            *get_clock(),
            1000,
            "Waiting for first Pan/Tilt state update...");
        return false;
    }

    const float init_pan_angle = 0.0f;
    const float init_tilt_angle = 0.0f;
    const float tolerance = 1.0f;
    bool pan_reached = fabs(current_actuator_horizontal_angle_ - init_pan_angle) < tolerance;
    bool tilt_reached = fabs(current_actuator_vertical_angle_ - init_tilt_angle) < tolerance;

    if (pan_reached && tilt_reached) {
        RCLCPP_INFO_ONCE(this->get_logger(), "Pan/Tilt module initialized successfully.");
    }

    return pan_reached && tilt_reached;
}

void CombatRobotOperationSystem::changePanTiltState(uint8_t transition_id)
{
    if (!m_client_change_state_->service_is_ready()) {
        RCLCPP_WARN(get_logger(), "Pan/Tilt Lifecycle service not ready");
        return;
    }

    auto request = std::make_shared<lifecycle_msgs::srv::ChangeState::Request>();
    request->transition.id = transition_id;

    m_client_change_state_->async_send_request(
        request,
        [this, transition_id](
            rclcpp::Client<lifecycle_msgs::srv::ChangeState>::SharedFuture future) {
            try {
                auto result = future.get();
                if (result->success) {
                    RCLCPP_INFO(
                        get_logger(),
                        "Pan/Tilt Lifecycle Transition %d success",
                        transition_id);
                } else {
                    RCLCPP_WARN(
                        get_logger(),
                        "Pan/Tilt Lifecycle Transition %d failed",
                        transition_id);
                }
            } catch (const std::exception& e) {
                RCLCPP_ERROR(get_logger(), "Service call failed: %s", e.what());
            }
        });
}

void CombatRobotOperationSystem::resetMissionExecution()
{
    std::lock_guard<std::mutex> lock(mtx_mission_);
    mission_execution_state_ = MissionExecutionState{};
}

void CombatRobotOperationSystem::fillMissionExecutionStatus(OperationState& msg) const
{
    std::lock_guard<std::mutex> lock(mtx_mission_);
    msg.mission_status = mission_execution_state_.status;
    msg.current_waypoint_index = mission_execution_state_.current_waypoint_index;
    msg.total_waypoints = mission_execution_state_.total_waypoints;
    msg.current_speed_mps = mission_execution_state_.current_speed_mps;
}

}  // namespace combat_robot_system
