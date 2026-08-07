cmake_minimum_required(VERSION 3.5)

# Set project name to match package.xml
project(robot_server)

# --- User's Original Environment Setup ---
set(CMAKE_INSTALL_ALWAYS 1)
set(TARGET_PROJECT_DIR ${CMAKE_SYSROOT})
set(TARGET_PATH_LIB ${TARGET_PROJECT_DIR}/usr/lib)
set(TARGET_PATH_INCLUDE ${TARGET_PROJECT_DIR}/usr/include)

message("Let's configure ARM64")
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

message("backend is wl")
set(BACKEND_LINK wayland-client wayland-cursor wayland-egl)

add_compile_definitions(EMBEDDED_DEVICE)
# --- End of User's Setup ---

# Find all necessary packages
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(rclcpp_components REQUIRED)
find_package(rclcpp_lifecycle REQUIRED)
find_package(lifecycle_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(image_transport REQUIRED)
find_package(combat_robot_msgs REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_search_module(LIBXML2 REQUIRED libxml-2.0)
# Find GStreamer components separately for robustness
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0)
pkg_search_module(GSTREAMER_RTSP REQUIRED gstreamer-rtsp-server-1.0)
pkg_search_module(GSTREAMER_APP REQUIRED gstreamer-app-1.0)

# Add all necessary include directories
include_directories(
  ${PROJECT_SOURCE_DIR}/include/
  ${TARGET_PATH_INCLUDE}/
  ${TARGET_PATH_INCLUDE}/apriltag/common/
  ${TARGET_PATH_INCLUDE}/glib-2.0/
  ${TARGET_PATH_INCLUDE}/glib-2.0/glib/
  ${GSTREAMER_INCLUDE_DIRS}
  ${GSTREAMER_RTSP_INCLUDE_DIRS}
  ${GSTREAMER_APP_INCLUDE_DIRS}
  ${cv_bridge_INCLUDE_DIRS}
)

# Define executables for each node
add_executable(command_server_node
  nodes/command_server_node.cpp
  src/command_server.cpp
  src/command_server_dispatch.cpp
  src/command_server_path_payload_parser.cpp
  src/command_server_status.cpp
  src/command_server_transport.cpp
)
add_executable(rtsp_server_node nodes/rtsp_server_node.cpp src/rtsp_server.cpp)

# Use ament_target_dependencies to handle ROS2 dependencies
target_link_libraries(command_server_node PUBLIC
  ${combat_robot_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  sensor_msgs::sensor_msgs_library
  rclcpp::rclcpp
  rclcpp_components::component
  rclcpp_components::component_manager
  m
  stdc++
  pthread
  dl
)
target_link_libraries(rtsp_server_node PUBLIC
  ${combat_robot_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  cv_bridge::cv_bridge
  image_transport::image_transport
  rclcpp::rclcpp
  rclcpp_components::component
  rclcpp_components::component_manager
  rclcpp_lifecycle::rclcpp_lifecycle
  ${lifecycle_msgs_TARGETS}
  sensor_msgs::sensor_msgs_library
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_RTSP_LIBRARIES}
  ${GSTREAMER_APP_LIBRARIES}
  m
  stdc++
  pthread
  dl
  libgbm.so
  libglib-2.0.so.0
  libgobject-2.0.so.0
  libgthread-2.0.so
  libpthread.so
  ${BACKEND_LINK}
  ${LIBXML2_LIBRARIES}
)

# Add original compile definitions and options
target_compile_definitions(command_server_node PUBLIC
    PROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}"
)
target_compile_definitions(rtsp_server_node PUBLIC
    PROJECT_SRC_DIR="${CMAKE_SOURCE_DIR}"
)
target_compile_options(command_server_node PUBLIC -Wall -Wno-poison-system-directories -O3 -rdynamic -DCALIBRATION_APP)

# Standard ROS2 install rule
install(TARGETS
  command_server_node
  rtsp_server_node
  DESTINATION lib/${PROJECT_NAME}
)
install(
  DIRECTORY launch config
  DESTINATION share/${PROJECT_NAME}
)
ament_package()
