cmake_minimum_required(VERSION 3.16)
project(san_l5_regression)

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

add_library(san_l5_regression_core SHARED
  src/failure_injector.cpp
  src/scenario_report.cpp
  src/scenario_runner.cpp
)
target_include_directories(san_l5_regression_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(san_l5_regression_core
  rclcpp combat_robot_msgs std_msgs)

add_executable(regression_main src/regression_main.cpp)
target_link_libraries(regression_main san_l5_regression_core)
ament_target_dependencies(regression_main rclcpp combat_robot_msgs std_msgs)

install(TARGETS regression_main RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_l5_regression_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_failure_injector test/test_failure_injector.cpp)
  target_link_libraries(test_failure_injector san_l5_regression_core)
  ament_target_dependencies(test_failure_injector rclcpp combat_robot_msgs)

  ament_add_gtest(test_topic_watcher test/test_topic_watcher.cpp)
  target_link_libraries(test_topic_watcher san_l5_regression_core)
  ament_target_dependencies(test_topic_watcher rclcpp combat_robot_msgs std_msgs)

  ament_add_gtest(test_scenario_report test/test_scenario_report.cpp)
  target_link_libraries(test_scenario_report san_l5_regression_core)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_l5_regression_core)
ament_export_dependencies(rclcpp combat_robot_msgs std_msgs)
ament_package()
