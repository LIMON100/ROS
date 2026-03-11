cmake_minimum_required(VERSION 3.14)
project(bytetrack)

# --- Standard C++ Settings ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# --- Find Dependencies ---
# This is the crucial part that was missing.
find_package(OpenCV REQUIRED)
message(STATUS "Found OpenCV for bytetrack library: ${OpenCV_VERSION}")

find_package(Eigen3 3.3 QUIET)
if(NOT EIGEN3_FOUND)
  include(FetchContent)
  FetchContent_Declare(eigen
    GIT_REPOSITORY  https://gitlab.com/libeigen/eigen.git
    GIT_TAG         3.4.0
    GIT_SHALLOW     ON
  )
  FetchContent_MakeAvailable(eigen)
  message(STATUS "Eigen was not found, downloaded and configured.")
else()
  message(STATUS "Found system-installed Eigen version: ${EIGEN3_VERSION_STRING}")
endif()

# --- Define the Library Target ---
add_library(${PROJECT_NAME} SHARED
  src/BYTETracker.cpp
  src/KalmanFilter.cpp
  src/lapjv.cpp
  src/Object.cpp
  src/Rect.cpp
  src/STrack.cpp
)

# --- Include Directories ---
# PUBLIC means that any target linking against this library will
# automatically inherit these include paths.
target_include_directories(${PROJECT_NAME} PUBLIC
  "${CMAKE_CURRENT_SOURCE_DIR}/include"
  ${EIGEN3_INCLUDE_DIR}
  ${OpenCV_INCLUDE_DIRS} # <-- Add OpenCV's include directory
)

# --- Linking for the Library ---
# Also link the library against the actual OpenCV libraries
target_link_libraries(${PROJECT_NAME} PRIVATE 
    ${OpenCV_LIBS}
)

# --- Compiler Options ---
target_compile_options(${PROJECT_NAME} PRIVATE 
  -Wall -Wextra
  $<$<CONFIG:Debug>:-g>
  $<$<CONFIG:Release>:-O3>
)