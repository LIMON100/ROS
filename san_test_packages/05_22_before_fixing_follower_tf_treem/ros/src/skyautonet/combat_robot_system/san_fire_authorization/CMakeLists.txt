cmake_minimum_required(VERSION 3.16)
project(san_fire_authorization)

# SAN v1.5 PHASE 9 — Per-trigger fire authorization gate.
# DCN-2026-001 D-004 (Limp Mode 발사 정책 Option A).

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(OpenSSL REQUIRED)

# ─── core library (HMAC + Two-key + Audit + Node) ─────────────────────
# fire_authorization_node.cpp is in the core lib (not the executable)
# so test_fire_authorization_node can link against the FireAuthorization
# Node ctor it instantiates. The executable target then only needs main().
add_library(${PROJECT_NAME}_core STATIC
  src/hmac_authenticator.cpp
  src/two_key_state_machine.cpp
  src/audit_logger.cpp
  src/fire_authorization_node.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(${PROJECT_NAME}_core
  OpenSSL::Crypto
)
ament_target_dependencies(${PROJECT_NAME}_core
  rclcpp
  std_msgs
  combat_robot_msgs
)

# ─── ROS 2 node executable ─────────────────────────────────────────────
add_executable(fire_authorization_node
  src/fire_authorization_main.cpp
)
target_include_directories(fire_authorization_node PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(fire_authorization_node
  ${PROJECT_NAME}_core
)
ament_target_dependencies(fire_authorization_node
  rclcpp
  std_msgs
  combat_robot_msgs
)

install(TARGETS
  fire_authorization_node
  DESTINATION lib/${PROJECT_NAME}
)
install(TARGETS
  ${PROJECT_NAME}_core
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin
  INCLUDES DESTINATION include
)
install(DIRECTORY include/
  DESTINATION include
)
install(DIRECTORY config launch
  DESTINATION share/${PROJECT_NAME}
  OPTIONAL
)

# ─── tests ────────────────────────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_hmac_authenticator
    test/test_hmac_authenticator.cpp
  )
  target_link_libraries(test_hmac_authenticator
    ${PROJECT_NAME}_core
  )
  target_include_directories(test_hmac_authenticator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  ament_add_gtest(test_two_key_state_machine
    test/test_two_key_state_machine.cpp
  )
  target_link_libraries(test_two_key_state_machine
    ${PROJECT_NAME}_core
  )
  target_include_directories(test_two_key_state_machine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  ament_add_gtest(test_audit_logger
    test/test_audit_logger.cpp
  )
  target_link_libraries(test_audit_logger
    ${PROJECT_NAME}_core
  )
  target_include_directories(test_audit_logger PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  # End-to-end integration: HMAC + Two-key + Audit gate decision flow.
  # Runs in standalone mode (no rclcpp), so it gates D-004 100% sign-off
  # without requiring a live ROS 2 network.
  ament_add_gtest(test_integration_scenarios
    test/test_integration_scenarios.cpp
  )
  target_link_libraries(test_integration_scenarios
    ${PROJECT_NAME}_core
  )
  target_include_directories(test_integration_scenarios PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  )

  # ROS-level node fixture (rclcpp + node spin) — requires ament_index
  # at test time, so it's compiled only when BUILD_TESTING is on AND
  # rclcpp is available. Validates topic plumbing + QoS + tick timer
  # in addition to the gate logic.
  find_package(rclcpp QUIET)
  if(rclcpp_FOUND)
    ament_add_gtest(test_fire_authorization_node
      test/test_fire_authorization_node.cpp
    )
    target_link_libraries(test_fire_authorization_node
      ${PROJECT_NAME}_core
    )
    target_include_directories(test_fire_authorization_node PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
    ament_target_dependencies(test_fire_authorization_node
      rclcpp
      combat_robot_msgs
    )
  endif()
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(
  rclcpp
  std_msgs
  combat_robot_msgs
  OpenSSL
)
ament_package()
