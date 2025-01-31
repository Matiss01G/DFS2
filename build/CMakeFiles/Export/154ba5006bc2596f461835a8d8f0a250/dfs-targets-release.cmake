#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "dfs::dfs_crypto" for configuration "Release"
set_property(TARGET dfs::dfs_crypto APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(dfs::dfs_crypto PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdfs_crypto.a"
  )

list(APPEND _cmake_import_check_targets dfs::dfs_crypto )
list(APPEND _cmake_import_check_files_for_dfs::dfs_crypto "${_IMPORT_PREFIX}/lib/libdfs_crypto.a" )

# Import target "dfs::dfs_store" for configuration "Release"
set_property(TARGET dfs::dfs_store APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(dfs::dfs_store PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdfs_store.a"
  )

list(APPEND _cmake_import_check_targets dfs::dfs_store )
list(APPEND _cmake_import_check_files_for_dfs::dfs_store "${_IMPORT_PREFIX}/lib/libdfs_store.a" )

# Import target "dfs::dfs_network" for configuration "Release"
set_property(TARGET dfs::dfs_network APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(dfs::dfs_network PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libdfs_network.a"
  )

list(APPEND _cmake_import_check_targets dfs::dfs_network )
list(APPEND _cmake_import_check_files_for_dfs::dfs_network "${_IMPORT_PREFIX}/lib/libdfs_network.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
