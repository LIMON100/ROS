cmake_minimum_required(VERSION 3.16)
project(combat_robot_operation_system)

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

add_library(combat_robot_operation_system_core SHARED
  src/combat_robot_operation_system.cpp
)
target_include_directories(combat_robot_operation_system_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(combat_robot_operation_system_core
  rclcpp combat_robot_msgs)

add_executable(combat_robot_operation_system src/operation_system_main.cpp)
target_link_libraries(combat_robot_operation_system
  combat_robot_operation_system_core)
ament_target_dependencies(combat_robot_operation_system
  rclcpp combat_robot_msgs)

install(TARGETS combat_robot_operation_system
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS combat_robot_operation_system_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_deployment_mode test/test_deployment_mode.cpp)
  target_link_libraries(test_deployment_mode
    combat_robot_operation_system_core)
  ament_target_dependencies(test_deployment_mode
    rclcpp combat_robot_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(combat_robot_operation_system_core)
ament_export_dependencies(rclcpp combat_robot_msgs)
ament_package()
