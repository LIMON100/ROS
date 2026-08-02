cmake_minimum_required(VERSION 3.8)
project(teleop_controller)

set(CMAKE_BUILD_TYPE Debug)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake_auto REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(std_msgs REQUIRED)

ament_auto_find_build_dependencies()

ament_auto_add_library(teleop_controller SHARED
  src/teleop_controller.cpp
)

target_link_libraries(teleop_controller
  ${combat_robot_msgs_TARGETS}
  rclcpp::rclcpp
  modbus
)

rclcpp_components_register_node(teleop_controller
  PLUGIN "teleop_controller::RobotController"
  EXECUTABLE teleop_controller_node
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

set(_teleop_controller_auto_package_args "")
if(DEFINED ament_cmake_auto_VERSION AND (
  (ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "1.3.12" AND
   ament_cmake_auto_VERSION VERSION_LESS "2.0.0") OR
  ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "2.5.4"))
  list(APPEND _teleop_controller_auto_package_args USE_SCOPED_HEADER_INSTALL_DIR)
endif()

ament_auto_package(
  ${_teleop_controller_auto_package_args}
  INSTALL_TO_SHARE
  launch
  param
)

install(PROGRAMS
  #src/keyboard_teleop.py
  src/remote_teleop.py
  DESTINATION lib/${PROJECT_NAME}
)

