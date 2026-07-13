cmake_minimum_required(VERSION 3.16)
project(san_hub_orchestrator)
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(combat_robot_msgs REQUIRED)

find_package(sensor_msgs REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(san_fire_authorization REQUIRED)
# san_fire_authorization_core is a STATIC archive — its OpenSSL link
# does not propagate across the package boundary (old-style export);
# link the crypto lib explicitly here.
find_package(OpenSSL REQUIRED)

add_library(${PROJECT_NAME}_core STATIC
  src/swarm_aggregator.cpp
  src/hub_orchestrator_node.cpp
  # Phase 2-E Turn 8 (extended) — threat aggregation
  src/threat_aggregator.cpp
  src/threat_aggregator_node.cpp
  # DCN-2026-010 D-028 — Detection → Threat converter + node
  src/detection_to_threat.cpp
  src/detection_to_threat_node.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(${PROJECT_NAME}_core
  rclcpp combat_robot_msgs sensor_msgs tf2 tf2_ros san_fire_authorization)
# AFTER the ament deps so -lcrypto lands behind the static fire-auth
# archive on the link line (--as-needed would otherwise drop it).
target_link_libraries(${PROJECT_NAME}_core OpenSSL::Crypto)

add_executable(hub_orchestrator_node src/hub_orchestrator_main.cpp)
target_link_libraries(hub_orchestrator_node ${PROJECT_NAME}_core)
ament_target_dependencies(hub_orchestrator_node rclcpp combat_robot_msgs)

# Phase 2-E Turn 8 (extended) — threat aggregator executable
add_executable(threat_aggregator_node src/threat_aggregator_main.cpp)
target_link_libraries(threat_aggregator_node ${PROJECT_NAME}_core)
ament_target_dependencies(threat_aggregator_node rclcpp combat_robot_msgs)

# DCN-2026-010 D-028 — Detection → Threat converter executable
add_executable(detection_to_threat_node src/detection_to_threat_main.cpp)
target_link_libraries(detection_to_threat_node ${PROJECT_NAME}_core)
ament_target_dependencies(detection_to_threat_node
  rclcpp combat_robot_msgs sensor_msgs tf2 tf2_ros san_fire_authorization)

install(TARGETS hub_orchestrator_node threat_aggregator_node
                detection_to_threat_node
        DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)
  # ament_copyright cannot validate the proprietary 'Proprietary - SkyAutoNet'
  # license (it only knows OSS templates); skip it. Copyright headers are
  # still present and enforced by cpplint's [legal/copyright] check.
  set(ament_cmake_copyright_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()

  ament_add_gtest(test_swarm_aggregator
    test/test_swarm_aggregator.cpp
    src/swarm_aggregator.cpp)
  target_include_directories(test_swarm_aggregator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

  # Phase 2-E Turn 8 (extended)
  ament_add_gtest(test_threat_aggregator
    test/test_threat_aggregator.cpp
    src/threat_aggregator.cpp)
  target_include_directories(test_threat_aggregator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

  # DCN-2026-010 D-028 — Detection → Threat converter tests
  ament_add_gtest(test_detection_to_threat
    test/test_detection_to_threat.cpp
    src/detection_to_threat.cpp)
  target_include_directories(test_detection_to_threat PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(rclcpp combat_robot_msgs)
ament_package()
