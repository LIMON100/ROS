// generated from rosidl_generator_cpp/resource/idl__builder.hpp.em
// with input from combat_robot_msgs:msg/CenterObject.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/center_object.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__BUILDER_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__BUILDER_HPP_

#include <algorithm>
#include <utility>

#include "combat_robot_msgs/msg/detail/center_object__struct.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


namespace combat_robot_msgs
{

namespace msg
{

namespace builder
{

class Init_CenterObject_zoom_level
{
public:
  explicit Init_CenterObject_zoom_level(::combat_robot_msgs::msg::CenterObject & msg)
  : msg_(msg)
  {}
  ::combat_robot_msgs::msg::CenterObject zoom_level(::combat_robot_msgs::msg::CenterObject::_zoom_level_type arg)
  {
    msg_.zoom_level = std::move(arg);
    return std::move(msg_);
  }

private:
  ::combat_robot_msgs::msg::CenterObject msg_;
};

class Init_CenterObject_laser_distance
{
public:
  explicit Init_CenterObject_laser_distance(::combat_robot_msgs::msg::CenterObject & msg)
  : msg_(msg)
  {}
  Init_CenterObject_zoom_level laser_distance(::combat_robot_msgs::msg::CenterObject::_laser_distance_type arg)
  {
    msg_.laser_distance = std::move(arg);
    return Init_CenterObject_zoom_level(msg_);
  }

private:
  ::combat_robot_msgs::msg::CenterObject msg_;
};

class Init_CenterObject_target_y
{
public:
  explicit Init_CenterObject_target_y(::combat_robot_msgs::msg::CenterObject & msg)
  : msg_(msg)
  {}
  Init_CenterObject_laser_distance target_y(::combat_robot_msgs::msg::CenterObject::_target_y_type arg)
  {
    msg_.target_y = std::move(arg);
    return Init_CenterObject_laser_distance(msg_);
  }

private:
  ::combat_robot_msgs::msg::CenterObject msg_;
};

class Init_CenterObject_target_x
{
public:
  explicit Init_CenterObject_target_x(::combat_robot_msgs::msg::CenterObject & msg)
  : msg_(msg)
  {}
  Init_CenterObject_target_y target_x(::combat_robot_msgs::msg::CenterObject::_target_x_type arg)
  {
    msg_.target_x = std::move(arg);
    return Init_CenterObject_target_y(msg_);
  }

private:
  ::combat_robot_msgs::msg::CenterObject msg_;
};

class Init_CenterObject_bounding_box
{
public:
  explicit Init_CenterObject_bounding_box(::combat_robot_msgs::msg::CenterObject & msg)
  : msg_(msg)
  {}
  Init_CenterObject_target_x bounding_box(::combat_robot_msgs::msg::CenterObject::_bounding_box_type arg)
  {
    msg_.bounding_box = std::move(arg);
    return Init_CenterObject_target_x(msg_);
  }

private:
  ::combat_robot_msgs::msg::CenterObject msg_;
};

class Init_CenterObject_class_id
{
public:
  explicit Init_CenterObject_class_id(::combat_robot_msgs::msg::CenterObject & msg)
  : msg_(msg)
  {}
  Init_CenterObject_bounding_box class_id(::combat_robot_msgs::msg::CenterObject::_class_id_type arg)
  {
    msg_.class_id = std::move(arg);
    return Init_CenterObject_bounding_box(msg_);
  }

private:
  ::combat_robot_msgs::msg::CenterObject msg_;
};

class Init_CenterObject_header
{
public:
  Init_CenterObject_header()
  : msg_(::rosidl_runtime_cpp::MessageInitialization::SKIP)
  {}
  Init_CenterObject_class_id header(::combat_robot_msgs::msg::CenterObject::_header_type arg)
  {
    msg_.header = std::move(arg);
    return Init_CenterObject_class_id(msg_);
  }

private:
  ::combat_robot_msgs::msg::CenterObject msg_;
};

}  // namespace builder

}  // namespace msg

template<typename MessageType>
auto build();

template<>
inline
auto build<::combat_robot_msgs::msg::CenterObject>()
{
  return combat_robot_msgs::msg::builder::Init_CenterObject_header();
}

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__BUILDER_HPP_
