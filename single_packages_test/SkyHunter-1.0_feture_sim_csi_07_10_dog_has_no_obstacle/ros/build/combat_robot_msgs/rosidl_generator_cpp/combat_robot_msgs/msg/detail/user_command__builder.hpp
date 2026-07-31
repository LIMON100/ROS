// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/UserCommand.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/user_command.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/user_command__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_UserCommand_stream_command
{
public:
  explicit Init_UserCommand_stream_command(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::UserCommand stream_command(::combat_robot_msgs::msg::UserCommand::_stream_command_type arg)
  {
    msg_.stream_command = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_zoom_command
{
public:
  explicit Init_UserCommand_zoom_command(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_stream_command zoom_command(::combat_robot_msgs::msg::UserCommand::_zoom_command_type arg)
  {
    msg_.zoom_command = std::move(arg);
    return Init_UserCommand_stream_command(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_tilt_speed
{
public:
  explicit Init_UserCommand_tilt_speed(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_zoom_command tilt_speed(::combat_robot_msgs::msg::UserCommand::_tilt_speed_type arg)
  {
    msg_.tilt_speed = std::move(arg);
    return Init_UserCommand_zoom_command(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_pan_speed
{
public:
  explicit Init_UserCommand_pan_speed(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_tilt_speed pan_speed(::combat_robot_msgs::msg::UserCommand::_pan_speed_type arg)
  {
    msg_.pan_speed = std::move(arg);
    return Init_UserCommand_tilt_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_gun_trigger_permission
{
public:
  explicit Init_UserCommand_gun_trigger_permission(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_pan_speed gun_trigger_permission(::combat_robot_msgs::msg::UserCommand::_gun_trigger_permission_type arg)
  {
    msg_.gun_trigger_permission = std::move(arg);
    return Init_UserCommand_pan_speed(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_gun_trigger
{
public:
  explicit Init_UserCommand_gun_trigger(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_gun_trigger_permission gun_trigger(::combat_robot_msgs::msg::UserCommand::_gun_trigger_type arg)
  {
    msg_.gun_trigger = std::move(arg);
    return Init_UserCommand_gun_trigger_permission(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_drone_target_valid
{
public:
  explicit Init_UserCommand_drone_target_valid(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_gun_trigger drone_target_valid(::combat_robot_msgs::msg::UserCommand::_drone_target_valid_type arg)
  {
    msg_.drone_target_valid = std::move(arg);
    return Init_UserCommand_gun_trigger(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_drone_target_lon
{
public:
  explicit Init_UserCommand_drone_target_lon(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_drone_target_valid drone_target_lon(::combat_robot_msgs::msg::UserCommand::_drone_target_lon_type arg)
  {
    msg_.drone_target_lon = std::move(arg);
    return Init_UserCommand_drone_target_valid(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_drone_target_lat
{
public:
  explicit Init_UserCommand_drone_target_lat(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_drone_target_lon drone_target_lat(::combat_robot_msgs::msg::UserCommand::_drone_target_lat_type arg)
  {
    msg_.drone_target_lat = std::move(arg);
    return Init_UserCommand_drone_target_lon(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_target_y
{
public:
  explicit Init_UserCommand_target_y(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_drone_target_lat target_y(::combat_robot_msgs::msg::UserCommand::_target_y_type arg)
  {
    msg_.target_y = std::move(arg);
    return Init_UserCommand_drone_target_lat(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_target_x
{
public:
  explicit Init_UserCommand_target_x(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_target_y target_x(::combat_robot_msgs::msg::UserCommand::_target_x_type arg)
  {
    msg_.target_x = std::move(arg);
    return Init_UserCommand_target_y(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_command_id
{
public:
  explicit Init_UserCommand_command_id(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_target_x command_id(::combat_robot_msgs::msg::UserCommand::_command_id_type arg)
  {
    msg_.command_id = std::move(arg);
    return Init_UserCommand_target_x(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_command_from
{
public:
  explicit Init_UserCommand_command_from(::combat_robot_msgs::msg::UserCommand & msg)
  : msg_(msg)
  {}
  Init_UserCommand_command_id command_from(::combat_robot_msgs::msg::UserCommand::_command_from_type arg)
  {
    msg_.command_from = std::move(arg);
    return Init_UserCommand_command_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

class Init_UserCommand_header
{
public:
  Init_UserCommand_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_UserCommand_command_from header(::combat_robot_msgs::msg::UserCommand::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_UserCommand_command_from(msg_);
  }

private:
  ::combat_robot_msgs::msg::UserCommand msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::UserCommand>()
{
  return combat_robot_msgs::msg::builder::Init_UserCommand_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__USER_COMMAND__BUILDER_HPP_
