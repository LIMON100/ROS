// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/GnssStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/gnss_status.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__STRUCT_HPP_

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
# define DEPRECATED__combat_robot_msgs__msg__GnssStatus __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__GnssStatus __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct GnssStatus_
{
  using Type = GnssStatus_<ContainerAllocator>;

  explicit GnssStatus_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->fix_status = 0;
      this->num_satellites = 0;
      this->latitude = 0.0;
      this->longitude = 0.0;
      this->altitude_m = 0.0;
      this->heading_deg = 0.0f;
      this->ground_speed_mps = 0.0f;
      this->horizontal_accuracy_m = 0.0f;
      this->vertical_accuracy_m = 0.0f;
    }
  }

  explicit GnssStatus_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  : header(_alloc, _init)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->fix_status = 0;
      this->num_satellites = 0;
      this->latitude = 0.0;
      this->longitude = 0.0;
      this->altitude_m = 0.0;
      this->heading_deg = 0.0f;
      this->ground_speed_mps = 0.0f;
      this->horizontal_accuracy_m = 0.0f;
      this->vertical_accuracy_m = 0.0f;
    }
  }

  // field types and members
  using _header_type =
    std_msgs::msg::Header_<ContainerAllocator>;
  _header_type header;
  using _fix_status_type =
    uint8_t;
  _fix_status_type fix_status;
  using _num_satellites_type =
    uint8_t;
  _num_satellites_type num_satellites;
  using _latitude_type =
    double;
  _latitude_type latitude;
  using _longitude_type =
    double;
  _longitude_type longitude;
  using _altitude_m_type =
    double;
  _altitude_m_type altitude_m;
  using _heading_deg_type =
    float;
  _heading_deg_type heading_deg;
  using _ground_speed_mps_type =
    float;
  _ground_speed_mps_type ground_speed_mps;
  using _horizontal_accuracy_m_type =
    float;
  _horizontal_accuracy_m_type horizontal_accuracy_m;
  using _vertical_accuracy_m_type =
    float;
  _vertical_accuracy_m_type vertical_accuracy_m;

  // setters for named parameter idiom
  Type & set__header(
    const std_msgs::msg::Header_<ContainerAllocator> & _arg)
  {
    this->header = _arg;
    return *this;
  }
  Type & set__fix_status(
    const uint8_t & _arg)
  {
    this->fix_status = _arg;
    return *this;
  }
  Type & set__num_satellites(
    const uint8_t & _arg)
  {
    this->num_satellites = _arg;
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
  Type & set__altitude_m(
    const double & _arg)
  {
    this->altitude_m = _arg;
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
  Type & set__horizontal_accuracy_m(
    const float & _arg)
  {
    this->horizontal_accuracy_m = _arg;
    return *this;
  }
  Type & set__vertical_accuracy_m(
    const float & _arg)
  {
    this->vertical_accuracy_m = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t FIX_NONE =
    0u;
  static constexpr uint8_t FIX_2D =
    1u;
  static constexpr uint8_t FIX_3D =
    2u;
  static constexpr uint8_t FIX_DGPS =
    3u;
  static constexpr uint8_t FIX_RTK_FLOAT =
    4u;
  static constexpr uint8_t FIX_RTK_FIXED =
    5u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::GnssStatus_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::GnssStatus_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::GnssStatus_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::GnssStatus_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__GnssStatus
    std::shared_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__GnssStatus
    std::shared_ptr<combat_robot_msgs::msg::GnssStatus_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const GnssStatus_ & other) const
  {
    if (this->header != other.header) {
      return false;
    }
    if (this->fix_status != other.fix_status) {
      return false;
    }
    if (this->num_satellites != other.num_satellites) {
      return false;
    }
    if (this->latitude != other.latitude) {
      return false;
    }
    if (this->longitude != other.longitude) {
      return false;
    }
    if (this->altitude_m != other.altitude_m) {
      return false;
    }
    if (this->heading_deg != other.heading_deg) {
      return false;
    }
    if (this->ground_speed_mps != other.ground_speed_mps) {
      return false;
    }
    if (this->horizontal_accuracy_m != other.horizontal_accuracy_m) {
      return false;
    }
    if (this->vertical_accuracy_m != other.vertical_accuracy_m) {
      return false;
    }
    return true;
  }
  bool operator!=(const GnssStatus_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct GnssStatus_

// alias to use template instance with default allocator
using GnssStatus =
  combat_robot_msgs::msg::GnssStatus_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t GnssStatus_<ContainerAllocator>::FIX_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t GnssStatus_<ContainerAllocator>::FIX_2D;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t GnssStatus_<ContainerAllocator>::FIX_3D;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t GnssStatus_<ContainerAllocator>::FIX_DGPS;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t GnssStatus_<ContainerAllocator>::FIX_RTK_FLOAT;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t GnssStatus_<ContainerAllocator>::FIX_RTK_FIXED;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__GNSS_STATUS__STRUCT_HPP_
