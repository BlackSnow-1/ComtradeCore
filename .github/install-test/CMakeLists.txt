cmake_minimum_required(VERSION 3.14)
project(ComtradeCoreInstallTest LANGUAGES CXX)

find_package(ComtradeCore 0.1 CONFIG REQUIRED)

add_executable(comtrade_install_test main.cpp)
target_link_libraries(comtrade_install_test PRIVATE comtrade::ComtradeCore)

enable_testing()
add_test(NAME ComtradeCore.InstallPackage COMMAND comtrade_install_test)
