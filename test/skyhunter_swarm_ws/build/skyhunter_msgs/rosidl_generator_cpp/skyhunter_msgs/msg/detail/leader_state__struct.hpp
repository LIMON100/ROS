// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from skyhunter_msgs:msg/LeaderState.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__STRUCT_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__STRUCT_HPP_

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
// Member 'pose'
#include "geometry_msgs/msg/detail/pose__struct.hpp"
// Member 'velocity'
#include "geometry_msgs/msg/detail/twist__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__skyhunter_msgs__msg__LeaderState __attribute__((deprecated))
#else
# define DEPRECATED__skyhunter_msgs__msg__LeaderState __declspec(deprecated)
#endif

namespace skyhunter_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct LeaderState_
{
  using Type = LeaderState_<ContainerAllocator>;

  explicit LeaderState_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init),
    pose(_init),
    velocity(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->formation_mode = 0;
      this->formation_state = 0;
    }
  }

  explicit LeaderState_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init),
    pose(_alloc, _init),
    velocity(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->formation_mode = 0;
      this->formation_state = 0;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _pose_type =
    geometry_msgs::msg::Pose_<ContainerAllocator>;
  _pose_type pose;
  using _velocity_type =
    geometry_msgs::msg::Twist_<ContainerAllocator>;
  _velocity_type velocity;
  using _formation_mode_type =
    uint8_t;
  _formation_mode_type formation_mode;
  using _formation_state_type =
    uint8_t;
  _formation_state_type formation_state;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__pose(
    const geometry_msgs::msg::Pose_<ContainerAllocator> & _arg)
  {
    this->pose = _arg;
    return *this;
  }
  Type & set__velocity(
    const geometry_msgs::msg::Twist_<ContainerAllocator> & _arg)
  {
    this->velocity = _arg;
    return *this;
  }
  Type & set__formation_mode(
    const uint8_t & _arg)
  {
    this->formation_mode = _arg;
    return *this;
  }
  Type & set__formation_state(
    const uint8_t & _arg)
  {
    this->formation_state = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    skyhunter_msgs::msg::LeaderState_<ContainerAllocator> *;
  using ConstRawPtr =
    const skyhunter_msgs::msg::LeaderState_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      skyhunter_msgs::msg::LeaderState_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      skyhunter_msgs::msg::LeaderState_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__skyhunter_msgs__msg__LeaderState
    std::shared_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__skyhunter_msgs__msg__LeaderState
    std::shared_ptr<skyhunter_msgs::msg::LeaderState_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const LeaderState_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->pose != other.pose) {
      return false;
    }
    if (this->velocity != other.velocity) {
      return false;
    }
    if (this->formation_mode != other.formation_mode) {
      return false;
    }
    if (this->formation_state != other.formation_state) {
      return false;
    }
    return true;
  }
  bool operator!=(const LeaderState_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct LeaderState_

// alias to use template instance with default allocator
using LeaderState =
  skyhunter_msgs::msg::LeaderState_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace skyhunter_msgs

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__LEADER_STATE__STRUCT_HPP_
