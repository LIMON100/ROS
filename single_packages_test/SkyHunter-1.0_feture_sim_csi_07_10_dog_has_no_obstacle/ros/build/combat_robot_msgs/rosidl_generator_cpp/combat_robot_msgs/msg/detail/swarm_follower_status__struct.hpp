// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/SwarmFollowerStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_follower_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__STRUCT_HPP_

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

#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__SwarmFollowerStatus __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__SwarmFollowerStatus __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct SwarmFollowerStatus_
{
  using Type = SwarmFollowerStatus_<ContainerAllocator>;

  explicit SwarmFollowerStatus_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->robot_id = 0ul;
      this->leader_robot_id = 0ul;
      this->link_status = 0;
      this->last_heartbeat_sequence = 0ul;
      this->heartbeat_age_sec = 0.0f;
      this->last_operation_mode = 0;
      this->last_formation_type = 0;
      this->last_formation_number = 0;
      this->last_grouping_index = 0;
      this->latitude = 0.0;
      this->longitude = 0.0;
      this->heading_deg = 0.0f;
      this->ground_speed_mps = 0.0f;
    }
  }

  explicit SwarmFollowerStatus_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->robot_id = 0ul;
      this->leader_robot_id = 0ul;
      this->link_status = 0;
      this->last_heartbeat_sequence = 0ul;
      this->heartbeat_age_sec = 0.0f;
      this->last_operation_mode = 0;
      this->last_formation_type = 0;
      this->last_formation_number = 0;
      this->last_grouping_index = 0;
      this->latitude = 0.0;
      this->longitude = 0.0;
      this->heading_deg = 0.0f;
      this->ground_speed_mps = 0.0f;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _robot_id_type =
    uint32_t;
  _robot_id_type robot_id;
  using _leader_robot_id_type =
    uint32_t;
  _leader_robot_id_type leader_robot_id;
  using _link_status_type =
    uint8_t;
  _link_status_type link_status;
  using _last_heartbeat_sequence_type =
    uint32_t;
  _last_heartbeat_sequence_type last_heartbeat_sequence;
  using _heartbeat_age_sec_type =
    float;
  _heartbeat_age_sec_type heartbeat_age_sec;
  using _last_operation_mode_type =
    uint8_t;
  _last_operation_mode_type last_operation_mode;
  using _last_formation_type_type =
    uint8_t;
  _last_formation_type_type last_formation_type;
  using _last_formation_number_type =
    uint8_t;
  _last_formation_number_type last_formation_number;
  using _last_grouping_index_type =
    uint8_t;
  _last_grouping_index_type last_grouping_index;
  using _latitude_type =
    double;
  _latitude_type latitude;
  using _longitude_type =
    double;
  _longitude_type longitude;
  using _heading_deg_type =
    float;
  _heading_deg_type heading_deg;
  using _ground_speed_mps_type =
    float;
  _ground_speed_mps_type ground_speed_mps;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__robot_id(
    const uint32_t & _arg)
  {
    this->robot_id = _arg;
    return *this;
  }
  Type & set__leader_robot_id(
    const uint32_t & _arg)
  {
    this->leader_robot_id = _arg;
    return *this;
  }
  Type & set__link_status(
    const uint8_t & _arg)
  {
    this->link_status = _arg;
    return *this;
  }
  Type & set__last_heartbeat_sequence(
    const uint32_t & _arg)
  {
    this->last_heartbeat_sequence = _arg;
    return *this;
  }
  Type & set__heartbeat_age_sec(
    const float & _arg)
  {
    this->heartbeat_age_sec = _arg;
    return *this;
  }
  Type & set__last_operation_mode(
    const uint8_t & _arg)
  {
    this->last_operation_mode = _arg;
    return *this;
  }
  Type & set__last_formation_type(
    const uint8_t & _arg)
  {
    this->last_formation_type = _arg;
    return *this;
  }
  Type & set__last_formation_number(
    const uint8_t & _arg)
  {
    this->last_formation_number = _arg;
    return *this;
  }
  Type & set__last_grouping_index(
    const uint8_t & _arg)
  {
    this->last_grouping_index = _arg;
    return *this;
  }
  Type & set__latitude(
    const double & _arg)
  {
    this->latitude = _arg;
    return *this;
  }
  Type & set__longitude(
    const double & _arg)
  {
    this->longitude = _arg;
    return *this;
  }
  Type & set__heading_deg(
    const float & _arg)
  {
    this->heading_deg = _arg;
    return *this;
  }
  Type & set__ground_speed_mps(
    const float & _arg)
  {
    this->ground_speed_mps = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t LINK_DISCONNECTED =
    0u;
  static constexpr uint8_t LINK_CONNECTED =
    1u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmFollowerStatus
    std::shared_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmFollowerStatus
    std::shared_ptr<combat_robot_msgs::msg::SwarmFollowerStatus_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SwarmFollowerStatus_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->robot_id != other.robot_id) {
      return false;
    }
    if (this->leader_robot_id != other.leader_robot_id) {
      return false;
    }
    if (this->link_status != other.link_status) {
      return false;
    }
    if (this->last_heartbeat_sequence != other.last_heartbeat_sequence) {
      return false;
    }
    if (this->heartbeat_age_sec != other.heartbeat_age_sec) {
      return false;
    }
    if (this->last_operation_mode != other.last_operation_mode) {
      return false;
    }
    if (this->last_formation_type != other.last_formation_type) {
      return false;
    }
    if (this->last_formation_number != other.last_formation_number) {
      return false;
    }
    if (this->last_grouping_index != other.last_grouping_index) {
      return false;
    }
    if (this->latitude != other.latitude) {
      return false;
    }
    if (this->longitude != other.longitude) {
      return false;
    }
    if (this->heading_deg != other.heading_deg) {
      return false;
    }
    if (this->ground_speed_mps != other.ground_speed_mps) {
      return false;
    }
    return true;
  }
  bool operator!=(const SwarmFollowerStatus_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SwarmFollowerStatus_

// alias to use template instance with default allocator
using SwarmFollowerStatus =
  combat_robot_msgs::msg::SwarmFollowerStatus_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmFollowerStatus_<ContainerAllocator>::LINK_DISCONNECTED;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t SwarmFollowerStatus_<ContainerAllocator>::LINK_CONNECTED;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_FOLLOWER_STATUS__STRUCT_HPP_
