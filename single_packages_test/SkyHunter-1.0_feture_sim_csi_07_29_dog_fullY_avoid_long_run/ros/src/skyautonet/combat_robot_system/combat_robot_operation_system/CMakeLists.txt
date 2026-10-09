cmake_minimum_required(VERSION 3.8)
project(combat_robot_operation_system)

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include_directories(
  include
  ${OpenCV_INCLUDE_DIRS}
  )

find_package(OpenCV REQUIRED)

if(NOT OpenCV_FOUND)
    message(FATAL_ERROR "OpenCV not found")
endif()



# find dependencies
find_package(ament_cmake_auto REQUIRED)
find_package(OpenCV REQUIRED)
find_package(std_msgs REQUIRED)

ament_auto_find_build_dependencies()

ament_auto_add_library(combat_robot_operation_system SHARED
  # src/KalmanTracker.cpp
  # src/Hungarian.cpp
  # src/SORT.cpp
  src/combat_robot_operation_system.cpp
  src/combat_robot_operation_system_callbacks.cpp
  src/combat_robot_operation_system_control.cpp
  src/combat_robot_operation_system_state.cpp
)

add_executable(combat_robot_test_dummy_node
  src/combat_robot_test_dummy_node.cpp
)
ament_target_dependencies(combat_robot_test_dummy_node
  rclcpp
  combat_robot_msgs
  std_msgs
)
install(TARGETS
  combat_robot_test_dummy_node
  DESTINATION lib/${PROJECT_NAME}
)

## Specify libraries to link a library or executable target against
target_link_libraries(${PROJECT_NAME}
${OpenCV_LIBS}
)
 
rclcpp_components_register_node(combat_robot_operation_system
  PLUGIN "combat_robot_system::CombatRobotOperationSystem"
  EXECUTABLE combat_robot_operation_system_node
)
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()

  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_fsm_transitions
    test/test_fsm_transitions.cpp
  )
  target_link_libraries(test_fsm_transitions
    combat_robot_operation_system
    ${OpenCV_LIBS}
  )
  ament_target_dependencies(test_fsm_transitions
    rclcpp
    rclcpp_components
    combat_robot_msgs
    std_msgs
    lifecycle_msgs
  )
endif()

set(_combat_robot_operation_system_auto_package_args "")
if(DEFINED ament_cmake_auto_VERSION AND (
  (ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "1.3.12" AND
   ament_cmake_auto_VERSION VERSION_LESS "2.0.0") OR
  ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "2.5.4"))
  list(APPEND _combat_robot_operation_system_auto_package_args USE_SCOPED_HEADER_INSTALL_DIR)
endif()

ament_auto_package(
  ${_combat_robot_operation_system_auto_package_args}
  INSTALL_TO_SHARE
  launch
  config
)
