cmake_minimum_required(VERSION 3.16)
project(san_formation)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(geometry_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)


# --- ADDED FOR TACTICAL LEADER NODE ---
find_package(nav_msgs REQUIRED)
find_package(visualization_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(pcl_conversions REQUIRED)
find_package(tf2 REQUIRED)
find_package(tf2_ros REQUIRED)
find_package(tf2_geometry_msgs REQUIRED)

add_library(${PROJECT_NAME}_core STATIC
  src/hungarian.cpp
  src/formations.cpp
  src/formation_node.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(${PROJECT_NAME}_core
  rclcpp std_msgs geometry_msgs combat_robot_msgs)

add_executable(formation_node src/formation_main.cpp)
target_link_libraries(formation_node ${PROJECT_NAME}_core)
ament_target_dependencies(formation_node
  rclcpp std_msgs geometry_msgs combat_robot_msgs)


add_executable(tactical_leader_node src/leader_node.cpp)
target_link_libraries(tactical_leader_node ${PROJECT_NAME}_core)
ament_target_dependencies(tactical_leader_node
  rclcpp geometry_msgs nav_msgs combat_robot_msgs visualization_msgs sensor_msgs pcl_conversions tf2 tf2_ros tf2_geometry_msgs
)
install(TARGETS tactical_leader_node DESTINATION lib/${PROJECT_NAME})

install(TARGETS formation_node DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  find_package(ament_cmake_gtest REQUIRED)
  ament_lint_auto_find_test_dependencies()

  # Pure-logic standalone gtest (no rclcpp / msg deps)
  ament_add_gtest(test_formation
    test/test_formation.cpp
    src/hungarian.cpp
    src/formations.cpp)
  target_include_directories(test_formation PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(rclcpp std_msgs geometry_msgs combat_robot_msgs)
ament_package()
