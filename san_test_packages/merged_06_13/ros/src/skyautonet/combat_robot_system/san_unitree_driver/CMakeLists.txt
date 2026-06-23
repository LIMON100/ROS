cmake_minimum_required(VERSION 3.16)
project(san_unitree_driver)

# SAN v1.5 Phase 2-E Turn 2 — Unitree Go2 ROS 2 driver (Tier 1 C++).

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(nav_msgs REQUIRED)

# Optional: unitree_sdk2. If present, link real SDK; otherwise stub
# (subject to the gate option below).
find_package(unitree_sdk2 QUIET)

# Phase 0 PR-C: SDK stub fallback gate.
# When OFF (production default), a missing unitree_sdk2 is a hard
# build error — paper-tiger drivers that look like they're running
# but never communicate with the Go2 are not allowed in production.
# When ON (CI / dev laptop default for now to keep CI green), the
# stub IS linked, but the node refuses to start at run-time unless
# `allow_stub_sdk:=true` is set in the launch.
option(SAN_UNITREE_ALLOW_STUB
  "Permit linking the stub Go2 SDK when unitree_sdk2 is not found. \
Allowed for CI/dev; production builds MUST install unitree_sdk2." ON)

# ─── Decide which SDK implementation to link ───────────────────────────
if(unitree_sdk2_FOUND)
  message(STATUS "[san_unitree_driver] Using real unitree_sdk2")
  set(SDK_IMPL_SOURCES src/real_go2_sdk.cpp)
  set(SDK_LINK_LIBS unitree_sdk2)
elseif(SAN_UNITREE_ALLOW_STUB)
  message(WARNING
      "[san_unitree_driver] unitree_sdk2 NOT FOUND — linking STUB. "
      "Node will refuse to start unless launch sets "
      "allow_stub_sdk:=true. Install unitree_sdk2 under /opt/unitree_sdk2 "
      "for production builds (or build with -DSAN_UNITREE_ALLOW_STUB=OFF "
      "to fail-fast here).")
  set(SDK_IMPL_SOURCES src/stub_go2_sdk.cpp)
  set(SDK_LINK_LIBS "")
else()
  message(FATAL_ERROR
      "[san_unitree_driver] unitree_sdk2 NOT FOUND and "
      "SAN_UNITREE_ALLOW_STUB=OFF — refusing to link stub. Install "
      "unitree_sdk2 under /opt/unitree_sdk2, or build with "
      "-DSAN_UNITREE_ALLOW_STUB=ON for bringup/CI.")
endif()

# ─── core node library ─────────────────────────────────────────────────
add_library(${PROJECT_NAME}_core STATIC
  src/unitree_go2_node.cpp
  ${SDK_IMPL_SOURCES}
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
if(SDK_LINK_LIBS)
  target_link_libraries(${PROJECT_NAME}_core ${SDK_LINK_LIBS})
endif()
ament_target_dependencies(${PROJECT_NAME}_core
  rclcpp
  std_msgs
  geometry_msgs
  sensor_msgs
  nav_msgs
)

# ─── executable ────────────────────────────────────────────────────────
add_executable(unitree_go2_node
  src/unitree_go2_main.cpp
)
target_include_directories(unitree_go2_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(unitree_go2_node
  ${PROJECT_NAME}_core
)
ament_target_dependencies(unitree_go2_node
  rclcpp
  std_msgs
  geometry_msgs
  sensor_msgs
  nav_msgs
)

install(TARGETS unitree_go2_node
  DESTINATION lib/${PROJECT_NAME}
)
install(TARGETS ${PROJECT_NAME}_core
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  INCLUDES DESTINATION include
)
install(DIRECTORY include/
  DESTINATION include
)
install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

# ─── tests ─────────────────────────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  # Node unit test uses MockGo2Sdk (defined in the test file) injected
  # into the test ctor — no live Go2 needed.
  ament_add_gtest(test_unitree_go2_node
    test/test_unitree_go2_node.cpp
  )
  target_link_libraries(test_unitree_go2_node
    ${PROJECT_NAME}_core
  )
  target_include_directories(test_unitree_go2_node PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  ament_target_dependencies(test_unitree_go2_node
    rclcpp
    std_msgs
    geometry_msgs
    sensor_msgs
    nav_msgs
  )
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(
  rclcpp
  std_msgs
  geometry_msgs
  sensor_msgs
  nav_msgs
)
ament_package()
