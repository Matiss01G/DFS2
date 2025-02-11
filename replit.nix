{pkgs}: {
  deps = [
    pkgs.gtest
    pkgs.gdb
    pkgs.pkg-config
    pkgs.openssl
    pkgs.boost
    pkgs.gcc
    pkgs.cmake
  ];
}
