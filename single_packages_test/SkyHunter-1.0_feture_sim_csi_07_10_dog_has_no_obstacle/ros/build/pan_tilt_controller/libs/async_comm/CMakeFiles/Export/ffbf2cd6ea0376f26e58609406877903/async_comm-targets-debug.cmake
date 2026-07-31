#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "async_comm" for configuration "Debug"
set_property(TARGET async_comm APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(async_comm PROPERTIES
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/lib/libasync_comm.so"
  IMPORTED_SONAME_DEBUG "libasync_comm.so"
  )

list(APPEND _cmake_import_check_targets async_comm )
list(APPEND _cmake_import_check_files_for_async_comm "${_IMPORT_PREFIX}/lib/libasync_comm.so" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
