// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from skyhunter_msgs:msg/SwarmHeartbeat.idl
// generated code does not contain a copyright notice

#ifndef SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__STRUCT_HPP_
#define SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__STRUCT_HPP_

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
# define DEPRECATED__skyhunter_msgs__msg__SwarmHeartbeat __attribute__((deprecated))
#else
# define DEPRECATED__skyhunter_msgs__msg__SwarmHeartbeat __declspec(deprecated)
#endif

namespace skyhunter_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct SwarmHeartbeat_
{
  using Type = SwarmHeartbeat_<ContainerAllocator>;

  explicit SwarmHeartbeat_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->robot_id = "";
      this->term = 0ul;
      this->is_leader = false;
      this->battery_level = 0.0f;
      this->leader_id_num = 0l;
    }
  }

  explicit SwarmHeartbeat_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init),
    robot_id(_alloc)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->robot_id = "";
      this->term = 0ul;
      this->is_leader = false;
      this->battery_level = 0.0f;
      this->leader_id_num = 0l;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _robot_id_type =
    std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>>;
  _robot_id_type robot_id;
  using _term_type =
    uint32_t;
  _term_type term;
  using _is_leader_type =
    bool;
  _is_leader_type is_leader;
  using _battery_level_type =
    float;
  _battery_level_type battery_level;
  using _leader_id_num_type =
    int32_t;
  _leader_id_num_type leader_id_num;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__robot_id(
    const std::basic_string<char, std::char_traits<char>, typename std::allocator_traits<ContainerAllocator>::template rebind_alloc<char>> & _arg)
  {
    this->robot_id = _arg;
    return *this;
  }
  Type & set__term(
    const uint32_t & _arg)
  {
    this->term = _arg;
    return *this;
  }
  Type & set__is_leader(
    const bool & _arg)
  {
    this->is_leader = _arg;
    return *this;
  }
  Type & set__battery_level(
    const float & _arg)
  {
    this->battery_level = _arg;
    return *this;
  }
  Type & set__leader_id_num(
    const int32_t & _arg)
  {
    this->leader_id_num = _arg;
    return *this;
  }

  // constant declarations

  // pointer types
  using RawPtr =
    skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator> *;
  using ConstRawPtr =
    const skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__skyhunter_msgs__msg__SwarmHeartbeat
    std::shared_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__skyhunter_msgs__msg__SwarmHeartbeat
    std::shared_ptr<skyhunter_msgs::msg::SwarmHeartbeat_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const SwarmHeartbeat_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->robot_id != other.robot_id) {
      return false;
    }
    if (this->term != other.term) {
      return false;
    }
    if (this->is_leader != other.is_leader) {
      return false;
    }
    if (this->battery_level != other.battery_level) {
      return false;
    }
    if (this->leader_id_num != other.leader_id_num) {
      return false;
    }
    return true;
  }
  bool operator!=(const SwarmHeartbeat_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct SwarmHeartbeat_

// alias to use template instance with default allocator
using SwarmHeartbeat =
  skyhunter_msgs::msg::SwarmHeartbeat_<std::allocator<void>>;

// constant definitions

}  // namespace msg

}  // namespace skyhunter_msgs

#endif  // SKYHUNTER_MSGS__MSG__DETAIL__SWARM_HEARTBEAT__STRUCT_HPP_
