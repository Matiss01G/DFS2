{pkgs}: {
  deps = [
    pkgs.pkg-config
    pkgs.openssl
    pkgs.boost
    pkgs.cmake
    pkgs.gcc
    pkgs.gtest
  ];
}
