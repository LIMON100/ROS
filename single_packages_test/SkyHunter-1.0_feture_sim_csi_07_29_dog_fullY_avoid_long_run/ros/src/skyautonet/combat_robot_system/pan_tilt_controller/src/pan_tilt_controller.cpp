#include "pan_tilt_controller.hpp"
#include "lifecycle_msgs/msg/state.hpp"

namespace combat_robot_system {

PanTiltController::PanTiltController(const rclcpp::NodeOptions & options) 
: rclcpp_lifecycle::LifecycleNode("pan_tilt_controller_node", options) {
  // Constructor: Declare parameters
  init_param();
  RCLCPP_INFO(get_logger(), "PanTiltController constructed. Waiting for configuration.");
}

PanTiltController::~PanTiltController() {
  if (serial_comm_ && serial_comm_->is_open()) {
    send_data_stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    send_data_stop(); // Redundant for safety
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    serial_comm_->close();
  }
}

// ------------------------ Lifecycle Callbacks ------------------------

CallbackReturn PanTiltController::on_configure(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(get_logger(), "Configuring PanTiltController...");

    // 1. Setup Serial Object (but don't open yet)
    serial_comm_ = std::make_unique<async_comm::Serial>(port_name_.c_str(), bard_rate_);
    serial_comm_->register_receive_callback(std::bind(&PanTiltController::on_read_data, this, _1, _2));

    // Open Serial Port (Moved from on_activate)
    if (!serial_comm_->init()) {
        RCLCPP_ERROR(get_logger(), "Failed to open serial port: %s", port_name_.c_str());
        return CallbackReturn::FAILURE;
    }

    // 2. Setup Communication Interfaces
    pub_actuator_state_ = this->create_publisher<PanTiltState>("output/state", 1);
    
    // Note: Subscriptions are usually created in configure or activate.
    // Since subscriptions don't have explicit lifecycle states, creating them here is fine.
    // They will receive messages, but we can choose to ignore them if not active.
    sub_control_command_ = this->create_subscription<PanTiltControlCommand>(
        "input/control_command", 1, std::bind(&PanTiltController::on_control_command, this, _1));

    RCLCPP_INFO(get_logger(), "PanTiltController configured.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn PanTiltController::on_activate(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(get_logger(), "Activating PanTiltController...");

    // 1. Activate Publisher
    pub_actuator_state_->on_activate();

    // 2. Initial State Check & Homing
    // Clear queue first
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        command_queue_.clear();
    }
    enqueue_command(GET_ANGLE_HOR);
    enqueue_command(GET_ANGLE_VER);

    if (enable_startup_homing_) {
        RCLCPP_INFO(get_logger(), "Startup Homing Enabled. Moving to home position.");
        int32_t pan_deg = static_cast<int32_t>((home_pan_angle_ + pan_offset_) * 100.0f);
        int32_t tilt_deg = static_cast<int32_t>((home_tilt_angle_ + tilt_offset_) * 100.0f);
        enqueue_command(SET_ANGLE_HOR, static_cast<uint16_t>(pan_deg));
        enqueue_command(SET_ANGLE_VER, static_cast<uint16_t>(tilt_deg));
    }

    last_command_time_ = rclcpp::Time(0, 0, this->get_clock()->get_clock_type());

    // 4. Start Timer
    timer_ = this->create_wall_timer(
        std::chrono::duration<double>(1.0 / controller_frequency_),
        std::bind(&PanTiltController::on_timer, this));

    RCLCPP_INFO(get_logger(), "PanTiltController activated.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn PanTiltController::on_deactivate(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(get_logger(), "Deactivating PanTiltController...");

    // 1. Stop Timer
    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }

    // 2. Stop Motion
    send_data_stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    send_data_stop();
    
    dir_control_enabled_ = false;

    // 3. Deactivate Publisher
    pub_actuator_state_->on_deactivate();

    RCLCPP_INFO(get_logger(), "PanTiltController deactivated.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn PanTiltController::on_cleanup(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(get_logger(), "Cleaning up PanTiltController...");

    // Release resources
    pub_actuator_state_.reset();
    sub_control_command_.reset();
    
    if (serial_comm_ && serial_comm_->is_open()) {
        send_data_stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        serial_comm_->close();
    }
    serial_comm_.reset(); // Deletes serial object

    RCLCPP_INFO(get_logger(), "PanTiltController cleaned up.");
    return CallbackReturn::SUCCESS;
}

CallbackReturn PanTiltController::on_shutdown(const rclcpp_lifecycle::State &) {
    RCLCPP_INFO(get_logger(), "Shutting down PanTiltController...");

    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }
    
    if (serial_comm_ && serial_comm_->is_open()) {
        send_data_stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        send_data_stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        serial_comm_->close();
    }
    
    pub_actuator_state_.reset();
    sub_control_command_.reset();
    serial_comm_.reset();

    return CallbackReturn::SUCCESS;
}

CallbackReturn PanTiltController::on_error(const rclcpp_lifecycle::State &) {
    RCLCPP_ERROR(get_logger(), "Error detected in PanTiltController. Cleaning up...");

    if (timer_) {
        timer_->cancel();
        timer_.reset();
    }
    
    if (serial_comm_ && serial_comm_->is_open()) {
        send_data_stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        send_data_stop();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        serial_comm_->close();
    }
    
    pub_actuator_state_.reset();
    sub_control_command_.reset();
    serial_comm_.reset();

    return CallbackReturn::SUCCESS;
}

// ------------------------ Internal Functions ------------------------

void PanTiltController::init_param(){
  port_name_ = declare_parameter("port", "/dev/ttyUSB0");
  bard_rate_ = declare_parameter("bard_rate", 9600);

  pan_offset_ = declare_parameter("pan_offset", 180);
  tilt_offset_ = declare_parameter("tilt_offset", 90);
  min_tilt_angle_ = declare_parameter("min_tilt_angle", 0);
  max_tilt_angle_ = declare_parameter("max_tilt_angle", 180);
  min_pan_angle_ = declare_parameter("min_pan_angle", 0);
  max_pan_angle_ = declare_parameter("max_pan_angle", 360);

  angle_tolerance_ = declare_parameter("angle_tolerance", 0.1);
  min_pan_speed_ = declare_parameter("min_pan_speed", 5);
  min_tilt_speed_ = declare_parameter("min_tilt_speed", 5);
  max_pan_speed_ = declare_parameter("max_pan_speed", 63);
  max_tilt_speed_ = declare_parameter("max_tilt_speed", 63);

  // PID Controller Gains
  pan_proportional_gain_ = declare_parameter("pid.pan.p", 10.0);
  pan_integral_gain_ = declare_parameter("pid.pan.i", 0.1);
  pan_derivative_gain_ = declare_parameter("pid.pan.d", 0.05);
  pan_integral_windup_limit_ = declare_parameter("pid.pan.windup_limit", 5.0);
  tilt_proportional_gain_ = declare_parameter("pid.tilt.p", 10.0);
  tilt_integral_gain_ = declare_parameter("pid.tilt.i", 0.1);
  tilt_derivative_gain_ = declare_parameter("pid.tilt.d", 0.05);
  tilt_integral_windup_limit_ = declare_parameter("pid.tilt.windup_limit", 5.0);

  // Velocity and Acceleration Limits
  max_pan_velocity_ = declare_parameter("limits.velocity.pan", 50.0);
  max_tilt_velocity_ = declare_parameter("limits.velocity.tilt", 50.0);
  max_pan_acceleration_ = declare_parameter("limits.acceleration.pan", 10.0);
  max_tilt_acceleration_ = declare_parameter("limits.acceleration.tilt", 10.0);

  // Safety and Timeout
  command_timeout_ms_ = declare_parameter("safety.command_timeout_ms", 1000.0);
  enable_startup_homing_ = declare_parameter("safety.enable_startup_homing", false);
  home_pan_angle_ = declare_parameter("safety.home_pan_angle", 0.0);
  home_tilt_angle_ = declare_parameter("safety.home_tilt_angle", 0.0);

  // Convenience
  invert_pan_direction_ = declare_parameter("convenience.invert_pan_direction", false);
  invert_tilt_direction_ = declare_parameter("convenience.invert_tilt_direction", false);
  controller_frequency_ = declare_parameter("convenience.controller_frequency", 50.0);
}

// init_serial_comm removed, logic moved to on_configure/on_activate

// init_ros removed, logic moved to on_configure

void PanTiltController::on_control_command(const PanTiltControlCommand::ConstSharedPtr msg){
  // Ignore commands if not active
  if (this->get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
      return;
  }

  last_command_time_ = this->now();
  //RCLCPP_INFO(this->get_logger(), "PT Control Mode: %d", msg->control_mode);
  switch (msg->control_mode){
    case PanTiltControlCommand::CONTROL_BRAKE: {
      dir_control_enabled_ = false;
      pan_integral_ = 0.0;
      tilt_integral_ = 0.0;
      last_pan_error_ = 0.0;
      last_tilt_error_ = 0.0;
      enqueue_command(SET_BRAKE);
      break;
    }
    case PanTiltControlCommand::CONTROL_HOR_POS: {
      dir_control_enabled_ = false;
      pan_integral_ = 0.0;
      last_pan_error_ = 0.0;

      int32_t deg = static_cast<int32_t>((msg->horizontal_angle + pan_offset_) * 100.0f);
      deg = std::clamp(deg, min_pan_angle_ * 100, max_pan_angle_ * 100);
      uint16_t SET_SPEED_ANGLE_HOR = static_cast<uint16_t>(msg->pan_speed) << 8 | SET_ANGLE_HOR;
      enqueue_command(SET_SPEED_ANGLE_HOR, static_cast<uint16_t>(deg));
      break;
    }
    case PanTiltControlCommand::CONTROL_VER_POS: {
      dir_control_enabled_ = false;
      tilt_integral_ = 0.0;
      last_tilt_error_ = 0.0;

      int32_t deg = static_cast<int32_t>((msg->vertical_angle + tilt_offset_) * 100.0f);
      deg = std::clamp(deg, min_tilt_angle_ * 100, max_tilt_angle_ * 100);
            
      if (deg < 0) deg += 36000; 

      uint16_t SET_SPEED_ANGLE_VER = static_cast<uint16_t>(msg->tilt_speed) << 8 | SET_ANGLE_VER;
      enqueue_command(SET_SPEED_ANGLE_VER, static_cast<uint16_t>(deg));
      break;
    }
    case PanTiltControlCommand::CONTROL_DIR: {
      if (!dir_control_enabled_) {
        // Reset PID states when switching to DIR control
        pan_integral_ = 0.0;
        tilt_integral_ = 0.0;
        last_pan_error_ = 0.0;
        last_tilt_error_ = 0.0;
      }
      dir_control_enabled_ = true;

      target_pan_angle_ = std::clamp((double)msg->horizontal_angle, (double)min_pan_angle_ - pan_offset_, (double)max_pan_angle_ - pan_offset_); 
      target_tilt_angle_ = std::clamp((double)msg->vertical_angle, (double)min_tilt_angle_ - tilt_offset_, (double)max_tilt_angle_ - tilt_offset_);  

      target_pan_speed_ = msg->pan_speed;;
      target_tilt_speed_ = msg->tilt_speed;

      update_dir_control();
      break;
    }
  }
}

void PanTiltController::update_dir_control() {
  double dt = 1.0 / controller_frequency_;
  if (dt <= 0) return;

  // 1. Calculate shortest path error for Pan (handles wrap-around)
  float pan_error = target_pan_angle_ - state_.horizontal_angle;
  if (pan_error > 180.0f) {
    pan_error -= 360.0f;
  } else if (pan_error < -180.0f) {
    pan_error += 360.0f;
  }

  // 2. Calculate error for Tilt
  float tilt_error = target_tilt_angle_ - state_.vertical_angle;

  // --- PID Calculation for Pan ---
  double p_term_pan = pan_proportional_gain_ * pan_error;
  pan_integral_ += pan_error * dt;
  pan_integral_ = std::clamp(pan_integral_, -pan_integral_windup_limit_, pan_integral_windup_limit_);
  double i_term_pan = pan_integral_gain_ * pan_integral_;
  double derivative_pan = (pan_error - last_pan_error_) / dt;
  double d_term_pan = pan_derivative_gain_ * derivative_pan;
  last_pan_error_ = pan_error;
  double pan_effort = p_term_pan + i_term_pan + d_term_pan;


  // --- PID Calculation for Tilt ---
  double p_term_tilt = tilt_proportional_gain_ * tilt_error;
  tilt_integral_ += tilt_error * dt;
  tilt_integral_ = std::clamp(tilt_integral_, -tilt_integral_windup_limit_, tilt_integral_windup_limit_);
  double i_term_tilt = tilt_integral_gain_ * tilt_integral_;
  double derivative_tilt = (tilt_error - last_tilt_error_) / dt;
  double d_term_tilt = tilt_derivative_gain_ * derivative_tilt;
  last_tilt_error_ = tilt_error;
  double tilt_effort = p_term_tilt + i_term_tilt + d_term_tilt;


  // --- Convert Effort to Speed and Direction ---
  uint8_t pan_dir_flag = 0;
  uint8_t pan_speed = 0;
  uint8_t tilt_dir_flag = 0;
  uint8_t tilt_speed = 0;

  // Pan
  if (std::abs(pan_error) > angle_tolerance_) {
    pan_dir_flag = (pan_effort > 0) ? 1 : 2; // 1: Right, 2: Left
    if (invert_pan_direction_) { pan_dir_flag = (pan_dir_flag == 1) ? 2 : 1; }

    double calculated_speed = std::abs(pan_effort);
    double max_speed_for_command = std::min({(double)target_pan_speed_, max_pan_velocity_, (double)max_pan_speed_});
    pan_speed = static_cast<uint8_t>(std::clamp(calculated_speed, (double)min_pan_speed_, max_speed_for_command));
  }

  // Tilt
  if (std::abs(tilt_error) > angle_tolerance_) {
    tilt_dir_flag = (tilt_effort > 0) ? 1 : 2; // 1: Up, 2: Down
    if (invert_tilt_direction_) { tilt_dir_flag = (tilt_dir_flag == 1) ? 2 : 1; }
    
    double calculated_speed = std::abs(tilt_effort);
    double max_speed_for_command = std::min({(double)target_tilt_speed_, max_tilt_velocity_, (double)max_tilt_speed_});
    tilt_speed = static_cast<uint8_t>(std::clamp(calculated_speed, (double)min_tilt_speed_, max_speed_for_command));
  }

  // 5. Create and enqueue command
  uint16_t dir = pan_dir_[pan_dir_flag] | tilt_dir_[tilt_dir_flag];
  
  if (dir != 0) {
    uint16_t speed = static_cast<uint16_t>((pan_speed << 8) | tilt_speed);
    // RCLCPP_INFO(this->get_logger(), "PT Control dir: %X,  Speed: %X", dir, speed);
    enqueue_command(dir, speed);
  } else {
    enqueue_command(SET_BRAKE);
  }
}

void PanTiltController::on_timer() {
  // Only run if active
  if (this->get_current_state().id() != lifecycle_msgs::msg::State::PRIMARY_STATE_ACTIVE) {
      return;
  }

  // Check for command timeout
  if (last_command_time_.nanoseconds() > 0 && (this->now() - last_command_time_).seconds() * 1000.0 > command_timeout_ms_) {
    RCLCPP_WARN_THROTTLE(get_logger(), *get_clock(), 5000, "Command timeout! Stopping pan-tilt motion.");
    dir_control_enabled_ = false;
    enqueue_command(SET_BRAKE);
  }

  enqueue_command(GET_ANGLE_VER);
  enqueue_command(GET_ANGLE_HOR);

  if (dir_control_enabled_) {
    update_dir_control();
  }

  send_data();
  pub_actuator_state_->publish(state_);
}

void PanTiltController::on_read_data(const uint8_t* buf, size_t len){
  recv_buffer_.insert(recv_buffer_.end(), buf, buf + len);
  while (recv_buffer_.size() >= 7) {
    auto it = std::find(recv_buffer_.begin(), recv_buffer_.end(), 0xFF);
    if (it == recv_buffer_.end()) return recv_buffer_.clear();

    size_t start = std::distance(recv_buffer_.begin(), it);
    if (recv_buffer_.size() - start < 7) break;

    uint8_t sum = recv_buffer_[start + 1] + recv_buffer_[start + 2] + recv_buffer_[start + 3] + recv_buffer_[start + 4] + recv_buffer_[start + 5];
    if (recv_buffer_[start + 6] != sum) {
      recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + start + 1);
      continue;
    }
    parsing_data(&recv_buffer_[start], 7);
    recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + start + 7);
  }
}

uint8_t PanTiltController::calc_check_sum(uint8_t* data){
  return (data[1] + data[2] + data[3] + data[4] + data[5]) & 0xFF;
}


void PanTiltController::parsing_data(const uint8_t* buf, [[maybe_unused]] size_t len) {
  state_.stamp = this->now();
  if (buf[3] == REQ_ANGLE_VER) {
    int32_t angle = static_cast<int32_t>(static_cast<uint16_t>((buf[4] << 8) | buf[5]));
    if (angle > 18000) angle = angle - 36000; 

    state_.vertical_angle = (angle/ 100.0f) - tilt_offset_;
  } else if (buf[3] == REQ_ANGLE_HOR) {
    state_.horizontal_angle = (((buf[4] << 8) | buf[5])/ 100.0f) - pan_offset_;
  }
}

void PanTiltController::send_data(){
  std::lock_guard<std::mutex> lock(queue_mutex_);

  if(command_queue_.empty()) return;

  auto& cmd = command_queue_.front();
  command_queue_.pop_front();

  uint8_t buf[7] = {0xFF, PAN_TILT_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00};

  buf[2] = static_cast<uint8_t>(cmd.first >> 8);
  buf[3] = static_cast<uint8_t>(cmd.first & 0xFF); 
  buf[4] = static_cast<uint8_t>((cmd.second >> 8) & 0xFF); 
  buf[5] = static_cast<uint8_t>(cmd.second & 0xFF); 

  buf[6] = calc_check_sum(buf);
  serial_comm_->send_bytes(buf, 7);
}

void PanTiltController::send_data_stop(){
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    command_queue_.clear();
  }
  uint8_t buf[7] = {0xFF, PAN_TILT_ADDR, 0x00, 0x00, 0x00, 0x00, 0x00};

  buf[6] = calc_check_sum(buf);
  serial_comm_->send_bytes(buf, 7);
}


void PanTiltController::enqueue_command(uint16_t type, uint16_t data) {
  auto is_direction_type = [](uint16_t t) {
    const uint16_t all_dirs = DIR_RIGHT | DIR_LEFT | DIR_UP | DIR_DOWN;
    switch (t) {
        case SET_BRAKE:
            return (t & all_dirs) == t; 
        default:
            return (t > 0) && ((t & all_dirs) == t);
    }
  };

  auto it = std::find_if(command_queue_.begin(), command_queue_.end(), [&](const auto& item) {
      return (is_direction_type(item.first) && is_direction_type(type)) || ((item.first&0x00FF) == (type&0x00FF));
  });

  if (it != command_queue_.end()) {
    it->first = type;
    it->second = data;
  } else {
    command_queue_.emplace_back(type, data);
  }
}

}  // namespace combat_robot_system
#include <rclcpp_components/register_node_macro.hpp>
RCLCPP_COMPONENTS_REGISTER_NODE(combat_robot_system::PanTiltController)