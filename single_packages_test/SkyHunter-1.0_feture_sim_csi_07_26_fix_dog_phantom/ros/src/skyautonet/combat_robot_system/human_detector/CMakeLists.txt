cmake_minimum_required(VERSION 3.8)
project(human_detector)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

add_compile_options(-Wall -Wextra -Wpedantic -Wno-deprecated-declarations)
add_compile_definitions(BOOST_ALLOW_DEPRECATED_HEADERS)

find_package(ament_cmake_auto REQUIRED)
ament_auto_find_build_dependencies()

# Create library
ament_auto_add_library(${PROJECT_NAME} SHARED
  nodes/human_detector_node.cpp
  src/human_detector.cpp
)

# Common includes
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)

# Link detection_processor module
add_subdirectory(libs/detection_processor detection_processor)
target_include_directories(${PROJECT_NAME} PRIVATE libs/detection_processor/include)
target_link_libraries(${PROJECT_NAME} detection_processor)

# Link Tracker
add_subdirectory(libs/sort_tracker sort_tracker)
target_include_directories(${PROJECT_NAME} PRIVATE libs/sort_tracker/include)
target_link_libraries(${PROJECT_NAME} sort_tracker)

# Link OpenCV module
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${OpenCV_LIBS})

rclcpp_components_register_node(${PROJECT_NAME} 
  PLUGIN "human_detector::HumanDetectorComponent"
  EXECUTABLE ${PROJECT_NAME}_node
)

install(TARGETS
  ${PROJECT_NAME}
  DESTINATION lib/${PROJECT_NAME}
)

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  ament_lint_auto_find_test_dependencies()
endif()

set(_human_detector_auto_package_args "")
if(DEFINED ament_cmake_auto_VERSION AND (
  (ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "1.3.12" AND
   ament_cmake_auto_VERSION VERSION_LESS "2.0.0") OR
  ament_cmake_auto_VERSION VERSION_GREATER_EQUAL "2.5.4"))
  list(APPEND _human_detector_auto_package_args USE_SCOPED_HEADER_INSTALL_DIR)
endif()

ament_auto_package(
  ${_human_detector_auto_package_args}
  INSTALL_TO_SHARE
  config
  models
  launch
)
