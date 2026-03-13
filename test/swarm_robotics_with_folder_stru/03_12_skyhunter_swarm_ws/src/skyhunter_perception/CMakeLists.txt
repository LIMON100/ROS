# cmake_minimum_required(VERSION 3.8)
# project(skyhunter_perception)

# # 1. Dependencies
# find_package(ament_cmake REQUIRED)
# find_package(rclcpp REQUIRED)
# find_package(sensor_msgs REQUIRED)
# find_package(cv_bridge REQUIRED)
# find_package(OpenCV REQUIRED)
# find_package(skyhunter_msgs REQUIRED)
# find_package(ament_index_cpp REQUIRED)
# find_package(std_msgs REQUIRED)
# find_package(Eigen3 REQUIRED) # Required for ByteTrack math

# # 2. Paths
# set(ONNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/onnxruntime_gpu")

# # 3. Add the Tracking Library Subdirectory
# add_subdirectory(tracking_lib)

# include_directories(
#   include 
#   tracking_lib/include # Add this
#   ${OpenCV_INCLUDE_DIRS} 
#   ${ONNX_DIR}/include
#   ${EIGEN3_INCLUDE_DIR}
# )

# link_directories("${ONNX_DIR}/lib")

# # 4. Build Engine Library
# add_library(yolo_engine_lib src/yolo_engine.cpp)
# target_link_libraries(yolo_engine_lib ${OpenCV_LIBS} onnxruntime)

# # 5. Build the Detector + Tracker Node
# add_executable(yolo_detector_node src/yolo_detector_node.cpp)
# # Link 'bytetrack' here
# target_link_libraries(yolo_detector_node yolo_engine_lib bytetrack ${OpenCV_LIBS} onnxruntime)
# ament_target_dependencies(yolo_detector_node rclcpp sensor_msgs cv_bridge skyhunter_msgs ament_index_cpp std_msgs)

# install(TARGETS bytetrack
#   ARCHIVE DESTINATION lib
#   LIBRARY DESTINATION lib
#   RUNTIME DESTINATION bin
# )


# # 6. Portability and Installation
# set_target_properties(yolo_detector_node PROPERTIES
#     BUILD_WITH_INSTALL_RPATH TRUE
#     # INSTALL_RPATH "${ONNX_DIR}/lib"
#     INSTALL_RPATH "$ORIGIN/..:${ONNX_DIR}/lib"
# )

# install(TARGETS yolo_detector_node DESTINATION lib/${PROJECT_NAME})
# install(DIRECTORY models DESTINATION share/${PROJECT_NAME})
# ament_package()


cmake_minimum_required(VERSION 3.8)
project(skyhunter_perception)

# 1. Dependencies
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(sensor_msgs REQUIRED)
find_package(cv_bridge REQUIRED)
find_package(OpenCV REQUIRED)
find_package(skyhunter_msgs REQUIRED)
find_package(ament_index_cpp REQUIRED)
find_package(std_msgs REQUIRED)
find_package(Eigen3 REQUIRED)
find_package(nav_msgs REQUIRED) 
find_package(tf2 REQUIRED)          
find_package(tf2_ros REQUIRED) 
find_package(tf2_geometry_msgs REQUIRED) 
find_package(OpenMP REQUIRED)
find_package(PCL REQUIRED)
find_package(pcl_conversions REQUIRED) # <--- ADDED


# 2. Paths
set(ONNX_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include/onnxruntime_gpu")

# 3. Add the Tracking Library
add_subdirectory(tracking_lib)

include_directories(
  include 
  tracking_lib/include 
  ${OpenCV_INCLUDE_DIRS} 
  "${ONNX_DIR}/include"
  ${EIGEN3_INCLUDE_DIR}
  ${PCL_INCLUDE_DIRS} # <--- ADDED
)

link_directories("${ONNX_DIR}/lib")

# 4. Build Engine Library
add_library(yolo_engine_lib src/yolo_engine.cpp)
target_link_libraries(yolo_engine_lib ${OpenCV_LIBS} onnxruntime)

# 5. Build the Node
add_executable(yolo_detector_node 
    src/yolo_detector_node.cpp
    src/main.cpp
    )

# --- CRITICAL FIX: Link the tf2 libraries here ---
target_link_libraries(yolo_detector_node 
  yolo_engine_lib 
  bytetrack 
  ${OpenCV_LIBS} 
  onnxruntime
  OpenMP::OpenMP_CXX   # <--- ADD THIS
  ${PCL_LIBRARIES}     # <--- ADD THIS
  tf2::tf2                  
  tf2_ros::tf2_ros        
  tf2_geometry_msgs::tf2_geometry_msgs
)

ament_target_dependencies(yolo_detector_node 
  rclcpp 
  sensor_msgs 
  nav_msgs           
  cv_bridge 
  skyhunter_msgs 
  ament_index_cpp 
  std_msgs
  tf2                
  tf2_ros
  tf2_geometry_msgs    
  pcl_conversions   
)

# 6. Installation
install(TARGETS bytetrack DESTINATION lib)
install(TARGETS yolo_detector_node DESTINATION lib/${PROJECT_NAME})
install(DIRECTORY models DESTINATION share/${PROJECT_NAME})

set_target_properties(yolo_detector_node PROPERTIES
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH "$ORIGIN/..:${ONNX_DIR}/lib"
)

ament_package()