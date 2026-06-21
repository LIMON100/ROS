cmake_minimum_required(VERSION 3.16)
project(san_formation)

# SAN v1.5 — Formation control (Hub-side or Leader-side).
#
# PATCH 2026-05-13 (Formation deep-dive review):
#   * Added formation_planner library (pure-logic frame transform +
#     velocity estimation + 1-second slot prediction)
#   * Patched formations.cpp (Box / Diamond ring-out 4-way symmetric)
#   * Patched formation_node.cpp (leader-anchored frame, listener-outside-lock,
#     v1.5 9-formation IDs, velocity-driven 1-s prediction)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)

# ─── Pure-logic libraries (no rclcpp dep) ────────────────────────────────

add_library(${PROJECT_NAME}_formations
  src/formations.cpp
)
target_include_directories(${PROJECT_NAME}_formations PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

add_library(${PROJECT_NAME}_hungarian
  src/hungarian.cpp
)
target_include_directories(${PROJECT_NAME}_hungarian PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

# ★ PATCH 2026-05-13: new pure-logic helper library
add_library(${PROJECT_NAME}_planner
  src/formation_planner.cpp
)
target_include_directories(${PROJECT_NAME}_planner PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_planner
  ${PROJECT_NAME}_formations
)

# ─── rclcpp Node library ────────────────────────────────────────────────

add_library(${PROJECT_NAME}_node
  src/formation_node.cpp
)
target_include_directories(${PROJECT_NAME}_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_node
  ${PROJECT_NAME}_formations
  ${PROJECT_NAME}_hungarian
  ${PROJECT_NAME}_planner
)
ament_target_dependencies(${PROJECT_NAME}_node
  rclcpp
  std_msgs
  geometry_msgs
  combat_robot_msgs
)

# ─── Executable ─────────────────────────────────────────────────────────

add_executable(formation_node
  src/formation_main.cpp
)
target_link_libraries(formation_node
  ${PROJECT_NAME}_node
)
ament_target_dependencies(formation_node rclcpp)

# ─── Install ────────────────────────────────────────────────────────────

install(DIRECTORY include/
  DESTINATION include
)
install(TARGETS
    ${PROJECT_NAME}_formations
    ${PROJECT_NAME}_hungarian
    ${PROJECT_NAME}_planner
    ${PROJECT_NAME}_node
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
)
install(TARGETS formation_node
  DESTINATION lib/${PROJECT_NAME}
)
install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

ament_export_include_directories(include)
ament_export_libraries(
  ${PROJECT_NAME}_formations
  ${PROJECT_NAME}_hungarian
  ${PROJECT_NAME}_planner
)
ament_export_dependencies(rclcpp std_msgs geometry_msgs combat_robot_msgs)

# ─── Tests ──────────────────────────────────────────────────────────────

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_formation
    test/test_formation.cpp
  )
  target_link_libraries(test_formation
    ${PROJECT_NAME}_formations
    ${PROJECT_NAME}_hungarian
    ${PROJECT_NAME}_planner
  )
endif()

ament_package()
