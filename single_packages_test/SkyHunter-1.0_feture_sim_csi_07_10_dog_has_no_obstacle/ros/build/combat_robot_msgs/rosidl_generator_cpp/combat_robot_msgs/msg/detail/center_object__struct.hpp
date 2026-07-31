// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/CenterObject.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/center_object.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.hpp"
// Member 'bounding_box'
#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__CenterObject __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__CenterObject __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct CenterObject_
{
  using Type = CenterObject_<ContainerAllocator>;

  explicit CenterObject_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init),
    bounding_box(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->class_id = 0l;
      this->target_x = 0.0f;
      this->target_y = 0.0f;
      this->laser_distance = 0.0f;
      this->zoom_level = 0.0f;
    }
  }

  explicit CenterObject_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init),
    bounding_box(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->class_id = 0l;
      this->target_x = 0.0f;
      this->target_y = 0.0f;
      this->laser_distance = 0.0f;
      this->zoom_level = 0.0f;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _class_id_type =
    int32_t;
  _class_id_type class_id;
  using _bounding_box_type =
    combat_robot_msgs::msg::BoundingBox2d_<ContainerAllocator>;
  _bounding_box_type bounding_box;
  using _target_x_type =
    float;
  _target_x_type target_x;
  using _target_y_type =
    float;
  _target_y_type target_y;
  using _laser_distance_type =
    float;
  _laser_distance_type laser_distance;
  using _zoom_level_type =
    float;
  _zoom_level_type zoom_level;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__class_id(
    const int32_t & _arg)
  {
    this->class_id = _arg;
    return *this;
  }
  Type & set__bounding_box(
    const combat_robot_msgs::msg::BoundingBox2d_<ContainerAllocator> & _arg)
  {
    this->bounding_box = _arg;
    return *this;
  }
  Type & set__target_x(
    const float & _arg)
  {
    this->target_x = _arg;
    return *this;
  }
  Type & set__target_y(
    const float & _arg)
  {
    this->target_y = _arg;
    return *this;
  }
  Type & set__laser_distance(
    const float & _arg)
  {
    this->laser_distance = _arg;
    return *this;
  }
  Type & set__zoom_level(
    const float & _arg)
  {
    this->zoom_level = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::CenterObject_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::CenterObject_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::CenterObject_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::CenterObject_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__CenterObject
    std::shared_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__CenterObject
    std::shared_ptr<combat_robot_msgs::msg::CenterObject_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const CenterObject_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->class_id != other.class_id) {
      return false;
    }
    if (this->bounding_box != other.bounding_box) {
      return false;
    }
    if (this->target_x != other.target_x) {
      return false;
    }
    if (this->target_y != other.target_y) {
      return false;
    }
    if (this->laser_distance != other.laser_distance) {
      return false;
    }
    if (this->zoom_level != other.zoom_level) {
      return false;
    }
    return true;
  }
  bool operator!=(const CenterObject_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct CenterObject_

// alias to use template instance with default allocator
using CenterObject =
  combat_robot_msgs::msg::CenterObject_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__CENTER_OBJECT__STRUCT_HPP_
