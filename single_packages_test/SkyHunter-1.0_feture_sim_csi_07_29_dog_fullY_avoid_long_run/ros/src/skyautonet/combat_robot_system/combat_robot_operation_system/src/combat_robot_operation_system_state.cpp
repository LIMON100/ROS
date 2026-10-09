#include "combat_robot_operation_system.hpp"

namespace combat_robot_system {

bool CombatRobotOperationSystem::Init_statefunc()
{
    RCLCPP_INFO_ONCE(this->get_logger(), "Combat Robot Operation System Init");

    if (isInitialized()) {
        RCLCPP_WARN(this->get_logger(), "Combat Robot Operation System is already initialized.");
        return true;
    }

    bool pan_tilt_initialized = InitPanTiltModule();
    return pan_tilt_initialized;
}

void CombatRobotOperationSystem::resetDemoSequence(bool stop_drive)
{
    if (stop_drive) {
        PublishDriveCommand(0.0, 0.0);
    }

    demo_phase_ = DEMO_PHASE_IDLE;
    demo_initialized_ = false;
    demo_completed_ = false;
    demo_fire_started_ = false;
    demo_active_track_id_ = -1;
    demo_engage_wait_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
    demo_total_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
    demo_reported_active_mode_id_ = OperationState::ACTIVE_MODE_RECON;
    demo_completed_track_ids_.clear();
    demo_target_queue_.clear();
    isScanning_.store(false);
    attack_mode_init_.store(false);
    request_attack_permission_.store(false);

    std::lock_guard<std::mutex> lock(mtx_mission_);
    mission_execution_state_.status = OperationState::MISSION_NONE;
    mission_execution_state_.current_waypoint_index = 0;
    mission_execution_state_.total_waypoints = 0;
    mission_execution_state_.current_speed_mps = 0.0f;
}

bool CombatRobotOperationSystem::shouldRunDemoForCommand(uint8_t command_id) const
{
    if (!demo_deployment_enabled_) {
        return false;
    }

    switch (command_id) {
    case MissionControlCommand::RECON:
    case MissionControlCommand::PROTECT_GENERAL:
    case MissionControlCommand::PROTECT_DRONE:
    case MissionControlCommand::DEBUG_ATTACK:
    case MissionControlCommand::DEBUG_TRACKING:
    case MissionControlCommand::ASSAULT:
        return true;
    default:
        return false;
    }
}

uint8_t CombatRobotOperationSystem::activeModeIdForCommand(uint8_t command_id) const
{
    switch (command_id) {
    case MissionControlCommand::PROTECT_GENERAL:
        return OperationState::ACTIVE_MODE_PROTECT_GENERAL;
    case MissionControlCommand::PROTECT_DRONE:
        return OperationState::ACTIVE_MODE_PROTECT_DRONE;
    case MissionControlCommand::ASSAULT:
    case MissionControlCommand::DEBUG_ATTACK:
    case MissionControlCommand::DEBUG_TRACKING:
        return OperationState::ACTIVE_MODE_ASSAULT;
    case MissionControlCommand::RECON:
    default:
        return OperationState::ACTIVE_MODE_RECON;
    }
}

bool CombatRobotOperationSystem::isDemoTrackCompleted(int32_t track_id) const
{
    return std::find(
               demo_completed_track_ids_.begin(),
               demo_completed_track_ids_.end(),
               track_id) != demo_completed_track_ids_.end();
}

void CombatRobotOperationSystem::Idle_statefunc()
{
    isScanning_.store(false);
    attack_mode_init_.store(false);
    request_attack_permission_.store(false);

    // IDLE = "everything off" — explicitly brake pan/tilt and stop fire each
    // tick (idempotent if already stopped). Don't auto-home: any motion in
    // IDLE is undesired; operator can request a separate move if needed.
    PublishDriveCommand(0.0, 0.0);
    PublishControllerStop();
    std_msgs::msg::Int8 stop_cmd;
    stop_cmd.data = 0;
    pub_gun_cmd_->publish(stop_cmd);
}

void CombatRobotOperationSystem::Attacking_statefunc()
{
    RCLCPP_INFO(get_logger(), "Combat Robot Operation System Attacking State");
    if (new_pan_tilt_command_.load()) {
        pantiltManualControl();

        attack_mode_init_.store(false);
        prev_target_pan_deg_ = current_actuator_horizontal_angle_;
        prev_target_tilt_deg_ = current_actuator_vertical_angle_;

        RCLCPP_INFO(
            get_logger(),
            "New pan/tilt command received: pan speed = %d, tilt speed = %d",
            mission_control_command_.pan_speed,
            mission_control_command_.tilt_speed);
        RCLCPP_INFO(
            get_logger(),
            "Moving to the new pan/tilt position: h_angle = %.2f, v_angle= %.2f",
            current_actuator_horizontal_angle_,
            current_actuator_vertical_angle_);
    } else if (new_touch_command_.load()) {
        const point_t center_point{0.5f, 0.5f};
        const point_t target_spot{touch_command_point_.touch_x, touch_command_point_.touch_y};

        if (std::abs(h_trans_) < 1e-6 || std::abs(v_trans_) < 1e-6) {
            RCLCPP_ERROR(
                get_logger(),
                "Invalid camera parameters: h_trans or v_trans is near zero. Skipping touch command.");
            new_touch_command_.store(false);
            return;
        }

        float curr_pan;
        float curr_tilt;
        {
            std::lock_guard<std::mutex> lock(mtx_actuator_);
            curr_pan = current_actuator_horizontal_angle_;
            curr_tilt = current_actuator_vertical_angle_;
        }

        float h_angle =
            atan((target_spot.x - center_point.x) / h_trans_) * (180.0f / pi) + curr_pan;
        float v_angle =
            atan((target_spot.y - center_point.y) / v_trans_) * (180.0f / pi) + curr_tilt;

        PublishDegControl(h_angle, v_angle);
        new_touch_command_.store(false);
        attack_mode_init_.store(false);

        prev_target_pan_deg_ = h_angle;
        prev_target_tilt_deg_ = v_angle;

        {
            std::lock_guard<std::mutex> lock(mtx_target_);
            target_object_.x = target_spot.x;
            target_object_.y = target_spot.y;
        }
        RCLCPP_INFO(
            get_logger(),
            "New target point received from touch command: target x = %.2f, target y = %.2f",
            touch_command_point_.touch_x,
            touch_command_point_.touch_y);
        RCLCPP_INFO(
            get_logger(),
            "Moving to the new target point: h_angle = %.2f, v_angle= %.2f",
            h_angle,
            v_angle);
    } else if (!attack_mode_init_.load() && !new_touch_command_.load()) {
        if (in_offset_range(current_actuator_horizontal_angle_, prev_target_pan_deg_, 0.1) &&
            in_offset_range(current_actuator_vertical_angle_, prev_target_tilt_deg_, 0.1)) {
            attack_mode_init_.store(true);
        } else {
            RCLCPP_INFO(
                this->get_logger(),
                "Pan/Tilt is still moving to the previous target position: pan = %.2f, tilt = %.2f",
                prev_target_pan_deg_,
                prev_target_tilt_deg_);
            PublishDegControl(prev_target_pan_deg_, prev_target_tilt_deg_);
        }
    } else if (attack_mode_init_.load()) {
        TargetObject target_copy;
        {
            std::lock_guard<std::mutex> lock(mtx_target_);
            target_copy = target_object_;
        }
        keepTarget(target_copy);
    } else {
        RCLCPP_WARN(
            get_logger(),
            "Attacking mode is not initialized, waiting for mission control command.");
    }
}

void CombatRobotOperationSystem::Assault_statefunc()
{
    // Hybrid ASSAULT scan/engage: identical scan + target-queue mechanics to
    // PROTECT_GENERAL (horizontal scan, engage persons class 0). Kept as its
    // own state so mode reporting stays ACTIVE_MODE_ASSAULT; the downstream
    // engagement flow (TRACKING -> approval -> fire) is shared via
    // isEngagementRunMode().
    protectSurveillanceTick(scan_general_tilt_deg_, 0);
}

void CombatRobotOperationSystem::Move_statefunc()
{
    if (run_mode_ == RUN_DEMO) {
        Demo_statefunc();
        return;
    }

    RCLCPP_INFO(this->get_logger(), "Combat Robot Operation System Recon/Move State");
    pantiltManualControl();
}

void CombatRobotOperationSystem::Demo_statefunc()
{
    const double drive_speed_mps = std::max(std::abs(demo_drive_speed_mps_), 0.05);
    const double forward_duration_sec = std::abs(demo_forward_distance_m_) / drive_speed_mps;
    const double reverse_duration_sec = std::abs(demo_reverse_distance_m_) / drive_speed_mps;

    auto update_demo_mission = [this](uint8_t status, float current_speed_mps) {
        std::lock_guard<std::mutex> lock(mtx_mission_);
        mission_execution_state_.status = status;
        mission_execution_state_.current_waypoint_index =
            static_cast<uint16_t>(demo_completed_track_ids_.size());
        mission_execution_state_.total_waypoints =
            static_cast<uint16_t>(std::max(1, demo_target_count_));
        mission_execution_state_.current_speed_mps = current_speed_mps;
    };

    if (!demo_initialized_) {
        demo_initialized_ = true;
        demo_completed_ = false;
        demo_fire_started_ = false;
        demo_active_track_id_ = -1;
        demo_completed_track_ids_.clear();
        demo_target_queue_.clear();
        demo_phase_ = DEMO_PHASE_FORWARD;
        demo_phase_start_time_ = this->now();
        demo_total_start_time_ = this->now();
        isScanning_.store(false);
        request_attack_permission_.store(false);
        attack_mode_init_.store(false);
        PublishControllerStop();
        PublishDriveCommand(0.0, 0.0);
        update_demo_mission(OperationState::MISSION_MOVING, static_cast<float>(drive_speed_mps));
        RCLCPP_INFO(
            this->get_logger(),
            "Demo sequence started: forward=%.2fm reverse=%.2fm targets=%d fire=%.2fs(single)/%.2fs(multi)",
            demo_forward_distance_m_,
            demo_reverse_distance_m_,
            demo_target_count_,
            demo_fire_duration_sec_,
            demo_fire_duration_multi_sec_);
    }

    switch (demo_phase_) {
    case DEMO_PHASE_FORWARD:
        if ((this->now() - demo_phase_start_time_).seconds() < forward_duration_sec) {
            PublishDriveCommand(drive_speed_mps, 0.0);
            update_demo_mission(
                OperationState::MISSION_MOVING,
                static_cast<float>(drive_speed_mps));
            return;
        }

        PublishDriveCommand(0.0, 0.0);
        demo_phase_ = DEMO_PHASE_SCAN;
        demo_phase_start_time_ = this->now();
        isScanning_.store(false);
        update_demo_mission(OperationState::MISSION_SURVEILLING, 0.0f);
        RCLCPP_INFO(this->get_logger(), "Demo forward motion completed. Starting front scan.");
        return;

    case DEMO_PHASE_SCAN:
    {
        PublishDriveCommand(0.0, 0.0);
        update_demo_mission(OperationState::MISSION_SURVEILLING, 0.0f);

        TargetObject target_copy;
        {
            std::lock_guard<std::mutex> lock(mtx_target_);
            target_copy = target_object_;
        }

        if (isTargetLocked_.load() &&
            target_copy.track_id >= 0 &&
            std::find(
                demo_target_queue_.begin(),
                demo_target_queue_.end(),
                target_copy.track_id) == demo_target_queue_.end()) {
            demo_target_queue_.push_back(target_copy.track_id);
            RCLCPP_INFO(
                this->get_logger(),
                "Demo queued target track_id=%d (queue size=%zu/%d)",
                target_copy.track_id,
                demo_target_queue_.size(),
                demo_target_count_);
        }

        const int queue_size = static_cast<int>(demo_target_queue_.size());
        const bool quota_met = queue_size >= demo_target_count_;
        const double scan_elapsed =
            (this->now() - demo_phase_start_time_).seconds();
        const bool window_elapsed = scan_elapsed >= demo_scan_duration_sec_;

        if (quota_met || window_elapsed) {
            isScanning_.store(false);
            PublishControllerStop();
            if (demo_target_queue_.empty()) {
                const double total_elapsed =
                    (this->now() - demo_total_start_time_).seconds();
                if (demo_total_duration_sec_ > 0.0 &&
                    total_elapsed < demo_total_duration_sec_) {
                    // Loop mode: stay in SCAN and try again instead of
                    // bailing to reverse on an empty queue.
                    demo_phase_start_time_ = this->now();
                    RCLCPP_INFO(
                        this->get_logger(),
                        "Demo scan window elapsed with no targets. Retrying scan (%.1f/%.1fs elapsed).",
                        total_elapsed,
                        demo_total_duration_sec_);
                    return;
                }
                demo_phase_ = DEMO_PHASE_REVERSE;
                demo_phase_start_time_ = this->now();
                update_demo_mission(
                    OperationState::MISSION_MOVING,
                    static_cast<float>(drive_speed_mps));
                RCLCPP_INFO(
                    this->get_logger(),
                    "Demo scan window elapsed with no targets. Starting reverse motion.");
                return;
            }
            demo_phase_ = DEMO_PHASE_ENGAGE;
            demo_phase_start_time_ = this->now();
            demo_active_track_id_ = -1;
            demo_fire_started_ = false;
            demo_active_fire_duration_sec_ = (queue_size >= 2)
                ? demo_fire_duration_multi_sec_
                : demo_fire_duration_sec_;
            request_attack_permission_.store(false);
            RCLCPP_INFO(
                this->get_logger(),
                "Demo scan complete. Engaging %d queued target(s) at %.2fs each.",
                queue_size,
                demo_active_fire_duration_sec_);
            return;
        }

        if (!isScanning_.load()) {
            initScanning(demo_scan_default_pan_deg_, demo_scan_default_tilt_deg_);
        } else {
            Scanning(demo_scan_default_pan_deg_, demo_scan_default_tilt_deg_);
        }
        return;
    }

    case DEMO_PHASE_ENGAGE:
    {
        PublishDriveCommand(0.0, 0.0);
        update_demo_mission(OperationState::MISSION_SURVEILLING, 0.0f);

        if (demo_active_track_id_ < 0) {
            if (demo_target_queue_.empty()) {
                const double total_elapsed =
                    (this->now() - demo_total_start_time_).seconds();
                if (demo_total_duration_sec_ > 0.0 &&
                    total_elapsed < demo_total_duration_sec_) {
                    // Wall-demo loop: scan→engage repeatedly until the
                    // total budget elapses, instead of reversing out.
                    demo_phase_ = DEMO_PHASE_SCAN;
                    demo_phase_start_time_ = this->now();
                    demo_completed_track_ids_.clear();
                    isScanning_.store(false);
                    request_attack_permission_.store(false);
                    PublishControllerStop();
                    RCLCPP_INFO(
                        this->get_logger(),
                        "Demo engagement queue drained. Looping back to scan (%.1f/%.1fs elapsed).",
                        total_elapsed,
                        demo_total_duration_sec_);
                    return;
                }
                demo_phase_ = DEMO_PHASE_REVERSE;
                demo_phase_start_time_ = this->now();
                request_attack_permission_.store(false);
                PublishControllerStop();
                update_demo_mission(
                    OperationState::MISSION_MOVING,
                    static_cast<float>(drive_speed_mps));
                RCLCPP_INFO(
                    this->get_logger(),
                    "Demo engagement queue drained. Starting reverse motion.");
                return;
            }
            demo_active_track_id_ = demo_target_queue_.front();
            demo_target_queue_.pop_front();
            demo_fire_started_ = false;
            demo_engage_wait_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            request_attack_permission_.store(false);
            RCLCPP_INFO(
                this->get_logger(),
                "Demo dequeued target track_id=%d (%zu remaining).",
                demo_active_track_id_,
                demo_target_queue_.size());
        }

        if (demo_fire_started_ &&
            (this->now() - demo_fire_start_time_).seconds() >= demo_active_fire_duration_sec_) {
            demo_completed_track_ids_.push_back(demo_active_track_id_);
            RCLCPP_INFO(
                this->get_logger(),
                "Demo engagement complete for track_id=%d (fired %.2fs).",
                demo_active_track_id_,
                demo_active_fire_duration_sec_);
            demo_active_track_id_ = -1;
            demo_fire_started_ = false;
            demo_engage_wait_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            request_attack_permission_.store(false);
            PublishControllerStop();
            return;
        }

        TargetObject target_copy;
        {
            std::lock_guard<std::mutex> lock(mtx_target_);
            target_copy = target_object_;
        }

        if (isTargetLocked_.load()) {
            // SORT often re-assigns track_ids after a brief track loss, so
            // require_exact_match would strand the demo waiting for an id
            // that never returns. Adopt whatever is currently locked.
            if (target_copy.track_id != demo_active_track_id_) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "Demo active target switched track_id=%d -> %d.",
                    demo_active_track_id_,
                    target_copy.track_id);
                demo_active_track_id_ = target_copy.track_id;
                demo_fire_started_ = false;
            }
            demo_engage_wait_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            keepTarget(target_copy);
        } else {
            PublishControllerStop();

            const rclcpp::Time now_t = this->now();
            if (demo_engage_wait_start_time_.nanoseconds() == 0) {
                demo_engage_wait_start_time_ = now_t;
            }
            const double waited =
                (now_t - demo_engage_wait_start_time_).seconds();
            if (waited >= demo_engage_target_timeout_sec_) {
                RCLCPP_WARN(
                    this->get_logger(),
                    "Demo target track_id=%d not re-acquired within %.2fs; abandoning and moving on.",
                    demo_active_track_id_,
                    demo_engage_target_timeout_sec_);
                demo_completed_track_ids_.push_back(demo_active_track_id_);
                demo_active_track_id_ = -1;
                demo_fire_started_ = false;
                demo_engage_wait_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
                request_attack_permission_.store(false);
            } else {
                RCLCPP_INFO_THROTTLE(
                    this->get_logger(),
                    *get_clock(),
                    500,
                    "Demo no locked target for active track_id=%d (waiting %.1f/%.1fs).",
                    demo_active_track_id_,
                    waited,
                    demo_engage_target_timeout_sec_);
            }
        }

        return;
    }

    case DEMO_PHASE_REVERSE:
        if ((this->now() - demo_phase_start_time_).seconds() < reverse_duration_sec) {
            PublishDriveCommand(-drive_speed_mps, 0.0);
            update_demo_mission(
                OperationState::MISSION_MOVING,
                static_cast<float>(drive_speed_mps));
            return;
        }

        PublishDriveCommand(0.0, 0.0);
        demo_phase_ = DEMO_PHASE_COMPLETE;
        demo_phase_start_time_ = this->now();
        demo_completed_ = true;
        update_demo_mission(OperationState::MISSION_NONE, 0.0f);
        RCLCPP_INFO(this->get_logger(), "Demo reverse motion completed. Returning to IDLE.");
        return;

    case DEMO_PHASE_COMPLETE:
        PublishDriveCommand(0.0, 0.0);
        update_demo_mission(OperationState::MISSION_NONE, 0.0f);
        demo_completed_ = true;
        return;

    case DEMO_PHASE_IDLE:
    default:
        PublishDriveCommand(0.0, 0.0);
        update_demo_mission(OperationState::MISSION_NONE, 0.0f);
        return;
    }
}

void CombatRobotOperationSystem::protectSurveillanceTick(
    float t_default_tilt_deg,
    int32_t t_wanted_class_id)
{
    const float default_pan_angle_ = 0.0f;

    if (isScanning_.load() == false) {
        initScanning(default_pan_angle_, t_default_tilt_deg);
    } else {
        Scanning(default_pan_angle_, t_default_tilt_deg);
    }

    // Accumulate newly-locked track_ids of the wanted class into the engage
    // queue while scanning. The class gate is what makes PROTECT_GENERAL engage
    // persons (class 0) and PROTECT_DRONE engage drones (class 1).
    if (isTargetLocked_.load() &&
        static_cast<int>(protect_target_queue_.size()) < protect_target_count_) {
        int32_t track_id;
        int32_t class_id;
        {
            std::lock_guard<std::mutex> lock(mtx_target_);
            track_id = target_object_.track_id;
            class_id = target_object_.class_id;
        }
        if (class_id == t_wanted_class_id &&
            track_id >= 0 &&
            track_id != protect_active_track_id_ &&
            std::find(
                protect_target_queue_.begin(),
                protect_target_queue_.end(),
                track_id) == protect_target_queue_.end() &&
            protect_approved_track_ids_.count(track_id) == 0 &&
            protect_denied_track_ids_.count(track_id) == 0) {
            protect_target_queue_.push_back(track_id);
            RCLCPP_INFO(
                this->get_logger(),
                "Protect queued target track_id=%d class=%d (queue size=%zu/%d)",
                track_id,
                class_id,
                protect_target_queue_.size(),
                protect_target_count_);
        }
    }
}

bool CombatRobotOperationSystem::wantedClassTargetLocked()
{
    if (!isTargetLocked_.load()) {
        return false;
    }
    std::lock_guard<std::mutex> lock(mtx_target_);
    return target_object_.class_id == wantedClassForRunMode();
}

void CombatRobotOperationSystem::Surveillance_statefunc()
{
    // PROTECT_GENERAL: horizontal scan, engage persons (class 0).
    protectSurveillanceTick(scan_general_tilt_deg_, 0);
}

void CombatRobotOperationSystem::SurveillanceDrone_statefunc()
{
    // PROTECT_DRONE: upward scan, engage drones (class 1). Identical to
    // PROTECT_GENERAL otherwise (shared queue / tracking / grace path).
    protectSurveillanceTick(scan_drone_tilt_deg_, 1);
}

void CombatRobotOperationSystem::Tracking_statefunc()
{
    TargetObject target_copy;
    {
        std::lock_guard<std::mutex> lock(mtx_target_);
        target_copy = target_object_;
    }

    const bool is_engagement_run = isEngagementRunMode();

    if (is_engagement_run) {
        // Only engage the class this mode targets: persons (0) in
        // PROTECT_GENERAL and ASSAULT, drones (1) in PROTECT_DRONE. The queue is
        // already class-filtered at accumulation time; this gates the live-lock
        // adopt paths so a wrong-class lock is never adopted.
        const int32_t wanted_class_id = wantedClassForRunMode();
        if (protect_active_track_id_ < 0) {
            if (!protect_target_queue_.empty()) {
                protect_active_track_id_ = protect_target_queue_.front();
                protect_target_queue_.pop_front();
                RCLCPP_INFO(
                    this->get_logger(),
                    "Protect dequeued target track_id=%d (%zu remaining).",
                    protect_active_track_id_,
                    protect_target_queue_.size());
            } else if (isTargetLocked_.load() && target_copy.track_id >= 0 &&
                       target_copy.class_id == wanted_class_id &&
                       protect_denied_track_ids_.count(target_copy.track_id) == 0) {
                // No queued ids but a target of the wanted class is locked —
                // adopt it. Skip ids the operator already DENIED this
                // session. A previously-APPROVED id only auto-fires on
                // re-adopt while its approval is still live; approvals are
                // expired when their target is lost beyond grace (see the
                // TRACKING lock-lost path), so a track_id recycled onto a
                // new target falls back to a fresh permission request.
                protect_active_track_id_ = target_copy.track_id;
            }
        } else if (isTargetLocked_.load() && target_copy.track_id >= 0 &&
                   target_copy.class_id == wanted_class_id &&
                   target_copy.track_id != protect_active_track_id_ &&
                   protect_denied_track_ids_.count(target_copy.track_id) == 0) {
            // SORT reassigned (or another target took the lock). Adopt the
            // new id so the FSM either reuses an existing approval or
            // raises a fresh request next tick. Denied ids are never
            // adopted; previously-approved ids are.
            RCLCPP_INFO(
                this->get_logger(),
                "Protect active track_id changed %d -> %d",
                protect_active_track_id_,
                target_copy.track_id);
            protect_active_track_id_ = target_copy.track_id;
        }

        // Permission semantics:
        //   APPROVE (1) — aim + fire (handleFireControl handles the gate)
        //   NONE    (0) — aim only, no fire; wait for operator decision
        //   DENY    (2) — explicitly skip this target; remove from queue
        uint8_t perm;
        {
            std::lock_guard<std::mutex> lock(mtx_cmd_);
            perm = mission_control_command_.attack_permission;
        }
        // Raise request only for an UNDECIDED active track_id:
        // not in approved_set and not in denied_set.
        if (protect_active_track_id_ >= 0 &&
            protect_approved_track_ids_.count(protect_active_track_id_) == 0 &&
            protect_denied_track_ids_.count(protect_active_track_id_) == 0) {
            request_attack_permission_.store(true);
        }
        if (perm == MissionControlCommand::ATTACK_PERMISSION_APPROVE &&
            protect_active_track_id_ >= 0) {
            // Insert into approved_set and consume the APPROVE locally so
            // the sticky latch from command_server's one-shot doesn't
            // auto-approve the NEXT adopted track_id.
            if (protect_approved_track_ids_.insert(
                    protect_active_track_id_).second) {
                request_attack_permission_.store(false);
                RCLCPP_INFO(
                    this->get_logger(),
                    "Protect target track_id=%d APPROVED — engagement firing",
                    protect_active_track_id_);
            }
            std::lock_guard<std::mutex> lock(mtx_cmd_);
            mission_control_command_.attack_permission =
                MissionControlCommand::ATTACK_PERMISSION_NONE;
        }
        if (perm == MissionControlCommand::ATTACK_PERMISSION_DENY &&
            protect_active_track_id_ >= 0) {
            RCLCPP_INFO(
                this->get_logger(),
                "Protect target track_id=%d explicitly DENIED",
                protect_active_track_id_);
            protect_denied_track_ids_.insert(protect_active_track_id_);
            protect_active_track_id_ = -1;
            request_attack_permission_.store(false);
            {
                std::lock_guard<std::mutex> lock(mtx_cmd_);
                mission_control_command_.attack_permission =
                    MissionControlCommand::ATTACK_PERMISSION_NONE;
            }
            std_msgs::msg::Int8 stop_cmd;
            stop_cmd.data = 0;
            pub_gun_cmd_->publish(stop_cmd);
            PublishControllerStop();
            return;
        }

        // No active target this tick — make sure the tablet doesn't see a
        // stale "permission requested" prompt with nothing to engage.
        if (protect_active_track_id_ < 0) {
            request_attack_permission_.store(false);
            return;
        }
    }

    keepTarget(target_copy);
}

void CombatRobotOperationSystem::EmergencyStop_statefunc()
{
    RCLCPP_WARN_ONCE(this->get_logger(), "EMERGENCY STOP ACTIVATED");

    PublishDriveCommand(0.0, 0.0);
    PublishControllerStop();

    std_msgs::msg::Int8 cmd;
    cmd.data = 0;
    pub_gun_cmd_->publish(cmd);

    isScanning_.store(false);
    attack_mode_init_.store(false);
    request_attack_permission_.store(false);
    isTargetLocked_.store(false);
}

void CombatRobotOperationSystem::Error_statefunc()
{
    RCLCPP_ERROR_THROTTLE(
        this->get_logger(),
        *get_clock(),
        2000,
        "Combat Robot Operation System Error State: %d",
        sensor_error_state_);

    PublishDriveCommand(0.0, 0.0);

    if (sensor_error_state_ == PANTILT_ERROR) {
        rclcpp::Time current_time = this->now();
        if ((current_time - last_error_recovery_attempt_time_).seconds() > 2.0) {
            last_error_recovery_attempt_time_ = current_time;

            RCLCPP_WARN(
                this->get_logger(),
                "Executing Pan/Tilt Recovery Step: %d",
                recovery_step_);

            switch (recovery_step_) {
            case 0:
                changePanTiltState(lifecycle_msgs::msg::Transition::TRANSITION_DEACTIVATE);
                recovery_step_++;
                break;
            case 1:
                changePanTiltState(lifecycle_msgs::msg::Transition::TRANSITION_CLEANUP);
                recovery_step_++;
                break;
            case 2:
                changePanTiltState(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);
                recovery_step_++;
                break;
            case 3:
                changePanTiltState(lifecycle_msgs::msg::Transition::TRANSITION_ACTIVATE);
                recovery_step_++;
                break;
            case 4:
                RCLCPP_INFO(this->get_logger(), "Waiting for Pan/Tilt to come back online...");
                if ((current_time - last_error_recovery_attempt_time_).seconds() > 10.0) {
                    recovery_step_ = 0;
                }
                break;
            default:
                recovery_step_ = 0;
                break;
            }
        }
    } else {
        recovery_step_ = 0;
    }
}

bool CombatRobotOperationSystem::transitState(e_operation_state next_state)
{
    bool isStateTransition = false;

    switch (next_state) {
    case INIT_STATE:
        if (state_ == INIT_STATE) {
            state_ = INIT_STATE;
            isStateTransition = true;
        }
        break;
    case IDLE:
        if (state_ == INIT_STATE || state_ == MOVE_STATE || state_ == SURVEILLANCE_STATE ||
            state_ == DRONE_SURVEILLANCE_STATE || state_ == ATTACKING_STATE ||
            state_ == ASSAULT_STATE || state_ == RTH_STATE ||
            state_ == TRACKING_STATE || state_ == EMERGENCY_STOP_STATE) {
            state_ = IDLE;
            isStateTransition = true;
            // Clear protect engagement state on every entry to IDLE so the
            // next PROTECT session starts fresh, and explicitly stop fire.
            protect_target_queue_.clear();
            protect_approved_track_ids_.clear();
            protect_denied_track_ids_.clear();
            protect_active_track_id_ = -1;
            std_msgs::msg::Int8 stop_cmd;
            stop_cmd.data = 0;
            pub_gun_cmd_->publish(stop_cmd);
        } else if (state_ == ERROR_STATE) {
            state_ = IDLE;
            isStateTransition = true;
            protect_target_queue_.clear();
            protect_approved_track_ids_.clear();
            protect_denied_track_ids_.clear();
            protect_active_track_id_ = -1;
        }
        break;
    case MOVE_STATE:
        if (state_ == IDLE) {
            state_ = MOVE_STATE;
            isStateTransition = true;
        }
        break;
    case ATTACKING_STATE:
        if (state_ == IDLE) {
            state_ = ATTACKING_STATE;
            isStateTransition = true;
        }
        break;
    case ASSAULT_STATE:
        // ASSAULT is a hybrid engagement mode (mirror of the PROTECT scan
        // states): entered directly from IDLE when already at the objective,
        // from MOVE_STATE when the formation drive finishes, and re-entered
        // from TRACKING_STATE after an engagement's lock is lost so the scan
        // resumes. protect_scan_start_time_ gates the scan-accumulation window.
        if (state_ == IDLE) {
            state_ = ASSAULT_STATE;
            isStateTransition = true;
            protect_scan_start_time_ = this->now();
        } else if (state_ == TRACKING_STATE) {
            state_ = ASSAULT_STATE;
            isStateTransition = true;
            isScanning_.store(false);
            request_attack_permission_.store(false);
            protect_scan_start_time_ = this->now();
        } else if (state_ == MOVE_STATE) {
            // Hybrid ASSAULT: drive-to-objective phase finished, enter scan/engage.
            state_ = ASSAULT_STATE;
            isStateTransition = true;
            isScanning_.store(false);
            request_attack_permission_.store(false);
            protect_scan_start_time_ = this->now();
        }
        break;
    case RTH_STATE:
        if (state_ == IDLE || state_ == MOVE_STATE || state_ == ASSAULT_STATE) {
            state_ = RTH_STATE;
            isStateTransition = true;
        }
        break;
    case SURVEILLANCE_STATE:
        if (state_ == IDLE) {
            state_ = SURVEILLANCE_STATE;
            isStateTransition = true;
            protect_scan_start_time_ = this->now();
        } else if (state_ == TRACKING_STATE) {
            state_ = SURVEILLANCE_STATE;
            isStateTransition = true;
            isScanning_.store(false);
            request_attack_permission_.store(false);
            protect_scan_start_time_ = this->now();
        } else if (state_ == MOVE_STATE) {
            // Hybrid PROTECT: drive phase finished, enter scan/engage.
            state_ = SURVEILLANCE_STATE;
            isStateTransition = true;
            isScanning_.store(false);
            request_attack_permission_.store(false);
            protect_scan_start_time_ = this->now();
        }
        break;
    case DRONE_SURVEILLANCE_STATE:
        if (state_ == IDLE) {
            state_ = DRONE_SURVEILLANCE_STATE;
            isStateTransition = true;
            protect_scan_start_time_ = this->now();
        } else if (state_ == TRACKING_STATE) {
            state_ = DRONE_SURVEILLANCE_STATE;
            isStateTransition = true;
            isScanning_.store(false);
            request_attack_permission_.store(false);
            protect_scan_start_time_ = this->now();
        } else if (state_ == MOVE_STATE) {
            // Hybrid PROTECT_DRONE: drive phase finished, enter scan/engage.
            state_ = DRONE_SURVEILLANCE_STATE;
            isStateTransition = true;
            isScanning_.store(false);
            request_attack_permission_.store(false);
            protect_scan_start_time_ = this->now();
        }
        break;
    case TRACKING_STATE:
        if (state_ == SURVEILLANCE_STATE || state_ == DRONE_SURVEILLANCE_STATE ||
            state_ == ASSAULT_STATE) {
            state_ = TRACKING_STATE;
            isStateTransition = true;
            tracking_lock_lost_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            tracking_lock_held_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
        } else if (mission_control_command_.command_id == MissionControlCommand::DEBUG_TRACKING) {
            state_ = TRACKING_STATE;
            isStateTransition = true;
            tracking_lock_lost_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            tracking_lock_held_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
        }
        break;
    case EMERGENCY_STOP_STATE:
        state_ = EMERGENCY_STOP_STATE;
        isStateTransition = true;
        break;
    case ERROR_STATE:
        if (state_ != ERROR_STATE) {
            state_ = ERROR_STATE;
            isStateTransition = true;
        }
        break;
    default:
        RCLCPP_ERROR(this->get_logger(), "Unknown state: %d", next_state);
        state_ = ERROR_STATE;
        break;
    }

    return isStateTransition;
}

e_error_state_ CombatRobotOperationSystem::checkSensorState() const
{
    rclcpp::Time current_time = this->now();

    rclcpp::Time last_update;
    {
        std::lock_guard<std::mutex> lock(mtx_actuator_);
        last_update = last_pantilt_state_update_time_;
    }

    if (check_pantilt_status_ &&
        abs((current_time - last_update).seconds()) > pantilt_status_timeout_sec_) {
        RCLCPP_WARN(this->get_logger(), "Pan/Tilt state update timeout, transitioning to ERROR_STATE");
        return PANTILT_ERROR;
    }

    if (check_detector_status_ && last_detector_update_time_.seconds() != 0.0 &&
        abs((current_time - last_detector_update_time_).seconds()) > detector_status_timeout_sec_) {
        RCLCPP_WARN(this->get_logger(), "Detector state update timeout, transitioning to ERROR_STATE");
        return DETECTOR_ERROR;
    }

    if (check_gun_status_ && last_gun_status_update_time_.seconds() != 0.0 &&
        abs((current_time - last_gun_status_update_time_).seconds()) > gun_status_timeout_sec_) {
        RCLCPP_WARN(this->get_logger(), "Gun Trigger state update timeout, transitioning to ERROR_STATE");
        return GUNTRIGGER_ERROR;
    }

    return NONE;
}

e_operation_state CombatRobotOperationSystem::updateState()
{
    e_operation_state next_state = state_;

    sensor_error_state_ = checkSensorState();

    std::lock_guard<std::mutex> lock(mtx_cmd_);
    if (sensor_error_state_ != NONE) {
        if (run_mode_ == RUN_DEMO) {
            resetDemoSequence();
            run_mode_ = RUN_IDLE;
        }
        RCLCPP_ERROR(this->get_logger(), "Sensor error detected, transitioning to ERROR_STATE");
        next_state = ERROR_STATE;
    } else if (mission_control_command_.estop_requested) {
        if (run_mode_ == RUN_DEMO) {
            resetDemoSequence();
        }
        if (state_ != EMERGENCY_STOP_STATE) {
            RCLCPP_WARN(
                this->get_logger(),
                "Emergency Stop Command Received! Transitioning to EMERGENCY_STOP_STATE");
        }
        run_mode_ = RUN_EMERGENCY_STOP;
        next_state = EMERGENCY_STOP_STATE;
    } else if (state_ == INIT_STATE) {
        if (isInitialized()) {
            next_state = IDLE;
        }
    } else if (state_ == IDLE) {
        if (last_mission_control_command_time_.seconds() < 0.0) {
            RCLCPP_INFO_THROTTLE(
                this->get_logger(),
                *get_clock(),
                1000,
                "No mission control command received, staying in current state: %d",
                state_);
        } else if ((this->now() - last_mission_control_command_time_).seconds() > 0.01) {
            const uint8_t command_id = mission_control_command_.command_id;
            if (shouldRunDemoForCommand(command_id)) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "Mission control command %u received in demo deployment. Starting demo sequence.",
                    static_cast<unsigned>(command_id));
                demo_reported_active_mode_id_ = activeModeIdForCommand(command_id);
                run_mode_ = RUN_DEMO;
                next_state = MOVE_STATE;
            } else if (command_id == MissionControlCommand::IDLE) {
                run_mode_ = RUN_IDLE;
                next_state = IDLE;
            } else if (command_id == MissionControlCommand::RECON) {
                RCLCPP_INFO(this->get_logger(), "Mission control command received: RECON, transitioning to MOVE_STATE");
                run_mode_ = RUN_MOVING;
                next_state = MOVE_STATE;
            } else if (command_id == MissionControlCommand::PROTECT_GENERAL) {
                run_mode_ = RUN_SURVEILLANCE;
                bool path_loaded;
                uint8_t mission_st;
                {
                    std::lock_guard<std::mutex> lock(mtx_mission_);
                    path_loaded = mission_execution_state_.path_loaded;
                    mission_st = mission_execution_state_.status;
                }
                // Hybrid model: if a path was loaded and the robot hasn't
                // arrived yet, drive to the defensive position first
                // (MOVE_STATE) and only switch to scan/engage when the
                // path follower (or operator) marks the mission REACHED.
                if (path_loaded && mission_st != OperationState::MISSION_REACHED) {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "PROTECT_GENERAL received with loaded path; transitioning to MOVE_STATE to drive to defensive position");
                    next_state = MOVE_STATE;
                } else {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "PROTECT_GENERAL received (no path or already at destination); transitioning to SURVEILLANCE_STATE");
                    next_state = SURVEILLANCE_STATE;
                }
            } else if (command_id == MissionControlCommand::PROTECT_DRONE) {
                run_mode_ = RUN_DRONE_SURVEILLANCE;
                bool path_loaded;
                uint8_t mission_st;
                {
                    std::lock_guard<std::mutex> lock(mtx_mission_);
                    path_loaded = mission_execution_state_.path_loaded;
                    mission_st = mission_execution_state_.status;
                }
                if (path_loaded && mission_st != OperationState::MISSION_REACHED) {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "PROTECT_DRONE received with loaded path; transitioning to MOVE_STATE to drive to defensive position");
                    next_state = MOVE_STATE;
                } else {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "PROTECT_DRONE received (no path or already at destination); transitioning to DRONE_SURVEILLANCE_STATE");
                    next_state = DRONE_SURVEILLANCE_STATE;
                }
            } else if (command_id == MissionControlCommand::DEBUG_ATTACK) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "Mission control command received: DEBUG_ATTACK, transitioning to ATTACKING_STATE");
                run_mode_ = RUN_ATTACKING;
                next_state = ATTACKING_STATE;
            } else if (command_id == MissionControlCommand::ASSAULT) {
                run_mode_ = RUN_ASSAULT;
                bool path_loaded;
                uint8_t mission_st;
                {
                    std::lock_guard<std::mutex> lock(mtx_mission_);
                    path_loaded = mission_execution_state_.path_loaded;
                    mission_st = mission_execution_state_.status;
                }
                // Hybrid ASSAULT (same model as PROTECT): if a waypoint path is
                // loaded and not yet reached, drive to the objective first
                // (MOVE_STATE) and switch to scan/engage (ASSAULT_STATE) only
                // once the path follower marks the mission REACHED. With no path
                // (or already there) go straight to scan/engage.
                if (path_loaded && mission_st != OperationState::MISSION_REACHED) {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "ASSAULT received with loaded path; transitioning to MOVE_STATE to drive to objective");
                    next_state = MOVE_STATE;
                } else {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "ASSAULT received (no path or already at objective); transitioning to ASSAULT_STATE for scan/engage");
                    next_state = ASSAULT_STATE;
                }
            } else if (command_id == MissionControlCommand::DEBUG_TRACKING) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "Mission control command received: DEBUG_TRACKING, transitioning to TRACKING_STATE");
                run_mode_ = RUN_MANUAL_ATTACK;
                next_state = TRACKING_STATE;
            } else if (command_id == MissionControlCommand::RETURN_TO_HOME) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "Mission control command received: RETURN_TO_HOME, transitioning to RTH_STATE");
                next_state = RTH_STATE;
            } else {
                RCLCPP_WARN(
                    this->get_logger(),
                    "Unknown mission control command: %d",
                    mission_control_command_.command_id);
            }
        }
    } else if (state_ == MOVE_STATE) {
        if (run_mode_ == RUN_DEMO) {
            if (mission_control_command_.command_id == MissionControlCommand::IDLE) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "Mission control command received: IDLE, aborting demo and transitioning to IDLE state");
                resetDemoSequence();
                run_mode_ = RUN_IDLE;
                next_state = IDLE;
            } else if (demo_completed_) {
                resetDemoSequence(false);
                // updateState() already holds mtx_cmd_ (taken at function entry),
                // so write directly without re-locking.
                mission_control_command_.command_id = MissionControlCommand::IDLE;
                run_mode_ = RUN_IDLE;
                next_state = IDLE;
                RCLCPP_INFO(
                    this->get_logger(),
                    "Demo sequence completed. Cleared mission command latch; awaiting next operator input.");
            }
        } else if (mission_control_command_.command_id == MissionControlCommand::IDLE) {
            run_mode_ = RUN_IDLE;
            next_state = IDLE;
            RCLCPP_INFO(
                this->get_logger(),
                "Mission control command received: IDLE_MODE, transitioning to IDLE state");
        } else if (
            mission_control_command_.command_id == MissionControlCommand::RETURN_TO_HOME) {
            next_state = RTH_STATE;
        } else if (run_mode_ == RUN_SURVEILLANCE || run_mode_ == RUN_DRONE_SURVEILLANCE ||
                   run_mode_ == RUN_ASSAULT) {
            // Hybrid PROTECT/ASSAULT drive phase: once the mission is marked
            // reached, switch from driving to the scan/engage phase. Two arrival
            // sources exist and either one advances the FSM:
            //   1) mission_execution_state_.status — command-driven, set
            //      REACHED only by an operator CMD_COMPLETE (onSwarmPathCommand).
            //   2) vehicle_nav_telemetry_.mission_status — the measured value
            //      the real swarm_path_executor publishes on /swarm/mission_state
            //      when FollowPath completes. Nothing sets source (1) on a
            //      physical formation arrival, so without accepting (2) the
            //      robot would drive to the defensive position and then stay
            //      stuck in MOVE_STATE. Require the telemetry to be fresh so a
            //      stale REACHED from a previous mission can't trigger.
            bool reached = false;
            {
                std::lock_guard<std::mutex> lock(mtx_mission_);
                const bool telemetry_fresh =
                    vehicle_nav_telemetry_.valid &&
                    last_swarm_mission_state_time_.nanoseconds() > 0 &&
                    (this->now() - last_swarm_mission_state_time_) <
                        rclcpp::Duration::from_seconds(2.0);
                reached =
                    mission_execution_state_.status == OperationState::MISSION_REACHED ||
                    (telemetry_fresh &&
                     vehicle_nav_telemetry_.mission_status ==
                         OperationState::MISSION_REACHED);
            }
            if (reached) {
                const char* dest_name;
                if (run_mode_ == RUN_SURVEILLANCE) {
                    next_state = SURVEILLANCE_STATE;
                    dest_name = "SURVEILLANCE_STATE";
                } else if (run_mode_ == RUN_DRONE_SURVEILLANCE) {
                    next_state = DRONE_SURVEILLANCE_STATE;
                    dest_name = "DRONE_SURVEILLANCE_STATE";
                } else {
                    next_state = ASSAULT_STATE;
                    dest_name = "ASSAULT_STATE";
                }
                RCLCPP_INFO(
                    this->get_logger(),
                    "Objective reached; transitioning to %s for scan/engage",
                    dest_name);
            }
        }
    } else if (state_ == ATTACKING_STATE) {
        if (mission_control_command_.command_id == MissionControlCommand::IDLE) {
            RCLCPP_INFO(
                this->get_logger(),
                "Mission control command received: IDLE_MODE, transitioning to IDLE state");
            run_mode_ = RUN_IDLE;
            next_state = IDLE;
        }
    } else if (state_ == RTH_STATE) {
        if (mission_control_command_.command_id == MissionControlCommand::IDLE) {
            RCLCPP_INFO(
                this->get_logger(),
                "Mission control command received: IDLE_MODE, transitioning to IDLE state");
            run_mode_ = RUN_IDLE;
            next_state = IDLE;
        }
    } else if (state_ == SURVEILLANCE_STATE || state_ == DRONE_SURVEILLANCE_STATE ||
               state_ == ASSAULT_STATE) {
        // Shared scan/engage decision for the three hybrid engagement scan
        // states: PROTECT_GENERAL (SURVEILLANCE_STATE), PROTECT_DRONE
        // (DRONE_SURVEILLANCE_STATE) and ASSAULT (ASSAULT_STATE). Same
        // queue/scan-window/TRACKING mechanics; they differ only in wanted
        // class and mode reporting.
        if (mission_control_command_.command_id == MissionControlCommand::IDLE) {
            RCLCPP_INFO(
                this->get_logger(),
                "Mission control command received: IDLE_MODE, transitioning to IDLE state");
            run_mode_ = RUN_IDLE;
            next_state = IDLE;
        } else if (state_ == ASSAULT_STATE &&
                   mission_control_command_.command_id ==
                       MissionControlCommand::RETURN_TO_HOME) {
            // ASSAULT keeps the operator RTH exit; the PROTECT scan states do not.
            next_state = RTH_STATE;
        } else {
            // A "wanted-class" target is one this engagement mode targets:
            // person (0) for PROTECT_GENERAL and ASSAULT, drone (1) for
            // PROTECT_DRONE. Only a
            // wanted-class lock (or already-queued wanted-class ids) advances to
            // TRACKING; a wrong-class lock is ignored so the scan keeps looking
            // (prevents getting stuck in TRACKING on an off-target lock).
            if (wantedClassTargetLocked() || !protect_target_queue_.empty()) {
                // Hold in SURVEILLANCE for protect.scan_duration_sec so the scan
                // can accumulate multiple track_ids into the queue, then
                // transition to TRACKING to start engaging them. If the queue
                // already hit target_count, transition immediately.
                const double scan_elapsed =
                    (this->now() - protect_scan_start_time_).seconds();
                const bool quota_met =
                    static_cast<int>(protect_target_queue_.size()) >= protect_target_count_;
                const bool scan_done = scan_elapsed >= protect_scan_duration_sec_;
                if (quota_met || scan_done) {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "Target locked and scan window complete (%zu queued, %.1f/%.1fs), transitioning to TRACKING_STATE",
                        protect_target_queue_.size(),
                        scan_elapsed,
                        protect_scan_duration_sec_);
                    next_state = TRACKING_STATE;
                } else {
                    RCLCPP_INFO_THROTTLE(
                        this->get_logger(),
                        *get_clock(),
                        1000,
                        "Target locked, scanning for more (%zu queued, %.1f/%.1fs)",
                        protect_target_queue_.size(),
                        scan_elapsed,
                        protect_scan_duration_sec_);
                }
            } else {
                RCLCPP_INFO_THROTTLE(
                    this->get_logger(),
                    *get_clock(),
                    1000,
                    "No wanted-class target locked, staying in SURVEILLANCE_STATE");
            }
        }
    } else if (state_ == TRACKING_STATE) {
        if (mission_control_command_.command_id == MissionControlCommand::IDLE) {
            RCLCPP_INFO(
                this->get_logger(),
                "Mission control command received: IDLE_MODE, transitioning to IDLE state");
            run_mode_ = RUN_IDLE;
            next_state = IDLE;
        } else if (wantedClassTargetLocked()) {
            // Wanted-class lock present — only reset the lost timer if lock has
            // been continuously held for `tracking.min_lock_held_sec`. A single
            // blip of lock=true (e.g. spurious detector frame) does not
            // erase the lost timer, so genuine target loss can still
            // accumulate to the grace threshold and transition out. A
            // wrong-class lock is treated as "not locked" here so it falls
            // through to the lost-grace path below and reverts to surveillance
            // instead of stranding the FSM in TRACKING.
            const rclcpp::Time now_t = this->now();
            if (tracking_lock_held_start_time_.nanoseconds() == 0) {
                tracking_lock_held_start_time_ = now_t;
            }
            const double held_for =
                (now_t - tracking_lock_held_start_time_).seconds();
            if (held_for >= tracking_min_lock_held_sec_) {
                tracking_lock_lost_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            }
        } else if (run_mode_ == RUN_SURVEILLANCE || run_mode_ == RUN_DRONE_SURVEILLANCE ||
                   run_mode_ == RUN_ASSAULT) {
            // Lock missing — debounce before reverting to the scan state.
            // Also clear the held-stability stamp so a blip doesn't carry
            // any credit forward when lock next becomes true.
            tracking_lock_held_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            const rclcpp::Time now_t = this->now();
            if (tracking_lock_lost_start_time_.nanoseconds() == 0) {
                tracking_lock_lost_start_time_ = now_t;
            }
            const double lost_for =
                (now_t - tracking_lock_lost_start_time_).seconds();
            if (lost_for >= tracking_lock_lost_grace_sec_) {
                RCLCPP_INFO(
                    this->get_logger(),
                    "Target lock lost for %.2fs (grace=%.2fs), transitioning to previous state",
                    lost_for,
                    tracking_lock_lost_grace_sec_);
                // Engagement ended because the target was lost beyond
                // grace. Expire this active id's APPROVE so that if SORT
                // later recycles the same track_id onto a DIFFERENT physical
                // target, it is treated as undecided and requires a fresh
                // operator APPROVE instead of auto-firing on the stale
                // approval. DENY stays sticky (denied_set retained) —
                // re-skipping a reused id is fail-safe; auto-firing is not.
                if (protect_active_track_id_ >= 0) {
                    RCLCPP_INFO(
                        this->get_logger(),
                        "Protect engagement complete for track_id=%d "
                        "(approval expired)",
                        protect_active_track_id_);
                    protect_approved_track_ids_.erase(protect_active_track_id_);
                    protect_active_track_id_ = -1;
                }
                std_msgs::msg::Int8 stop_cmd;
                stop_cmd.data = 0;
                pub_gun_cmd_->publish(stop_cmd);
                if (run_mode_ == RUN_SURVEILLANCE) {
                    next_state = SURVEILLANCE_STATE;
                } else if (run_mode_ == RUN_DRONE_SURVEILLANCE) {
                    next_state = DRONE_SURVEILLANCE_STATE;
                } else {
                    next_state = ASSAULT_STATE;
                }
                tracking_lock_lost_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
                tracking_lock_held_start_time_ = rclcpp::Time(0, 0, RCL_ROS_TIME);
            } else {
                RCLCPP_INFO_THROTTLE(
                    this->get_logger(),
                    *get_clock(),
                    500,
                    "Target lock lost (%.2f/%.2fs grace), holding TRACKING_STATE",
                    lost_for,
                    tracking_lock_lost_grace_sec_);
            }
        }
    } else if (state_ == EMERGENCY_STOP_STATE) {
        if (mission_control_command_.command_id == MissionControlCommand::IDLE) {
            RCLCPP_INFO(
                this->get_logger(),
                "Mission control command received: IDLE (IDLE), resetting from Emergency Stop");
            run_mode_ = RUN_IDLE;
            next_state = IDLE;
        }
    } else if (state_ == ERROR_STATE) {
        if (sensor_error_state_ == NONE && error_state_ == NONE) {
            RCLCPP_INFO(this->get_logger(), "Error state resolved, transitioning to IDLE state");
            next_state = IDLE;
        } else {
            RCLCPP_WARN(this->get_logger(), "Still in ERROR_STATE, error state: %d", error_state_);
        }
    }

    transitState(next_state);
    return state_;
}

void CombatRobotOperationSystem::OperateCombatRobotSystem(e_operation_state state)
{
    switch (state) {
    case INIT_STATE:
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "INIT STATE");
        isInitialized_ = Init_statefunc();
        break;
    case IDLE:
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "IDLE STATE");
        Idle_statefunc();
        break;
    case MOVE_STATE:
        if (run_mode_ == RUN_DEMO) {
            RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "DEMO STATE");
        } else {
            RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "MOVE STATE");
        }
        Move_statefunc();
        break;
    case SURVEILLANCE_STATE:
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "SURVEILLANCE STATE");
        Surveillance_statefunc();
        break;
    case DRONE_SURVEILLANCE_STATE:
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "DRONE SURVEILLANCE STATE");
        SurveillanceDrone_statefunc();
        break;
    case ATTACKING_STATE:
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "ATTACKING STATE");
        Attacking_statefunc();
        break;
    case ASSAULT_STATE:
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "ASSAULT STATE");
        Assault_statefunc();
        break;
    case RTH_STATE:
        RCLCPP_INFO_THROTTLE(this->get_logger(), *get_clock(), 1000, "RTH STATE (Placeholder)");
        pantiltManualControl();
        break;
    case TRACKING_STATE:
        Tracking_statefunc();
        break;
    case EMERGENCY_STOP_STATE:
        EmergencyStop_statefunc();
        break;
    case ERROR_STATE:
        Error_statefunc();
        break;
    default:
        RCLCPP_ERROR(this->get_logger(), "Unknown state: %d", state);
        break;
    }
}

void CombatRobotOperationSystem::on_timer()
{
    state_ = updateState();

    if (m_pub_operation_state) {
        OperationState msg;
        msg.state = static_cast<uint8_t>(state_);
        msg.mission_status = OperationState::MISSION_NONE;
        msg.estop_active = (state_ == EMERGENCY_STOP_STATE);
        msg.permission_request_active = request_attack_permission_.load();
        msg.current_zoom_level = static_cast<float>(current_zoom_level_.load());
        if (dummy_leader_state_.enabled) {
            msg.gps_lat = dummy_leader_state_.gps_lat;
            msg.gps_lon = dummy_leader_state_.gps_lon;
            msg.gps_heading = dummy_leader_state_.gps_heading;
            msg.current_speed_mps = dummy_leader_state_.current_speed_mps;
            msg.current_waypoint_index = dummy_leader_state_.current_waypoint_index;
            msg.total_waypoints = dummy_leader_state_.total_waypoints;
            msg.progress_ratio = dummy_leader_state_.progress_ratio;
            msg.distance_to_next_wp_m = dummy_leader_state_.distance_to_next_wp_m;
            msg.distance_to_goal_m = dummy_leader_state_.distance_to_goal_m;
        } else {
            msg.gps_lat = 0.0;
            msg.gps_lon = 0.0;
            msg.gps_heading = 0.0f;
            msg.current_speed_mps = 0.0f;
            msg.current_waypoint_index = 0;
            msg.total_waypoints = 0;
            msg.progress_ratio = 0.0f;
            msg.distance_to_next_wp_m = 0.0f;
            msg.distance_to_goal_m = 0.0f;
        }
        fillMissionExecutionStatus(msg);
        msg.error_code =
            static_cast<uint8_t>(sensor_error_state_ != NONE ? sensor_error_state_ : error_state_);

        // 차량 mission_control 의 실측 nav 텔레메트리를 통합한다. command_server 는 더
        // 이상 /swarm/mission_state 를 직접 구독하지 않으므로 FSM 이 단일 창구로서
        // 실 GPS/속도/waypoint 진행률을 /operation_state 에 실어 보낸다. dummy 리더
        // 모드(벤치 테스트)에서는 위에서 채운 더미값을 유지한다.
        if (!dummy_leader_state_.enabled) {
            std::lock_guard<std::mutex> lock(mtx_mission_);
            const bool telemetry_fresh =
                vehicle_nav_telemetry_.valid &&
                last_swarm_mission_state_time_.nanoseconds() > 0 &&
                (this->now() - last_swarm_mission_state_time_) <
                    rclcpp::Duration::from_seconds(2.0);
            if (telemetry_fresh) {
                msg.gps_lat = vehicle_nav_telemetry_.gps_lat;
                msg.gps_lon = vehicle_nav_telemetry_.gps_lon;
                msg.gps_heading = vehicle_nav_telemetry_.gps_heading;
                msg.current_speed_mps = vehicle_nav_telemetry_.current_speed_mps;
                msg.mission_status = vehicle_nav_telemetry_.mission_status;
                msg.current_waypoint_index = vehicle_nav_telemetry_.current_waypoint_index;
                msg.total_waypoints = vehicle_nav_telemetry_.total_waypoints;
                msg.progress_ratio = vehicle_nav_telemetry_.progress_ratio;
                msg.distance_to_next_wp_m = vehicle_nav_telemetry_.distance_to_next_wp_m;
                msg.distance_to_goal_m = vehicle_nav_telemetry_.distance_to_goal_m;
                // FSM 자체 sensor/error 가 우선; 없을 때만 차량 nav 에러를 반영.
                if (msg.error_code == 0) {
                    msg.error_code = vehicle_nav_telemetry_.error_code;
                }
            }
        }

        uint8_t current_mode_id = OperationState::ACTIVE_MODE_IDLE;
        switch (state_) {
        case MOVE_STATE:
            if (run_mode_ == RUN_DEMO) {
                current_mode_id = demo_reported_active_mode_id_;
            } else if (run_mode_ == RUN_SURVEILLANCE) {
                // Hybrid PROTECT_GENERAL drive phase.
                current_mode_id = OperationState::ACTIVE_MODE_PROTECT_GENERAL;
            } else if (run_mode_ == RUN_DRONE_SURVEILLANCE) {
                // Hybrid PROTECT_DRONE drive phase.
                current_mode_id = OperationState::ACTIVE_MODE_PROTECT_DRONE;
            } else if (run_mode_ == RUN_ASSAULT) {
                // Hybrid ASSAULT drive-to-objective phase.
                current_mode_id = OperationState::ACTIVE_MODE_ASSAULT;
            } else {
                current_mode_id = OperationState::ACTIVE_MODE_RECON;
            }
            break;
        case SURVEILLANCE_STATE:
            current_mode_id = OperationState::ACTIVE_MODE_PROTECT_GENERAL;
            break;
        case DRONE_SURVEILLANCE_STATE:
            current_mode_id = OperationState::ACTIVE_MODE_PROTECT_DRONE;
            break;
        case TRACKING_STATE:
            // TRACKING is a sub-state of whichever surveillance/attack run
            // mode we were in — report the parent so the tablet doesn't
            // see active_mode_id flip to IDLE during the engagement.
            if (run_mode_ == RUN_SURVEILLANCE) {
                current_mode_id = OperationState::ACTIVE_MODE_PROTECT_GENERAL;
            } else if (run_mode_ == RUN_DRONE_SURVEILLANCE) {
                current_mode_id = OperationState::ACTIVE_MODE_PROTECT_DRONE;
            } else {
                current_mode_id = OperationState::ACTIVE_MODE_ASSAULT;
            }
            break;
        case ASSAULT_STATE:
            current_mode_id = OperationState::ACTIVE_MODE_ASSAULT;
            break;
        case RTH_STATE:
            current_mode_id = OperationState::ACTIVE_MODE_RETURN_TO_HOME;
            break;
        default:
            current_mode_id = OperationState::ACTIVE_MODE_IDLE;
            break;
        }
        if (state_ != EMERGENCY_STOP_STATE) {
            last_non_estop_active_mode_id_ = current_mode_id;
            msg.active_mode_id = current_mode_id;
        } else {
            msg.active_mode_id = last_non_estop_active_mode_id_;
        }

        bool crosshair_valid = false;
        float crosshair_x = -1.0f;
        float crosshair_y = -1.0f;
        if (state_ == TRACKING_STATE || state_ == SURVEILLANCE_STATE ||
            state_ == DRONE_SURVEILLANCE_STATE || state_ == ASSAULT_STATE) {
            crosshair_valid = isTargetLocked_.load();
        }
        if (crosshair_valid) {
            std::lock_guard<std::mutex> lock(mtx_target_);
            crosshair_x = target_object_.x;
            crosshair_y = target_object_.y;
        }
        msg.crosshair_x = crosshair_x;
        msg.crosshair_y = crosshair_y;

        m_pub_operation_state->publish(msg);
    }

    OperateCombatRobotSystem(state_);
}

}  // namespace combat_robot_system
