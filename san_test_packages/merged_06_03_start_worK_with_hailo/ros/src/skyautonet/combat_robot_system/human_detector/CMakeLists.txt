cmake_minimum_required(VERSION 3.16)
project(human_detector)

if(NOT CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 17)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(OpenCV REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(combat_robot_msgs REQUIRED)
find_package(cv_bridge REQUIRED)

find_package(Eigen3 REQUIRED)

set(ONNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/onnxruntime_gpu")

# ─── Optional vendor SDKs ────────────────────────────────────────
# RKNN: rknn-toolkit-lite2 ships librknnrt.so + rknn_api.h. We probe
# for the header; when absent we still build, just without the RK3588
# code path (initialize() returns false -> factory falls back).
find_path(RKNN_INCLUDE_DIR rknn_api.h
  HINTS /usr/include /usr/local/include
        /opt/rknn/include
)
find_library(RKNN_LIBRARY NAMES rknnrt rknn_api
  HINTS /usr/lib /usr/local/lib
        /opt/rknn/lib
)
if(RKNN_INCLUDE_DIR AND RKNN_LIBRARY)
  set(HAVE_RKNN ON)
  message(STATUS "human_detector: RKNN SDK FOUND (${RKNN_LIBRARY})")
else()
  set(HAVE_RKNN OFF)
  message(STATUS "human_detector: RKNN SDK not found - rk3588 stub mode")
endif()

# HailoRT: shipped via apt `hailort`. We probe the C++ header.
find_path(HAILORT_INCLUDE_DIR hailo/hailort.hpp
  HINTS /usr/include /usr/local/include
)
find_library(HAILORT_LIBRARY NAMES hailort
  HINTS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu
        /usr/lib/aarch64-linux-gnu
)
if(HAILORT_INCLUDE_DIR AND HAILORT_LIBRARY)
  set(HAVE_HAILORT ON)
  message(STATUS "human_detector: HailoRT FOUND (${HAILORT_LIBRARY})")
else()
  set(HAVE_HAILORT OFF)
  message(STATUS "human_detector: HailoRT not found - hailo8 stub mode")
endif()

link_directories("${ONNX_DIR}/lib")

# ─── Core library ───────────────────────────────────────────────
add_library(human_detector_core SHARED
  src/inference_backend.cpp
  src/rk3588_npu_backend.cpp
  src/hailo8_backend.cpp
  src/human_detector_node.cpp
  src/onnx_backend.cpp
  src/yolo_engine.cpp
  tracking_lib/src/BYTETracker.cpp
  tracking_lib/src/STrack.cpp
  tracking_lib/src/KalmanFilter.cpp
  tracking_lib/src/lapjv.cpp
  tracking_lib/src/Object.cpp
  tracking_lib/src/Rect.cpp
)
target_include_directories(human_detector_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
  ${OpenCV_INCLUDE_DIRS}
  ${CMAKE_CURRENT_SOURCE_DIR}/tracking_lib/include
  ${EIGEN3_INCLUDE_DIRS}
  ${ONNX_DIR}/include
)
target_link_libraries(human_detector_core onnxruntime)
set_target_properties(human_detector_core PROPERTIES
  BUILD_WITH_INSTALL_RPATH TRUE
  INSTALL_RPATH "$ORIGIN/..:${ONNX_DIR}/lib")

target_link_libraries(human_detector_core ${OpenCV_LIBS})


ament_target_dependencies(human_detector_core
  rclcpp
  sensor_msgs
  combat_robot_msgs
  cv_bridge)

if(HAVE_RKNN)
  target_compile_definitions(human_detector_core PRIVATE HAVE_RKNN)
  target_include_directories(human_detector_core PRIVATE ${RKNN_INCLUDE_DIR})
  target_link_libraries(human_detector_core ${RKNN_LIBRARY})
endif()

if(HAVE_HAILORT)
  target_compile_definitions(human_detector_core PRIVATE HAVE_HAILORT)
  target_include_directories(human_detector_core PRIVATE ${HAILORT_INCLUDE_DIR})
  target_link_libraries(human_detector_core ${HAILORT_LIBRARY})
endif()

# ─── Executable ─────────────────────────────────────────────────
add_executable(human_detector_node src/detector_main.cpp)
target_link_libraries(human_detector_node human_detector_core)
ament_target_dependencies(human_detector_node
  rclcpp
  sensor_msgs
  combat_robot_msgs
  cv_bridge)

install(TARGETS human_detector_node RUNTIME DESTINATION lib/${PROJECT_NAME})
install(TARGETS human_detector_core LIBRARY DESTINATION lib)
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY launch config models DESTINATION share/${PROJECT_NAME})

# ─── Tests ──────────────────────────────────────────────────────
if(BUILD_TESTING)
  find_package(ament_cmake_gtest REQUIRED)

  ament_add_gtest(test_factory test/test_factory.cpp)
  target_link_libraries(test_factory human_detector_core)

  ament_add_gtest(test_fallback test/test_fallback.cpp)
  target_link_libraries(test_fallback human_detector_core)
  ament_target_dependencies(test_fallback rclcpp)

  ament_add_gtest(test_backend_contract test/test_backend_contract.cpp)
  target_link_libraries(test_backend_contract human_detector_core)

  # v1.5.1 (DCN-2026-003 D-003): full pipeline gtest
  ament_add_gtest(test_pipeline test/test_pipeline.cpp)
  target_link_libraries(test_pipeline human_detector_core)
  ament_target_dependencies(test_pipeline rclcpp sensor_msgs combat_robot_msgs)

  # [DCN-2026-006 EXT D-018/D-019] postprocess helpers — header-only,
  # so no library link needed; just include path.
  ament_add_gtest(test_postprocess_d018_d019
                  test/test_postprocess_d018_d019.cpp)
  target_include_directories(test_postprocess_d018_d019 PRIVATE include)
endif()

ament_export_include_directories(include)
ament_export_libraries(human_detector_core)
ament_export_dependencies(rclcpp OpenCV sensor_msgs combat_robot_msgs cv_bridge)
ament_package()
