# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.29

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:

#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:

# Disable VCS-based implicit rules.
% : %,v

# Disable VCS-based implicit rules.
% : RCS/%

# Disable VCS-based implicit rules.
% : RCS/%,v

# Disable VCS-based implicit rules.
% : SCCS/s.%

# Disable VCS-based implicit rules.
% : s.%

.SUFFIXES: .hpux_make_needs_suffix_list

# Command-line flag to silence nested $(MAKE).
$(VERBOSE)MAKESILENT = -s

#Suppress display of executed commands.
$(VERBOSE).SILENT:

# A target that is always out of date.
cmake_force:
.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /nix/store/q1nssraba326p2kp6627hldd2bhg254c-cmake-3.29.2/bin/cmake

# The command to remove a file.
RM = /nix/store/q1nssraba326p2kp6627hldd2bhg254c-cmake-3.29.2/bin/cmake -E rm -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/runner/workspace

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/runner/workspace/build

# Utility rule file for run_tests.

# Include any custom commands dependencies for this target.
include CMakeFiles/run_tests.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/run_tests.dir/progress.make

CMakeFiles/run_tests: crypto_tests
CMakeFiles/run_tests: store_tests
CMakeFiles/run_tests: channel_tests
CMakeFiles/run_tests: codec_tests
CMakeFiles/run_tests: bootstrap_tests
	/home/runner/workspace/build/crypto_tests
	/home/runner/workspace/build/store_tests
	/home/runner/workspace/build/channel_tests
	/home/runner/workspace/build/codec_tests
	/home/runner/workspace/build/bootstrap_tests

run_tests: CMakeFiles/run_tests
run_tests: CMakeFiles/run_tests.dir/build.make
.PHONY : run_tests

# Rule to build all files generated by this target.
CMakeFiles/run_tests.dir/build: run_tests
.PHONY : CMakeFiles/run_tests.dir/build

CMakeFiles/run_tests.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/run_tests.dir/cmake_clean.cmake
.PHONY : CMakeFiles/run_tests.dir/clean

CMakeFiles/run_tests.dir/depend:
	cd /home/runner/workspace/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/runner/workspace /home/runner/workspace /home/runner/workspace/build /home/runner/workspace/build /home/runner/workspace/build/CMakeFiles/run_tests.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/run_tests.dir/depend

