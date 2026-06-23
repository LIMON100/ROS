cmake_minimum_required(VERSION 3.16)
project(san_external_mocks)

# SkyHunter v1.5.3 — DCN-2026-021 external vendor mock services.
# Tier 3 per ADR-008 — test/tool, C++ for CI integration.

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
find_package(diagnostic_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# rosbridge_mock_server — synthetic publishers for Aban Android dev.
add_executable(rosbridge_mock_server src/rosbridge_mock_server.cpp)
ament_target_dependencies(rosbridge_mock_server
  rclcpp combat_robot_msgs diagnostic_msgs
  sensor_msgs geometry_msgs std_msgs)

install(TARGETS rosbridge_mock_server
  DESTINATION lib/${PROJECT_NAME})

install(DIRECTORY launch
  DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  find_package(ament_index_cpp REQUIRED)

  # 6-case structural validator gtest — verifies the mock source +
  # spec doc + launch file are consistent. Pure-logic (parses files
  # on disk), runs in <100 ms, no live rosbridge needed.
  ament_add_gtest(test_schema_validator
    test/test_schema_validator.cpp)
  ament_target_dependencies(test_schema_validator ament_index_cpp)
  # T1/T6 walk the source tree (mock cpp + spec md + launch xml);
  # SAN_REPO_ROOT lets the test find them without hardcoded paths.
  set_tests_properties(test_schema_validator PROPERTIES
    ENVIRONMENT "SAN_REPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
endif()

ament_package()
