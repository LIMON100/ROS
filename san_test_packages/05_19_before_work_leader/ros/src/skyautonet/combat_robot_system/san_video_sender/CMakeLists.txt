cmake_minimum_required(VERSION 3.16)
project(san_video_sender)

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

add_library(san_video_sender_core SHARED
  src/video_sender_node.cpp
)
target_include_directories(san_video_sender_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${GST_INCLUDE_DIRS}
)
target_link_libraries(san_video_sender_core ${GST_LIBRARIES})
ament_target_dependencies(san_video_sender_core rclcpp combat_robot_msgs)

add_executable(video_sender_node src/sender_main.cpp)
target_link_libraries(video_sender_node san_video_sender_core)
ament_target_dependencies(video_sender_node rclcpp combat_robot_msgs)

install(TARGETS video_sender_node RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_video_sender_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_video_sender test/test_video_sender.cpp)
  target_link_libraries(test_video_sender san_video_sender_core)
  ament_target_dependencies(test_video_sender rclcpp combat_robot_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_video_sender_core)
ament_export_dependencies(rclcpp combat_robot_msgs)
ament_package()
