cmake_minimum_required(VERSION 3.16)
project(san_video_decoder)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# ─── GStreamer 1.x (pkg-config) ───────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET
  gstreamer-1.0
  gstreamer-app-1.0
  gstreamer-video-1.0
)

# ─── Core library ─────────────────────────────────────────────────────
add_library(san_video_decoder_core SHARED
  src/video_decoder_node.cpp
)
target_include_directories(san_video_decoder_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)
target_link_libraries(san_video_decoder_core
  PkgConfig::GST
)
ament_target_dependencies(san_video_decoder_core
  rclcpp
  sensor_msgs
  std_msgs
)

# ─── Executable ───────────────────────────────────────────────────────
add_executable(video_decoder_node src/decoder_main.cpp)
target_link_libraries(video_decoder_node san_video_decoder_core)
ament_target_dependencies(video_decoder_node
  rclcpp
  sensor_msgs
  std_msgs
)

install(TARGETS video_decoder_node RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS san_video_decoder_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config DESTINATION share/${PROJECT_NAME}
        OPTIONAL)

# ─── Tests ────────────────────────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)
  ament_add_gtest(test_video_decoder test/test_video_decoder.cpp)
  target_link_libraries(test_video_decoder san_video_decoder_core)
  ament_target_dependencies(test_video_decoder rclcpp sensor_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(san_video_decoder_core)
ament_export_dependencies(rclcpp sensor_msgs std_msgs)
ament_package()
