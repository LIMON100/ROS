// generated from rosidl_generator_cpp/resource/idl__struct.hpp.em
// with input from combat_robot_msgs:msg/OperationState.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/operation_state.hpp"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__STRUCT_HPP_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__STRUCT_HPP_

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "rosidl_runtime_cpp/bounded_vector.hpp"
#include "rosidl_runtime_cpp/message_initialization.hpp"


#ifndef _WIN32
# define DEPRECATED__combat_robot_msgs__msg__OperationState __attribute__((deprecated))
#else
# define DEPRECATED__combat_robot_msgs__msg__OperationState __declspec(deprecated)
#endif

namespace combat_robot_msgs
{

namespace msg
{

// message struct
template<class ContainerAllocator>
struct OperationState_
{
  using Type = OperationState_<ContainerAllocator>;

  explicit OperationState_(rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->state = 0;
      this->active_mode_id = 0;
      this->mission_status = 0;
      this->estop_active = false;
      this->permission_request_active = false;
      this->crosshair_x = 0.0f;
      this->crosshair_y = 0.0f;
      this->current_zoom_level = 0.0f;
      this->gps_lat = 0.0;
      this->gps_lon = 0.0;
      this->gps_heading = 0.0f;
      this->current_speed_mps = 0.0f;
      this->current_waypoint_index = 0;
      this->total_waypoints = 0;
      this->progress_ratio = 0.0f;
      this->distance_to_next_wp_m = 0.0f;
      this->distance_to_goal_m = 0.0f;
      this->error_code = 0;
    }
  }

  explicit OperationState_(const ContainerAllocator & _alloc, rosidl_runtime_cpp::MessageInitialization _init = rosidl_runtime_cpp::MessageInitialization::ALL)
  {
    (void)_alloc;
    if (rosidl_runtime_cpp::MessageInitialization::ALL == _init ||
      rosidl_runtime_cpp::MessageInitialization::ZERO == _init)
    {
      this->state = 0;
      this->active_mode_id = 0;
      this->mission_status = 0;
      this->estop_active = false;
      this->permission_request_active = false;
      this->crosshair_x = 0.0f;
      this->crosshair_y = 0.0f;
      this->current_zoom_level = 0.0f;
      this->gps_lat = 0.0;
      this->gps_lon = 0.0;
      this->gps_heading = 0.0f;
      this->current_speed_mps = 0.0f;
      this->current_waypoint_index = 0;
      this->total_waypoints = 0;
      this->progress_ratio = 0.0f;
      this->distance_to_next_wp_m = 0.0f;
      this->distance_to_goal_m = 0.0f;
      this->error_code = 0;
    }
  }

  // field types and members
  using _state_type =
    uint8_t;
  _state_type state;
  using _active_mode_id_type =
    uint8_t;
  _active_mode_id_type active_mode_id;
  using _mission_status_type =
    uint8_t;
  _mission_status_type mission_status;
  using _estop_active_type =
    bool;
  _estop_active_type estop_active;
  using _permission_request_active_type =
    bool;
  _permission_request_active_type permission_request_active;
  using _crosshair_x_type =
    float;
  _crosshair_x_type crosshair_x;
  using _crosshair_y_type =
    float;
  _crosshair_y_type crosshair_y;
  using _current_zoom_level_type =
    float;
  _current_zoom_level_type current_zoom_level;
  using _gps_lat_type =
    double;
  _gps_lat_type gps_lat;
  using _gps_lon_type =
    double;
  _gps_lon_type gps_lon;
  using _gps_heading_type =
    float;
  _gps_heading_type gps_heading;
  using _current_speed_mps_type =
    float;
  _current_speed_mps_type current_speed_mps;
  using _current_waypoint_index_type =
    uint16_t;
  _current_waypoint_index_type current_waypoint_index;
  using _total_waypoints_type =
    uint16_t;
  _total_waypoints_type total_waypoints;
  using _progress_ratio_type =
    float;
  _progress_ratio_type progress_ratio;
  using _distance_to_next_wp_m_type =
    float;
  _distance_to_next_wp_m_type distance_to_next_wp_m;
  using _distance_to_goal_m_type =
    float;
  _distance_to_goal_m_type distance_to_goal_m;
  using _error_code_type =
    uint8_t;
  _error_code_type error_code;

  // setters for named parameter idiom
  Type & set__state(
    const uint8_t & _arg)
  {
    this->state = _arg;
    return *this;
  }
  Type & set__active_mode_id(
    const uint8_t & _arg)
  {
    this->active_mode_id = _arg;
    return *this;
  }
  Type & set__mission_status(
    const uint8_t & _arg)
  {
    this->mission_status = _arg;
    return *this;
  }
  Type & set__estop_active(
    const bool & _arg)
  {
    this->estop_active = _arg;
    return *this;
  }
  Type & set__permission_request_active(
    const bool & _arg)
  {
    this->permission_request_active = _arg;
    return *this;
  }
  Type & set__crosshair_x(
    const float & _arg)
  {
    this->crosshair_x = _arg;
    return *this;
  }
  Type & set__crosshair_y(
    const float & _arg)
  {
    this->crosshair_y = _arg;
    return *this;
  }
  Type & set__current_zoom_level(
    const float & _arg)
  {
    this->current_zoom_level = _arg;
    return *this;
  }
  Type & set__gps_lat(
    const double & _arg)
  {
    this->gps_lat = _arg;
    return *this;
  }
  Type & set__gps_lon(
    const double & _arg)
  {
    this->gps_lon = _arg;
    return *this;
  }
  Type & set__gps_heading(
    const float & _arg)
  {
    this->gps_heading = _arg;
    return *this;
  }
  Type & set__current_speed_mps(
    const float & _arg)
  {
    this->current_speed_mps = _arg;
    return *this;
  }
  Type & set__current_waypoint_index(
    const uint16_t & _arg)
  {
    this->current_waypoint_index = _arg;
    return *this;
  }
  Type & set__total_waypoints(
    const uint16_t & _arg)
  {
    this->total_waypoints = _arg;
    return *this;
  }
  Type & set__progress_ratio(
    const float & _arg)
  {
    this->progress_ratio = _arg;
    return *this;
  }
  Type & set__distance_to_next_wp_m(
    const float & _arg)
  {
    this->distance_to_next_wp_m = _arg;
    return *this;
  }
  Type & set__distance_to_goal_m(
    const float & _arg)
  {
    this->distance_to_goal_m = _arg;
    return *this;
  }
  Type & set__error_code(
    const uint8_t & _arg)
  {
    this->error_code = _arg;
    return *this;
  }

  // constant declarations
  static constexpr uint8_t INIT =
    0u;
  static constexpr uint8_t IDLE =
    1u;
  static constexpr uint8_t MOVE =
    2u;
  static constexpr uint8_t SURVEILLANCE =
    3u;
  static constexpr uint8_t DRONE_SURVEILLANCE =
    4u;
  static constexpr uint8_t MANUAL_ATTACK =
    5u;
  static constexpr uint8_t ASSAULT =
    6u;
  static constexpr uint8_t TRACKING =
    7u;
  static constexpr uint8_t EMERGENCY_STOP =
    8u;
  // guard against 'ERROR' being predefined by MSVC by temporarily undefining it
#if defined(_WIN32)
#  if defined(ERROR)
#    pragma push_macro("ERROR")
#    undef ERROR
#  endif
#endif
  static constexpr uint8_t ERROR =
    9u;
#if defined(_WIN32)
#  pragma warning(suppress : 4602)
#  pragma pop_macro("ERROR")
#endif
  static constexpr uint8_t ACTIVE_MODE_IDLE =
    0u;
  static constexpr uint8_t ACTIVE_MODE_RECON =
    1u;
  static constexpr uint8_t ACTIVE_MODE_PROTECT_GENERAL =
    2u;
  static constexpr uint8_t ACTIVE_MODE_PROTECT_DRONE =
    3u;
  static constexpr uint8_t ACTIVE_MODE_ASSAULT =
    6u;
  static constexpr uint8_t ACTIVE_MODE_RETURN_TO_HOME =
    7u;
  static constexpr uint8_t ACTIVE_MODE_ESTOP =
    8u;
  static constexpr uint8_t MISSION_NONE =
    0u;
  static constexpr uint8_t MISSION_READY =
    1u;
  static constexpr uint8_t MISSION_MOVING =
    2u;
  static constexpr uint8_t MISSION_PAUSED =
    3u;
  static constexpr uint8_t MISSION_REACHED =
    4u;
  static constexpr uint8_t MISSION_SURVEILLING =
    5u;
  static constexpr uint8_t MISSION_ERROR =
    6u;

  // pointer types
  using RawPtr =
    combat_robot_msgs::msg::OperationState_<ContainerAllocator> *;
  using ConstRawPtr =
    const combat_robot_msgs::msg::OperationState_<ContainerAllocator> *;
  using SharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator>>;
  using ConstSharedPtr =
    std::shared_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator> const>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::OperationState_<ContainerAllocator>>>
  using UniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator>, Deleter>;

  using UniquePtr = UniquePtrWithDeleter<>;

  template<typename Deleter = std::default_delete<
      combat_robot_msgs::msg::OperationState_<ContainerAllocator>>>
  using ConstUniquePtrWithDeleter =
    std::unique_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator> const, Deleter>;
  using ConstUniquePtr = ConstUniquePtrWithDeleter<>;

  using WeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator>>;
  using ConstWeakPtr =
    std::weak_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator> const>;

  // pointer types similar to ROS 1, use SharedPtr / ConstSharedPtr instead
  // NOTE: Can't use 'using' here because GNU C++ can't parse attributes properly
  typedef DEPRECATED__combat_robot_msgs__msg__OperationState
    std::shared_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator>>
    Ptr;
  typedef DEPRECATED__combat_robot_msgs__msg__OperationState
    std::shared_ptr<combat_robot_msgs::msg::OperationState_<ContainerAllocator> const>
    ConstPtr;

  // comparison operators
  bool operator==(const OperationState_ & other) const
  {
    if (this->state != other.state) {
      return false;
    }
    if (this->active_mode_id != other.active_mode_id) {
      return false;
    }
    if (this->mission_status != other.mission_status) {
      return false;
    }
    if (this->estop_active != other.estop_active) {
      return false;
    }
    if (this->permission_request_active != other.permission_request_active) {
      return false;
    }
    if (this->crosshair_x != other.crosshair_x) {
      return false;
    }
    if (this->crosshair_y != other.crosshair_y) {
      return false;
    }
    if (this->current_zoom_level != other.current_zoom_level) {
      return false;
    }
    if (this->gps_lat != other.gps_lat) {
      return false;
    }
    if (this->gps_lon != other.gps_lon) {
      return false;
    }
    if (this->gps_heading != other.gps_heading) {
      return false;
    }
    if (this->current_speed_mps != other.current_speed_mps) {
      return false;
    }
    if (this->current_waypoint_index != other.current_waypoint_index) {
      return false;
    }
    if (this->total_waypoints != other.total_waypoints) {
      return false;
    }
    if (this->progress_ratio != other.progress_ratio) {
      return false;
    }
    if (this->distance_to_next_wp_m != other.distance_to_next_wp_m) {
      return false;
    }
    if (this->distance_to_goal_m != other.distance_to_goal_m) {
      return false;
    }
    if (this->error_code != other.error_code) {
      return false;
    }
    return true;
  }
  bool operator!=(const OperationState_ & other) const
  {
    return !this->operator==(other);
  }
};  // struct OperationState_

// alias to use template instance with default allocator
using OperationState =
  combat_robot_msgs::msg::OperationState_<std::allocator<void>>;

// constant definitions
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::INIT;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::IDLE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MOVE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::SURVEILLANCE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::DRONE_SURVEILLANCE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MANUAL_ATTACK;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ASSAULT;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::TRACKING;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::EMERGENCY_STOP;
#endif  // __cplusplus < 201703L
// guard against 'ERROR' being predefined by MSVC by temporarily undefining it
#if defined(_WIN32)
#  if defined(ERROR)
#    pragma push_macro("ERROR")
#    undef ERROR
#  endif
#endif
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ERROR;
#endif  // __cplusplus < 201703L
#if defined(_WIN32)
#  pragma warning(suppress : 4602)
#  pragma pop_macro("ERROR")
#endif
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ACTIVE_MODE_IDLE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ACTIVE_MODE_RECON;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ACTIVE_MODE_PROTECT_GENERAL;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ACTIVE_MODE_PROTECT_DRONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ACTIVE_MODE_ASSAULT;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ACTIVE_MODE_RETURN_TO_HOME;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::ACTIVE_MODE_ESTOP;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MISSION_NONE;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MISSION_READY;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MISSION_MOVING;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MISSION_PAUSED;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MISSION_REACHED;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MISSION_SURVEILLING;
#endif  // __cplusplus < 201703L
#if __cplusplus < 201703L
// static constexpr member variable definitions are only needed in C++14 and below, deprecated in C++17
template<typename ContainerAllocator>
constexpr uint8_t OperationState_<ContainerAllocator>::MISSION_ERROR;
#endif  // __cplusplus < 201703L

}  // namespace msg

}  // namespace combat_robot_msgs

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__OPERATION_STATE__STRUCT_HPP_
