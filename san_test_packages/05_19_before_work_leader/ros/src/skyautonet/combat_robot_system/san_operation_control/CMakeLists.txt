cmake_minimum_required(VERSION 3.16)
project(san_operation_control)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(combat_robot_msgs REQUIRED)
# [DCN-2026-011 D-033] Dual-SBC heartbeat uses std_msgs/Header.
find_package(std_msgs REQUIRED)

add_library(san_operation_control_core SHARED
  src/deployment_mode.cpp
  src/demo_sequencer.cpp
  src/sensor_watchdog.cpp
  src/operation_control_node.cpp
)
target_include_directories(san_operation_control_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(san_operation_control_core
  rclcpp combat_robot_msgs std_msgs)

add_executable(operation_control_node src/control_main.cpp)
target_link_libraries(operation_control_node san_operation_control_core)
ament_target_dependencies(operation_control_node rclcpp combat_robot_msgs std_msgs)

install(TARGETS operation_control_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_operation_control_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_deployment_mode test/test_deployment_mode.cpp)
  target_link_libraries(test_deployment_mode san_operation_control_core)

  ament_add_gtest(test_demo_sequencer test/test_demo_sequencer.cpp)
  target_link_libraries(test_demo_sequencer san_operation_control_core)

  ament_add_gtest(test_sensor_watchdog test/test_sensor_watchdog.cpp)
  target_link_libraries(test_sensor_watchdog san_operation_control_core)

  ament_add_gtest(test_command_echo test/test_command_echo.cpp)
  target_link_libraries(test_command_echo san_operation_control_core)
  ament_target_dependencies(test_command_echo rclcpp combat_robot_msgs)

  # [DCN-2026-011 D-032] sbc_id resolver tests (5 cases).
  ament_add_gtest(test_resolve_sbc_id test/test_resolve_sbc_id.cpp)
  target_link_libraries(test_resolve_sbc_id san_operation_control_core)
  ament_target_dependencies(test_resolve_sbc_id rclcpp combat_robot_msgs)

  # [DCN-2026-011 D-033] Dual-SBC heartbeat tests (4 cases).
  ament_add_gtest(test_sbc_heartbeat test/test_sbc_heartbeat.cpp)
  target_link_libraries(test_sbc_heartbeat san_operation_control_core)
  ament_target_dependencies(test_sbc_heartbeat
    rclcpp combat_robot_msgs std_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_operation_control_core)
ament_export_dependencies(rclcpp combat_robot_msgs)
ament_package()
