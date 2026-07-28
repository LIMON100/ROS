cmake_minimum_required(VERSION 3.8)
project(camera_interface)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-deprecated-declarations)
add_compile_definitions(BOOST_ALLOW_DEPRECATED_HEADERS)

find_package(ament_cmake_auto REQUIRED)
find_package(rclcpp_components REQUIRED)
ament_auto_find_build_dependencies()

find_package(PkgConfig REQUIRED)
pkg_search_module(GSTREAMER REQUIRED gstreamer-1.0 gstreamer-video-1.0)
pkg_search_module(GSTREAMER_APP REQUIRED gstreamer-app-1.0)

# ========== camera_driver (컴포넌트 유지)
add_library(camera_driver SHARED
  src/camera_driver.cpp
)
target_include_directories(camera_driver PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include
  ${GSTREAMER_INCLUDE_DIRS}
  ${GSTREAMER_APP_INCLUDE_DIRS}
)
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(camera_driver PUBLIC
  rclcpp::rclcpp
  ${rclcpp_lifecycle_TARGETS}
  ${cv_bridge_TARGETS}
  ${image_transport_TARGETS}
  ${OpenCV_LIBS}
  ${GSTREAMER_LIBRARIES}
  ${GSTREAMER_APP_LIBRARIES}
  rclcpp_components::component
)
rclcpp_components_register_node(camera_driver
  PLUGIN "camera_interface::CameraDriver"
  EXECUTABLE camera_driver_node
)


add_library(zoom_controller_lib
  src/zoom_controller.cpp
)
target_include_directories(zoom_controller_lib PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(zoom_controller_lib PUBLIC
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  gpiod
)


add_executable(zoom_controller_node
  nodes/zoom_controller_node.cpp
)


target_include_directories(zoom_controller_node PUBLIC
  ${CMAKE_CURRENT_LIST_DIR}/include)
target_link_libraries(zoom_controller_node PUBLIC
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  zoom_controller_lib
)

install(TARGETS
  camera_driver
  zoom_controller_lib
  DESTINATION lib
)

install(TARGETS
  zoom_controller_node
  camera_driver_node
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

ament_auto_package(
  INSTALL_TO_SHARE
  config
  launch
)