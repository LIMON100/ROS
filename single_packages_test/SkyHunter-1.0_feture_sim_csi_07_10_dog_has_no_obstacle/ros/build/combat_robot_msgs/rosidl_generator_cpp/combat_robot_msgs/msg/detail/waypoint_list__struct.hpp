// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/WaypointList.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/waypoint_list.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


// Include directives for member types
// Member 'waypoints'
#include "combat_robot_msgs/msg/detail/waypoint__struct.hpp"

#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__WaypointList __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__WaypointList __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct WaypointList_
{
  using Type = WaypointList_<ContainerAllocator>;

  explicit WaypointList_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->mode = 0l;
      this->formation = 0l;
      this->mission_id = 0l;
      this->mission_status = 0l;
    }
  }

  explicit WaypointList_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    (void)_alloc;
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->mode = 0l;
      this->formation = 0l;
      this->mission_id = 0l;
      this->mission_status = 0l;
    }
  }

  // field types and members
  using _mode_type =
    int32_t;
  _mode_type mode;
  using _formation_type =
    int32_t;
  _formation_type formation;
  using _mission_id_type =
    int32_t;
  _mission_id_type mission_id;
  using _mission_status_type =
    int32_t;
  _mission_status_type mission_status;
  using _waypoints_type =
    std::vector<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>>>;
  _waypoints_type waypoints;

  // setters for named parameter idiom
  Type & set__mode(
    const int32_t & _arg)
  {
    this->mode = _arg;
    return *this;
  }
  Type & set__formation(
    const int32_t & _arg)
  {
    this->formation = _arg;
    return *this;
  }
  Type & set__mission_id(
    const int32_t & _arg)
  {
    this->mission_id = _arg;
    return *this;
  }
  Type & set__mission_status(
    const int32_t & _arg)
  {
    this->mission_status = _arg;
    return *this;
  }
  Type & set__waypoints(
    const std::vector<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<combat_robot_msgs::msg::Waypoint_<ContainerAllocator>>> & _arg)
  {
    this->waypoints = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::WaypointList_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::WaypointList_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::WaypointList_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::WaypointList_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__WaypointList
    std::shared_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__WaypointList
    std::shared_ptr<combat_robot_msgs::msg::WaypointList_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const WaypointList_ & other) const
  {
    if (this->mode != other.mode) {
      return false;
    }
    if (this->formation != other.formation) {
      return false;
    }
    if (this->mission_id != other.mission_id) {
      return false;
    }
    if (this->mission_status != other.mission_status) {
      return false;
    }
    if (this->waypoints != other.waypoints) {
      return false;
    }
    return true;
  }
  bool operator!=(const WaypointList_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct WaypointList_

// alias to use template instance with default allocator
using WaypointList =
  combat_robot_msgs::msg::WaypointList_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__WAYPOINT_LIST__STRUCT_HPP_
