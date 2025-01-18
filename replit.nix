{pkgs}: {
  deps = [
    pkgs.gtest
    pkgs.gdb
    pkgs.pkg-config
    pkgs.boost
    pkgs.openssl
    pkgs.gcc
    pkgs.cmake
  ];
}
