cmake_minimum_required(VERSION 3.16)
project(san_role_management)

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
find_package(san_lte_redundancy REQUIRED)
find_package(std_msgs REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(rcl_interfaces REQUIRED)
# [DCN-2026-006 EXT D-024] RobotStatus audit (DiagnosticArray).
find_package(diagnostic_msgs REQUIRED)

add_library(san_role_management_core SHARED
  src/battery_monitor.cpp
  src/hub_role_manager.cpp
  src/leader_role_manager.cpp
  src/limp_mode_manager.cpp
)
target_include_directories(san_role_management_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(san_role_management_core
  rclcpp combat_robot_msgs san_lte_redundancy
  std_msgs lifecycle_msgs rcl_interfaces
  diagnostic_msgs)

add_executable(role_management_node src/role_management_node.cpp)
target_link_libraries(role_management_node san_role_management_core)
ament_target_dependencies(role_management_node
  rclcpp combat_robot_msgs san_lte_redundancy
  std_msgs lifecycle_msgs rcl_interfaces
  diagnostic_msgs)

install(TARGETS role_management_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_role_management_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_battery_selection test/test_battery_selection.cpp)
  target_link_libraries(test_battery_selection san_role_management_core)

  ament_add_gtest(test_leader_succession test/test_leader_succession.cpp)
  target_link_libraries(test_leader_succession san_role_management_core)
  ament_target_dependencies(test_leader_succession
    rclcpp combat_robot_msgs)

  ament_add_gtest(test_hub_deputy_takeover test/test_hub_deputy_takeover.cpp)
  target_link_libraries(test_hub_deputy_takeover san_role_management_core)
  ament_target_dependencies(test_hub_deputy_takeover
    rclcpp combat_robot_msgs)

  ament_add_gtest(test_limp_mode test/test_limp_mode.cpp)
  target_link_libraries(test_limp_mode san_role_management_core)
  ament_target_dependencies(test_limp_mode
    rclcpp combat_robot_msgs)

  # [DCN-2026-006 EXT D-024] RobotStatus audit + hub-only guard tests.
  ament_add_gtest(test_robot_status_audit test/test_robot_status_audit.cpp)
  target_link_libraries(test_robot_status_audit san_role_management_core)
  ament_target_dependencies(test_robot_status_audit
    rclcpp combat_robot_msgs diagnostic_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_role_management_core)
ament_export_dependencies(
  rclcpp combat_robot_msgs san_lte_redundancy
  std_msgs lifecycle_msgs rcl_interfaces)
ament_package()
