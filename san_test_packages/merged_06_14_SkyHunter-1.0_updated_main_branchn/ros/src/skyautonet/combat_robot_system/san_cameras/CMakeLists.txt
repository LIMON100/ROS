cmake_minimum_required(VERSION 3.16)
project(san_cameras)
if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(std_msgs REQUIRED)

# ─── Phase 0 PR-B: stub-V4L2 fallback gate ─────────────────────────────
# When OFF (production default), `makeRealV4l2()` triggers a #error in
# stub_v4l2.cpp because no real backend is linked. When ON (bringup,
# CI, tests), the stub IS linked but nodes still refuse to use it
# unless `allow_stub_v4l2:=true` is set in the launch.
option(SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK
  "Permit linking the stub V4L2 backend as the makeRealV4l2() fallback. \
Allowed for CI/bringup; production builds MUST link a real backend." ON)

add_library(${PROJECT_NAME}_core STATIC
  src/frame_metadata.cpp
  src/camera_node_base.cpp
  src/stub_v4l2.cpp
)
target_include_directories(${PROJECT_NAME}_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
ament_target_dependencies(${PROJECT_NAME}_core rclcpp sensor_msgs std_msgs)

if(SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK)
  target_compile_definitions(${PROJECT_NAME}_core PRIVATE
    SAN_CAMERAS_ALLOW_STUB_V4L2_FALLBACK=1)
  message(STATUS "san_cameras: stub V4L2 fallback ENABLED (bringup/CI mode)")
else()
  message(STATUS "san_cameras: stub V4L2 fallback DISABLED — production-safe")
endif()

add_executable(imx678_camera_node src/imx678_node.cpp)
target_link_libraries(imx678_camera_node ${PROJECT_NAME}_core)
ament_target_dependencies(imx678_camera_node rclcpp sensor_msgs std_msgs)

add_executable(thermal_camera_node src/thermal_node.cpp)
target_link_libraries(thermal_camera_node ${PROJECT_NAME}_core)
ament_target_dependencies(thermal_camera_node rclcpp sensor_msgs std_msgs)

install(TARGETS imx678_camera_node thermal_camera_node
        DESTINATION lib/${PROJECT_NAME})
install(TARGETS ${PROJECT_NAME}_core
        ARCHIVE DESTINATION lib LIBRARY DESTINATION lib)
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
  ament_add_gtest(test_frame_metadata
    test/test_frame_metadata.cpp
    src/frame_metadata.cpp)
  target_include_directories(test_frame_metadata PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include)

  # ★ PATCH 2026-05-13: deep-dive testcases (parameter overrides,
  # atomic counters, start() idempotency, exception safety).
  ament_add_gtest(test_patch_cameras
    test/test_patch_cameras.cpp)
  target_link_libraries(test_patch_cameras ${PROJECT_NAME}_core)
  ament_target_dependencies(test_patch_cameras rclcpp sensor_msgs)
endif()

ament_export_include_directories(include)
ament_export_libraries(${PROJECT_NAME}_core)
ament_export_dependencies(rclcpp sensor_msgs std_msgs)
ament_package()
