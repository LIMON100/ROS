// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/DetectedObjects.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/detected_objects.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECTS__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECTS__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/detected_objects__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_DetectedObjects_objects
{
public:
  explicit Init_DetectedObjects_objects(::combat_robot_msgs::msg::DetectedObjects & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::DetectedObjects objects(::combat_robot_msgs::msg::DetectedObjects::_objects_type arg)
  {
    msg_.objects = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::DetectedObjects msg_;
};

class Init_DetectedObjects_image_height
{
public:
  explicit Init_DetectedObjects_image_height(::combat_robot_msgs::msg::DetectedObjects & msg)
  : msg_(msg)
  {}
  Init_DetectedObjects_objects image_height(::combat_robot_msgs::msg::DetectedObjects::_image_height_type arg)
  {
    msg_.image_height = std::move(arg);
    return Init_DetectedObjects_objects(msg_);
  }

private:
  ::combat_robot_msgs::msg::DetectedObjects msg_;
};

class Init_DetectedObjects_image_width
{
public:
  explicit Init_DetectedObjects_image_width(::combat_robot_msgs::msg::DetectedObjects & msg)
  : msg_(msg)
  {}
  Init_DetectedObjects_image_height image_width(::combat_robot_msgs::msg::DetectedObjects::_image_width_type arg)
  {
    msg_.image_width = std::move(arg);
    return Init_DetectedObjects_image_height(msg_);
  }

private:
  ::combat_robot_msgs::msg::DetectedObjects msg_;
};

class Init_DetectedObjects_header
{
public:
  Init_DetectedObjects_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_DetectedObjects_image_width header(::combat_robot_msgs::msg::DetectedObjects::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_DetectedObjects_image_width(msg_);
  }

private:
  ::combat_robot_msgs::msg::DetectedObjects msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::DetectedObjects>()
{
  return combat_robot_msgs::msg::builder::Init_DetectedObjects_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__DETECTED_OBJECTS__BUILDER_HPP_
