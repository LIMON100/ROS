// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/TouchTargetPoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/touch_target_point.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/touch_target_point__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_TouchTargetPoint_touch_y
{
public:
  explicit Init_TouchTargetPoint_touch_y(::combat_robot_msgs::msg::TouchTargetPoint & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::TouchTargetPoint touch_y(::combat_robot_msgs::msg::TouchTargetPoint::_touch_y_type arg)
  {
    msg_.touch_y = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::TouchTargetPoint msg_;
};

class Init_TouchTargetPoint_touch_x
{
public:
  Init_TouchTargetPoint_touch_x()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_TouchTargetPoint_touch_y touch_x(::combat_robot_msgs::msg::TouchTargetPoint::_touch_x_type arg)
  {
    msg_.touch_x = std::move(arg);
    return Init_TouchTargetPoint_touch_y(msg_);
  }

private:
  ::combat_robot_msgs::msg::TouchTargetPoint msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::TouchTargetPoint>()
{
  return combat_robot_msgs::msg::builder::Init_TouchTargetPoint_touch_x();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__TOUCH_TARGET_POINT__BUILDER_HPP_
