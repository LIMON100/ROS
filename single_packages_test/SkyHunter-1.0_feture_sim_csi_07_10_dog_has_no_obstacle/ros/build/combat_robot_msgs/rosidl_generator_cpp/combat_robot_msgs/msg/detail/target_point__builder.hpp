// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/TargetPoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/target_point.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/target_point__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_TargetPoint_track_id
{
public:
  explicit Init_TargetPoint_track_id(::combat_robot_msgs::msg::TargetPoint & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::TargetPoint track_id(::combat_robot_msgs::msg::TargetPoint::_track_id_type arg)
  {
    msg_.track_id = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

class Init_TargetPoint_box
{
public:
  explicit Init_TargetPoint_box(::combat_robot_msgs::msg::TargetPoint & msg)
  : msg_(msg)
  {}
  Init_TargetPoint_track_id box(::combat_robot_msgs::msg::TargetPoint::_box_type arg)
  {
    msg_.box = std::move(arg);
    return Init_TargetPoint_track_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

class Init_TargetPoint_class_id
{
public:
  explicit Init_TargetPoint_class_id(::combat_robot_msgs::msg::TargetPoint & msg)
  : msg_(msg)
  {}
  Init_TargetPoint_box class_id(::combat_robot_msgs::msg::TargetPoint::_class_id_type arg)
  {
    msg_.class_id = std::move(arg);
    return Init_TargetPoint_box(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

class Init_TargetPoint_height
{
public:
  explicit Init_TargetPoint_height(::combat_robot_msgs::msg::TargetPoint & msg)
  : msg_(msg)
  {}
  Init_TargetPoint_class_id height(::combat_robot_msgs::msg::TargetPoint::_height_type arg)
  {
    msg_.height = std::move(arg);
    return Init_TargetPoint_class_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

class Init_TargetPoint_y
{
public:
  explicit Init_TargetPoint_y(::combat_robot_msgs::msg::TargetPoint & msg)
  : msg_(msg)
  {}
  Init_TargetPoint_height y(::combat_robot_msgs::msg::TargetPoint::_y_type arg)
  {
    msg_.y = std::move(arg);
    return Init_TargetPoint_height(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

class Init_TargetPoint_x
{
public:
  explicit Init_TargetPoint_x(::combat_robot_msgs::msg::TargetPoint & msg)
  : msg_(msg)
  {}
  Init_TargetPoint_y x(::combat_robot_msgs::msg::TargetPoint::_x_type arg)
  {
    msg_.x = std::move(arg);
    return Init_TargetPoint_y(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

class Init_TargetPoint_is_locked
{
public:
  explicit Init_TargetPoint_is_locked(::combat_robot_msgs::msg::TargetPoint & msg)
  : msg_(msg)
  {}
  Init_TargetPoint_x is_locked(::combat_robot_msgs::msg::TargetPoint::_is_locked_type arg)
  {
    msg_.is_locked = std::move(arg);
    return Init_TargetPoint_x(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

class Init_TargetPoint_header
{
public:
  Init_TargetPoint_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_TargetPoint_is_locked header(::combat_robot_msgs::msg::TargetPoint::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_TargetPoint_is_locked(msg_);
  }

private:
  ::combat_robot_msgs::msg::TargetPoint msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::TargetPoint>()
{
  return combat_robot_msgs::msg::builder::Init_TargetPoint_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__BUILDER_HPP_
