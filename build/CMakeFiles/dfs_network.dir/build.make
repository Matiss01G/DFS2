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
include CMakeFiles/dfs_network.dir/depend.make
# Include any dependencies generated by the compiler for this target.
include CMakeFiles/dfs_network.dir/compiler_depend.make

# Include the progress variables for this target.
include CMakeFiles/dfs_network.dir/progress.make

# Include the compile flags for this target's objects.
include CMakeFiles/dfs_network.dir/flags.make

CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o: CMakeFiles/dfs_network.dir/flags.make
CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o: /home/runner/StreamCryptoDFS/src/network/tcp_peer.cpp
CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o: CMakeFiles/dfs_network.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_1) "Building CXX object CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o -MF CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o.d -o CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o -c /home/runner/StreamCryptoDFS/src/network/tcp_peer.cpp

CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.i"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/runner/StreamCryptoDFS/src/network/tcp_peer.cpp > CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.i

CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.s"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/runner/StreamCryptoDFS/src/network/tcp_peer.cpp -o CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.s

CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o: CMakeFiles/dfs_network.dir/flags.make
CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o: /home/runner/StreamCryptoDFS/src/network/peer_manager.cpp
CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o: CMakeFiles/dfs_network.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_2) "Building CXX object CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o -MF CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o.d -o CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o -c /home/runner/StreamCryptoDFS/src/network/peer_manager.cpp

CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.i"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/runner/StreamCryptoDFS/src/network/peer_manager.cpp > CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.i

CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.s"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/runner/StreamCryptoDFS/src/network/peer_manager.cpp -o CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.s

CMakeFiles/dfs_network.dir/src/network/channel.cpp.o: CMakeFiles/dfs_network.dir/flags.make
CMakeFiles/dfs_network.dir/src/network/channel.cpp.o: /home/runner/StreamCryptoDFS/src/network/channel.cpp
CMakeFiles/dfs_network.dir/src/network/channel.cpp.o: CMakeFiles/dfs_network.dir/compiler_depend.ts
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_3) "Building CXX object CMakeFiles/dfs_network.dir/src/network/channel.cpp.o"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -MD -MT CMakeFiles/dfs_network.dir/src/network/channel.cpp.o -MF CMakeFiles/dfs_network.dir/src/network/channel.cpp.o.d -o CMakeFiles/dfs_network.dir/src/network/channel.cpp.o -c /home/runner/StreamCryptoDFS/src/network/channel.cpp

CMakeFiles/dfs_network.dir/src/network/channel.cpp.i: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Preprocessing CXX source to CMakeFiles/dfs_network.dir/src/network/channel.cpp.i"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -E /home/runner/StreamCryptoDFS/src/network/channel.cpp > CMakeFiles/dfs_network.dir/src/network/channel.cpp.i

CMakeFiles/dfs_network.dir/src/network/channel.cpp.s: cmake_force
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green "Compiling CXX source to assembly CMakeFiles/dfs_network.dir/src/network/channel.cpp.s"
	/nix/store/9bv7dcvmfcjnmg5mnqwqlq2wxfn8d7yi-gcc-wrapper-13.2.0/bin/g++ $(CXX_DEFINES) $(CXX_INCLUDES) $(CXX_FLAGS) -S /home/runner/StreamCryptoDFS/src/network/channel.cpp -o CMakeFiles/dfs_network.dir/src/network/channel.cpp.s

# Object files for target dfs_network
dfs_network_OBJECTS = \
"CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o" \
"CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o" \
"CMakeFiles/dfs_network.dir/src/network/channel.cpp.o"

# External object files for target dfs_network
dfs_network_EXTERNAL_OBJECTS =

libdfs_network.a: CMakeFiles/dfs_network.dir/src/network/tcp_peer.cpp.o
libdfs_network.a: CMakeFiles/dfs_network.dir/src/network/peer_manager.cpp.o
libdfs_network.a: CMakeFiles/dfs_network.dir/src/network/channel.cpp.o
libdfs_network.a: CMakeFiles/dfs_network.dir/build.make
libdfs_network.a: CMakeFiles/dfs_network.dir/link.txt
	@$(CMAKE_COMMAND) -E cmake_echo_color "--switch=$(COLOR)" --green --bold --progress-dir=/home/runner/StreamCryptoDFS/build/CMakeFiles --progress-num=$(CMAKE_PROGRESS_4) "Linking CXX static library libdfs_network.a"
	$(CMAKE_COMMAND) -P CMakeFiles/dfs_network.dir/cmake_clean_target.cmake
	$(CMAKE_COMMAND) -E cmake_link_script CMakeFiles/dfs_network.dir/link.txt --verbose=$(VERBOSE)

# Rule to build all files generated by this target.
CMakeFiles/dfs_network.dir/build: libdfs_network.a
.PHONY : CMakeFiles/dfs_network.dir/build

CMakeFiles/dfs_network.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/dfs_network.dir/cmake_clean.cmake
.PHONY : CMakeFiles/dfs_network.dir/clean

CMakeFiles/dfs_network.dir/depend:
	cd /home/runner/StreamCryptoDFS/build && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/runner/StreamCryptoDFS /home/runner/StreamCryptoDFS /home/runner/StreamCryptoDFS/build /home/runner/StreamCryptoDFS/build /home/runner/StreamCryptoDFS/build/CMakeFiles/dfs_network.dir/DependInfo.cmake "--color=$(COLOR)"
.PHONY : CMakeFiles/dfs_network.dir/depend

