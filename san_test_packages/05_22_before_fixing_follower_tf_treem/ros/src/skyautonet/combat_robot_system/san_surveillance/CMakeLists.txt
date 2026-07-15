cmake_minimum_required(VERSION 3.16)
project(san_surveillance)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)

add_library(${PROJECT_NAME}_core STATIC
  src/sector_allocator.cpp
  src/pan_tilt_controller.cpp
  src/surveillance_node.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(${PROJECT_NAME}_core
  rclcpp std_msgs combat_robot_msgs)

add_executable(surveillance_node src/surveillance_main.cpp)
target_link_libraries(surveillance_node ${PROJECT_NAME}_core)
ament_target_dependencies(surveillance_node
  rclcpp std_msgs combat_robot_msgs)

install(TARGETS surveillance_node DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # Pure-logic standalone gtest (no rclcpp / msg deps)
  ament_add_gtest(test_surveillance
    test/test_surveillance.cpp
    src/sector_allocator.cpp
    src/pan_tilt_controller.cpp)
  target_include_directories(test_surveillance PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(rclcpp std_msgs combat_robot_msgs)
ament_package()
