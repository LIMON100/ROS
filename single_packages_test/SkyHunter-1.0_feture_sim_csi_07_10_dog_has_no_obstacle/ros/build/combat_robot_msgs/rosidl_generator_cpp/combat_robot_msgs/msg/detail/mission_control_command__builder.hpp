// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/MissionControlCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/mission_control_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/mission_control_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_MissionControlCommand_drone_target_valid
{
public:
  explicit Init_MissionControlCommand_drone_target_valid(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::MissionControlCommand drone_target_valid(::combat_robot_msgs::msg::MissionControlCommand::_drone_target_valid_type arg)
  {
    msg_.drone_target_valid = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_drone_target_lon
{
public:
  explicit Init_MissionControlCommand_drone_target_lon(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_drone_target_valid drone_target_lon(::combat_robot_msgs::msg::MissionControlCommand::_drone_target_lon_type arg)
  {
    msg_.drone_target_lon = std::move(arg);
    return Init_MissionControlCommand_drone_target_valid(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_drone_target_lat
{
public:
  explicit Init_MissionControlCommand_drone_target_lat(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_drone_target_lon drone_target_lat(::combat_robot_msgs::msg::MissionControlCommand::_drone_target_lat_type arg)
  {
    msg_.drone_target_lat = std::move(arg);
    return Init_MissionControlCommand_drone_target_lon(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_lateral_wind_speed
{
public:
  explicit Init_MissionControlCommand_lateral_wind_speed(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_drone_target_lat lateral_wind_speed(::combat_robot_msgs::msg::MissionControlCommand::_lateral_wind_speed_type arg)
  {
    msg_.lateral_wind_speed = std::move(arg);
    return Init_MissionControlCommand_drone_target_lat(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_zoom_command
{
public:
  explicit Init_MissionControlCommand_zoom_command(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_lateral_wind_speed zoom_command(::combat_robot_msgs::msg::MissionControlCommand::_zoom_command_type arg)
  {
    msg_.zoom_command = std::move(arg);
    return Init_MissionControlCommand_lateral_wind_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_tilt_speed
{
public:
  explicit Init_MissionControlCommand_tilt_speed(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_zoom_command tilt_speed(::combat_robot_msgs::msg::MissionControlCommand::_tilt_speed_type arg)
  {
    msg_.tilt_speed = std::move(arg);
    return Init_MissionControlCommand_zoom_command(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_pan_speed
{
public:
  explicit Init_MissionControlCommand_pan_speed(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_tilt_speed pan_speed(::combat_robot_msgs::msg::MissionControlCommand::_pan_speed_type arg)
  {
    msg_.pan_speed = std::move(arg);
    return Init_MissionControlCommand_tilt_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_attack_permission
{
public:
  explicit Init_MissionControlCommand_attack_permission(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_pan_speed attack_permission(::combat_robot_msgs::msg::MissionControlCommand::_attack_permission_type arg)
  {
    msg_.attack_permission = std::move(arg);
    return Init_MissionControlCommand_pan_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_estop_requested
{
public:
  explicit Init_MissionControlCommand_estop_requested(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_attack_permission estop_requested(::combat_robot_msgs::msg::MissionControlCommand::_estop_requested_type arg)
  {
    msg_.estop_requested = std::move(arg);
    return Init_MissionControlCommand_attack_permission(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_command_id
{
public:
  explicit Init_MissionControlCommand_command_id(::combat_robot_msgs::msg::MissionControlCommand & msg)
  : msg_(msg)
  {}
  Init_MissionControlCommand_estop_requested command_id(::combat_robot_msgs::msg::MissionControlCommand::_command_id_type arg)
  {
    msg_.command_id = std::move(arg);
    return Init_MissionControlCommand_estop_requested(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

class Init_MissionControlCommand_header
{
public:
  Init_MissionControlCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_MissionControlCommand_command_id header(::combat_robot_msgs::msg::MissionControlCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_MissionControlCommand_command_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::MissionControlCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::MissionControlCommand>()
{
  return combat_robot_msgs::msg::builder::Init_MissionControlCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__MISSION_CONTROL_COMMAND__BUILDER_HPP_
