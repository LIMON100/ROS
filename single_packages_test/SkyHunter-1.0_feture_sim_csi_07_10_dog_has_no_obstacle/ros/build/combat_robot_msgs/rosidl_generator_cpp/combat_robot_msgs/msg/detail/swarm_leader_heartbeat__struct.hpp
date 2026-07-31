// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/SwarmLeaderHeartbeat.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/swarm_leader_heartbeat.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__SwarmLeaderHeartbeat __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__SwarmLeaderHeartbeat __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct SwarmLeaderHeartbeat_
{
  using Type = SwarmLeaderHeartbeat_<ContainerAllocator>;

  explicit SwarmLeaderHeartbeat_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->sequence = 0ul;
      this->leader_robot_id = 0ul;
      this->operation_mode = 0;
      this->estop_active = false;
      this->formation_type = 0;
      this->formation_number = 0;
      this->grouping_index = 0;
      this->selected_robot_count = 0;
      std::fill<typename std::array<uint32_t, 8>::iterator, uint32_t>(this->selected_robot_ids.begin(), this->selected_robot_ids.end(), 0ul);
    }
  }

  explicit SwarmLeaderHeartbeat_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init),
    selected_robot_ids(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->sequence = 0ul;
      this->leader_robot_id = 0ul;
      this->operation_mode = 0;
      this->estop_active = false;
      this->formation_type = 0;
      this->formation_number = 0;
      this->grouping_index = 0;
      this->selected_robot_count = 0;
      std::fill<typename std::array<uint32_t, 8>::iterator, uint32_t>(this->selected_robot_ids.begin(), this->selected_robot_ids.end(), 0ul);
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _sequence_type =
    uint32_t;
  _sequence_type sequence;
  using _leader_robot_id_type =
    uint32_t;
  _leader_robot_id_type leader_robot_id;
  using _operation_mode_type =
    uint8_t;
  _operation_mode_type operation_mode;
  using _estop_active_type =
    bool;
  _estop_active_type estop_active;
  using _formation_type_type =
    uint8_t;
  _formation_type_type formation_type;
  using _formation_number_type =
    uint8_t;
  _formation_number_type formation_number;
  using _grouping_index_type =
    uint8_t;
  _grouping_index_type grouping_index;
  using _selected_robot_count_type =
    uint8_t;
  _selected_robot_count_type selected_robot_count;
  using _selected_robot_ids_type =
    std::array<uint32_t, 8>;
  _selected_robot_ids_type selected_robot_ids;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__sequence(
    const uint32_t & _arg)
  {
    this->sequence = _arg;
    return *this;
  }
  Type & set__leader_robot_id(
    const uint32_t & _arg)
  {
    this->leader_robot_id = _arg;
    return *this;
  }
  Type & set__operation_mode(
    const uint8_t & _arg)
  {
    this->operation_mode = _arg;
    return *this;
  }
  Type & set__estop_active(
    const bool & _arg)
  {
    this->estop_active = _arg;
    return *this;
  }
  Type & set__formation_type(
    const uint8_t & _arg)
  {
    this->formation_type = _arg;
    return *this;
  }
  Type & set__formation_number(
    const uint8_t & _arg)
  {
    this->formation_number = _arg;
    return *this;
  }
  Type & set__grouping_index(
    const uint8_t & _arg)
  {
    this->grouping_index = _arg;
    return *this;
  }
  Type & set__selected_robot_count(
    const uint8_t & _arg)
  {
    this->selected_robot_count = _arg;
    return *this;
  }
  Type & set__selected_robot_ids(
    const std::array<uint32_t, 8> & _arg)
  {
    this->selected_robot_ids = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmLeaderHeartbeat
    std::shared_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__SwarmLeaderHeartbeat
    std::shared_ptr<combat_robot_msgs::msg::SwarmLeaderHeartbeat_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SwarmLeaderHeartbeat_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->sequence != other.sequence) {
      return false;
    }
    if (this->leader_robot_id != other.leader_robot_id) {
      return false;
    }
    if (this->operation_mode != other.operation_mode) {
      return false;
    }
    if (this->estop_active != other.estop_active) {
      return false;
    }
    if (this->formation_type != other.formation_type) {
      return false;
    }
    if (this->formation_number != other.formation_number) {
      return false;
    }
    if (this->grouping_index != other.grouping_index) {
      return false;
    }
    if (this->selected_robot_count != other.selected_robot_count) {
      return false;
    }
    if (this->selected_robot_ids != other.selected_robot_ids) {
      return false;
    }
    return true;
  }
  bool operator!=(const SwarmLeaderHeartbeat_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SwarmLeaderHeartbeat_

// alias to use template instance with default allocator
using SwarmLeaderHeartbeat =
  combat_robot_msgs::msg::SwarmLeaderHeartbeat_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__SWARM_LEADER_HEARTBEAT__STRUCT_HPP_
