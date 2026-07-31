// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/TargetPoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/target_point.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__STRUCT_HPP_

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
// Member 'box'
#include "combat_robot_msgs/msg/detail/bounding_box2d__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__TargetPoint __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__TargetPoint __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct TargetPoint_
{
  using Type = TargetPoint_<ContainerAllocator>;

  explicit TargetPoint_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init),
    box(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->is_locked = false;
      this->x = 0.0;
      this->y = 0.0;
      this->height = 0.0f;
      this->class_id = 0;
      this->track_id = 0l;
    }
  }

  explicit TargetPoint_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init),
    box(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->is_locked = false;
      this->x = 0.0;
      this->y = 0.0;
      this->height = 0.0f;
      this->class_id = 0;
      this->track_id = 0l;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _is_locked_type =
    bool;
  _is_locked_type is_locked;
  using _x_type =
    double;
  _x_type x;
  using _y_type =
    double;
  _y_type y;
  using _height_type =
    float;
  _height_type height;
  using _class_id_type =
    uint8_t;
  _class_id_type class_id;
  using _box_type =
    combat_robot_msgs::msg::BoundingBox2d_<ContainerAllocator>;
  _box_type box;
  using _track_id_type =
    int32_t;
  _track_id_type track_id;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__is_locked(
    const bool & _arg)
  {
    this->is_locked = _arg;
    return *this;
  }
  Type & set__x(
    const double & _arg)
  {
    this->x = _arg;
    return *this;
  }
  Type & set__y(
    const double & _arg)
  {
    this->y = _arg;
    return *this;
  }
  Type & set__height(
    const float & _arg)
  {
    this->height = _arg;
    return *this;
  }
  Type & set__class_id(
    const uint8_t & _arg)
  {
    this->class_id = _arg;
    return *this;
  }
  Type & set__box(
    const combat_robot_msgs::msg::BoundingBox2d_<ContainerAllocator> & _arg)
  {
    this->box = _arg;
    return *this;
  }
  Type & set__track_id(
    const int32_t & _arg)
  {
    this->track_id = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::TargetPoint_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::TargetPoint_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::TargetPoint_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::TargetPoint_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__TargetPoint
    std::shared_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__TargetPoint
    std::shared_ptr<combat_robot_msgs::msg::TargetPoint_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const TargetPoint_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->is_locked != other.is_locked) {
      return false;
    }
    if (this->x != other.x) {
      return false;
    }
    if (this->y != other.y) {
      return false;
    }
    if (this->height != other.height) {
      return false;
    }
    if (this->class_id != other.class_id) {
      return false;
    }
    if (this->box != other.box) {
      return false;
    }
    if (this->track_id != other.track_id) {
      return false;
    }
    return true;
  }
  bool operator!=(const TargetPoint_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct TargetPoint_

// alias to use template instance with default allocator
using TargetPoint =
  combat_robot_msgs::msg::TargetPoint_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__TARGET_POINT__STRUCT_HPP_
