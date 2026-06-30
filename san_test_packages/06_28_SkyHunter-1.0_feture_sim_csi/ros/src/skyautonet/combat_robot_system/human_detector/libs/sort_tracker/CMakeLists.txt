cmake_minimum_required(VERSION 3.14)
project(sort_tracker)

find_package(OpenCV REQUIRED)
find_package (Eigen3 REQUIRED)


include_directories(
  include
  ${EIGEN3_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS}
)

add_library(${PROJECT_NAME} STATIC
  src/tracker.cpp
  src/bounding_box.cpp
  src/common_helper.cpp
  src/common_helper_cv.cpp
)

# For OpenCV
target_include_directories(${PROJECT_NAME} PUBLIC 
  ../image_processor/include
)

target_link_libraries(${PROJECT_NAME} 
  ${OpenCV_LIBS}
  ${Eigen3_LIBS}
)