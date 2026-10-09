cmake_minimum_required(VERSION 3.10)
project(pan_tilt_controller)
set(CMAKE_BUILD_TYPE Debug)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_subdirectory(libs/async_comm)

# find dependencies
find_package(ament_cmake_auto REQUIRED)
find_package(rclcpp_components REQUIRED)
ament_auto_find_build_dependencies()


add_library(pan_tilt_controller SHARED
  src/pan_tilt_controller.cpp
)
target_include_directories(pan_tilt_controller PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_link_libraries(pan_tilt_controller PUBLIC
  rclcpp::rclcpp
  ${rclcpp_lifecycle_TARGETS}
  ${std_msgs_TARGETS}
  ${combat_robot_msgs_TARGETS}
  async_comm
  rclcpp_components::component
)

# Register component (provide a distinct executable name to satisfy the macro, but we won't use it)
rclcpp_components_register_node(pan_tilt_controller
  PLUGIN "combat_robot_system::PanTiltController"
  EXECUTABLE pan_tilt_controller_node_auto
)

# Manually create executable for LifecycleNode compatibility
add_executable(pan_tilt_controller_node
  nodes/pan_tilt_controller_node.cpp
)
target_include_directories(pan_tilt_controller_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(pan_tilt_controller_node PUBLIC
  rclcpp::rclcpp
  ${rclcpp_lifecycle_TARGETS}
  ${std_msgs_TARGETS}
  ${combat_robot_msgs_TARGETS}
  pan_tilt_controller
)

# Install
install(TARGETS
  pan_tilt_controller
  pan_tilt_controller_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

set(_pan_tilt_controller_auto_package_args "")
if(DEFINED ament_cmake_auto_VERSION AND (
  (ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "1.3.12" AND
   ament_cmake_auto_VERSION VERSION_LESS "2.0.0") OR
  ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "2.5.4"))
  list(APPEND _pan_tilt_controller_auto_package_args USE_SCOPED_HEADER_INSTALL_DIR)
endif()

ament_auto_package(
  ${_pan_tilt_controller_auto_package_args}
  INSTALL_TO_SHARE
  launch param
)
