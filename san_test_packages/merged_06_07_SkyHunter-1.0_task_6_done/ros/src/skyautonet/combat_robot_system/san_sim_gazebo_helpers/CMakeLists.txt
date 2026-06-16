cmake_minimum_required(VERSION 3.16)
project(san_sim_gazebo_helpers)

# SAN v1.5 — Gazebo simulation helper nodes (C++ patch).
#
# PATCH 2026-05-13 (Gazebo deep-dive review):
#   Adds C++ helpers the existing san_sim_gazebo was missing:
#     * drone_target_simulator — spawns FPV-style drone targets so
#       KPP-2 (avoidance ≤ 300ms) can be validated end-to-end
#     * gps_jump_injector — disturbs /gps/fix to test the Limon
#       dual-EKF patches
#
# Both nodes follow the Tier 1 C++ / rclcpp pattern (DCN-2026-002 D-007)
# and the Real/Stub/Mock abstraction used by other comm modules.

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)

# ─── Pure-logic libraries (no rclcpp dep — standalone testable) ──────────

add_library(${PROJECT_NAME}_trajectory
  src/drone_trajectory.cpp
)
target_include_directories(${PROJECT_NAME}_trajectory PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_library(${PROJECT_NAME}_gps_disturbance
  src/gps_disturbance.cpp
)
target_include_directories(${PROJECT_NAME}_gps_disturbance PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ─── rclcpp Node libraries ──────────────────────────────────────────────

add_library(${PROJECT_NAME}_drone_node
  src/drone_target_simulator_node.cpp
)
target_include_directories(${PROJECT_NAME}_drone_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_drone_node
  ${PROJECT_NAME}_trajectory
)
ament_target_dependencies(${PROJECT_NAME}_drone_node
  rclcpp
  geometry_msgs
  combat_robot_msgs
)

add_library(${PROJECT_NAME}_gps_node
  src/gps_jump_injector_node.cpp
)
target_include_directories(${PROJECT_NAME}_gps_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_gps_node
  ${PROJECT_NAME}_gps_disturbance
)
ament_target_dependencies(${PROJECT_NAME}_gps_node
  rclcpp
  sensor_msgs
)

# ─── Executables ────────────────────────────────────────────────────────

add_executable(drone_target_simulator
  src/drone_target_simulator_main.cpp
)
target_link_libraries(drone_target_simulator
  ${PROJECT_NAME}_drone_node
)
ament_target_dependencies(drone_target_simulator rclcpp)

add_executable(gps_jump_injector
  src/gps_jump_injector_main.cpp
)
target_link_libraries(gps_jump_injector
  ${PROJECT_NAME}_gps_node
)
ament_target_dependencies(gps_jump_injector rclcpp)

# ─── Install ────────────────────────────────────────────────────────────

install(DIRECTORY include/
  DESTINATION include
)
install(TARGETS
    ${PROJECT_NAME}_trajectory
    ${PROJECT_NAME}_gps_disturbance
    ${PROJECT_NAME}_drone_node
    ${PROJECT_NAME}_gps_node
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS
    drone_target_simulator
    gps_jump_injector
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

ament_export_include_directories(include)
ament_export_libraries(
  ${PROJECT_NAME}_trajectory
  ${PROJECT_NAME}_gps_disturbance
)
ament_export_dependencies(rclcpp geometry_msgs sensor_msgs combat_robot_msgs)

# ─── Tests ──────────────────────────────────────────────────────────────

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_drone_trajectory
    test/test_drone_trajectory.cpp
  )
  target_link_libraries(test_drone_trajectory
    ${PROJECT_NAME}_trajectory
  )

  ament_add_gtest(test_gps_disturbance
    test/test_gps_disturbance.cpp
  )
  target_link_libraries(test_gps_disturbance
    ${PROJECT_NAME}_gps_disturbance
  )
endif()

ament_package()
