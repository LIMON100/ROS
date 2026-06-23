cmake_minimum_required(VERSION 3.16)
project(san_surveillance)

# SAN v1.5 — 360° Surveillance Sector + Pan-Tilt Controller.
#
# PATCH 2026-05-13 (Surveillance deep-dive review):
#   * Added sector_frame library (Heading↔World transform +
#     DriveClassifier with hysteresis)
#   * Patched sector_allocator (world-frame sectors + leader yaw anchor)
#   * Patched pan_tilt_controller (dt-independent Track + sweep residual)
#   * Patched surveillance_node (recursive_mutex, ThreatAlert bearing,
#     yaw tracking, World→Heading per-robot transform on publish)

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
find_package(std_msgs REQUIRED)
find_package(san_fire_authorization REQUIRED)
find_package(OpenSSL REQUIRED)
# ─── Pure-logic libraries ────────────────────────────────────────────────

# ★ PATCH 2026-05-13: new frame helper library
add_library(${PROJECT_NAME}_frame
  src/sector_frame.cpp
)
target_include_directories(${PROJECT_NAME}_frame PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_library(${PROJECT_NAME}_sector
  src/sector_allocator.cpp
)
target_include_directories(${PROJECT_NAME}_sector PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_sector
  ${PROJECT_NAME}_frame
)

add_library(${PROJECT_NAME}_pantilt
  src/pan_tilt_controller.cpp
)
target_include_directories(${PROJECT_NAME}_pantilt PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_pantilt
  ${PROJECT_NAME}_frame
)

# ─── rclcpp Node library ────────────────────────────────────────────────

add_library(${PROJECT_NAME}_node
  src/surveillance_node.cpp
)
target_include_directories(${PROJECT_NAME}_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_node
  ${PROJECT_NAME}_frame
  ${PROJECT_NAME}_sector
)
ament_target_dependencies(${PROJECT_NAME}_node
  rclcpp
  combat_robot_msgs
  san_fire_authorization
)
target_link_libraries(${PROJECT_NAME}_node OpenSSL::Crypto)
# ─── Executable ─────────────────────────────────────────────────────────

add_executable(surveillance_node
  src/surveillance_main.cpp
)
target_link_libraries(surveillance_node
  ${PROJECT_NAME}_node
)
ament_target_dependencies(surveillance_node rclcpp)

add_executable(pan_tilt_driver
  src/pan_tilt_driver_node.cpp
  src/pan_tilt_driver_main.cpp
) 
target_link_libraries(pan_tilt_driver
  ${PROJECT_NAME}_pantilt
) 
ament_target_dependencies(pan_tilt_driver
  rclcpp combat_robot_msgs std_msgs
) 


# ─── Install ────────────────────────────────────────────────────────────

install(DIRECTORY include/ DESTINATION include)
install(TARGETS
    ${PROJECT_NAME}_frame
    ${PROJECT_NAME}_sector
    ${PROJECT_NAME}_pantilt
    ${PROJECT_NAME}_node
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS surveillance_node pan_tilt_driver
  DESTINATION lib/${PROJECT_NAME} 
) 

install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)
ament_export_include_directories(include)
ament_export_libraries(
  ${PROJECT_NAME}_frame
  ${PROJECT_NAME}_sector
  ${PROJECT_NAME}_pantilt
)
ament_export_dependencies(rclcpp combat_robot_msgs)

# ─── Tests ──────────────────────────────────────────────────────────────

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_surveillance
    test/test_surveillance.cpp
  )
  target_link_libraries(test_surveillance
    ${PROJECT_NAME}_frame
    ${PROJECT_NAME}_sector
    ${PROJECT_NAME}_pantilt
  )
endif()

ament_package()
