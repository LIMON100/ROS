# generated from ament/cmake/core/templates/nameConfig.cmake.in

# prevent multiple inclusion
if(_skyhunter_formation_CONFIG_INCLUDED)
  # ensure to keep the found flag the same
  if(NOT DEFINED skyhunter_formation_FOUND)
    # explicitly set it to FALSE, otherwise CMake will set it to TRUE
    set(skyhunter_formation_FOUND FALSE)
  elseif(NOT skyhunter_formation_FOUND)
    # use separate condition to avoid uninitialized variable warning
    set(skyhunter_formation_FOUND FALSE)
  endif()
  return()
endif()
set(_skyhunter_formation_CONFIG_INCLUDED TRUE)

# output package information
if(NOT skyhunter_formation_FIND_QUIETLY)
  message(STATUS "Found skyhunter_formation: 0.0.0 (${skyhunter_formation_DIR})")
endif()

# warn when using a deprecated package
if(NOT "" STREQUAL "")
  set(_msg "Package 'skyhunter_formation' is deprecated")
  # append custom deprecation text if available
  if(NOT "" STREQUAL "TRUE")
    set(_msg "${_msg} ()")
  endif()
  # optionally quiet the deprecation message
  if(NOT ${skyhunter_formation_DEPRECATED_QUIET})
    message(DEPRECATION "${_msg}")
  endif()
endif()

# flag package as ament-based to distinguish it after being find_package()-ed
set(skyhunter_formation_FOUND_AMENT_PACKAGE TRUE)

# include all config extra files
set(_extras "")
foreach(_extra ${_extras})
  include("${skyhunter_formation_DIR}/${_extra}")
endforeach()
