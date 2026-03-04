cmake_minimum_required(VERSION 3.8)
project(skyhunter_perception)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# 1. ROS 2 Dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(skyhunter_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)

# 2. ONNX Runtime Path Logic
set(ONNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/onnxruntime_gpu")

# 3. Tell the compiler where to look for .h and .hpp files
include_directories(
  include
  ${OpenCV_INCLUDE_DIRS}
  "${ONNX_DIR}/include"
)

# 4. Tell the compiler where the .so (library) files are
link_directories("${ONNX_DIR}/lib")

# 5. Build the Internal Logic Library
add_library(yolo_engine_lib src/yolo_engine.cpp)
target_link_libraries(yolo_engine_lib ${OpenCV_LIBS} onnxruntime)

# 6. Build the ROS 2 Node
add_executable(yolo_detector_node src/yolo_detector_node.cpp)
target_link_libraries(yolo_detector_node yolo_engine_lib ${OpenCV_LIBS} onnxruntime)
ament_target_dependencies(yolo_detector_node 
  rclcpp 
  sensor_msgs 
  cv_bridge 
  skyhunter_msgs 
  ament_index_cpp
)

# 7. Portability fix (Crucial for GPU libraries)
set_target_properties(yolo_detector_node PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "${ONNX_DIR}/lib"
)

# 8. INSTALLATION (CRITICAL FIX: DO NOT COMMENT THESE OUT)
# This puts the executable in install/skyhunter_perception/lib/skyhunter_perception/
install(TARGETS yolo_detector_node 
  DESTINATION lib/${PROJECT_NAME}
)

# This puts the models in install/skyhunter_perception/share/skyhunter_perception/models/
install(DIRECTORY models
  DESTINATION share/${PROJECT_NAME}
)

ament_package()