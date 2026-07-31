// NOLINT: This file starts with a BOM since it contain non-ASCII characters
// generated from rosidl_generator_c/resource/idl__struct.h.em
// with input from combat_robot_msgs:msg/ChassisStatus.idl
// generated code does not contain a copyright notice

// IWYU pragma: private, include "combat_robot_msgs/msg/chassis_status.h"


#ifndef COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__STRUCT_H_
#define COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__STRUCT_H_

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants defined in the message

/// Constant 'DRIVE_OK'.
/**
  * Drive state
  * 정상 운행 가능
 */
enum
{
  combat_robot_msgs__msg__ChassisStatus__DRIVE_OK = 0
};

/// Constant 'DRIVE_FAULT'.
/**
  * 모터/통신 장애 — 운행 불가
 */
enum
{
  combat_robot_msgs__msg__ChassisStatus__DRIVE_FAULT = 1
};

/// Constant 'DRIVE_ESTOP'.
/**
  * E-Stop 인가됨
 */
enum
{
  combat_robot_msgs__msg__ChassisStatus__DRIVE_ESTOP = 2
};

/// Constant 'FAULT_NONE'.
/**
  * Fault flags — bitmask. 여러 결함 동시 표현 가능.
 */
enum
{
  combat_robot_msgs__msg__ChassisStatus__FAULT_NONE = 0ul
};

/// Constant 'FAULT_LEFT_WHEEL'.
enum
{
  combat_robot_msgs__msg__ChassisStatus__FAULT_LEFT_WHEEL = 1ul
};

/// Constant 'FAULT_RIGHT_WHEEL'.
enum
{
  combat_robot_msgs__msg__ChassisStatus__FAULT_RIGHT_WHEEL = 2ul
};

/// Constant 'FAULT_LOW_BATTERY'.
enum
{
  combat_robot_msgs__msg__ChassisStatus__FAULT_LOW_BATTERY = 4ul
};

/// Constant 'FAULT_OVERTEMP'.
enum
{
  combat_robot_msgs__msg__ChassisStatus__FAULT_OVERTEMP = 8ul
};

/// Constant 'FAULT_COMM'.
/**
  * chassis ↔ robot_server 통신 (Modbus 등) 장애
 */
enum
{
  combat_robot_msgs__msg__ChassisStatus__FAULT_COMM = 16ul
};

// Include directives for member types
// Member 'header'
#include "std_msgs/msg/detail/header__struct.h"

/// Struct defined in msg/ChassisStatus in the package combat_robot_msgs.
/**
  * 차량(chassis) 컨트롤러가 publish 하는 status.
  * robot_server 가 /chassis/status 토픽으로 subscribe 하여 BMS / 구동 상태를
  * 앱 상태 패킷의 battery_pct, velocity 등에 반영함.
  *
  * 가능한 한 모든 필드를 매 주기 채워 보내주세요. 값을 모르면 0 또는 NaN 사용 가능.
 */
typedef struct combat_robot_msgs__msg__ChassisStatus
{
  std_msgs__msg__Header header;
  uint8_t drive_state;
  /// 배터리 — BMS 값
  /// 0~100 (255 = 알 수 없음)
  uint8_t battery_pct;
  /// V
  float battery_voltage_v;
  /// A  ( + = 방전 / - = 충전 )
  float battery_current_a;
  /// Velocity feedback — 차량이 실제로 움직이는 속도 (encoder/odometry 기반)
  /// 알 수 없으면 NaN.
  /// 전진 양 = +, 후진 = -
  float linear_velocity_mps;
  /// rad/s, 반시계 양 = +
  float angular_velocity_rps;
  uint32_t fault_flags;
  /// 모터 최고 온도 (좌/우 휠 중 큰 값). 알 수 없으면 NaN.
  float motor_temp_c;
} combat_robot_msgs__msg__ChassisStatus;

// Struct for a sequence of combat_robot_msgs__msg__ChassisStatus.
typedef struct combat_robot_msgs__msg__ChassisStatus__Sequence
{
  combat_robot_msgs__msg__ChassisStatus * data;
  /// The number of valid items in data
  size_t size;
  /// The number of allocated items in data
  size_t capacity;
} combat_robot_msgs__msg__ChassisStatus__Sequence;

#ifdef __cplusplus
}
#endif

#endif  // COMBAT_ROBOT_MSGS__MSG__DETAIL__CHASSIS_STATUS__STRUCT_H_
