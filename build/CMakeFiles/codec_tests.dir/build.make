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
CMAKE_SOURCE_DIR = /home/runner/StreamCryptoDFS

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/runner/StreamCryptoDFS/build

# Include any dependencies generated for this target.
include CMakeFiles/codec_tests.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/codec_tests.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/codec_tests.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/codec_tests.dir/flags.make

CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o: CMakeFiles/codec_tests.dir/flags.make
CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o: /home/runner/StreamCryptoDFS/src/tests/codec_test.cpp
CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o: CMakeFiles/codec_tests.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o -MF CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o.d -o CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o -c /home/runner/StreamCryptoDFS/src/tests/codec_test.cpp

CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.i"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/runner/StreamCryptoDFS/src/tests/codec_test.cpp > CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.i

CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.s"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/runner/StreamCryptoDFS/src/tests/codec_test.cpp -o CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.s

CMakeFiles/codec_tests.dir/src/network/codec.cpp.o: CMakeFiles/codec_tests.dir/flags.make
CMakeFiles/codec_tests.dir/src/network/codec.cpp.o: /home/runner/StreamCryptoDFS/src/network/codec.cpp
CMakeFiles/codec_tests.dir/src/network/codec.cpp.o: CMakeFiles/codec_tests.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/codec_tests.dir/src/network/codec.cpp.o"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/codec_tests.dir/src/network/codec.cpp.o -MF CMakeFiles/codec_tests.dir/src/network/codec.cpp.o.d -o CMakeFiles/codec_tests.dir/src/network/codec.cpp.o -c /home/runner/StreamCryptoDFS/src/network/codec.cpp

CMakeFiles/codec_tests.dir/src/network/codec.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/codec_tests.dir/src/network/codec.cpp.i"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/runner/StreamCryptoDFS/src/network/codec.cpp > CMakeFiles/codec_tests.dir/src/network/codec.cpp.i

CMakeFiles/codec_tests.dir/src/network/codec.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/codec_tests.dir/src/network/codec.cpp.s"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/runner/StreamCryptoDFS/src/network/codec.cpp -o CMakeFiles/codec_tests.dir/src/network/codec.cpp.s

CMakeFiles/codec_tests.dir/src/network/channel.cpp.o: CMakeFiles/codec_tests.dir/flags.make
CMakeFiles/codec_tests.dir/src/network/channel.cpp.o: /home/runner/StreamCryptoDFS/src/network/channel.cpp
CMakeFiles/codec_tests.dir/src/network/channel.cpp.o: CMakeFiles/codec_tests.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/codec_tests.dir/src/network/channel.cpp.o"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/codec_tests.dir/src/network/channel.cpp.o -MF CMakeFiles/codec_tests.dir/src/network/channel.cpp.o.d -o CMakeFiles/codec_tests.dir/src/network/channel.cpp.o -c /home/runner/StreamCryptoDFS/src/network/channel.cpp

CMakeFiles/codec_tests.dir/src/network/channel.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/codec_tests.dir/src/network/channel.cpp.i"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/runner/StreamCryptoDFS/src/network/channel.cpp > CMakeFiles/codec_tests.dir/src/network/channel.cpp.i

CMakeFiles/codec_tests.dir/src/network/channel.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/codec_tests.dir/src/network/channel.cpp.s"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/runner/StreamCryptoDFS/src/network/channel.cpp -o CMakeFiles/codec_tests.dir/src/network/channel.cpp.s

# Object files for target codec_tests
codec_tests_OBJECTS = \
"CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o" \
"CMakeFiles/codec_tests.dir/src/network/codec.cpp.o" \
"CMakeFiles/codec_tests.dir/src/network/channel.cpp.o"

# External object files for target codec_tests
codec_tests_EXTERNAL_OBJECTS =

codec_tests: CMakeFiles/codec_tests.dir/src/tests/codec_test.cpp.o
codec_tests: CMakeFiles/codec_tests.dir/src/network/codec.cpp.o
codec_tests: CMakeFiles/codec_tests.dir/src/network/channel.cpp.o
codec_tests: CMakeFiles/codec_tests.dir/build.make
codec_tests: libdfs_network.a
codec_tests: libdfs_crypto.a
codec_tests: /nix/store/gp504m4dvw5k2pdx6pccf1km79fkcwgf-openssl-3.0.13/lib/libcrypto.so
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_log_setup.so.1.81.0
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_system.so.1.81.0
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_log.so.1.81.0
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_thread.so.1.81.0
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_chrono.so.1.81.0
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_filesystem.so.1.81.0
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_atomic.so.1.81.0
codec_tests: /nix/store/62sh2bwllmkl8zzpqhglzgpk7lmsmrsa-boost-1.81.0/lib/libboost_regex.so.1.81.0
codec_tests: /nix/store/g1xqvy5p9xgl33iywjy8192xpsfw33b1-gtest-1.14.0/lib/libgtest_main.so.1.14.0
codec_tests: /nix/store/g1xqvy5p9xgl33iywjy8192xpsfw33b1-gtest-1.14.0/lib/libgtest.so.1.14.0
codec_tests: CMakeFiles/codec_tests.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX executable codec_tests"
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/codec_tests.dir/link.txt --verbose=$(VERBOSE)
	/nix/store/q1nssraba326p2kp6627hldd2bhg254c-cmake-3.29.2/bin/cmake -D TEST_TARGET=codec_tests -D TEST_EXECUTABLE=/home/runner/StreamCryptoDFS/build/codec_tests -D TEST_EXECUTOR= -D TEST_WORKING_DIR=/home/runner/StreamCryptoDFS/build -D TEST_EXTRA_ARGS= -D TEST_PROPERTIES= -D TEST_PREFIX= -D TEST_SUFFIX= -D TEST_FILTER= -D NO_PRETTY_TYPES=FALSE -D NO_PRETTY_VALUES=FALSE -D TEST_LIST=codec_tests_TESTS -D CTEST_FILE=/home/runner/StreamCryptoDFS/build/codec_tests[1]_tests.cmake -D TEST_DISCOVERY_TIMEOUT=5 -D TEST_XML_OUTPUT_DIR= -P /nix/store/q1nssraba326p2kp6627hldd2bhg254c-cmake-3.29.2/share/cmake-3.29/Modules/GoogleTestAddTests.cmake

# Rule to build all files generated by this target.
CMakeFiles/codec_tests.dir/build: codec_tests
.PHONY : CMakeFiles/codec_tests.dir/build

CMakeFiles/codec_tests.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/codec_tests.dir/cmake_clean.cmake
.PHONY : CMakeFiles/codec_tests.dir/clean

CMakeFiles/codec_tests.dir/depend:
	cd /home/runner/StreamCryptoDFS/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/runner/StreamCryptoDFS /home/runner/StreamCryptoDFS /home/runner/StreamCryptoDFS/build /home/runner/StreamCryptoDFS/build /home/runner/StreamCryptoDFS/build/CMakeFiles/codec_tests.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/codec_tests.dir/depend

