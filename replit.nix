{pkgs}: {
  deps = [
    pkgs.pkg-config
    pkgs.gcc
    pkgs.boost
    pkgs.gtest
    pkgs.openssl
    pkgs.cmake
  ];
}
