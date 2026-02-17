# cmake_minimum_required(VERSION 3.8)
# project(skyhunter_gazebo)

# # Set the C++ standard (good practice, even if not compiling C++)
# if(NOT CMAKE_CXX_STANDARD)
#   set(CMAKE_CXX_STANDARD 17)
# endif()

# if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#   add_compile_options(-Wall -Wextra -Wpedantic)
# endif()

# # Find necessary ROS 2 packages
# find_package(ament_cmake REQUIRED)
# # You can add other dependencies here if needed, like find_package(rclcpp REQUIRED)

# # This is the most important part for this package.
# # It tells colcon to copy your resource directories into the 'install' space.
# install(
#   DIRECTORY launch worlds meshes
#   DESTINATION share/${PROJECT_NAME}
# )

# # Standard testing setup (optional but recommended)
# if(BUILD_TESTING)
#   find_package(ament_lint_auto REQUIRED)
#   ament_lint_auto_find_test_dependencies()
# endif()

# # This finalizes the package and installs the package.xml file
# ament_package()


cmake_minimum_required(VERSION 3.8)
project(skyhunter_gazebo)

find_package(ament_cmake REQUIRED)

# Install the directories containing your launch files, worlds, and meshes.
install(
  DIRECTORY launch worlds meshes
  DESTINATION share/${PROJECT_NAME}
)

ament_package()