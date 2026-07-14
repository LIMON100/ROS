cmake_minimum_required(VERSION 3.16)
project(san_follower_tier)

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
  src/tier_fsm.cpp
  src/tier_node.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(${PROJECT_NAME}_core
  rclcpp std_msgs combat_robot_msgs)

add_executable(tier_node src/tier_main.cpp)
target_link_libraries(tier_node ${PROJECT_NAME}_core)
ament_target_dependencies(tier_node
  rclcpp std_msgs combat_robot_msgs)

install(TARGETS tier_node DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_tier_fsm
    test/test_tier_fsm.cpp
    src/tier_fsm.cpp)
  target_include_directories(test_tier_fsm PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(rclcpp std_msgs combat_robot_msgs)
ament_package()
