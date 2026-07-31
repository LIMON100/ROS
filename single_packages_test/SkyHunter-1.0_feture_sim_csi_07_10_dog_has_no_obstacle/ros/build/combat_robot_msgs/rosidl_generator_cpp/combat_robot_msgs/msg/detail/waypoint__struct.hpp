// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/Waypoint.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__Waypoint __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__Waypoint __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct Waypoint_
{
  using Type = Waypoint_<ContainerAllocator>;

  explicit Waypoint_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->way_id = 0l;
      this->way_lon = 0.0;
      this->way_lat = 0.0;
      this->way_status = 0l;
    }
  }

  explicit Waypoint_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    (void)_alloc;
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->way_id = 0l;
      this->way_lon = 0.0;
      this->way_lat = 0.0;
      this->way_status = 0l;
    }
  }

  // field types and members
  using _way_id_type =
    int32_t;
  _way_id_type way_id;
  using _way_lon_type =
    double;
  _way_lon_type way_lon;
  using _way_lat_type =
    double;
  _way_lat_type way_lat;
  using _way_status_type =
    int32_t;
  _way_status_type way_status;

  // setters for named parameter idiom
  Type & set__way_id(
    const int32_t & _arg)
  {
    this->way_id = _arg;
    return *this;
  }
  Type & set__way_lon(
    const double & _arg)
  {
    this->way_lon = _arg;
    return *this;
  }
  Type & set__way_lat(
    const double & _arg)
  {
    this->way_lat = _arg;
    return *this;
  }
  Type & set__way_status(
    const int32_t & _arg)
  {
    this->way_status = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::Waypoint_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::Waypoint_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::Waypoint_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::Waypoint_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__Waypoint
    std::shared_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__Waypoint
    std::shared_ptr<combat_robot_msgs::msg::Waypoint_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const Waypoint_ & other) const
  {
    if (this->way_id != other.way_id) {
      return false;
    }
    if (this->way_lon != other.way_lon) {
      return false;
    }
    if (this->way_lat != other.way_lat) {
      return false;
    }
    if (this->way_status != other.way_status) {
      return false;
    }
    return true;
  }
  bool operator!=(const Waypoint_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct Waypoint_

// alias to use template instance with default allocator
using Waypoint =
  combat_robot_msgs::msg::Waypoint_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT__STRUCT_HPP_
