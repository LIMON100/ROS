
cmake_minimum_required(VERSION 3.8)
project(display)

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)

find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)


# OpenGL, EGL, X11 (pkg-config)
find_package(PkgConfig REQUIRED)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(EGL REQUIRED egl)
pkg_check_modules(GLESv2 REQUIRED glesv2)

include_directories(
  include
  src/gui
  ${X11_INCLUDE_DIRS}
  ${EGL_INCLUDE_DIRS}
  ${GLESv2_INCLUDE_DIRS}
  ${PROJECT_SOURCE_DIR}/imgui
  ${PROJECT_SOURCE_DIR}/imgui/backends
)


# imgui core
file(GLOB IMGUI_CORE
  imgui/imgui.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_widgets.cpp
)

set(IMGUI_BACKENDS
  imgui/backends/imgui_impl_opengl3.cpp
  imgui/backends/imgui_impl_glut.cpp
  #imgui/backends/imgui_impl_glfw.cpp  # 필요시 활성화
  #imgui/backends/imgui_impl_x11.cpp   # 필요시 활성화
)

file(GLOB SRC_CPP RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} src/*.cpp)
list(REMOVE_ITEM SRC_CPP src/main.cpp)
file(GLOB SRC_GUI src/gui/*.cpp)
set(SOURCES node/display_node.cpp ${SRC_CPP} ${SRC_GUI} ${IMGUI_CORE} ${IMGUI_BACKENDS})


add_executable(display_node ${SOURCES})
target_compile_definitions(display_node PRIVATE IMGUI_IMPL_OPENGL_ES2)
target_link_libraries(display_node
  rclcpp::rclcpp
  ${std_msgs_TARGETS}
  ${sensor_msgs_TARGETS}
  ament_index_cpp::ament_index_cpp
  cv_bridge::cv_bridge
  ${EGL_LIBRARIES}
  ${GLESv2_LIBRARIES}
  ${X11_LIBRARIES}
  ${OpenCV_LIBS}
  GL
  GLU
  glut
  m pthread dl
)

install(TARGETS display_node
  DESTINATION lib/${PROJECT_NAME}
)

install(DIRECTORY include/
  DESTINATION include/
)

install(DIRECTORY imgui/
  DESTINATION share/${PROJECT_NAME}/imgui/
)

install(DIRECTORY src/gui/
  DESTINATION share/${PROJECT_NAME}/src/gui/
)

install(DIRECTORY launch/
  DESTINATION share/${PROJECT_NAME}/launch/
)

install(DIRECTORY assets/
  DESTINATION share/${PROJECT_NAME}/assets/
)

ament_package()
