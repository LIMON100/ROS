// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/DetectedObject.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/detected_object.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECT__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/detected_object__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_DetectedObject_box
{
public:
  explicit Init_DetectedObject_box(::combat_robot_msgs::msg::DetectedObject & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::DetectedObject box(::combat_robot_msgs::msg::DetectedObject::_box_type arg)
  {
    msg_.box = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::DetectedObject msg_;
};

class Init_DetectedObject_prob
{
public:
  explicit Init_DetectedObject_prob(::combat_robot_msgs::msg::DetectedObject & msg)
  : msg_(msg)
  {}
  Init_DetectedObject_box prob(::combat_robot_msgs::msg::DetectedObject::_prob_type arg)
  {
    msg_.prob = std::move(arg);
    return Init_DetectedObject_box(msg_);
  }

private:
  ::combat_robot_msgs::msg::DetectedObject msg_;
};

class Init_DetectedObject_id
{
public:
  Init_DetectedObject_id()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_DetectedObject_prob id(::combat_robot_msgs::msg::DetectedObject::_id_type arg)
  {
    msg_.id = std::move(arg);
    return Init_DetectedObject_prob(msg_);
  }

private:
  ::combat_robot_msgs::msg::DetectedObject msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::DetectedObject>()
{
  return combat_robot_msgs::msg::builder::Init_DetectedObject_id();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECT__BUILDER_HPP_
