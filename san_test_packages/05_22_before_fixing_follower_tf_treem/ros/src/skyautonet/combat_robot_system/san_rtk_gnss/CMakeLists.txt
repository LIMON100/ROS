cmake_minimum_required(VERSION 3.16)
project(san_rtk_gnss)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)

add_library(${PROJECT_NAME}_core STATIC
  src/nmea_parser.cpp
  src/rtk_gnss_node.cpp
  src/stub_serial.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
ament_target_dependencies(${PROJECT_NAME}_core
  rclcpp std_msgs sensor_msgs combat_robot_msgs)

add_executable(rtk_gnss_node src/rtk_gnss_main.cpp)
target_link_libraries(rtk_gnss_node ${PROJECT_NAME}_core)
ament_target_dependencies(rtk_gnss_node
  rclcpp std_msgs sensor_msgs combat_robot_msgs)

install(TARGETS rtk_gnss_node
  DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib
  INCLUDES DESTINATION include)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # Pure-logic standalone gtest (no rclcpp, no serial)
  ament_add_gtest(test_nmea_parser test/test_nmea_parser.cpp
                   src/nmea_parser.cpp)
  target_include_directories(test_nmea_parser PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

  # [DCN-2026-006 EXT D-022] HDT (dual-antenna heading) parser tests.
  ament_add_gtest(test_nmea_parser_hdt test/test_nmea_parser_hdt.cpp
                   src/nmea_parser.cpp)
  target_include_directories(test_nmea_parser_hdt PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

  # [DCN-2026-006 EXT D-022 + D-023] buildHeadingMsg quaternion +
  # covariance scaling tests (no parsing, no node — pure helper).
  ament_add_gtest(test_publish_heading_d022_d023
                   test/test_publish_heading_d022_d023.cpp)
  target_link_libraries(test_publish_heading_d022_d023 ${PROJECT_NAME}_core)
  ament_target_dependencies(test_publish_heading_d022_d023
    rclcpp sensor_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(rclcpp std_msgs sensor_msgs combat_robot_msgs)
ament_package()
