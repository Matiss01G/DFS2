#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dfs::dfs_crypto" for configuration ""
set_property(TARGET dfs::dfs_crypto APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(dfs::dfs_crypto PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "CXX"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libdfs_crypto.a"
  )

list(APPEND _cmake_import_check_targets dfs::dfs_crypto )
list(APPEND _cmake_import_check_files_for_dfs::dfs_crypto "${_IMPORT_PREFIX}/lib/libdfs_crypto.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
