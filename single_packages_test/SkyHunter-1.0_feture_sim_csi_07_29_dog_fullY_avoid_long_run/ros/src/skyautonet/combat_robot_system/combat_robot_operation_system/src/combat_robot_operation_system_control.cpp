#include "combat_robot_operation_system.hpp"

namespace combat_robot_system {

void CombatRobotOperationSystem::PublishPanDegControl(float deg, uint8_t speed)
{
    PanTiltControlCommand msg;
    msg.control_mode = PanTiltControlCommand::CONTROL_HOR_POS;
    msg.horizontal_angle = deg;
    msg.vertical_angle = 0;
    msg.pan_speed = speed;
    msg.tilt_speed = 0;
    msg.pan_dir = (deg < current_actuator_horizontal_angle_) ? 1 : 2;
    msg.tilt_dir = 0;

    m_pub_pan_tilt_control_command->publish(msg);
}

void CombatRobotOperationSystem::PublishTiltDegControl(float deg, uint8_t speed)
{
    PanTiltControlCommand msg;
    msg.control_mode = PanTiltControlCommand::CONTROL_VER_POS;
    msg.horizontal_angle = 0;
    msg.vertical_angle = deg;
    msg.pan_speed = 0;
    msg.tilt_speed = speed;
    msg.pan_dir = 0;
    msg.tilt_dir = (deg < current_actuator_vertical_angle_) ? 1 : 2;

    m_pub_pan_tilt_control_command->publish(msg);
}

void CombatRobotOperationSystem::PublishControllerStop()
{
    PanTiltControlCommand msg;
    msg.control_mode = PanTiltControlCommand::CONTROL_BRAKE;
    msg.pan_speed = 0;
    msg.tilt_speed = 0;
    msg.pan_dir = 0;
    msg.tilt_dir = 0;

    m_pub_pan_tilt_control_command->publish(msg);
}

void CombatRobotOperationSystem::PublishDegControl(
    float pan_deg,
    float tilt_deg,
    uint8_t pan_speed,
    uint8_t tilt_speed)
{
    float tilt_diff = fabs(current_actuator_vertical_angle_ - tilt_deg);
    float pan_diff = fabs(current_actuator_horizontal_angle_ - pan_deg);

    if (tilt_diff < pan_diff) {
        if (pan_diff > 0.1f) {
            PublishPanDegControl(pan_deg, pan_speed);
        }
    } else {
        if (tilt_diff > 0.1f) {
            PublishTiltDegControl(tilt_deg, tilt_speed);
        }
    }
}

void CombatRobotOperationSystem::PublishDirControl(
    float pan_deg,
    float tilt_deg,
    uint8_t pan_speed,
    uint8_t tilt_speed)
{
    float curr_pan;
    float curr_tilt;
    {
        std::lock_guard<std::mutex> lock(mtx_actuator_);
        curr_pan = current_actuator_horizontal_angle_;
        curr_tilt = current_actuator_vertical_angle_;
    }

    RCLCPP_INFO(
        this->get_logger(),
        "Current Actuator Angles: pan = %.2f, tilt = %.2f",
        curr_pan,
        curr_tilt);
    RCLCPP_INFO(
        this->get_logger(),
        "Publishing Control: pan = %.2f, tilt = %.2f",
        pan_deg,
        tilt_deg);

    pan_deg = std::clamp(pan_deg, min_pan_deg_, max_pan_deg_);
    tilt_deg = std::clamp(tilt_deg, min_tilt_deg_, max_tilt_deg_);

    uint8_t pan_dir = curr_pan < pan_deg ? 1 : 2;
    uint8_t tilt_dir = curr_tilt < tilt_deg ? 1 : 2;

    if (in_offset_range(curr_pan, pan_deg, 1)) {
        pan_speed = 0;
        pan_dir = 0;
    }

    if (in_offset_range(curr_tilt, tilt_deg, 1)) {
        tilt_speed = 0;
        tilt_dir = 0;
    }

    PanTiltControlCommand msg;
    msg.control_mode = PanTiltControlCommand::CONTROL_DIR;
    msg.horizontal_angle = pan_deg;
    msg.vertical_angle = tilt_deg;
    msg.pan_speed = pan_speed;
    msg.tilt_speed = tilt_speed;
    msg.pan_dir = pan_dir;
    msg.tilt_dir = tilt_dir;

    m_pub_pan_tilt_control_command->publish(msg);
}

void CombatRobotOperationSystem::PublishCenterObject(
    const TargetObject& target,
    const std_msgs::msg::Header& header)
{
    CenterObject center_object_msg;
    center_object_msg.header = header;
    center_object_msg.class_id = target.class_id;
    center_object_msg.bounding_box = target.bounding_box;
    center_object_msg.target_x = target.x;
    center_object_msg.target_y = target.y;
    center_object_msg.laser_distance = static_cast<float>(current_laser_distance_.load());
    center_object_msg.zoom_level = static_cast<float>(current_zoom_level_.load());
    m_pub_center_object->publish(center_object_msg);
}

void CombatRobotOperationSystem::PublishDriveCommand(
    double linear_velocity,
    double angular_velocity)
{
    if (!m_pub_drive_command_) {
        return;
    }

    DriveCommand msg;
    msg.linear_velocity = static_cast<float>(linear_velocity);
    msg.angular_velocity = static_cast<float>(angular_velocity);
    m_pub_drive_command_->publish(msg);
}

std::pair<uint8_t, uint8_t> CombatRobotOperationSystem::CalculatePanTiltSpeed(
    float target_pan_deg,
    float target_tilt_deg,
    float curr_pan_deg,
    float curr_tilt_deg)
{
    const float min_speed = pan_tilt_min_speed_;
    const uint8_t max_safe_speed = 255;
    float pan_coeff_speed = (pan_speed_divider_ > 0.0f) ? range_deg_ / pan_speed_divider_ : range_deg_;
    float tilt_coeff_speed = (tilt_speed_divider_ > 0.0f) ? range_deg_ / tilt_speed_divider_ : range_deg_;

    float pan_diff = target_pan_deg - curr_pan_deg;
    float tilt_diff = target_tilt_deg - curr_tilt_deg;

    float raw_pan_speed = (fabs(pan_diff) + min_speed) * pan_coeff_speed;
    float raw_tilt_speed = (fabs(tilt_diff) + min_speed) * tilt_coeff_speed;

    uint8_t pan_speed = static_cast<uint8_t>(std::clamp(
        raw_pan_speed, static_cast<float>(min_speed), static_cast<float>(max_safe_speed)));
    uint8_t tilt_speed = static_cast<uint8_t>(std::clamp(
        raw_tilt_speed, static_cast<float>(min_speed), static_cast<float>(max_safe_speed)));

    return {pan_speed, tilt_speed};
}

bool CombatRobotOperationSystem::keepTarget(const TargetObject& target_object)
{
    float curr_pan_angle;
    float curr_tilt_angle;
    {
        std::lock_guard<std::mutex> lock(mtx_actuator_);
        curr_pan_angle = current_actuator_horizontal_angle_;
        curr_tilt_angle = current_actuator_vertical_angle_;
    }

    if (isOutofMaximumAngle()) {
        RCLCPP_INFO(this->get_logger(), "Pan/Tilt Maximum reached, stopping tracking.");
        PublishDirControl(0.0f, 0.0f, 0, 0);
        prev_target_pan_deg_ = 0.0f;
        prev_target_tilt_deg_ = 0.0f;
        return false;
    }

    if (isTargetLocked_.load()) {
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "Target is locked, keeping the target.");
        point_t target_point;
        target_point.x = target_object.x;
        target_point.y = target_object.y;

        // For human targets (class_id=0) shift the aim from bbox center to
        // `aim.human_height_ratio` of the way from bbox TOP to bottom.
        // Image y grows downward, so y_top = y_center - height/2 and
        // aim_y = y_top + ratio*height = y_center + (ratio - 0.5)*height.
        // ratio=0.3 lifts the aim ~20% of bbox height above center
        // (head/face for an upright person).
        if (target_object.class_id == 0 && target_object.height > 0.0f) {
            const float offset = (aim_human_height_ratio_ - 0.5f) *
                                  target_object.height;
            target_point.y += offset;
        }

        RCLCPP_INFO(this->get_logger(), "target x: %.2f, target y: %.2f", target_point.x, target_point.y);
        if (isCenter(target_point)) {
            RCLCPP_INFO(this->get_logger(), "Target is centered");
            PublishCenterObject(target_object, last_target_header_);
            handleFireControl();
        } else {
            RCLCPP_INFO(this->get_logger(), "Target is not centered, adjusting pan/tilt control.");
            point_t center;
            center.x = 0.5f;
            center.y = 0.5f;

            point_t final_target = target_point;
            double ff_pan_deg = 0.0;
            double ff_tilt_deg = 0.0;

            {
                std::lock_guard<std::mutex> lock(mtx_target_);
                if (m_enable_prediction_ && m_prev_target_time_ > 0) {
                    double time_diff = last_Target_time_ - m_prev_target_time_;
                    if (time_diff > 1e-9) {
                        if (target_object.track_id == m_prev_target_object_.track_id) {
                            double raw_velocity_x =
                                (target_object.x - m_prev_target_object_.x) / time_diff;
                            double raw_velocity_y =
                                (target_object.y - m_prev_target_object_.y) / time_diff;

                            raw_velocity_x = std::clamp(raw_velocity_x, -1.0, 1.0);
                            raw_velocity_y = std::clamp(raw_velocity_y, -1.0, 1.0);

                            m_velocity_x_ = m_velocity_lpf_alpha_ * raw_velocity_x +
                                            (1.0 - m_velocity_lpf_alpha_) * m_velocity_x_;
                            m_velocity_y_ = m_velocity_lpf_alpha_ * raw_velocity_y +
                                            (1.0 - m_velocity_lpf_alpha_) * m_velocity_y_;

                            final_target.x += m_velocity_x_ * m_pan_tilt_delay_;
                            final_target.y += m_velocity_y_ * m_pan_tilt_delay_;

                            ff_pan_deg = m_velocity_x_ * range_deg_ * m_ff_gain_;
                            ff_tilt_deg = m_velocity_y_ * range_deg_ * tilt_aspect_factor_ * m_ff_gain_;

                            RCLCPP_INFO(
                                this->get_logger(),
                                "[Prediction] vel=(%.3f, %.3f) ff_pan=%.2f ff_tilt=%.2f",
                                m_velocity_x_,
                                m_velocity_y_,
                                ff_pan_deg,
                                ff_tilt_deg);
                        } else {
                            m_velocity_x_ = 0.0;
                            m_velocity_y_ = 0.0;
                        }
                    }
                }
            }

            float target_diff_x = final_target.x - center.x;
            float target_diff_y = final_target.y - center.y;

            float pan_diff = target_diff_x * range_deg_;
            float tilt_diff = target_diff_y * range_deg_ * tilt_aspect_factor_;

            float target_pan_deg = curr_pan_angle + pan_diff + static_cast<float>(ff_pan_deg);
            float target_tilt_deg = curr_tilt_angle + tilt_diff + static_cast<float>(ff_tilt_deg);

            RCLCPP_INFO(
                this->get_logger(),
                "target_diff_x: %.2f, target_diff_y: %.2f",
                target_diff_x,
                target_diff_y);

            std::pair<uint8_t, uint8_t> speeds =
                CalculatePanTiltSpeed(target_pan_deg, target_tilt_deg, curr_pan_angle, curr_tilt_angle);

            prev_target_pan_deg_ = target_pan_deg;
            prev_target_tilt_deg_ = target_tilt_deg;

            PublishDirControl(target_pan_deg, target_tilt_deg, speeds.first, speeds.second);
        }
    } else {
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "Target is not locked, stopping tracking.");
        PublishControllerStop();
    }

    return false;
}

void CombatRobotOperationSystem::handleFireControl()
{
    if (run_mode_ == RUN_DEMO) {
        if (!demo_fire_started_) {
            demo_fire_started_ = true;
            demo_fire_start_time_ = this->now();
            RCLCPP_INFO(
                this->get_logger(),
                "Demo fire window started for track_id=%d",
                demo_active_track_id_);
        }

        PublishFireWeapon();
        return;
    }

    bool gun_trigger_perm = false;
    {
        std::lock_guard<std::mutex> lock(mtx_cmd_);
        gun_trigger_perm =
            mission_control_command_.attack_permission ==
            MissionControlCommand::ATTACK_PERMISSION_APPROVE;
    }

    // Engagement modes (PROTECT_GENERAL / PROTECT_DRONE / ASSAULT):
    // Tracking_statefunc owns the request-flag lifecycle (raises on new
    // track_id, clears on APPROVE/DENY). The command_server delivers
    // APPROVE/DENY exactly once and then resets to NONE, so we also keep firing
    // while the latched approved_track_id matches the current active one —
    // without touching request_attack_permission_.
    const bool is_engagement_run = isEngagementRunMode();
    if (is_engagement_run) {
        // Fire only when the live, centered target under the crosshair IS the
        // active, wanted-class engagement target. Guards against firing on a
        // wrong-class or wrong-id object that transiently holds the lock while
        // a stale/approved active id is latched (e.g. a person entering frame
        // during drone engagement, or a SORT track-id collision).
        const int32_t wanted_class_id = wantedClassForRunMode();
        int32_t live_track_id;
        int32_t live_class_id;
        {
            std::lock_guard<std::mutex> lock(mtx_target_);
            live_track_id = target_object_.track_id;
            live_class_id = target_object_.class_id;
        }
        const bool live_is_active_wanted =
            protect_active_track_id_ >= 0 &&
            live_track_id == protect_active_track_id_ &&
            live_class_id == wanted_class_id;
        const bool already_approved =
            live_is_active_wanted &&
            protect_approved_track_ids_.count(protect_active_track_id_) > 0;
        if (live_is_active_wanted && (gun_trigger_perm || already_approved)) {
            PublishFireWeapon();
        }
        return;
    }

    if (request_attack_permission_.load() && gun_trigger_perm) {
        PublishFireWeapon();
    } else {
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "Waiting for permission to fire.");
        request_attack_permission_.store(true);
    }
}

void CombatRobotOperationSystem::PublishFireWeapon()
{
    if (gun_status_.load(std::memory_order_relaxed) == 0) {
        std_msgs::msg::Int8 cmd;
        cmd.data = 1;
        pub_gun_cmd_->publish(cmd);
        RCLCPP_INFO(get_logger(), "Shoot: Target centered -> FIRE command sent");
    } else {
        RCLCPP_INFO(get_logger(), "Shoot: Gun is busy or error, skip fire");
    }
}

void CombatRobotOperationSystem::initScanning(
    const float default_pan_angle_,
    const float default_tilt_angle_)
{
    prev_target_pan_deg_ = default_pan_angle_;
    prev_target_tilt_deg_ = default_tilt_angle_;
    prev_scanning_action_ = SCAN_FIRST_ROW;
    isScanning_.store(true);
    last_scanning_move_start_time_ = this->now();

    float pan_diff = fabs(prev_target_pan_deg_ - current_actuator_horizontal_angle_);
    float tilt_diff = fabs(prev_target_tilt_deg_ - current_actuator_vertical_angle_);
    float max_diff = std::max(pan_diff, tilt_diff);
    const double divider = (scan_timeout_speed_divider_ > 0.0)
        ? scan_timeout_speed_divider_ : 1.0;
    scanning_timeout_duration_ = max_diff / divider + scan_base_timeout_sec_;

    PublishDegControl(prev_target_pan_deg_, prev_target_tilt_deg_);
}

void CombatRobotOperationSystem::Scanning(
    const float default_pan_angle_,
    const float default_tilt_angle_)
{
    if (isScanning_.load() == false) {
        return;
    }

    bool is_pan_reached = fabs(prev_target_pan_deg_ - current_actuator_horizontal_angle_) <= 1.0;
    bool is_tilt_reached = fabs(prev_target_tilt_deg_ - current_actuator_vertical_angle_) <= 1.0;

    if (!is_pan_reached || !is_tilt_reached) {
        waiting_for_stabilization_ = false;

        if ((this->now() - last_scanning_move_start_time_).seconds() > scanning_timeout_duration_) {
            RCLCPP_ERROR(
                this->get_logger(),
                "Scanning movement timeout (%.1fs exceeded). Triggering Pan/Tilt Error.",
                scanning_timeout_duration_);
            sensor_error_state_ = PANTILT_ERROR;
            return;
        }

        RCLCPP_INFO_THROTTLE(
            this->get_logger(),
            *get_clock(),
            1000,
            "Pan/Tilt is still moving: pan_err=%.2f, tilt_err=%.2f",
            fabs(prev_target_pan_deg_ - current_actuator_horizontal_angle_),
            fabs(prev_target_tilt_deg_ - current_actuator_vertical_angle_));
        PublishDegControl(prev_target_pan_deg_, prev_target_tilt_deg_);
        return;
    }

    if (!waiting_for_stabilization_) {
        waiting_for_stabilization_ = true;
        last_scanning_stable_time_ = this->now();
        PublishDegControl(prev_target_pan_deg_, prev_target_tilt_deg_);
        return;
    }

    if ((this->now() - last_scanning_stable_time_).seconds() < 1.0) {
        PublishDegControl(prev_target_pan_deg_, prev_target_tilt_deg_);
        return;
    }

    waiting_for_stabilization_ = false;
    const float scan_pan_range = scan_pan_range_deg_;
    const float scan_tilt_range = scan_tilt_range_deg_;
    const float scan_tilt_step = scan_tilt_step_deg_;

    const float scan_left_limit = default_pan_angle_ - scan_pan_range / 2.0f;
    const float scan_right_limit = default_pan_angle_ + scan_pan_range / 2.0f;
    const float scan_bottom_limit = default_tilt_angle_ + scan_tilt_range / 2.0f;
    const float scan_top_limit = default_tilt_angle_ - scan_tilt_range / 2.0f;

    float target_pan_deg = 0.0f;
    float target_tilt_deg = 0.0f;
    e_scanning_action_ scanning_action = prev_scanning_action_;

    bool at_start_point =
        (fabs(current_actuator_horizontal_angle_ - scan_left_limit) < 2.0f) &&
        (fabs(current_actuator_vertical_angle_ - scan_top_limit) < 2.0f);

    if (prev_scanning_action_ == SCAN_FIRST_ROW) {
        if (at_start_point) {
            scanning_action = SCAN_RIGHT;
        } else {
            scanning_action = SCAN_FIRST_ROW;
        }
    } else if (prev_scanning_action_ == SCAN_LEFT) {
        if (scan_tilt_range > 0.0f) {
            scanning_action = SCAN_NEXT_ROW;
        } else {
            scanning_action = SCAN_RIGHT;
        }
    } else if (prev_scanning_action_ == SCAN_RIGHT) {
        if (scan_tilt_range > 0.0f) {
            scanning_action = SCAN_NEXT_ROW;
        } else {
            scanning_action = SCAN_LEFT;
        }
    } else if (prev_scanning_action_ == SCAN_NEXT_ROW) {
        if (prev_target_pan_deg_ >= default_pan_angle_) {
            scanning_action = SCAN_LEFT;
        } else {
            scanning_action = SCAN_RIGHT;
        }
    } else {
        scanning_action = SCAN_FIRST_ROW;
    }

    if (scanning_action == SCAN_FIRST_ROW) {
        target_pan_deg = scan_left_limit;
        target_tilt_deg = scan_top_limit;
    } else if (scanning_action == SCAN_RIGHT) {
        target_pan_deg = scan_right_limit;
        target_tilt_deg = prev_target_tilt_deg_;
    } else if (scanning_action == SCAN_LEFT) {
        target_pan_deg = scan_left_limit;
        target_tilt_deg = prev_target_tilt_deg_;
    } else if (scanning_action == SCAN_NEXT_ROW) {
        target_pan_deg = prev_target_pan_deg_;
        target_tilt_deg = prev_target_tilt_deg_ + scan_tilt_step;

        if (target_tilt_deg > scan_bottom_limit) {
            target_pan_deg = scan_left_limit;
            target_tilt_deg = scan_top_limit;
            scanning_action = SCAN_FIRST_ROW;
        }
    }

    prev_target_pan_deg_ = target_pan_deg;
    prev_target_tilt_deg_ = target_tilt_deg;
    prev_scanning_action_ = scanning_action;
    last_scanning_move_start_time_ = this->now();

    float pan_diff = fabs(target_pan_deg - current_actuator_horizontal_angle_);
    float tilt_diff = fabs(target_tilt_deg - current_actuator_vertical_angle_);
    float max_diff = std::max(pan_diff, tilt_diff);
    const double divider = (scan_timeout_speed_divider_ > 0.0)
        ? scan_timeout_speed_divider_ : 1.0;
    scanning_timeout_duration_ = max_diff / divider + scan_base_timeout_sec_;

    const float min_pan_speed = 10.0f;
    const float min_tilt_speed = 10.0f;
    const float max_scanning_pan_speed = 30.0f;
    const float max_scanning_tilt_speed = 30.0f;
    const float max_zoom_level = 24.0f;
    const float max_zoom_ratio = 5.0f;
    const uint8_t max_safe_speed = 255;

    float inv_current_zoom_ratio =
        1.0f / ((max_zoom_ratio - 1.0f) * current_zoom_level_.load() / max_zoom_level + 1.0f);

    float raw_pan_speed = std::max(min_pan_speed, max_scanning_pan_speed * inv_current_zoom_ratio);
    float raw_tilt_speed =
        std::max(min_tilt_speed, max_scanning_tilt_speed * inv_current_zoom_ratio);

    uint8_t pan_speed =
        static_cast<uint8_t>(std::clamp(raw_pan_speed, 0.0f, static_cast<float>(max_safe_speed)));
    uint8_t tilt_speed =
        static_cast<uint8_t>(std::clamp(raw_tilt_speed, 0.0f, static_cast<float>(max_safe_speed)));

    PublishDegControl(target_pan_deg, target_tilt_deg, pan_speed, tilt_speed);
}

void CombatRobotOperationSystem::pantiltManualControl()
{
    float curr_pan;
    float curr_tilt;
    {
        std::lock_guard<std::mutex> lock(mtx_actuator_);
        curr_pan = current_actuator_horizontal_angle_;
        curr_tilt = current_actuator_vertical_angle_;
    }

    int max_pan_speed = 63;
    int max_tilt_speed = 63;

    int pan_speed;
    int tilt_speed;
    {
        std::lock_guard<std::mutex> lock(mtx_cmd_);
        pan_speed = std::clamp(
            static_cast<int>(mission_control_command_.pan_speed), -max_pan_speed, max_pan_speed);
        tilt_speed = std::clamp(
            static_cast<int>(mission_control_command_.tilt_speed), -max_tilt_speed, max_tilt_speed);
    }

    pan_speed = std::abs(pan_speed) < 10 ? 0 : pan_speed;
    tilt_speed = std::abs(tilt_speed) < 10 ? 0 : tilt_speed;

    float pan_angle = curr_pan + static_cast<float>(pan_speed) * 0.1f;
    float tilt_angle = curr_tilt - static_cast<float>(tilt_speed) * 0.1f;
    PublishDirControl(pan_angle, tilt_angle, std::abs(pan_speed), std::abs(tilt_speed));
}

}  // namespace combat_robot_system
