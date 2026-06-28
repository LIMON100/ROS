cmake_minimum_required(VERSION 3.16)
project(san_hub_comm)

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
find_package(PkgConfig REQUIRED)

pkg_check_modules(GST REQUIRED gstreamer-1.0 gobject-2.0 glib-2.0)

add_library(san_hub_comm_core SHARED
  src/gstreamer_pipeline.cpp
  src/passphrase_generator.cpp
  src/auto_rate_controller.cpp
  src/gstreamer_relay_node.cpp
)
target_include_directories(san_hub_comm_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${GST_INCLUDE_DIRS}
)
target_link_libraries(san_hub_comm_core ${GST_LIBRARIES})
ament_target_dependencies(san_hub_comm_core rclcpp combat_robot_msgs)

add_executable(gstreamer_relay_node src/relay_main.cpp)
target_link_libraries(gstreamer_relay_node san_hub_comm_core)
ament_target_dependencies(gstreamer_relay_node rclcpp combat_robot_msgs)

install(TARGETS gstreamer_relay_node
  RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_hub_comm_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_pipeline_wrapper test/test_pipeline_wrapper.cpp)
  target_link_libraries(test_pipeline_wrapper san_hub_comm_core)
  ament_target_dependencies(test_pipeline_wrapper rclcpp)

  ament_add_gtest(test_passphrase_generator test/test_passphrase_generator.cpp)
  target_link_libraries(test_passphrase_generator san_hub_comm_core)

  ament_add_gtest(test_thumbnail_downgrade test/test_thumbnail_downgrade.cpp)
  target_link_libraries(test_thumbnail_downgrade san_hub_comm_core)
  ament_target_dependencies(test_thumbnail_downgrade rclcpp combat_robot_msgs)

  ament_add_gtest(test_handle_publishing test/test_handle_publishing.cpp)
  target_link_libraries(test_handle_publishing san_hub_comm_core)
  ament_target_dependencies(test_handle_publishing rclcpp combat_robot_msgs)

  ament_add_gtest(test_auto_rate_controller test/test_auto_rate_controller.cpp)
  target_link_libraries(test_auto_rate_controller san_hub_comm_core)
  ament_target_dependencies(test_auto_rate_controller combat_robot_msgs)

  # ★ PATCH 2026-05-13: deep-dive testcases.
  ament_add_gtest(test_patch_hub_comm test/test_patch_hub_comm.cpp)
  target_link_libraries(test_patch_hub_comm san_hub_comm_core)
  ament_target_dependencies(test_patch_hub_comm rclcpp combat_robot_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_hub_comm_core)
ament_export_dependencies(rclcpp combat_robot_msgs)
ament_package()
