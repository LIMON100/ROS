cmake_minimum_required(VERSION 3.5)
project(combat_robot_msgs)

# Default to C99
if(NOT CMAKE_C_STANDARD)
  set(CMAKE_C_STANDARD 99)
endif()

# Default to C++14
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 14)
endif()

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)
find_package(builtin_interfaces REQUIRED)
find_package(rosidl_default_generators REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)

set(msg_files
  "msg/BoundingBox2d.msg"
  "msg/DetectedObject.msg"
  "msg/DetectedObjects.msg"
  "msg/TargetPoint.msg"
  "msg/TouchTargetPoint.msg"
  "msg/UserCommand.msg"
  "msg/MissionControlCommand.msg"
  "msg/StreamControlCommand.msg"
  "msg/SwarmControlCommand.msg"
  "msg/SwarmFollowerStatus.msg"
  "msg/SwarmLeaderHeartbeat.msg"
  "msg/SwarmPathCommand.msg"
  "msg/SwarmRobotCommand.msg"
  "msg/IMUState.msg"
  "msg/OperationState.msg"
  "msg/DriveCommand.msg"
  "msg/PanTiltControlCommand.msg"
  "msg/PanTiltState.msg"
  "msg/CenterObject.msg"
  "msg/Waypoint.msg"
  "msg/WaypointList.msg"
  "msg/GnssStatus.msg"
  "msg/ChassisStatus.msg"
  "msg/LidarStatus.msg"
)

rosidl_generate_interfaces(${PROJECT_NAME}
  ${msg_files}
  DEPENDENCIES builtin_interfaces std_msgs sensor_msgs
)

ament_export_dependencies(rosidl_default_runtime)
ament_export_include_directories(include)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
